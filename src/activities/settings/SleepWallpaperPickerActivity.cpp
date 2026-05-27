#include "SleepWallpaperPickerActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/boot_sleep/SleepWallpaperCache.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr const char* kSleepWallpaperDir = "/sleep";

void sortWallpaperList(std::vector<std::string>& entries) {
  std::sort(entries.begin(), entries.end(), [](const std::string& lhs, const std::string& rhs) {
    const char* left = lhs.c_str();
    const char* right = rhs.c_str();

    while (*left != '\0' && *right != '\0') {
      if (std::isdigit(static_cast<unsigned char>(*left)) && std::isdigit(static_cast<unsigned char>(*right))) {
        while (*left == '0') left++;
        while (*right == '0') right++;

        int leftDigits = 0;
        int rightDigits = 0;
        while (std::isdigit(static_cast<unsigned char>(left[leftDigits]))) leftDigits++;
        while (std::isdigit(static_cast<unsigned char>(right[rightDigits]))) rightDigits++;

        if (leftDigits != rightDigits) {
          return leftDigits < rightDigits;
        }

        for (int i = 0; i < leftDigits; i++) {
          if (left[i] != right[i]) {
            return left[i] < right[i];
          }
        }

        left += leftDigits;
        right += rightDigits;
      } else {
        const auto leftChar = static_cast<char>(std::tolower(static_cast<unsigned char>(*left)));
        const auto rightChar = static_cast<char>(std::tolower(static_cast<unsigned char>(*right)));
        if (leftChar != rightChar) {
          return leftChar < rightChar;
        }
        left++;
        right++;
      }
    }

    return *left == '\0' && *right != '\0';
  });
}
}  // namespace

void SleepWallpaperPickerActivity::onEnter() {
  Activity::onEnter();
  loadWallpapers();
  requestUpdate();
}

void SleepWallpaperPickerActivity::onExit() {
  Activity::onExit();
  files.clear();
}

void SleepWallpaperPickerActivity::loadWallpapers() {
  files.clear();

  auto dir = Storage.open(kSleepWallpaperDir);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    selectedIndex = 0;
    return;
  }

  dir.rewindDirectory();

  char name[500];
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    file.getName(name, sizeof(name));
    if (file.isDirectory() || name[0] == '.' || !FsHelpers::hasBmpExtension(name)) {
      file.close();
      continue;
    }

    files.emplace_back(name);
    file.close();
  }

  dir.close();
  sortWallpaperList(files);

  // Empty path → "Random from folder" sentinel (index 0 once the synthetic entry
  // is overlaid in the list rendering).
  if (SETTINGS.customSleepImagePath[0] == '\0') {
    selectedIndex = 0;
    return;
  }
  const auto it = std::find(files.begin(), files.end(), SETTINGS.customSleepImagePath);
  // The synthetic "Random" entry occupies row 0, so file rows are offset by 1
  // when at least one wallpaper is available.
  if (it != files.end()) {
    selectedIndex = static_cast<int>(it - files.begin()) + (files.empty() ? 0 : 1);
  } else {
    selectedIndex = 0;
  }
}

void SleepWallpaperPickerActivity::onBack() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}

void SleepWallpaperPickerActivity::handleSelection() {
  if (files.empty()) {
    return;
  }

  // Index 0 (when files exist) is the synthetic "Random rotation" entry.
  if (selectedIndex == 0) {
    SETTINGS.customSleepImagePath[0] = '\0';
    finish();
    return;
  }

  const auto fileIndex = static_cast<size_t>(selectedIndex - 1);
  const std::string& selectedFile = files[fileIndex];
  std::snprintf(SETTINGS.customSleepImagePath, sizeof(SETTINGS.customSleepImagePath), "%s", selectedFile.c_str());

  const std::string cacheSourcePath = std::string(kSleepWallpaperDir) + "/" + selectedFile;
  if (!SleepWallpaperCache::warmCache(renderer, cacheSourcePath)) {
    LOG_ERR("SLP", "Failed to warm sleep wallpaper cache for %s", cacheSourcePath.c_str());
  }

  finish();
}

void SleepWallpaperPickerActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onBack();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  if (files.empty()) {
    return;
  }

  // +1 for the synthetic "Random rotation" entry shown when wallpapers exist.
  const int itemCount = static_cast<int>(files.size()) + 1;
  buttonNavigator.onNextRelease([this, itemCount] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, itemCount] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount);
    requestUpdate();
  });
}

void SleepWallpaperPickerActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SELECT_WALLPAPER));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (files.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_FILES_FOUND));
    renderer.drawText(SMALL_FONT_ID, metrics.contentSidePadding, contentTop + 48, kSleepWallpaperDir);
  } else {
    // Layout: row 0 = synthetic "Random rotation" entry, rows 1..N = wallpapers.
    const int totalRows = static_cast<int>(files.size()) + 1;
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, totalRows, selectedIndex,
        [this](int index) {
          if (index == 0) return std::string(tr(STR_RANDOM_ROTATION));
          return files[static_cast<size_t>(index - 1)];
        },
        nullptr, nullptr,
        [this](int index) {
          if (index == 0) {
            return SETTINGS.customSleepImagePath[0] == '\0' ? std::string(tr(STR_SELECTED)) : std::string{};
          }
          return files[static_cast<size_t>(index - 1)] == SETTINGS.customSleepImagePath
                     ? std::string(tr(STR_SELECTED))
                     : std::string{};
        },
        true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
