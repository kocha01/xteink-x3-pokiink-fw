#include <Arduino.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <FontCacheManager.h>
#include <FontDecompressor.h>
#include <GfxRenderer.h>
#include <SdCardFontSystem.h>
#include <esp_ota_ops.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalMotionSensor.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <HalSystem.h>
#include <I18n.h>
#include <Logging.h>
#include <NtpClock.h>
#include <SPI.h>
#include <builtinFonts/all.h>
#include <esp_system.h>

#include <cstring>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "Epub/hyphenation/ThaiWordBreaker.h"
#include "KOReaderCredentialStore.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "activities/Activity.h"
#include "activities/ActivityManager.h"
#include "activities/boot_sleep/SleepWallpaperCache.h"
#include "SdAutoRecovery.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ButtonNavigator.h"
#include "util/ScreenshotUtil.h"

HalDisplay display;
HalGPIO gpio;
MappedInputManager mappedInputManager(gpio);
GfxRenderer renderer(display);
ActivityManager activityManager(renderer, mappedInputManager);
FontDecompressor fontDecompressor;
FontCacheManager fontCacheManager(renderer.getFontMap(), renderer.getSdCardFonts());

// SD card font system — discovers .cpfont files in /.crosspoint/fonts/ and loads
// the user's chosen family.  Defined here so SdCardFontGlobals.h's `extern`
// declarations resolve, and so `ensureSdFontLoaded()` (called from the
// settings activity after a font change) can dispatch to it.
SdCardFontSystem sdFontSystem;
void ensureSdFontLoaded() { sdFontSystem.ensureLoaded(renderer); }

// Fonts
EpdFont smallFont(&baijamjuree_8_regular);
EpdFontFamily smallFontFamily(&smallFont);

EpdFont ui10RegularFont(&baijamjuree_10_regular);
EpdFontFamily ui10FontFamily(&ui10RegularFont);  // faux bold via renderer

EpdFont ui12RegularFont(&baijamjuree_12_regular);
EpdFont ui12BoldFont(&baijamjuree_12_bold);
EpdFontFamily ui12FontFamily(&ui12RegularFont, &ui12BoldFont);

EpdFont cjk8RegularFont(&notosanssc_8_regular);
EpdFontFamily cjk8FontFamily(&cjk8RegularFont);

EpdFont cjk10RegularFont(&notosanssc_10_regular);
EpdFont cjk10BoldFont(&notosanssc_10_bold);
EpdFontFamily cjk10FontFamily(&cjk10RegularFont, &cjk10BoldFont);

EpdFont cjk12RegularFont(&notosanssc_12_regular);
EpdFont cjk12BoldFont(&notosanssc_12_bold);
EpdFontFamily cjk12FontFamily(&cjk12RegularFont, &cjk12BoldFont);

// Mali — Poki's handwriting voice (EN + TH). Used by boot/sleep greeting lines and the
// illustrated pre-sleep popup. Declared outside OMIT_FONTS because it must be available
// before any book is opened. The 14 size is a shrink-to-fit fallback for the popup so
// long localized lines stay inside the bubble without truncation.
EpdFont poki14RegularFont(&mali_14_regular);
EpdFontFamily poki14FontFamily(&poki14RegularFont);
EpdFont poki18RegularFont(&mali_18_regular);
EpdFontFamily poki18FontFamily(&poki18RegularFont);

#ifndef OMIT_FONTS
// Bai Jamjuree (Thai + Latin sans-serif — reader font + UI number display + Thai UI fallback)
EpdFont baijamjuree8RegularFont(&baijamjuree_8_regular);
EpdFontFamily baijamjuree8FontFamily(&baijamjuree8RegularFont);
EpdFont baijamjuree10RegularFont(&baijamjuree_10_regular);
EpdFontFamily baijamjuree10FontFamily(&baijamjuree10RegularFont);
EpdFont baijamjuree12RegularFont(&baijamjuree_12_regular);
EpdFont baijamjuree12BoldFont(&baijamjuree_12_bold);
EpdFontFamily baijamjuree12FontFamily(&baijamjuree12RegularFont, &baijamjuree12BoldFont);
EpdFont baijamjuree14RegularFont(&baijamjuree_14_regular);
EpdFont baijamjuree14BoldFont(&baijamjuree_14_bold);
EpdFontFamily baijamjuree14FontFamily(&baijamjuree14RegularFont, &baijamjuree14BoldFont);
EpdFont baijamjuree16RegularFont(&baijamjuree_16_regular);
EpdFont baijamjuree16BoldFont(&baijamjuree_16_bold);
EpdFontFamily baijamjuree16FontFamily(&baijamjuree16RegularFont, &baijamjuree16BoldFont);
EpdFont baijamjuree18RegularFont(&baijamjuree_18_regular);
EpdFont baijamjuree18BoldFont(&baijamjuree_18_bold);
EpdFontFamily baijamjuree18FontFamily(&baijamjuree18RegularFont, &baijamjuree18BoldFont);
EpdFont baijamjuree20RegularFont(&baijamjuree_20_regular);
EpdFont baijamjuree20BoldFont(&baijamjuree_20_bold);
EpdFontFamily baijamjuree20FontFamily(&baijamjuree20RegularFont, &baijamjuree20BoldFont);
EpdFont baijamjuree22RegularFont(&baijamjuree_22_regular);
EpdFont baijamjuree22BoldFont(&baijamjuree_22_bold);
EpdFontFamily baijamjuree22FontFamily(&baijamjuree22RegularFont, &baijamjuree22BoldFont);
EpdFont cloudloop36Font(&cloudloop_36_regular);
EpdFontFamily cloudloop36FontFamily(&cloudloop36Font);  // UI number display (font-size selector)

EpdFont cloudloop12RegularFont(&cloudloop_12_regular);
EpdFontFamily cloudloop12FontFamily(&cloudloop12RegularFont);
EpdFont cloudloop14RegularFont(&cloudloop_14_regular);
EpdFontFamily cloudloop14FontFamily(&cloudloop14RegularFont);
EpdFont cloudloop16RegularFont(&cloudloop_16_regular);
EpdFontFamily cloudloop16FontFamily(&cloudloop16RegularFont);
EpdFont cloudloop18RegularFont(&cloudloop_18_regular);
EpdFontFamily cloudloop18FontFamily(&cloudloop18RegularFont);
EpdFont cloudloop20RegularFont(&cloudloop_20_regular);
EpdFontFamily cloudloop20FontFamily(&cloudloop20RegularFont);
EpdFont cloudloop22RegularFont(&cloudloop_22_regular);
EpdFontFamily cloudloop22FontFamily(&cloudloop22RegularFont);

// Bookerly (Latin serif — fallback to Literata for Thai).
// Italic is wired only for body-text sizes 14/16/18 to keep flash budget under
// control (~80KB each in flash). Sizes 12, 20, 22 and bold-italic fall back to
// regular/bold via EpdFontFamily's nullptr fallback in getFont().
EpdFont bookerly12RegularFont(&bookerly_12_regular);
EpdFont bookerly12BoldFont(&bookerly_12_bold);
EpdFontFamily bookerly12FontFamily(&bookerly12RegularFont, &bookerly12BoldFont);
EpdFont bookerly14RegularFont(&bookerly_14_regular);
EpdFont bookerly14BoldFont(&bookerly_14_bold);
EpdFont bookerly14ItalicFont(&bookerly_14_italic);
EpdFontFamily bookerly14FontFamily(&bookerly14RegularFont, &bookerly14BoldFont,
                                    &bookerly14ItalicFont);
EpdFont bookerly16RegularFont(&bookerly_16_regular);
EpdFont bookerly16BoldFont(&bookerly_16_bold);
EpdFont bookerly16ItalicFont(&bookerly_16_italic);
EpdFontFamily bookerly16FontFamily(&bookerly16RegularFont, &bookerly16BoldFont,
                                    &bookerly16ItalicFont);
EpdFont bookerly18RegularFont(&bookerly_18_regular);
EpdFont bookerly18BoldFont(&bookerly_18_bold);
EpdFont bookerly18ItalicFont(&bookerly_18_italic);
EpdFontFamily bookerly18FontFamily(&bookerly18RegularFont, &bookerly18BoldFont,
                                    &bookerly18ItalicFont);
EpdFont bookerly20RegularFont(&bookerly_20_regular);
EpdFont bookerly20BoldFont(&bookerly_20_bold);
EpdFontFamily bookerly20FontFamily(&bookerly20RegularFont, &bookerly20BoldFont);
EpdFont bookerly22RegularFont(&bookerly_22_regular);
EpdFont bookerly22BoldFont(&bookerly_22_bold);
EpdFontFamily bookerly22FontFamily(&bookerly22RegularFont, &bookerly22BoldFont);

// Noto Serif (serif + Thai — auto-selected when Bookerly + Thai book)
EpdFont notoserif12RegularFont(&notoserif_12_regular);
EpdFont notoserif12BoldFont(&notoserif_12_bold);
EpdFontFamily notoserif12FontFamily(&notoserif12RegularFont, &notoserif12BoldFont);
EpdFont notoserif14RegularFont(&notoserif_14_regular);
EpdFont notoserif14BoldFont(&notoserif_14_bold);
EpdFontFamily notoserif14FontFamily(&notoserif14RegularFont, &notoserif14BoldFont);
EpdFont notoserif16RegularFont(&notoserif_16_regular);
EpdFont notoserif16BoldFont(&notoserif_16_bold);
EpdFontFamily notoserif16FontFamily(&notoserif16RegularFont, &notoserif16BoldFont);
EpdFont notoserif18RegularFont(&notoserif_18_regular);
EpdFont notoserif18BoldFont(&notoserif_18_bold);
EpdFontFamily notoserif18FontFamily(&notoserif18RegularFont, &notoserif18BoldFont);
EpdFont notoserif20RegularFont(&notoserif_20_regular);
EpdFont notoserif20BoldFont(&notoserif_20_bold);
EpdFontFamily notoserif20FontFamily(&notoserif20RegularFont, &notoserif20BoldFont);
EpdFont notoserif22RegularFont(&notoserif_22_regular);
EpdFont notoserif22BoldFont(&notoserif_22_bold);
EpdFontFamily notoserif22FontFamily(&notoserif22RegularFont, &notoserif22BoldFont);
#endif  // OMIT_FONTS

// Power button state tracking.
// On X3 (Halo 2), sleep and wake are instant — no hold duration required.
// ignorePowerSleepUntilRelease prevents the wake-up press from immediately re-triggering sleep.
unsigned long t1 = 0;
unsigned long t2 = 0;
bool ignorePowerSleepUntilRelease = false;
bool displayRuntimeReady = false;
bool fontsReady = false;
unsigned long lastActivityTime = 0;

bool isDeepSleepResume() { return esp_reset_reason() == ESP_RST_DEEPSLEEP; }

// Minimum power-button hold time (ms) required to trigger sleep on X3.
// Short enough to feel responsive, long enough to avoid accidental taps.
constexpr unsigned long kX3SleepHoldMs = 500;

bool shouldIgnorePowerSleepUntilRelease(const HalGPIO::WakeupReason wakeupReason) {
  return BoardConfig::kTargetBoard == BoardConfig::TargetBoard::X3 &&
         wakeupReason == HalGPIO::WakeupReason::PowerButton;
}

void setupDisplayRuntime() {
  if (displayRuntimeReady) {
    return;
  }

  display.begin();
  renderer.begin();
  activityManager.begin();
  displayRuntimeReady = true;
  LOG_DBG("MAIN", "Display initialized");
}

void waitForPowerRelease() {
  gpio.update();
  while (gpio.isPowerButtonPhysicallyPressed()) {
    delay(50);
    gpio.update();
  }
}

bool shouldUseX3LightSleep(const bool triggeredByPowerButton) {
  return BoardConfig::kTargetBoard == BoardConfig::TargetBoard::X3 && triggeredByPowerButton;
}

void resumeFromX3LightSleep() {
  LOG_DBG("MAIN", "Resuming from X3 light sleep");
  setupDisplayRuntime();
  display.begin();
  renderer.begin();
  // Re-initialize fuel gauge I2C after light sleep tear-down
  powerManager.begin();

  ignorePowerSleepUntilRelease = gpio.isPowerButtonPhysicallyPressed();
  lastActivityTime = millis();

  // Reload APP_STATE from file in case anything changed during sleep
  // (enterSleep() saves lastSleepFromReader right before going under).
  APP_STATE.loadFromFile();
  // Always reload recent books — ReaderActivity::onEnter calls addBook(), which
  // would otherwise save an empty list + current book and wipe out all previous
  // recent entries the next time Home renders.
  RECENT_BOOKS.loadFromFile();
  const bool shouldResumeReader = !APP_STATE.openEpubPath.empty() && APP_STATE.lastSleepFromReader &&
                                  !mappedInputManager.isPressed(MappedInputManager::Button::Back) &&
                                  APP_STATE.readerActivityLoadCount == 0;

  // Wake sequence:
  // 1. Normal boot screen — gives the user feedback that the device is waking.
  //    Ghost clearing between the pre-sleep activity and the boot screen is
  //    driven by the HALF_REFRESH cadence inside EInkDisplay (a full-sync
  //    refresh runs whenever HALF is requested, matching X4 semantics).
  // 2. Reader (if we slept from reader) or Home.
  activityManager.goToBoot();
  activityManager.loop();
  activityManager.requestUpdateAndWait();

  if (shouldResumeReader) {
    // Clear the path + bump the load counter before re-entering reader. If the
    // reader crashes during load, next boot sees count > 0 and falls through
    // to home instead of boot-looping. ReaderActivity resets the counter on
    // successful onEnter.
    const auto path = APP_STATE.openEpubPath;
    APP_STATE.openEpubPath = "";
    APP_STATE.readerActivityLoadCount++;
    APP_STATE.saveToFile();
    activityManager.goToReader(path);
  } else {
    activityManager.goHome();
  }
  activityManager.loop();
  activityManager.requestUpdateAndWait();
}

void enterSleep(const bool triggeredByPowerButton) {
  HalPowerManager::Lock powerLock;  // Ensure we are at normal CPU frequency for sleep preparation
  APP_STATE.lastSleepFromReader = activityManager.isReaderActivity();
  APP_STATE.saveToFile();

  activityManager.goToSleep();

  display.deepSleep();
  LOG_DBG("MAIN", "Power button press calibration value: %lu ms", t2 - t1);
  if (shouldUseX3LightSleep(triggeredByPowerButton)) {
    if (powerManager.startLightSleep(gpio)) {
      resumeFromX3LightSleep();
      return;
    }
    LOG_ERR("MAIN", "Falling back to deep sleep after light sleep failure");
  }

  LOG_DBG("MAIN", "Entering deep sleep");
  powerManager.startDeepSleep(gpio);
}

// Load user-supplied Thai dictionary words from SD card (/crosspoint/thai_dict.txt).
// Each line should contain one Thai word. Lines starting with '#' are comments.
// Maximum 200 words to stay within ESP32-C3 memory constraints.
void loadThaiUserDictionary() {
  static constexpr char DICT_PATH[] = "/crosspoint/thai_dict.txt";
  static constexpr size_t MAX_BUF = 8192;  // 8 KB max file size

  if (!Storage.exists(DICT_PATH)) {
    return;
  }

  // Stack buffer would be too large (> 256 bytes), so use heap allocation.
  auto* buf = static_cast<char*>(malloc(MAX_BUF));
  if (!buf) {
    LOG_ERR("MAIN", "Failed to allocate buffer for Thai user dictionary");
    return;
  }

  const size_t bytesRead = Storage.readFileToBuffer(DICT_PATH, buf, MAX_BUF);
  if (bytesRead == 0) {
    free(buf);
    return;
  }

  std::vector<std::string> words;
  words.reserve(64);

  // Parse line by line
  size_t lineStart = 0;
  for (size_t i = 0; i <= bytesRead; ++i) {
    if (i == bytesRead || buf[i] == '\n' || buf[i] == '\r') {
      if (i > lineStart) {
        // Trim trailing whitespace
        size_t lineEnd = i;
        while (lineEnd > lineStart && (buf[lineEnd - 1] == ' ' || buf[lineEnd - 1] == '\t')) {
          --lineEnd;
        }
        if (lineEnd > lineStart && buf[lineStart] != '#') {
          words.emplace_back(buf + lineStart, lineEnd - lineStart);
        }
      }
      lineStart = i + 1;
    }
  }

  free(buf);
  buf = nullptr;

  if (!words.empty()) {
    ThaiWordBreaker::setUserDictionary(words);
    LOG_DBG("MAIN", "Thai user dictionary: %d words from %s", static_cast<int>(words.size()), DICT_PATH);
  }
}

bool readerFontsReady = false;

// Phase 1: UI-only fonts — needed for boot/home screen.  Fast (~5ms).
void setupUIFonts() {
  // Initialize font decompressor for compressed reader fonts
  if (!fontDecompressor.init()) {
    LOG_ERR("MAIN", "Font decompressor init failed");
  }
  fontCacheManager.setFontDecompressor(&fontDecompressor);
  renderer.setFontCacheManager(&fontCacheManager);

  if constexpr (BoardConfig::kTargetBoard == BoardConfig::TargetBoard::X3) {
    renderer.insertFont(UI_10_FONT_ID, ui12FontFamily);
    renderer.insertFont(UI_12_FONT_ID, baijamjuree14FontFamily);
    renderer.insertFont(SMALL_FONT_ID, ui10FontFamily);
    renderer.setFallbackFont(SMALL_FONT_ID, &cjk10FontFamily);
    renderer.setFallbackFont(UI_10_FONT_ID, &cjk12FontFamily);
    renderer.setFallbackFont(UI_12_FONT_ID, &cjk12FontFamily);
  } else {
    renderer.insertFont(UI_10_FONT_ID, ui10FontFamily);
    renderer.insertFont(UI_12_FONT_ID, ui12FontFamily);
    renderer.insertFont(SMALL_FONT_ID, smallFontFamily);
    renderer.setFallbackFont(SMALL_FONT_ID, &cjk8FontFamily);
    renderer.setFallbackFont(UI_10_FONT_ID, &cjk10FontFamily);
    renderer.setFallbackFont(UI_12_FONT_ID, &cjk12FontFamily);
  }

  // Poki's handwriting voice (EN + TH native in Mali). CJK fallback only for safety;
  // Poki messages are authored in EN/TH and won't normally need it. Mali 14 is a
  // shrink-to-fit fallback used by the pre-sleep popup when the localized line is
  // too wide for the bubble's text window at Mali 18.
  renderer.insertFont(POKI_14_FONT_ID, poki14FontFamily);
  renderer.setFallbackFont(POKI_14_FONT_ID, &cjk12FontFamily);
  renderer.insertFont(POKI_18_FONT_ID, poki18FontFamily);
  renderer.setFallbackFont(POKI_18_FONT_ID, &cjk12FontFamily);

  LOG_DBG("MAIN", "UI fonts ready");
}

// Phase 2: Reader fonts — deferred until a book is opened for the first time.
void ensureReaderFonts() {
  if (readerFontsReady) return;
#ifndef OMIT_FONTS
  const unsigned long t = millis();
  // Bai Jamjuree (reader + UI number display)
  renderer.insertFont(BAIJAMJUREE_12_FONT_ID, baijamjuree12FontFamily);
  renderer.insertFont(BAIJAMJUREE_14_FONT_ID, baijamjuree14FontFamily);
  renderer.insertFont(BAIJAMJUREE_16_FONT_ID, baijamjuree16FontFamily);
  renderer.insertFont(BAIJAMJUREE_18_FONT_ID, baijamjuree18FontFamily);
  renderer.insertFont(BAIJAMJUREE_20_FONT_ID, baijamjuree20FontFamily);
  renderer.insertFont(BAIJAMJUREE_22_FONT_ID, baijamjuree22FontFamily);
  renderer.insertFont(NOTOSANS_18_FONT_ID, baijamjuree18FontFamily);  // font-name display in reader menu
  renderer.insertFont(NOTOSANS_36_FONT_ID, cloudloop36FontFamily);    // font-size display in reader menu
  // CloudLoop
  renderer.insertFont(CLOUDLOOP_12_FONT_ID, cloudloop12FontFamily);
  renderer.insertFont(CLOUDLOOP_14_FONT_ID, cloudloop14FontFamily);
  renderer.insertFont(CLOUDLOOP_16_FONT_ID, cloudloop16FontFamily);
  renderer.insertFont(CLOUDLOOP_18_FONT_ID, cloudloop18FontFamily);
  renderer.insertFont(CLOUDLOOP_20_FONT_ID, cloudloop20FontFamily);
  renderer.insertFont(CLOUDLOOP_22_FONT_ID, cloudloop22FontFamily);
  // Bookerly
  renderer.insertFont(BOOKERLY_12_FONT_ID, bookerly12FontFamily);
  renderer.insertFont(BOOKERLY_14_FONT_ID, bookerly14FontFamily);
  renderer.insertFont(BOOKERLY_16_FONT_ID, bookerly16FontFamily);
  renderer.insertFont(BOOKERLY_18_FONT_ID, bookerly18FontFamily);
  renderer.insertFont(BOOKERLY_20_FONT_ID, bookerly20FontFamily);
  renderer.insertFont(BOOKERLY_22_FONT_ID, bookerly22FontFamily);
  // Noto Serif (serif + Thai — auto-selected when Bookerly + Thai book)
  renderer.insertFont(NOTOSERIF_12_FONT_ID, notoserif12FontFamily);
  renderer.insertFont(NOTOSERIF_14_FONT_ID, notoserif14FontFamily);
  renderer.insertFont(NOTOSERIF_16_FONT_ID, notoserif16FontFamily);
  renderer.insertFont(NOTOSERIF_18_FONT_ID, notoserif18FontFamily);
  renderer.insertFont(NOTOSERIF_20_FONT_ID, notoserif20FontFamily);
  renderer.insertFont(NOTOSERIF_22_FONT_ID, notoserif22FontFamily);
  // Noto Serif as fallback for Bookerly (provides Thai glyphs for mixed-language text)
  renderer.setFallbackFont(BOOKERLY_12_FONT_ID, &notoserif12FontFamily);
  renderer.setFallbackFont(BOOKERLY_14_FONT_ID, &notoserif14FontFamily);
  renderer.setFallbackFont(BOOKERLY_16_FONT_ID, &notoserif16FontFamily);
  renderer.setFallbackFont(BOOKERLY_18_FONT_ID, &notoserif18FontFamily);
  renderer.setFallbackFont(BOOKERLY_20_FONT_ID, &notoserif20FontFamily);
  renderer.setFallbackFont(BOOKERLY_22_FONT_ID, &notoserif22FontFamily);
  LOG_DBG("MAIN", "Reader fonts loaded in %lu ms", millis() - t);
#endif  // OMIT_FONTS
  readerFontsReady = true;
}

void setupDisplayAndFonts() {
  setupDisplayRuntime();

  if (fontsReady) {
    return;
  }

  setupUIFonts();

  fontsReady = true;
  LOG_DBG("MAIN", "Fonts setup (UI only — reader fonts deferred)");
}

void warmCustomSleepWallpaperCacheIfNeeded() {
  if (SETTINGS.sleepScreen != CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM ||
      SETTINGS.customSleepImagePath[0] == '\0') {
    return;
  }

  const std::string filename = SETTINGS.customSleepImagePath;
  if (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos ||
      !FsHelpers::hasBmpExtension(filename)) {
    LOG_ERR("MAIN", "Skipping custom sleep wallpaper cache warm-up for invalid filename: %s",
            SETTINGS.customSleepImagePath);
    return;
  }

  const unsigned long startMs = millis();
  const std::string sourcePath = "/sleep/" + filename;
  if (SleepWallpaperCache::warmCache(renderer, sourcePath)) {
    LOG_DBG("MAIN", "Sleep wallpaper cache ready in %lu ms for %s", millis() - startMs, sourcePath.c_str());
  }
}

void setup() {
  t1 = millis();
  lastActivityTime = t1;

  HalSystem::begin();
  gpio.begin();
  powerManager.begin();
  const auto wakeupReason = gpio.getWakeupReason();
  ignorePowerSleepUntilRelease = shouldIgnorePowerSleepUntilRelease(wakeupReason);
  if constexpr (BoardConfig::kTargetBoard == BoardConfig::TargetBoard::X3) {
    // On X3, a cold boot may happen while the user is still holding the power button.
    // Ignore sleep-on-press until the button is physically released, so the wake press
    // doesn't immediately trigger a new sleep.
    if (gpio.isPowerButtonPhysicallyPressed()) {
      ignorePowerSleepUntilRelease = true;
      LOG_DBG("MAIN", "Power button held at boot, ignoring sleep shortcut until release");
    }
  }

  // X3 exposes USB directly, but waiting for a host here makes wake-from-sleep feel sluggish.
  if constexpr (BoardConfig::kTargetBoard == BoardConfig::TargetBoard::X3) {
    Serial.begin(115200);
  } else if constexpr (!BoardConfig::Features::kHasUsbDetectPin) {
    Serial.begin(115200);
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 3000) {
      delay(10);
    }
  } else if (gpio.isUsbConnected()) {
    Serial.begin(115200);
    // Wait up to 3 seconds for Serial to be ready to catch early logs
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 3000) {
      delay(10);
    }
  }

  // SD Card Initialization
  // We need 6 open files concurrently when parsing a new chapter
  if (!Storage.begin()) {
    LOG_ERR("MAIN", "SD card initialization failed");
    setupDisplayAndFonts();
    activityManager.goToFullScreenMessage("SD card error", EpdFontFamily::BOLD);
    return;
  }

  HalSystem::checkPanic();
  HalSystem::clearPanic();  // TODO: move this to an activity when we have one to display the panic info

  // Boot-time SD recovery hook.  Drops everything (display, fonts, network)
  // out of the critical path so we can install a firmware from SD before a
  // potentially broken previous firmware gets a chance to run.  Full filename
  // list, safety gates, and rejection suffixes are documented in
  // src/SdAutoRecovery.h.
  SdAutoRecovery::runIfRequested();

  SETTINGS.loadFromFile();

  // SD card font discovery — must run after Storage.begin() and SETTINGS.loadFromFile().
  // Scans /.crosspoint/fonts/ for .cpfont families and loads the saved selection (if any).
  sdFontSystem.begin(renderer);

  NTP_CLOCK.begin(SETTINGS.timezone);
  I18N.loadSettings();
  KOREADER_STORE.loadFromFile();
  UITheme::getInstance().reload();
  ButtonNavigator::setMappedInputManager(mappedInputManager);

  // Load optional Thai user dictionary from SD card (non-blocking, silently skips if absent)
  loadThaiUserDictionary();

  // First-boot-after-flash detection must run BEFORE the AfterUSBPower check
  // below.  esp_ota_mark_app_valid_cancel_rollback() flips the otadata image
  // state from NEW → VALID so the NEXT boot of this same image is treated as
  // a normal cold boot and the AfterUSBPower → deep-sleep optimization can
  // work normally.  Without this, every boot after a web flash would also
  // appear as AfterFlash (state stays NEW) and the user would never see the
  // straight-to-sleep behavior they get from a plug-in.
  // (See docs/x3-backport-handoff.md Critical Bug 5.)
  if (wakeupReason == HalGPIO::WakeupReason::AfterFlash) {
    LOG_DBG("MAIN", "Wakeup reason: After flash — marking OTA image valid");
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
      LOG_ERR("MAIN", "esp_ota_mark_app_valid_cancel_rollback failed: %d", static_cast<int>(err));
    }
  }

  // On X3 (Halo 2) wake/boot is always instant — no button-hold verification, no boot screen,
  // no clear-screen wake transition. A single home-screen refresh is all the user ever sees.
  if (wakeupReason == HalGPIO::WakeupReason::AfterUSBPower) {
    // USB power caused a cold boot → go straight back to sleep
    LOG_DBG("MAIN", "Wakeup reason: After USB Power");
    powerManager.startDeepSleep(gpio);
  }

  LOG_DBG("MAIN", "Starting PokiInk Halo - X3 version " CROSSPOINT_VERSION);

  APP_STATE.loadFromFile();
  // Always load RECENT_BOOKS: ReaderActivity::onEnter() calls addBook(), which
  // must merge with the existing list — otherwise the file gets overwritten
  // with just the current book and all other recent entries disappear.
  RECENT_BOOKS.loadFromFile();
  const bool shouldResumeReader = !APP_STATE.openEpubPath.empty() && APP_STATE.lastSleepFromReader &&
                                  !mappedInputManager.isPressed(MappedInputManager::Button::Back) &&
                                  APP_STATE.readerActivityLoadCount == 0;

  setupDisplayAndFonts();
  warmCustomSleepWallpaperCacheIfNeeded();

  // Show boot screen on every cold boot (X3 deep-sleep wake + X4 boot-up) so the user
  // has visual feedback that the device is starting up.
  activityManager.goToBoot();

  // Boot to home screen if no book is open, last sleep was not from reader, back button is held, or reader activity
  // crashed (indicated by readerActivityLoadCount > 0)
  if (!shouldResumeReader) {
    activityManager.goHome();
  } else {
    // Clear app state to avoid getting into a boot loop if the epub doesn't load
    const auto path = APP_STATE.openEpubPath;
    APP_STATE.openEpubPath = "";
    APP_STATE.readerActivityLoadCount++;
    APP_STATE.saveToFile();
    activityManager.goToReader(path);
  }

  // Ensure we're not still holding the power button before leaving setup
  if (!ignorePowerSleepUntilRelease) {
    waitForPowerRelease();
  }
}

void loop() {
  static unsigned long maxLoopDuration = 0;
  const unsigned long loopStartTime = millis();
  static unsigned long lastMemPrint = 0;

  gpio.update();

  bool ignoredWakeReleaseThisLoop = false;
  if (ignorePowerSleepUntilRelease && !gpio.isPowerButtonPhysicallyPressed()) {
    ignorePowerSleepUntilRelease = false;
    ignoredWakeReleaseThisLoop = true;
    lastActivityTime = millis();
    LOG_DBG("MAIN", "Power button released after wake, re-enabling sleep shortcut");
  }

  renderer.setFadingFix(SETTINGS.fadingFix);

  if (Serial && millis() - lastMemPrint >= 10000) {
    LOG_INF("MEM", "Free: %d bytes, Total: %d bytes, Min Free: %d bytes, MaxAlloc: %d bytes", ESP.getFreeHeap(),
            ESP.getHeapSize(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    lastMemPrint = millis();
  }

  // Handle incoming serial commands,
  // nb: we use logSerial from logging to avoid deprecation warnings
  if (logSerial.available() > 0) {
    String line = logSerial.readStringUntil('\n');
    if (line.startsWith("CMD:")) {
      String cmd = line.substring(4);
      cmd.trim();
      if (cmd == "SCREENSHOT") {
        logSerial.printf("SCREENSHOT_START:%d\n", HalDisplay::BUFFER_SIZE);
        uint8_t* buf = display.getFrameBuffer();
        logSerial.write(buf, HalDisplay::BUFFER_SIZE);
        logSerial.printf("SCREENSHOT_END\n");
      }
    }
  }

  // Check for any user activity (button press or release) or active background work
  if (gpio.wasAnyPressed() || gpio.wasAnyReleased() || activityManager.preventAutoSleep()) {
    lastActivityTime = millis();         // Reset inactivity timer
    powerManager.setPowerSaving(false);  // Restore normal CPU frequency on user activity
  }

  static bool screenshotButtonsReleased = true;
  if (gpio.isPressed(HalGPIO::BTN_POWER) && gpio.isPressed(HalGPIO::BTN_DOWN)) {
    if (screenshotButtonsReleased) {
      screenshotButtonsReleased = false;
      {
        RenderLock lock;
        ScreenshotUtil::takeScreenshot(renderer);
      }
    }
    return;
  } else {
    screenshotButtonsReleased = true;
  }

  const unsigned long sleepTimeoutMs = SETTINGS.getSleepTimeoutMs();
  if (millis() - lastActivityTime >= sleepTimeoutMs) {
    LOG_DBG("SLP", "Auto-sleep triggered after %lu ms of inactivity", sleepTimeoutMs);
    enterSleep(false);
    // This should never be hit as `enterDeepSleep` calls esp_deep_sleep_start
    return;
  }

  // X3 (Halo 2): fires the moment power is held past kX3SleepHoldMs (500 ms) — user does NOT
  //              have to release. Feels like a phone's power button: hold 500 ms and it sleeps.
  //              ignorePowerSleepUntilRelease prevents an immediate re-sleep if the user is
  //              still holding the button from the wake-up press.
  // X4: legacy hold-to-sleep behaviour, fires on release, duration from SETTINGS.
  const bool shouldSleepOnPowerButton =
      BoardConfig::kTargetBoard == BoardConfig::TargetBoard::X3
          ? (!ignorePowerSleepUntilRelease && !ignoredWakeReleaseThisLoop &&
             gpio.isPressed(HalGPIO::BTN_POWER) && gpio.getHeldTime() > kX3SleepHoldMs)
          : (!ignorePowerSleepUntilRelease && !ignoredWakeReleaseThisLoop && gpio.wasReleased(HalGPIO::BTN_POWER) &&
             gpio.getHeldTime() > SETTINGS.getPowerButtonDuration());

  if (shouldSleepOnPowerButton) {
    // If the screenshot combination is potentially being pressed, don't sleep
    if (gpio.isPressed(HalGPIO::BTN_DOWN)) {
      return;
    }
    enterSleep(true);
    // This should never be hit as `enterDeepSleep` calls esp_deep_sleep_start
    return;
  }

  const unsigned long activityStartTime = millis();
  activityManager.loop();
  const unsigned long activityDuration = millis() - activityStartTime;
#if defined(CROSSPOINT_BOARD_X3)
  if (motionSensor.consumeActivity()) {
    lastActivityTime = millis();
    powerManager.setPowerSaving(false);
  }
#endif

  const unsigned long loopDuration = millis() - loopStartTime;
  if (loopDuration > maxLoopDuration) {
    maxLoopDuration = loopDuration;
    if (maxLoopDuration > 50) {
      LOG_DBG("LOOP", "New max loop duration: %lu ms (activity: %lu ms)", maxLoopDuration, activityDuration);
    }
  }

  // Add delay at the end of the loop to prevent tight spinning
  // When an activity requests skip loop delay (e.g., webserver running), use yield() for faster response
  // Otherwise, use longer delay to save power
  if (activityManager.skipLoopDelay()) {
    powerManager.setPowerSaving(false);  // Make sure we're at full performance when skipLoopDelay is requested
    yield();                             // Give FreeRTOS a chance to run tasks, but return immediately
  } else {
    if (millis() - lastActivityTime >= HalPowerManager::IDLE_POWER_SAVING_MS) {
      // If we've been inactive for a while, increase the delay to save power
      powerManager.setPowerSaving(true);  // Lower CPU frequency after extended inactivity
      delay(50);
    } else {
      // Short delay to prevent tight loop while still being responsive
      delay(10);
    }
  }
}
