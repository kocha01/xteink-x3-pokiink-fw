#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// SdAutoRecovery — boot-time firmware install from SD card.
//
// Called once from main.cpp's setup() right after Storage.begin() and BEFORE
// the display / activity stack come up.  Scans the SD root for a recovery
// firmware file, verifies it's a same-board PokiInk binary, writes it to the
// inactive OTA partition, flips otadata, and reboots.
//
// Designed as the "escape hatch" for users whose currently installed firmware
// is bricked (X4 binary on X3 hardware, corrupted settings, etc.):
//   1. They drop a recovery file at the SD root from a PC
//   2. Power-cycle the device
//   3. PokiInk boots from this same SD file before the broken firmware has
//      a chance to run anything destructive
//   4. Recovery completes in ~10 seconds, then reboots into the new firmware
//
// File search order (first match wins, rest are ignored this boot):
//   /pokiink-recovery.bin   — preferred name, matches docs
//   /pokiink-update.bin     — alternate spelling some forums use
//   /update.bin             — Xteink OEM bootloader convention; accepting it
//                             here means the same SD card works as a fallback
//                             for both the PokiInk boot-time auto-flow AND
//                             the Xteink LEFT+POWER hardware recovery, if
//                             that one is also still functional
//
// All filenames must be at the SD root (no subdirectory).  All must be raw
// app-only binaries (firmware.bin format), NOT full-flash images (recovery.bin
// format).  A full-flash image would have its bootloader bytes flashed into
// the middle of an app partition and fail validation.
//
// Failure modes & rename suffixes (visible on the SD card after boot):
//   .rejected.size         — file size outside [256 KB, 6 MB] sanity bounds
//   .rejected.readerr      — couldn't read the header off SD
//   .rejected.notesp32     — first byte isn't 0xE9
//   .rejected.noappdesc    — app_desc magic at offset 32 isn't 0xABCD5432
//   .rejected.nopartition  — no inactive OTA partition (shouldn't happen)
//   .rejected.toolarge     — file bigger than the partition it would go to
//   .rejected.eraseerr     — partition erase failed
//   .rejected.seekerr      — seek-to-zero on the file handle failed
//   .rejected.shortread    — file read returned fewer bytes than requested
//   .rejected.writeerr     — partition write failed mid-stream
//   .rejected.wrongboard   — POKIINK_X3_FW_MAGIC not found (probably X4 or
//                            stock Xteink) — refused to switch boot target,
//                            so the device still boots its current slot
//   .rejected.nootadata    — otadata partition lookup failed
//   .rejected.otadata_erase / .rejected.otadata_write — otadata write failed
//   .applied               — SUCCESS: file flashed + otadata flipped; the
//                            rename prevents re-flashing on next boot
//
// The function performs NO display output — it's called too early in boot to
// safely drive the e-ink panel, and the typical caller is "previous firmware
// just bricked the display anyway, recovery has to work blind."  All
// observability is via Serial log (subsystem tag "RECOV").
// ─────────────────────────────────────────────────────────────────────────────

namespace SdAutoRecovery {

// Outcome of a flash attempt.  We expose the failure category (not just a
// bool) so the Settings-menu UI can react specifically to WRONG_BOARD —
// that's the one rejection a knowledgeable user might legitimately want
// to override (e.g. installing CrossInk for X3 on a PokiInk-X3 device, or
// downgrading to stock Xteink), and we offer a "Force install" path for
// it.  Every other failure is a real problem (bad file, hardware error,
// truncated download) where bypassing the check would just waste another
// flash cycle.
enum class FlashResult {
  SUCCESS,            // Boot target switched; caller should reboot.
  OPEN_FAIL,          // SD file couldn't be opened (missing / SD glitch).
  SIZE_OUT_OF_RANGE,  // File too small (< 256 KB) or too big (> 6 MB).
  BAD_HEADER,         // ESP32 image magic / app_desc magic mismatch.
  PARTITION_ERROR,    // Couldn't find or erase the inactive OTA partition.
  WRITE_FAIL,         // esp_partition_write returned an error mid-stream.
  WRONG_BOARD,        // POKIINK_X3_FW_MAGIC not found in flashed partition.
                      // Caller may retry with skipBoardCheck=true if the
                      // user accepts the brick risk.
  OTADATA_FAIL,       // Flash succeeded but otadata couldn't be flipped.
};

// Verify a single file on the SD card and flash it to the inactive OTA
// partition.  Returns FlashResult::SUCCESS on success (caller should
// reboot); any other value means the source file got renamed with a
// .rejected.<reason> suffix so the user can diagnose on a PC.  Used by:
//   * runIfRequested() below — the auto-discovery flow
//   * SdFirmwareUpdateActivity — the Settings → System → Update from SD
//     menu, which lets the user pick any .bin file by name rather than
//     relying on the hardcoded filename list
//
// `path` is an absolute path on the SD root (e.g. "/myfirmware.bin").
// Verification gates: size, ESP32 image magic, app_desc magic, and
// POKIINK_X3_FW_MAGIC marker (the board-tag check).
//
// `skipBoardCheck=true` SKIPS the POKIINK_X3_FW_MAGIC verification.  The
// other gates still run.  Intended for explicit user opt-in from the
// Settings menu's "Force install" prompt after the safe path already
// flagged the file as wrong-board.  USING THIS ON AN X4 BINARY WILL BRICK
// AN X3 DEVICE — it bypasses the only line of defense we have against
// blind board-mismatch installs, so callers must surface a clear warning
// to the user first.
FlashResult flashFromFile(const char* path, bool skipBoardCheck = false);

// Quick pre-flight: read the first 256 KB of a file on SD and check for the
// POKIINK_X3_FW_MAGIC marker.  Returns true if the marker is present
// (file is a PokiInk-X3 build) or false otherwise.  Takes ~250 ms on a
// typical SD card.
//
// Used by SdFirmwareUpdateActivity to decide BEFORE the multi-second flash
// whether to immediately route to the "Force install" warning.  Without
// this pre-check the user would have to wait through a full flash + verify
// cycle (~30-60 s) only to discover the file is wrong-board.
//
// Returns false on any I/O error too — that means the file is also bad
// for flashing, so the caller can treat both cases the same way (don't
// auto-flash, show some kind of warning).
bool hasMagicInFile(const char* path);

// Scan SD root for a recovery file, verify, flash to inactive OTA partition,
// flip otadata, reboot.  Returns immediately if no candidate file is found.
//
// Two operating modes:
//   * normal (forceRecoveryMode=false, default): scans the SD root ONCE for
//     a known recovery filename.  Returns immediately if none is present so
//     that boot can continue to the running firmware.  Designed for the
//     "previous firmware crashed, drop file on SD, power-cycle" workflow
//     where the file is in place before the device powers on.
//   * forced recovery (forceRecoveryMode=true): BLOCKS for up to 60 seconds
//     polling the SD root every 500 ms.  Use this when the user explicitly
//     asks for recovery (e.g. holds UP+POWER at boot — the Xteink OEM
//     bootloader behaviour PokiInk users expect to inherit) and may not
//     have inserted the SD card yet.  Returns once a valid file is found
//     and flashed (then reboots), or after the 60-second deadline expires.
//
// Side effects on success: reboots the device (does not return).
// Side effects on failure: renames the candidate file with a .rejected.*
// suffix and returns normally so boot can continue.
void runIfRequested(bool forceRecoveryMode = false);

}  // namespace SdAutoRecovery
