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

// Scan SD root for a recovery file, verify, flash to inactive OTA partition,
// flip otadata, reboot.  Returns immediately if no candidate file is found.
//
// Side effects on success: reboots the device (does not return).
// Side effects on failure: renames the candidate file with a .rejected.*
// suffix and returns normally so boot can continue.
void runIfRequested();

}  // namespace SdAutoRecovery
