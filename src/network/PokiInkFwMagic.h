#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// PokiInk-X3 board identification marker.
//
// Embedded as a fixed ASCII string in .rodata so any PokiInk-X3 firmware
// binary contains it at SOME offset (always within the first ~256 KB because
// the linker places .flash.rodata right after .flash.appdesc).  The OTA
// updater scans the freshly-downloaded firmware partition for this exact
// string before committing the boot pointer — if absent, the binary is
// assumed to be for a different board (e.g. X4) and the update is rejected.
//
// This is the second line of defense against a brick.  The first is the
// OTA URL itself (which must point at a PokiInk-X3 release repo, not the
// upstream X4 release feed).  Together they keep an X4 binary from being
// flashed onto an X3 device even if the URL is misconfigured.
//
// The string must stay byte-stable across builds — DO NOT include build
// metadata (timestamps, git hashes) here; keep it as a fixed sentinel.
// ─────────────────────────────────────────────────────────────────────────────

#ifdef __cplusplus
extern "C" {
#endif

extern const char POKIINK_X3_FW_MAGIC[];

// Length excludes the terminating NUL — for memmem-style scans.
constexpr int POKIINK_X3_FW_MAGIC_LEN = 27;

#ifdef __cplusplus
}
#endif
