"""
PlatformIO post-build script: generate a full-flash recovery image.

The normal firmware.bin is only the application image for 0x10000. On devices
using dual OTA slots, flashing app-only binaries can be confusing if the
bootloader is currently pointed at the other slot. This merged image bundles
bootloader + partitions + clean otadata + firmware so it can be flashed once at
0x0000 as a deterministic recovery/update image.

WHY WE GENERATE OUR OWN OTADATA (instead of arduino-esp32's boot_app0.bin):
The framework's boot_app0.bin uses ota_seq=1 with state=UNDEFINED.  That
worked fine on fresh hardware, but on devices that had cycled OTA a few
times (each OTA bumps ota_seq), residual state in flash made the boot
selection ambiguous and produced the "have to flash twice for the new
firmware to actually run" symptom users reported.  This script writes a
hand-built otadata that:
  - Uses ota_seq = OTADATA_BOOT_SEQ (a large odd number, well above any
    realistic OTA-bumped seq) so the bootloader's max-seq vote can't be
    won by a leftover sector.
  - Maps to slot 0 (app0) because (OTADATA_BOOT_SEQ - 1) % 2 == 0.
  - Has state = ESP_OTA_IMG_VALID (0x2) explicitly, so the bootloader's
    rollback timer doesn't start counting against the freshly flashed image.
  - Has sector 1 of the otadata partition written to all-0xFF (erased
    state, CRC mismatch → bootloader ignores it), removing ANY possibility
    that stale higher-seq data in that sector could be picked.

Result: USB flash takes effect on the first try, no matter what state the
device's otadata was in before.
"""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib


# Hand-built otadata constants ───────────────────────────────────────────────
# esp_ota_select_entry_t (32 bytes):
#   uint32_t ota_seq          (offset 0, 4 bytes)
#   uint8_t  seq_label[20]    (offset 4, 20 bytes; 0xFF for "no label")
#   uint32_t ota_state        (offset 24, 4 bytes; see esp_ota_img_states_t)
#   uint32_t crc              (offset 28, 4 bytes; CRC-32 of ota_seq only)
OTADATA_PARTITION_SIZE = 0x2000      # 8 KB total (two 4 KB sectors)
OTADATA_SECTOR_SIZE    = 0x1000      # 4 KB per sector
OTADATA_BOOT_SEQ       = 999         # odd → (999-1) % 2 = 0 → app0; high enough to win any leftover
OTADATA_STATE_VALID    = 0x00000002  # ESP_OTA_IMG_VALID — boot without rollback timer
OTADATA_CRC_SEED       = 0xFFFFFFFF  # matches ESP-IDF's esp_rom_crc32_le(UINT32_MAX, ...)


def _make_otadata_bin() -> bytes:
    """Build the 8 KB otadata blob that points to app0 cleanly."""
    crc = zlib.crc32(struct.pack("<I", OTADATA_BOOT_SEQ), OTADATA_CRC_SEED) & 0xFFFFFFFF

    sector0 = (
        struct.pack("<I", OTADATA_BOOT_SEQ)        # ota_seq
        + b"\xff" * 20                              # seq_label (unused)
        + struct.pack("<I", OTADATA_STATE_VALID)   # ota_state
        + struct.pack("<I", crc)                    # crc
    )
    sector0 = sector0.ljust(OTADATA_SECTOR_SIZE, b"\xff")

    # Sector 1: deliberately all 0xFF.  Bootloader's CRC check on the all-FF
    # bytes will fail (stored crc=0xFFFFFFFF, computed crc of all-FF seq won't
    # match), so this sector is treated as invalid and ignored.  This is what
    # we want — only sector 0 should be authoritative right after a flash.
    sector1 = b"\xff" * OTADATA_SECTOR_SIZE

    blob = sector0 + sector1
    assert len(blob) == OTADATA_PARTITION_SIZE, f"otadata blob size mismatch: {len(blob)}"
    return blob


def build_recovery(source, target, env):
    build_dir = Path(env.subst("$BUILD_DIR"))
    bootloader = build_dir / "bootloader.bin"
    partitions = build_dir / "partitions.bin"
    firmware = build_dir / "firmware.bin"
    recovery = build_dir / "recovery.bin"

    required = (bootloader, partitions, firmware)
    missing = [str(path) for path in required if not path.exists()]
    if missing:
        print(
            "WARNING [build_recovery_image.py]: missing build artifacts; skipping recovery image: "
            + ", ".join(missing),
            file=sys.stderr,
        )
        return

    # Drop our hand-built otadata to a temp file the esptool merge can ingest.
    # Use the build_dir so concurrent builds don't collide on a shared /tmp path.
    otadata_path = build_dir / "otadata_boot_app0.bin"
    otadata_path.write_bytes(_make_otadata_bin())

    cmd = [
        env.subst("$PYTHONEXE"),
        "-m",
        "esptool",
        "--chip",
        "esp32c3",
        "merge-bin",
        "-o",
        str(recovery),
        "--flash-mode",
        "dio",
        "--flash-freq",
        "80m",
        "--flash-size",
        "16MB",
        "0x0000",
        str(bootloader),
        "0x8000",
        str(partitions),
        "0xe000",
        str(otadata_path),
        "0x10000",
        str(firmware),
    ]

    subprocess.run(cmd, check=True)
    print(f"Generated recovery image: {recovery}")
    print(
        f"  otadata override: ota_seq={OTADATA_BOOT_SEQ}, "
        f"state=VALID, slot=app0 (one-shot USB flash should take effect immediately)"
    )


Import("env")
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", build_recovery)
