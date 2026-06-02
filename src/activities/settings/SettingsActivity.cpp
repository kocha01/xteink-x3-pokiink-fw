#include "SettingsActivity.h"

#include <GfxRenderer.h>
#include <Logging.h>
#include <SdCardFontRegistry.h>

#include <algorithm>

#include "SdCardFontGlobals.h"
#include "SdCardFontSystem.h"

#include "ButtonRemapActivity.h"
#include <NtpClock.h>

#include "AboutActivity.h"
#include "CalibreSettingsActivity.h"
#include "ClearCacheActivity.h"
#include "CrossPointSettings.h"
#include "FwSlotSwitchActivity.h"
#include "KOReaderSettingsActivity.h"
#include "LanguageSelectActivity.h"
#include "MappedInputManager.h"
#include "OtaUpdateActivity.h"
#include "SdFirmwareUpdateActivity.h"
#include "SettingsList.h"
#include "SdCardFontPickerActivity.h"
#include "SleepWallpaperPickerActivity.h"
#include "StatusBarSettingsActivity.h"
#include "ThaiDictionaryActivity.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

#include <algorithm>

const StrId SettingsActivity::categoryNames[categoryCount] = {StrId::STR_CAT_DISPLAY, StrId::STR_CAT_READER,
                                                              StrId::STR_CAT_CONTROLS, StrId::STR_CAT_SYSTEM};

namespace {

std::string joinSummary(const std::string& a, const std::string& b, const std::string& c) {
  return a + " / " + b + " / " + c;
}

}  // namespace

void SettingsActivity::onEnter() {
  Activity::onEnter();

  // Build per-category vectors from the shared settings list
  displaySettings.clear();
  readerSettings.clear();
  controlsSettings.clear();
  systemSettings.clear();

  for (const auto& setting : getSettingsList()) {
    if (setting.category == StrId::STR_NONE_OPT) continue;
    if (setting.category == StrId::STR_CAT_DISPLAY) {
      if (setting.type == SettingType::STRING && setting.nameId == StrId::STR_SELECT_WALLPAPER) {
        continue;
      }
      displaySettings.push_back(setting);
    } else if (setting.category == StrId::STR_CAT_READER) {
      // sdFontFamilyName is registered as a SettingInfo::String purely to wire
      // up JSON persistence + the web settings page; the device UI uses a
      // dedicated Action row (SettingAction::SelectSdFont) that launches the
      // picker activity instead.
      if (setting.type == SettingType::STRING && setting.nameId == StrId::STR_SD_FONT) {
        continue;
      }
      readerSettings.push_back(setting);
    } else if (setting.category == StrId::STR_CAT_CONTROLS) {
      controlsSettings.push_back(setting);
    } else if (setting.category == StrId::STR_CAT_SYSTEM) {
      systemSettings.push_back(setting);
    }
    // Web-only categories (KOReader Sync, OPDS Browser) are skipped for device UI
  }

  // Append device-only ACTION items
  if (displaySettings.empty()) {
    displaySettings.push_back(SettingInfo::Action(StrId::STR_SELECT_WALLPAPER, SettingAction::SelectWallpaper));
  } else {
    displaySettings.insert(displaySettings.begin() + 1,
                           SettingInfo::Action(StrId::STR_SELECT_WALLPAPER, SettingAction::SelectWallpaper));
  }
  // Insert "Custom Font (SD)" picker right after the built-in font-family enum.
  // Position 1 puts it just below STR_FONT_FAMILY which is always the first
  // reader-category entry in SettingsList.h.
  if (readerSettings.empty()) {
    readerSettings.push_back(SettingInfo::Action(StrId::STR_SD_FONT, SettingAction::SelectSdFont));
  } else {
    readerSettings.insert(readerSettings.begin() + 1,
                          SettingInfo::Action(StrId::STR_SD_FONT, SettingAction::SelectSdFont));
  }

  controlsSettings.insert(controlsSettings.begin(),
                          SettingInfo::Action(StrId::STR_REMAP_FRONT_BUTTONS, SettingAction::RemapFrontButtons));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_WIFI_NETWORKS, SettingAction::Network));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_KOREADER_SYNC, SettingAction::KOReaderSync));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_OPDS_BROWSER, SettingAction::OPDSBrowser));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_CLEAR_READING_CACHE, SettingAction::ClearCache));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_CHECK_UPDATES, SettingAction::CheckForUpdates));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_SWITCH_FW_SLOT, SettingAction::SwitchFirmwareSlot));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_SD_FIRMWARE_UPDATE, SettingAction::SdFirmwareUpdate));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_LANGUAGE, SettingAction::Language));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_ABOUT, SettingAction::About));
  readerSettings.push_back(SettingInfo::Action(StrId::STR_CUSTOMISE_STATUS_BAR, SettingAction::CustomiseStatusBar));
  readerSettings.push_back(SettingInfo::Action(StrId::STR_THAI_DICTIONARY, SettingAction::ThaiDictionary));

  currentView = ViewMode::CategoryHome;
  selectedCategoryIndex = 0;
  selectedSettingIndex = 0;
  selectedSettingIndexByCategory = {0, 0, 0, 0};
  currentSettings = &displaySettings;
  settingsCount = static_cast<int>(displaySettings.size());

  requestUpdate();
}

void SettingsActivity::onExit() {
  Activity::onExit();

  UITheme::getInstance().reload();  // Re-apply theme in case it was changed
  NTP_CLOCK.applyTimezoneByIndex(SETTINGS.timezone);  // Apply timezone in case it was changed
}

void SettingsActivity::enterCategory(const int categoryIndex) {
  if (categoryIndex < 0 || categoryIndex >= categoryCount) {
    return;
  }

  selectedCategoryIndex = categoryIndex;
  currentView = ViewMode::CategorySettings;

  switch (selectedCategoryIndex) {
    case 0:
      currentSettings = &displaySettings;
      break;
    case 1:
      currentSettings = &readerSettings;
      break;
    case 2:
      currentSettings = &controlsSettings;
      break;
    case 3:
    default:
      currentSettings = &systemSettings;
      break;
  }

  settingsCount = static_cast<int>(currentSettings->size());
  if (settingsCount <= 0) {
    selectedSettingIndex = 0;
  } else {
    const int rememberedIndex = selectedSettingIndexByCategory[selectedCategoryIndex];
    selectedSettingIndex = std::max(0, std::min(rememberedIndex, settingsCount - 1));
    selectedSettingIndexByCategory[selectedCategoryIndex] = selectedSettingIndex;
  }
  requestUpdate();
}

std::string SettingsActivity::getCategorySummary(const int categoryIndex) const {
  switch (categoryIndex) {
    case 0: {
      static constexpr StrId sleepModes[] = {StrId::STR_DARK, StrId::STR_LIGHT, StrId::STR_CUSTOM, StrId::STR_NONE_OPT};
      static constexpr StrId refreshModes[] = {StrId::STR_PAGES_1, StrId::STR_PAGES_5, StrId::STR_PAGES_10,
                                               StrId::STR_PAGES_15, StrId::STR_PAGES_30};
      static constexpr StrId batteryModes[] = {StrId::STR_NEVER, StrId::STR_IN_READER, StrId::STR_ALWAYS};
      const uint8_t sleepIndex = std::min<uint8_t>(SETTINGS.sleepScreen, static_cast<uint8_t>(std::size(sleepModes) - 1));
      const uint8_t refreshIndex =
          std::min<uint8_t>(SETTINGS.refreshFrequency, static_cast<uint8_t>(std::size(refreshModes) - 1));
      const uint8_t batteryIndex = std::min<uint8_t>(SETTINGS.hideBatteryPercentage,
                                                     static_cast<uint8_t>(std::size(batteryModes) - 1));
      return joinSummary(I18N.get(sleepModes[sleepIndex]), I18N.get(refreshModes[refreshIndex]),
                         I18N.get(batteryModes[batteryIndex]));
    }
    case 1: {
      static constexpr StrId fontFamilies[] = {StrId::STR_BAI_JAMJUREE, StrId::STR_CLOUD_LOOP, StrId::STR_BOOKERLY};
      static constexpr StrId lineSpacing[] = {StrId::STR_TIGHT, StrId::STR_NORMAL, StrId::STR_WIDE};
      const uint8_t fontIndex =
          std::min<uint8_t>(SETTINGS.fontFamily, static_cast<uint8_t>(std::size(fontFamilies) - 1));
      const uint8_t spacingIndex =
          std::min<uint8_t>(SETTINGS.lineSpacing, static_cast<uint8_t>(std::size(lineSpacing) - 1));
      return joinSummary(I18N.get(fontFamilies[fontIndex]), std::to_string(SETTINGS.fontSize) + " pt",
                         I18N.get(lineSpacing[spacingIndex]));
    }
    case 2: {
      static constexpr StrId motionModes[] = {StrId::STR_OFF,
                                              StrId::STR_MOTION_LR_HEAVY,
                                              StrId::STR_MOTION_LR_LIGHT,
                                              StrId::STR_MOTION_R_HEAVY,
                                              StrId::STR_MOTION_R_LIGHT,
                                              StrId::STR_MOTION_TILT_LR};
      static constexpr StrId sideLayouts[] = {StrId::STR_PREV_NEXT, StrId::STR_NEXT_PREV};
      static constexpr StrId powerModes[] = {StrId::STR_IGNORE, StrId::STR_PAGE_TURN};
      const uint8_t motionIndex =
          std::min<uint8_t>(SETTINGS.motionPageTurn, static_cast<uint8_t>(std::size(motionModes) - 1));
      const uint8_t sideIndex =
          std::min<uint8_t>(SETTINGS.sideButtonLayout, static_cast<uint8_t>(std::size(sideLayouts) - 1));
      const uint8_t powerIndex =
          std::min<uint8_t>(SETTINGS.shortPwrBtn, static_cast<uint8_t>(std::size(powerModes) - 1));
      return joinSummary(I18N.get(motionModes[motionIndex]), I18N.get(sideLayouts[sideIndex]),
                         I18N.get(powerModes[powerIndex]));
    }
    case 3:
    default: {
      static constexpr StrId timezones[] = {StrId::STR_TZ_UTC_M8, StrId::STR_TZ_UTC_M5, StrId::STR_TZ_UTC_0,
                                            StrId::STR_TZ_UTC_P1, StrId::STR_TZ_UTC_P3, StrId::STR_TZ_UTC_P7,
                                            StrId::STR_TZ_UTC_P8, StrId::STR_TZ_UTC_P9, StrId::STR_TZ_UTC_P10};
      static constexpr StrId sleepTimeouts[] = {StrId::STR_MIN_1, StrId::STR_MIN_5, StrId::STR_MIN_10,
                                                StrId::STR_MIN_15, StrId::STR_MIN_30};
      const uint8_t timezoneIndex =
          std::min<uint8_t>(SETTINGS.timezone, static_cast<uint8_t>(std::size(timezones) - 1));
      const uint8_t timeoutIndex =
          std::min<uint8_t>(SETTINGS.sleepTimeout, static_cast<uint8_t>(std::size(sleepTimeouts) - 1));
      return joinSummary(I18N.getLanguageName(I18N.getLanguage()), I18N.get(timezones[timezoneIndex]),
                         I18N.get(sleepTimeouts[timeoutIndex]));
    }
  }
}

std::string SettingsActivity::getSettingValueText(const SettingInfo& setting) const {
  if (setting.type == SettingType::TOGGLE && setting.valuePtr != nullptr) {
    return SETTINGS.*(setting.valuePtr) ? tr(STR_STATE_ON) : tr(STR_STATE_OFF);
  }

  if (setting.type == SettingType::ENUM) {
    uint8_t value = 0;
    if (setting.valuePtr != nullptr) {
      value = SETTINGS.*(setting.valuePtr);
    } else if (setting.valueGetter) {
      value = setting.valueGetter();
    }

    if (!setting.enumValues.empty()) {
      value = std::min<uint8_t>(value, static_cast<uint8_t>(setting.enumValues.size() - 1));
      return I18N.get(setting.enumValues[value]);
    }
  }

  if (setting.type == SettingType::VALUE) {
    if (setting.valuePtr != nullptr) {
      return std::to_string(SETTINGS.*(setting.valuePtr));
    }
    if (setting.valueGetter) {
      return std::to_string(setting.valueGetter());
    }
  }

  if (setting.type == SettingType::STRING) {
    if (setting.stringOffset != 0) {
      const char* value = reinterpret_cast<const char*>(reinterpret_cast<const uint8_t*>(&SETTINGS) + setting.stringOffset);
      return value[0] != '\0' ? value : tr(STR_NOT_SET);
    }
    if (setting.stringGetter) {
      const auto value = setting.stringGetter();
      return value.empty() ? tr(STR_NOT_SET) : value;
    }
  }

  if (setting.type == SettingType::ACTION) {
    switch (setting.action) {
      case SettingAction::SelectWallpaper:
        // Empty path → folder rotation; non-empty → specific filename.
        return SETTINGS.customSleepImagePath[0] != '\0' ? SETTINGS.customSleepImagePath : tr(STR_RANDOM_ROTATION);
      case SettingAction::SelectSdFont:
        // Empty → fall back to built-in fontFamily; non-empty → discovered family name.
        return SETTINGS.sdFontFamilyName[0] != '\0' ? SETTINGS.sdFontFamilyName : tr(STR_DEFAULT_VALUE);
      case SettingAction::Language:
        return I18N.getLanguageName(I18N.getLanguage());
      case SettingAction::About:
        return CROSSPOINT_VERSION;
      default:
        return "";
    }
  }

  return "";
}

void SettingsActivity::loop() {
  if (currentView == ViewMode::CategoryHome) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      enterCategory(selectedCategoryIndex);
      return;
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      SETTINGS.saveToFile();
      onGoHome();
      return;
    }

    auto moveNextCategory = [this]() {
      selectedCategoryIndex = ButtonNavigator::nextIndex(selectedCategoryIndex, categoryCount);
      requestUpdate();
    };
    auto movePreviousCategory = [this]() {
      selectedCategoryIndex = ButtonNavigator::previousIndex(selectedCategoryIndex, categoryCount);
      requestUpdate();
    };

    buttonNavigator.onNextRelease(moveNextCategory);
    buttonNavigator.onPreviousRelease(movePreviousCategory);
    buttonNavigator.onNextContinuous(moveNextCategory);
    buttonNavigator.onPreviousContinuous(movePreviousCategory);
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    currentView = ViewMode::CategoryHome;
    requestUpdate();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    toggleCurrentSetting();
    requestUpdate();
    return;
  }

  auto moveNextSetting = [this]() {
    if (settingsCount <= 0) {
      return;
    }
    selectedSettingIndex = ButtonNavigator::nextIndex(selectedSettingIndex, settingsCount);
    selectedSettingIndexByCategory[selectedCategoryIndex] = selectedSettingIndex;
    requestUpdate();
  };
  auto movePreviousSetting = [this]() {
    if (settingsCount <= 0) {
      return;
    }
    selectedSettingIndex = ButtonNavigator::previousIndex(selectedSettingIndex, settingsCount);
    selectedSettingIndexByCategory[selectedCategoryIndex] = selectedSettingIndex;
    requestUpdate();
  };

  buttonNavigator.onNextRelease(moveNextSetting);
  buttonNavigator.onPreviousRelease(movePreviousSetting);
  buttonNavigator.onNextContinuous(moveNextSetting);
  buttonNavigator.onPreviousContinuous(movePreviousSetting);
}

void SettingsActivity::toggleCurrentSetting() {
  int selectedSetting = selectedSettingIndex;
  if (selectedSetting < 0 || selectedSetting >= settingsCount) {
    return;
  }

  const auto& setting = (*currentSettings)[selectedSetting];

  if (setting.type == SettingType::TOGGLE && setting.valuePtr != nullptr) {
    // Toggle the boolean value using the member pointer
    const bool currentValue = SETTINGS.*(setting.valuePtr);
    SETTINGS.*(setting.valuePtr) = !currentValue;
  } else if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
    const uint8_t currentValue = SETTINGS.*(setting.valuePtr);
    SETTINGS.*(setting.valuePtr) = (currentValue + 1) % static_cast<uint8_t>(setting.enumValues.size());
  } else if (setting.type == SettingType::VALUE && setting.valuePtr != nullptr) {
    bool handled = false;
    // Dynamic cycling for the Reader font-size row: when an SD card font is
    // selected, walk through its actual .cpfont sizes (e.g. 18/20/22/24/26
    // for a custom font compiled at those sizes) instead of the built-in
    // 14-22 step range.  Falls back to the built-in cycling when no SD font
    // is active or the family has no files on disk.
    if (setting.nameId == StrId::STR_FONT_SIZE && SETTINGS.sdFontFamilyName[0] != '\0') {
      const auto* family = sdFontSystem.registry().findFamily(SETTINGS.sdFontFamilyName);
      if (family && !family->files.empty()) {
        std::vector<uint8_t> sizes;
        sizes.reserve(family->files.size());
        for (const auto& f : family->files) sizes.push_back(f.pointSize);
        std::sort(sizes.begin(), sizes.end());
        sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());
        const uint8_t current = SETTINGS.*(setting.valuePtr);
        // Find the next size strictly greater than current; wrap to first.
        uint8_t next = sizes.front();
        for (uint8_t s : sizes) {
          if (s > current) {
            next = s;
            break;
          }
        }
        SETTINGS.*(setting.valuePtr) = next;
        // Re-resolve to the closest-matching .cpfont file (the cycle picks
        // values that exist on disk, so this is normally an exact match).
        ensureSdFontLoaded();
        handled = true;
      }
    }
    if (!handled) {
      // Built-in cycling: step through valueRange.min..max.
      const int8_t currentValue = SETTINGS.*(setting.valuePtr);
      if (currentValue + setting.valueRange.step > setting.valueRange.max) {
        SETTINGS.*(setting.valuePtr) = setting.valueRange.min;
      } else {
        SETTINGS.*(setting.valuePtr) = currentValue + setting.valueRange.step;
      }
    }
  } else if (setting.type == SettingType::ACTION) {
    auto resultHandler = [this](const ActivityResult&) { SETTINGS.saveToFile(); };

    switch (setting.action) {
      case SettingAction::RemapFrontButtons:
        startActivityForResult(std::make_unique<ButtonRemapActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::CustomiseStatusBar:
        startActivityForResult(std::make_unique<StatusBarSettingsActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::KOReaderSync:
        startActivityForResult(std::make_unique<KOReaderSettingsActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::OPDSBrowser:
        startActivityForResult(std::make_unique<CalibreSettingsActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::Network:
        startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, false), resultHandler);
        break;
      case SettingAction::ClearCache:
        startActivityForResult(std::make_unique<ClearCacheActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::RefreshRecentBooks:
        startActivityForResult(
            std::make_unique<ClearCacheActivity>(renderer, mappedInput, ClearCacheActivity::Mode::RefreshRecents),
            resultHandler);
        break;
      case SettingAction::ClearRecentBooks:
        startActivityForResult(
            std::make_unique<ClearCacheActivity>(renderer, mappedInput, ClearCacheActivity::Mode::ClearRecents),
            resultHandler);
        break;
      case SettingAction::CheckForUpdates:
        startActivityForResult(std::make_unique<OtaUpdateActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::SwitchFirmwareSlot:
        startActivityForResult(std::make_unique<FwSlotSwitchActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::SdFirmwareUpdate:
        startActivityForResult(std::make_unique<SdFirmwareUpdateActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::Language:
        startActivityForResult(std::make_unique<LanguageSelectActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::SelectWallpaper:
        startActivityForResult(std::make_unique<SleepWallpaperPickerActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::SelectSdFont:
        startActivityForResult(std::make_unique<SdCardFontPickerActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::ThaiDictionary:
        startActivityForResult(std::make_unique<ThaiDictionaryActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::About:
        startActivityForResult(std::make_unique<AboutActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::None:
        // Do nothing
        break;
    }
    return;  // Results will be handled in the result handler, so we can return early here
  } else {
    return;
  }

  SETTINGS.saveToFile();
}

void SettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  const auto& metrics = UITheme::getInstance().getMetrics();

  if (currentView == ViewMode::CategoryHome) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SETTINGS_TITLE));

    const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
    GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, categoryCount, selectedCategoryIndex,
                 [](int index) { return std::string(I18N.get(categoryNames[index])); },
                 [this](int index) { return getCategorySummary(index); });

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  const auto& settings = *currentSettings;
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SETTINGS_TITLE),
                 I18N.get(categoryNames[selectedCategoryIndex]));

  const int subHeaderTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const std::string categorySummary = getCategorySummary(selectedCategoryIndex);
  GUI.drawSubHeader(renderer, Rect{0, subHeaderTop, pageWidth, metrics.tabBarHeight}, categorySummary.c_str());

  const int listTop = subHeaderTop + metrics.tabBarHeight + metrics.verticalSpacing;
  const int listHeight = pageHeight - listTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  GUI.drawList(
      renderer, Rect{0, listTop, pageWidth, listHeight}, settingsCount, selectedSettingIndex,
      [&settings](int index) { return std::string(I18N.get(settings[index].nameId)); }, nullptr, nullptr,
      [this, &settings](int i) { return getSettingValueText(settings[i]); }, true);

  const auto confirmLabel =
      (settingsCount > 0 && settings[selectedSettingIndex].type == SettingType::ACTION) ? tr(STR_OPEN) : tr(STR_TOGGLE);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
