#include "FwSlotSwitchActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <cstring>

#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "bootloader_common.h"
#include "esp_flash_partitions.h"

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/PokiInkFwMagic.h"

FwSlotSwitchActivity::SlotInfo FwSlotSwitchActivity::readSlotInfo(int slotIndex) {
  SlotInfo info;
  info.slotIndex = slotIndex;

  const esp_partition_t* part = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP,
      static_cast<esp_partition_subtype_t>(ESP_PARTITION_SUBTYPE_APP_OTA_0 + slotIndex), nullptr);
  if (!part) return info;

  const esp_partition_t* running = esp_ota_get_running_partition();
  info.isActive = (running != nullptr && running->address == part->address);

  // ESP32 image header + app_desc magic check (same first-48-bytes inspection
  // the OTA installer does post-download).  Tells us whether this slot
  // contains *something* bootable — distinct from the PokiInk magic check
  // which tells us whether it's a same-board build.
  uint8_t buf[48];
  if (esp_partition_read(part, 0, buf, sizeof(buf)) != ESP_OK) return info;
  if (buf[0] != 0xE9) return info;
  uint32_t app_magic = 0;
  std::memcpy(&app_magic, buf + 32, sizeof(app_magic));
  if (app_magic != 0xABCD5432) return info;
  info.hasValidImage = true;

  // app_desc.version field is at offset 32 (start of app_desc) + 16 (after
  // magic/secure_version/reserv1) = 48, 32 bytes long.
  char ver[33];
  std::memset(ver, 0, sizeof(ver));
  if (esp_partition_read(part, 48, ver, 32) == ESP_OK) {
    ver[32] = 0;
    // Trim trailing whitespace/garbage (some builds null-pad, some don't)
    for (int i = 31; i >= 0; --i) {
      if (ver[i] == 0 || ver[i] == ' ') ver[i] = 0;
      else break;
    }
    info.version = std::string(ver);
  }

  // POKIINK_X3_FW_MAGIC is placed in .rodata_custom_desc, which the linker
  // wires immediately after the 256-byte app_desc → fixed flash offset 288
  // (0x120).  Constant-time check rather than the 256 KB scan the OTA does,
  // because we trust our own build's placement.
  char marker[POKIINK_X3_FW_MAGIC_LEN];
  std::memset(marker, 0, sizeof(marker));
  if (esp_partition_read(part, 288, marker, POKIINK_X3_FW_MAGIC_LEN) == ESP_OK) {
    if (std::memcmp(marker, POKIINK_X3_FW_MAGIC, POKIINK_X3_FW_MAGIC_LEN) == 0) {
      info.hasPokiInkMagic = true;
    }
  }

  return info;
}

bool FwSlotSwitchActivity::switchToOtherSlot() {
  if (otherSlot.slotIndex < 0 || !otherSlot.hasValidImage) {
    LOG_ERR("FWSW", "Cannot switch: other slot invalid (slot=%d hasImage=%d)",
            otherSlot.slotIndex, otherSlot.hasValidImage ? 1 : 0);
    return false;
  }

  // Defensive: refuse to switch to a slot that lacks the PokiInk magic.
  // Mirrors the OTA-time check.  Without this, a user could brick themselves
  // by switching to an X4 binary that happened to be left in the inactive
  // slot from a botched experiment.
  if (!otherSlot.hasPokiInkMagic) {
    LOG_ERR("FWSW", "Refusing to switch to slot %d: PokiInk magic missing", otherSlot.slotIndex);
    return false;
  }

  const esp_partition_t* otadata_part =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
  if (!otadata_part) {
    LOG_ERR("FWSW", "otadata partition not found");
    return false;
  }

  esp_ota_select_entry_t entry[2];
  esp_partition_read(otadata_part, 0, &entry[0], sizeof(entry[0]));
  esp_partition_read(otadata_part, 0x1000, &entry[1], sizeof(entry[1]));

  const bool valid0 = bootloader_common_ota_select_valid(&entry[0]);
  const bool valid1 = bootloader_common_ota_select_valid(&entry[1]);

  uint32_t max_seq = 0;
  if (valid0) max_seq = entry[0].ota_seq;
  if (valid1 && entry[1].ota_seq > max_seq) max_seq = entry[1].ota_seq;

  // Bootloader maps (ota_seq - 1) % num_partitions → slot index.  We pick a
  // seq that's both > max_seq AND lands on the target slot.
  uint32_t new_seq = max_seq + 1;
  if ((new_seq - 1) % 2 != static_cast<uint32_t>(otherSlot.slotIndex)) {
    new_seq++;
  }

  esp_ota_select_entry_t new_entry;
  std::memset(&new_entry, 0xFF, sizeof(new_entry));
  new_entry.ota_seq = new_seq;
  // VALID (not UNDEFINED) — this slot has already been booted-and-marked-
  // valid in a previous OTA cycle, so don't re-trigger the rollback timer.
  // The user is doing a manual switch, not an unverified update.
  new_entry.ota_state = ESP_OTA_IMG_VALID;
  new_entry.crc = bootloader_common_ota_select_crc(&new_entry);

  // Write to the sector with the LOWER seq (wear leveling).
  int write_sector;
  if (!valid0) {
    write_sector = 0;
  } else if (!valid1) {
    write_sector = 1;
  } else {
    write_sector = (entry[0].ota_seq <= entry[1].ota_seq) ? 0 : 1;
  }
  const uint32_t write_offset = static_cast<uint32_t>(write_sector) * 0x1000;

  esp_err_t esp_err = esp_partition_erase_range(otadata_part, write_offset, 0x1000);
  if (esp_err != ESP_OK) {
    LOG_ERR("FWSW", "otadata erase failed: %s", esp_err_to_name(esp_err));
    return false;
  }
  esp_err = esp_partition_write(otadata_part, write_offset, &new_entry, sizeof(new_entry));
  if (esp_err != ESP_OK) {
    LOG_ERR("FWSW", "otadata write failed: %s", esp_err_to_name(esp_err));
    return false;
  }

  LOG_INF("FWSW", "Switched to slot %d (ota_seq=%lu, sector=%d)", otherSlot.slotIndex,
          static_cast<unsigned long>(new_seq), write_sector);
  return true;
}

void FwSlotSwitchActivity::onEnter() {
  Activity::onEnter();

  const esp_partition_t* running = esp_ota_get_running_partition();
  int activeSlot = 0;
  if (running) {
    activeSlot = running->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0;
    if (activeSlot < 0 || activeSlot > 1) activeSlot = 0;
  }
  currentSlot = readSlotInfo(activeSlot);
  otherSlot = readSlotInfo(activeSlot == 0 ? 1 : 0);

  LOG_INF("FWSW", "Current slot=%d ver=\"%s\" magic=%d | Other slot=%d valid=%d magic=%d ver=\"%s\"",
          currentSlot.slotIndex, currentSlot.version.c_str(), currentSlot.hasPokiInkMagic ? 1 : 0,
          otherSlot.slotIndex, otherSlot.hasValidImage ? 1 : 0, otherSlot.hasPokiInkMagic ? 1 : 0,
          otherSlot.version.c_str());

  state = State::READY;
  requestUpdate();
}

void FwSlotSwitchActivity::loop() {
  if (state == State::READY) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
      return;
    }
    // Allow confirm only when the other slot is switchable.
    const bool switchable = otherSlot.hasValidImage && otherSlot.hasPokiInkMagic;
    if (switchable && mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      state = State::SWITCHING;
      requestUpdateAndWait();
      const bool ok = switchToOtherSlot();
      if (!ok) {
        state = State::FAILED;
        failedAt = millis();
        requestUpdate();
        return;
      }
      state = State::REBOOTING;
      requestUpdate();
      delay(800);  // Let the e-ink finish drawing "Switching..." before we reset.
      ESP.restart();
    }
    return;
  }

  if (state == State::FAILED) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        (failedAt > 0 && (millis() - failedAt) > 3000)) {
      finish();
    }
    return;
  }
}

void FwSlotSwitchActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SWITCH_FW_SLOT));

  const auto lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const auto smallH = renderer.getLineHeight(SMALL_FONT_ID);

  if (state == State::SWITCHING || state == State::REBOOTING) {
    const int top = (pageHeight - lineH) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_FW_SLOT_SWITCHING), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }
  if (state == State::FAILED) {
    const int top = (pageHeight - lineH) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_FW_SLOT_SWITCH_FAILED), true, EpdFontFamily::BOLD);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // READY state: render two cards (current + other) stacked vertically.
  auto drawSlotCard = [&](int y, const char* heading, const SlotInfo& info) -> int {
    constexpr int cardPadX = 10;
    constexpr int cardPadY = 10;
    const int x = metrics.contentSidePadding;
    const int w = pageWidth - metrics.contentSidePadding * 2;
    const int textX = x + cardPadX;

    // Heading
    renderer.drawText(SMALL_FONT_ID, textX, y + cardPadY, heading, true, EpdFontFamily::REGULAR);

    // "Slot A" / "Slot B" + version on the next line
    const std::string slotLabel = std::string("Slot ") + (info.slotIndex == 0 ? "A" : "B");
    renderer.drawText(UI_10_FONT_ID, textX, y + cardPadY + smallH + 4, slotLabel.c_str(), true, EpdFontFamily::BOLD);

    std::string versionLine;
    if (!info.hasValidImage) {
      versionLine = tr(STR_FW_SLOT_EMPTY);
    } else if (!info.hasPokiInkMagic) {
      versionLine = info.version.empty() ? tr(STR_FW_SLOT_NOT_POKIINK)
                                         : info.version + " " + tr(STR_FW_SLOT_NOT_POKIINK);
    } else {
      versionLine = info.version.empty() ? "(no version field)" : info.version;
    }
    const std::string truncated =
        renderer.truncatedText(SMALL_FONT_ID, versionLine.c_str(), w - cardPadX * 2, EpdFontFamily::REGULAR);
    renderer.drawText(SMALL_FONT_ID, textX, y + cardPadY + smallH + lineH + 6, truncated.c_str());

    const int cardH = cardPadY + smallH + lineH + smallH + cardPadY + 6;
    renderer.drawRoundedRect(x, y, w, cardH, 1, 6, true);
    return cardH;
  };

  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
  const int currentCardH = drawSlotCard(y, tr(STR_FW_SLOT_CURRENT), currentSlot);
  y += currentCardH + metrics.verticalSpacing;
  drawSlotCard(y, tr(STR_FW_SLOT_OTHER), otherSlot);

  const bool switchable = otherSlot.hasValidImage && otherSlot.hasPokiInkMagic;
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), switchable ? tr(STR_FW_SLOT_SWITCH_AND_REBOOT) : "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
