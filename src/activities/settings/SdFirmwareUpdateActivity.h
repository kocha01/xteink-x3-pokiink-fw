#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"

// ─────────────────────────────────────────────────────────────────────────────
// SdFirmwareUpdateActivity — install firmware from a .bin file on the SD card
// via a Settings menu (Settings → System → Update from SD).
//
// Why this exists in addition to the boot-time SdAutoRecovery + UP+POWER
// recovery flows: those both require the device to either reboot first or
// be reset by the user.  This activity lets the user pick a file by name
// while the device is running normally — no power-cycle, no button combo.
//
// Recovery path coverage:
//   * USB-flash works                    → CrossPoint/EinkHub web flasher
//   * USB-flash blocked, OTA works       → Settings → System Update
//   * USB-flash blocked, no Wi-Fi        → THIS ACTIVITY  ⭐
//                                          (or boot-time SdAutoRecovery)
//   * Device won't boot                  → UP+POWER on boot
//                                          (or pre-2.3.1: boot-time
//                                          SdAutoRecovery with file pre-
//                                          dropped on SD before power-up)
//
// File selection:
//   * Scans SD root for *.bin files (case-insensitive).
//   * Hides files whose names end in `.applied` (already-flashed by the
//     boot-time auto-recovery — including them would just confuse users).
//   * Hides files starting with `.` (macOS metadata sidecars like
//     `._update.bin`).
//
// Verification gates: same as boot-time SdAutoRecovery
// (SdAutoRecovery::flashFromFile is called under the hood):
//   * Size in [256 KB, 6 MB]
//   * ESP32 image header magic 0xE9
//   * app_desc magic 0xABCD5432
//   * POKIINK_X3_FW_MAGIC marker found within first 256 KB
// On any rejection the source file gets renamed with a .rejected.<reason>
// suffix and the activity returns to its READY state with an error message
// — the user can pick a different file or back out.
//
// Buttons:
//   * Up/Down — move selection through the file list
//   * Confirm — show "Flash <filename>? Y/N" confirmation
//   * Back    — exit to Settings menu
// ─────────────────────────────────────────────────────────────────────────────

class SdFirmwareUpdateActivity final : public Activity {
 public:
  enum class State {
    LIST,             // Showing file list, waiting for selection
    CONFIRM,          // User picked a file; showing "Flash this? Y/N"
    FLASHING,         // verifyAndFlash in progress
    REBOOTING,        // Success — about to ESP.restart()
    FAILED,           // verify or flash failed for a reason that isn't
                      // wrong-board (size, header, hardware) — terminal
    WRONG_BOARD,      // File flashed cleanly but missing PokiInk magic.
                      // Offer "Force install" with a clear brick warning.
    FLASHING_FORCED,  // User confirmed Force install; verifyAndFlash
                      // running again with skipBoardCheck=true
    EMPTY,            // No .bin files on SD; instructions screen
  };

  struct FileInfo {
    std::string name;     // Filename (without leading "/")
    size_t sizeBytes = 0;
  };

 private:
  State state = State::LIST;
  std::vector<FileInfo> files;
  int selectedIndex = 0;
  std::string failedReason;     // Set when state == FAILED
  std::string flashingFilename; // Set during FLASHING/REBOOTING

  void loadFileList();
  void enterConfirm();
  void performFlash(bool skipBoardCheck);

 public:
  explicit SdFirmwareUpdateActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SdFirmwareUpdate", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
