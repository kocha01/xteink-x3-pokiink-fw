#include "SdFirmwareUpdateActivity.h"

#include <Arduino.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>

#include "MappedInputManager.h"
#include "SdAutoRecovery.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

// Lowercase check for ".bin" extension — Storage iteration sometimes returns
// "FOO.BIN" with different casing.  FsHelpers doesn't have a hasBinExtension
// helper, so do it inline.
bool hasBinExtension(const char* name) {
  if (!name) return false;
  const size_t len = std::strlen(name);
  if (len < 5) return false;
  const char* ext = name + len - 4;
  return (ext[0] == '.' &&
          (ext[1] == 'b' || ext[1] == 'B') &&
          (ext[2] == 'i' || ext[2] == 'I') &&
          (ext[3] == 'n' || ext[3] == 'N'));
}

bool endsWith(const char* haystack, const char* needle) {
  const size_t hl = std::strlen(haystack);
  const size_t nl = std::strlen(needle);
  if (nl > hl) return false;
  return std::strcmp(haystack + (hl - nl), needle) == 0;
}

}  // namespace

void SdFirmwareUpdateActivity::loadFileList() {
  files.clear();

  auto dir = Storage.open("/");
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    LOG_ERR("SDFW", "SD root not openable as directory");
    return;
  }
  dir.rewindDirectory();

  char name[256];
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    file.getName(name, sizeof(name));
    // Skip directories and macOS metadata sidecars
    if (file.isDirectory() || name[0] == '.') {
      file.close();
      continue;
    }
    // Only .bin files
    if (!hasBinExtension(name)) {
      file.close();
      continue;
    }
    // Hide files the boot-time auto-recovery already consumed
    if (endsWith(name, ".applied")) {
      file.close();
      continue;
    }

    files.push_back({std::string(name), static_cast<size_t>(file.size())});
    file.close();
  }
  dir.close();

  // Sort alphabetically — predictable order across mount/remount cycles so
  // the user's muscle memory ("update.bin is the 3rd one") survives.
  std::sort(files.begin(), files.end(),
            [](const FileInfo& a, const FileInfo& b) { return a.name < b.name; });

  if (selectedIndex >= static_cast<int>(files.size())) {
    selectedIndex = files.empty() ? 0 : static_cast<int>(files.size()) - 1;
  }
}

void SdFirmwareUpdateActivity::onEnter() {
  Activity::onEnter();

  loadFileList();
  state = files.empty() ? State::EMPTY : State::LIST;
  selectedIndex = 0;
  requestUpdate();
}

void SdFirmwareUpdateActivity::enterConfirm() {
  if (files.empty() || selectedIndex < 0 ||
      selectedIndex >= static_cast<int>(files.size())) {
    return;
  }
  state = State::CONFIRM;
  requestUpdate();
}

void SdFirmwareUpdateActivity::performFlash() {
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(files.size())) {
    state = State::FAILED;
    failedReason = "Invalid selection";
    return;
  }

  flashingFilename = files[selectedIndex].name;
  state = State::FLASHING;
  requestUpdateAndWait();  // ensure FLASHING screen renders before we block

  const std::string fullPath = "/" + flashingFilename;
  LOG_INF("SDFW", "User requested flash of %s", fullPath.c_str());

  const bool ok = SdAutoRecovery::flashFromFile(fullPath.c_str());

  if (ok) {
    state = State::REBOOTING;
    requestUpdateAndWait();
    delay(2500);    // hold the "Rebooting..." screen briefly so the user sees it
    ESP.restart();  // does not return
  }

  state = State::FAILED;
  // We don't have direct access to the .rejected.<reason> suffix that
  // SdAutoRecovery wrote, but we know the file was renamed.  Tell the user
  // to inspect the SD card on a PC and look for *.rejected.* if they want
  // the specifics; the high-level reasons map cleanly to the suffix list
  // documented in SdAutoRecovery.h.
  failedReason = "Verification failed — see SD card for *.rejected.* file with reason";
  requestUpdate();
}

void SdFirmwareUpdateActivity::loop() {
  if (state == State::LIST) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
      return;
    }
    if (files.empty()) return;
    if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
      selectedIndex = (selectedIndex - 1 + static_cast<int>(files.size())) % static_cast<int>(files.size());
      requestUpdate();
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
      selectedIndex = (selectedIndex + 1) % static_cast<int>(files.size());
      requestUpdate();
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      enterConfirm();
    }
    return;
  }

  if (state == State::CONFIRM) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      // User chickened out — back to file list.
      state = State::LIST;
      requestUpdate();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      performFlash();
    }
    return;
  }

  if (state == State::EMPTY || state == State::FAILED) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      // Reload the list — user may have inserted/removed SD card or fixed
      // the rejected file.
      loadFileList();
      state = files.empty() ? State::EMPTY : State::LIST;
      requestUpdate();
    }
    return;
  }

  // State::FLASHING / State::REBOOTING — no input handled (we're either
  // blocked inside performFlash or about to reboot).
}

void SdFirmwareUpdateActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                 tr(STR_SD_FIRMWARE_UPDATE));

  const auto lineH = renderer.getLineHeight(UI_10_FONT_ID);

  if (state == State::FLASHING) {
    const int top = (pageHeight - lineH * 3) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_SD_FW_FLASHING), true, EpdFontFamily::BOLD);
    const std::string truncated =
        renderer.truncatedText(SMALL_FONT_ID, flashingFilename.c_str(),
                               pageWidth - metrics.contentSidePadding * 2, EpdFontFamily::REGULAR);
    renderer.drawCenteredText(SMALL_FONT_ID, top + lineH + metrics.verticalSpacing, truncated.c_str());
    renderer.drawCenteredText(SMALL_FONT_ID, top + lineH * 2 + metrics.verticalSpacing * 2,
                              tr(STR_SD_FW_DO_NOT_POWER_OFF));
    renderer.displayBuffer();
    return;
  }
  if (state == State::REBOOTING) {
    const int top = (pageHeight - lineH) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_SD_FW_REBOOTING), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }
  if (state == State::FAILED) {
    const int top = (pageHeight - lineH * 3) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_SD_FW_FAILED), true, EpdFontFamily::BOLD);
    const std::string truncated =
        renderer.truncatedText(SMALL_FONT_ID, failedReason.c_str(),
                               pageWidth - metrics.contentSidePadding * 2, EpdFontFamily::REGULAR);
    renderer.drawText(SMALL_FONT_ID, metrics.contentSidePadding,
                      top + lineH + metrics.verticalSpacing, truncated.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }
  if (state == State::EMPTY) {
    const int top = (pageHeight - lineH * 4) / 2;
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_SD_FW_NO_FILES), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(SMALL_FONT_ID, top + lineH + metrics.verticalSpacing,
                              tr(STR_SD_FW_NO_FILES_HINT_1));
    renderer.drawCenteredText(SMALL_FONT_ID, top + lineH * 2 + metrics.verticalSpacing,
                              tr(STR_SD_FW_NO_FILES_HINT_2));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_REFRESH), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }
  if (state == State::CONFIRM) {
    const auto& f = files[selectedIndex];
    const int top = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 3;
    int y = top;
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_SD_FW_CONFIRM_TITLE), true, EpdFontFamily::BOLD);
    y += lineH + metrics.verticalSpacing * 2;

    const std::string truncatedName =
        renderer.truncatedText(SMALL_FONT_ID, f.name.c_str(),
                               pageWidth - metrics.contentSidePadding * 2, EpdFontFamily::BOLD);
    renderer.drawCenteredText(SMALL_FONT_ID, y, truncatedName.c_str(), true, EpdFontFamily::BOLD);
    y += lineH + metrics.verticalSpacing;

    // Size in MB with one decimal
    char sizeBuf[32];
    snprintf(sizeBuf, sizeof(sizeBuf), "%.1f MB", static_cast<double>(f.sizeBytes) / (1024.0 * 1024.0));
    renderer.drawCenteredText(SMALL_FONT_ID, y, sizeBuf);
    y += lineH + metrics.verticalSpacing * 3;

    renderer.drawCenteredText(SMALL_FONT_ID, y, tr(STR_SD_FW_CONFIRM_WARN_1));
    y += lineH;
    renderer.drawCenteredText(SMALL_FONT_ID, y, tr(STR_SD_FW_CONFIRM_WARN_2));

    const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_SD_FW_CONFIRM_FLASH), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // State::LIST — main file picker
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight},
      static_cast<int>(files.size()), selectedIndex,
      [this](int i) { return files[i].name; },
      [this](int i) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%.1f MB",
                 static_cast<double>(files[i].sizeBytes) / (1024.0 * 1024.0));
        return std::string(buf);
      });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
