#pragma once

#include <string>

#include "activities/Activity.h"

// ─────────────────────────────────────────────────────────────────────────────
// FwSlotSwitchActivity — manually flip the OTA boot pointer between app0/app1.
//
// The device has two app partitions (otadata picks which boots).  An OTA
// install writes to the inactive one and flips otadata.  This activity lets
// the user flip it back manually — useful for:
//   1. Testing a new build while keeping the previous build as a known-good
//      fallback ("if the new one is broken, just switch back").
//   2. Comparing two firmware versions side-by-side without re-flashing
//      over USB each time.
//   3. Manual rollback if an issue surfaces hours/days after the OTA, past
//      the auto-rollback window (which only triggers on boot-time crash).
//
// On confirm: writes a new otadata entry with higher seq pointing at the
// inactive slot (state = VALID so the auto-rollback guard doesn't kick in),
// then ESP.restart().  The bootloader on the next boot reads otadata, picks
// the higher seq, boots the other slot.
// ─────────────────────────────────────────────────────────────────────────────

class FwSlotSwitchActivity final : public Activity {
 public:
  enum class State {
    READY,       // Showing both slots + confirm hint
    SWITCHING,   // User confirmed; otadata write in progress
    FAILED,      // Other slot invalid / write failed
    REBOOTING,   // Brief flash before ESP.restart()
  };

  struct SlotInfo {
    int slotIndex = -1;       // 0 (app0) or 1 (app1)
    bool isActive = false;    // Currently running this slot
    bool hasValidImage = false;   // ESP32 image header magic + app_desc magic OK
    bool hasPokiInkMagic = false; // POKIINK_X3_FW_MAGIC marker found at offset 288
    std::string version;          // app_desc.version[32] field — best-effort
  };

 private:
  State state = State::READY;
  SlotInfo currentSlot;
  SlotInfo otherSlot;
  unsigned long failedAt = 0;

  static SlotInfo readSlotInfo(int slotIndex);
  bool switchToOtherSlot();

 public:
  explicit FwSlotSwitchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("FwSlotSwitch", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
