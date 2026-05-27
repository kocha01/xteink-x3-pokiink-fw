#include "PokiInkFwMagic.h"

// Place the marker into `.rodata_custom_desc` — the ESP-IDF linker script
// wires this section IMMEDIATELY after `.rodata_desc` (the 256-byte app
// descriptor), inside `.flash.appdesc`, so the marker always ends up at
// flash offset 0x120 (288) regardless of what else lands in rodata.
//
// We tried `.rodata.pokiink_fw_magic` first.  That fell into a generic
// wildcard near the bottom of the rodata wildcard chain and ended up at
// ~4.35 MB into the 6 MB firmware — past the OTA's 256 KB scan window,
// so the board-tag check spuriously rejected even valid PokiInk-X3 binaries.
// Using `.rodata_custom_desc` gives us a stable, near-start placement that
// keeps the scan fast and the verification deterministic.
//
// The "used" attribute prevents LTO from stripping the symbol when no other
// translation unit references it directly.
extern "C" {
__attribute__((used, section(".rodata_custom_desc")))
const char POKIINK_X3_FW_MAGIC[] = "POKIINK-X3-FW-MAGIC-MARKER";
// length excluding NUL = 26; POKIINK_X3_FW_MAGIC_LEN in header is 27 to
// include NUL for safer memcmp coverage during scan (acts as a delimiter
// so we don't false-match a substring of an adjacent rodata blob).
}
