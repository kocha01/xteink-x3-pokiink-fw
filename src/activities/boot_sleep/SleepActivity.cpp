#include "SleepActivity.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Txt.h>
#include <Xtc.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <esp_system.h>
#include <string>
#include <vector>

#include "Bitmap.h"
#include "PokiBranding.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "SleepWallpaperCache.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "images/PokiInkBrand320Rot.h"
#include "images/PokiSleepPopup500Rot.h"

namespace {
HalDisplay::RefreshMode sleepRefreshMode() {
  // After the explicit pre-sleep ghost clear pass, HALF_REFRESH still gives a
  // clean final sleep render without reintroducing visible stale fragments.
  return HalDisplay::HALF_REFRESH;
}

// Illustrated pre-sleep popup geometry.
//
// The asset in PokiSleepPopup500Rot.h is pre-rotated 90 deg counter-clockwise,
// so its stored (physical) layout is 104 wide x 496 tall, while the logical
// on-screen appearance in Portrait is 496 wide x 104 tall. BOTH dimensions are
// multiples of 8 because EInkDisplay::drawImage uses `imageWidthBytes = w / 8`
// with integer truncation — mismatched dims render as diagonal stripes.
//
// GfxRenderer::drawImage's (width, height) parameters are passed straight
// through to display.drawImage in PHYSICAL coordinates, so we must pass the
// STORED dims (104, 496), not the logical appearance dims (496, 104).
//
// The speech-bubble text window was cleared in the asset so firmware can
// overlay the localized STR_ENTERING_SLEEP line in Poki's handwriting font.
constexpr int kPokiPopupAppearanceW = 496;  // logical on-screen width
constexpr int kPokiPopupAppearanceH = 104;  // logical on-screen height
constexpr int kPokiPopupStoredW = 104;      // drawImage `width` param (physical)
constexpr int kPokiPopupStoredH = 496;      // drawImage `height` param (physical)
// Text window in logical appearance coords. The bounds were calibrated by
// probing the generated preview PNG:
//   bubble frame rightmost dark pixel ≈ x=482..490
//   bunny head rightmost dark pixel  ≈ x=104
// so the safe band for drawing text is roughly 115..475 (sitting just right
// of the bunny/divider and just left of the bubble frame stroke). The original
// projection from source ERASE_BOX (250,40)-(945,215) → (108,17)-(407,90) in
// 496×104 left ~70 px of unused space on the right that made the rendering
// look off-center and caused long lines to overflow both sides once we also
// centred the text horizontally within the too-narrow box.
constexpr int kPokiPopupTextLeft = 115;
constexpr int kPokiPopupTextTop = 17;
constexpr int kPokiPopupTextRight = 475;
constexpr int kPokiPopupTextBottom = 90;
// Keep the popup aligned with the previous text-only popup's vertical position
// so animated transitions between activities still feel consistent.
constexpr int kPokiPopupTopY = 60;

constexpr std::array<StrId, 5> kSleepGreetings = {
    StrId::STR_SLEEP_GREETING_1,
    StrId::STR_SLEEP_GREETING_2,
    StrId::STR_SLEEP_GREETING_3,
    StrId::STR_SLEEP_GREETING_4,
    StrId::STR_SLEEP_GREETING_5,
};

StrId pickRandomSleepGreeting() {
  return kSleepGreetings[esp_random() % kSleepGreetings.size()];
}

void drawPokiPopup(const GfxRenderer& renderer, const char* message) {
  const int popupX = (renderer.getScreenWidth() - kPokiPopupAppearanceW) / 2;
  const int popupY = kPokiPopupTopY;

  // The asset is a fully opaque 496x104 rectangle (white background + black
  // line art), so drawImage overwrites whatever was underneath — no fillRect
  // scrub is needed before it. Pass STORED dims (104x496) to drawImage since
  // the bitmap is pre-rotated 90 deg CCW for the Portrait path.
  renderer.drawImage(PokiSleepPopup500Rot, popupX, popupY, kPokiPopupStoredW, kPokiPopupStoredH);

  if (message != nullptr && message[0] != '\0') {
    const int textBoxWidth = kPokiPopupTextRight - kPokiPopupTextLeft;
    const int textBoxHeight = kPokiPopupTextBottom - kPokiPopupTextTop;
    // Shrink-to-fit: try Mali 18 first (the intended Poki voice), drop to
    // Mali 14 if the line is wider than the bubble's text window. Both sizes
    // are centred horizontally and vertically inside the window — no text is
    // ever clipped because drawText has no background fill and overflow would
    // land on the asset's line art.
    int chosenFont = POKI_18_FONT_ID;
    int textWidth = renderer.getTextWidth(chosenFont, message);
    if (textWidth > textBoxWidth) {
      chosenFont = POKI_14_FONT_ID;
      textWidth = renderer.getTextWidth(chosenFont, message);
    }
    const int lineHeight = renderer.getLineHeight(chosenFont);
    const int textX = popupX + kPokiPopupTextLeft + (textBoxWidth - textWidth) / 2;
    const int textY = popupY + kPokiPopupTextTop + (textBoxHeight - lineHeight) / 2;
    renderer.drawText(chosenFont, textX, textY, message);
  }

  renderer.displayBuffer();
}

// Random folder pick: scan /.sleep (preferred) then /sleep for valid BMPs,
// pick one avoiding immediate repeat via APP_STATE.lastSleepImage. Returns
// the absolute path of the chosen file or empty string on failure.
std::string pickRandomSleepFromFolder() {
  for (const char* sleepDir : {"/.sleep", "/sleep"}) {
    auto dir = Storage.open(sleepDir);
    if (!dir || !dir.isDirectory()) {
      if (dir) dir.close();
      continue;
    }

    std::vector<std::string> files;
    char name[500];
    for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
      if (file.isDirectory()) {
        file.close();
        continue;
      }
      file.getName(name, sizeof(name));
      const std::string filename(name);
      if (filename.empty() || filename[0] == '.' || !FsHelpers::hasBmpExtension(filename)) {
        file.close();
        continue;
      }
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() != BmpReaderError::Ok) {
        LOG_DBG("SLP", "Skipping invalid BMP file: %s", name);
        file.close();
        continue;
      }
      files.emplace_back(filename);
      file.close();
    }
    dir.close();

    const auto numFiles = files.size();
    if (numFiles == 0) {
      continue;
    }

    // Pick a random index avoiding immediate repeat (when there's >1 file).
    auto randomIndex = static_cast<uint8_t>(esp_random() % numFiles);
    while (numFiles > 1 && APP_STATE.lastSleepImage != UINT8_MAX && randomIndex == APP_STATE.lastSleepImage) {
      randomIndex = static_cast<uint8_t>(esp_random() % numFiles);
    }
    APP_STATE.lastSleepImage = randomIndex;
    APP_STATE.saveToFile();

    return std::string(sleepDir) + "/" + files[randomIndex];
  }
  return {};
}
}  // namespace

void SleepActivity::onEnter() {
  Activity::onEnter();
  drawPokiPopup(renderer, tr(STR_ENTERING_SLEEP));

  switch (SETTINGS.sleepScreen) {
    case CrossPointSettings::SLEEP_SCREEN_MODE::BLANK:
      return renderBlankSleepScreen();
    case CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM:
      return renderCustomSleepScreen();
    case CrossPointSettings::SLEEP_SCREEN_MODE::COVER:
    case CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM:
      return renderCoverSleepScreen();
    default:
      return renderDefaultSleepScreen();
  }
}

void SleepActivity::renderCustomSleepScreen() const {
  // Specific filename selected → use the cached fast path (warmed on boot).
  if (SETTINGS.customSleepImagePath[0] != '\0') {
    const std::string filename = SETTINGS.customSleepImagePath;
    if (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos ||
        !FsHelpers::hasBmpExtension(filename)) {
      LOG_ERR("SLP", "Ignoring invalid custom sleep wallpaper path: %s", SETTINGS.customSleepImagePath);
      return renderDefaultSleepScreen();
    }

    const std::string filePath = "/sleep/" + filename;
    if (SleepWallpaperCache::renderFromCache(renderer, filePath, sleepRefreshMode())) {
      return;
    }

    LOG_DBG("SLP", "Sleep wallpaper cache miss; rendering source file: %s", filePath.c_str());
    if (SleepWallpaperCache::renderAndCacheFromSource(renderer, filePath, sleepRefreshMode())) {
      return;
    }
    LOG_ERR("SLP", "Failed to render sleep wallpaper from source: %s", filePath.c_str());
    return renderDefaultSleepScreen();
  }

  // Empty path → random folder rotation through /.sleep or /sleep.
  const std::string randomPath = pickRandomSleepFromFolder();
  if (randomPath.empty()) {
    LOG_DBG("SLP", "No sleep wallpapers found in /.sleep or /sleep; falling back to default");
    return renderDefaultSleepScreen();
  }

  FsFile file;
  if (Storage.openFileForRead("SLP", randomPath, file)) {
    LOG_DBG("SLP", "Loading random sleep wallpaper: %s", randomPath.c_str());
    Bitmap bitmap(file, true);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      renderBitmapSleepScreen(bitmap);
      file.close();
      return;
    }
    file.close();
  }

  LOG_ERR("SLP", "Failed to render random sleep wallpaper: %s", randomPath.c_str());
  return renderDefaultSleepScreen();
}

void SleepActivity::renderCoverSleepScreen() const {
  // COVER falls back to default brand; COVER_CUSTOM falls back to custom wallpaper logic
  // (which itself falls back to default if no folder/file is configured).
  void (SleepActivity::*renderNoCoverSleepScreen)() const =
      (SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM)
          ? &SleepActivity::renderCustomSleepScreen
          : &SleepActivity::renderDefaultSleepScreen;

  if (APP_STATE.openEpubPath.empty()) {
    return (this->*renderNoCoverSleepScreen)();
  }

  const bool cropped = SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP;
  std::string coverBmpPath;

  auto tryRenderCachedCover = [&](const std::string& bmpPath) -> bool {
    FsFile file;
    if (!Storage.openFileForRead("SLP", bmpPath, file)) {
      return false;
    }
    Bitmap bitmap(file);
    if (bitmap.parseHeaders() != BmpReaderError::Ok) {
      file.close();
      return false;
    }
    LOG_DBG("SLP", "Rendering cached sleep cover: %s", bmpPath.c_str());
    renderBitmapSleepScreen(bitmap);
    file.close();
    return true;
  };

  if (FsHelpers::hasXtcExtension(APP_STATE.openEpubPath)) {
    Xtc lastXtc(APP_STATE.openEpubPath, "/.crosspoint");
    coverBmpPath = lastXtc.getCoverBmpPath();
    if (tryRenderCachedCover(coverBmpPath)) return;

    if (!lastXtc.load()) {
      LOG_ERR("SLP", "Failed to load last XTC");
      return (this->*renderNoCoverSleepScreen)();
    }
    if (!lastXtc.generateCoverBmp()) {
      LOG_ERR("SLP", "Failed to generate XTC cover bmp");
      return (this->*renderNoCoverSleepScreen)();
    }
  } else if (FsHelpers::hasTxtExtension(APP_STATE.openEpubPath)) {
    Txt lastTxt(APP_STATE.openEpubPath, "/.crosspoint");
    coverBmpPath = lastTxt.getCoverBmpPath();
    if (tryRenderCachedCover(coverBmpPath)) return;

    if (!lastTxt.load()) {
      LOG_ERR("SLP", "Failed to load last TXT");
      return (this->*renderNoCoverSleepScreen)();
    }
    if (!lastTxt.generateCoverBmp()) {
      LOG_ERR("SLP", "No cover image found for TXT file");
      return (this->*renderNoCoverSleepScreen)();
    }
  } else if (FsHelpers::hasEpubExtension(APP_STATE.openEpubPath)) {
    Epub lastEpub(APP_STATE.openEpubPath, "/.crosspoint");
    coverBmpPath = lastEpub.getCoverBmpPath(cropped);
    if (tryRenderCachedCover(coverBmpPath)) return;

    // Skip loading css since we only need cover metadata here.
    if (!lastEpub.load(true, true)) {
      LOG_ERR("SLP", "Failed to load last epub");
      return (this->*renderNoCoverSleepScreen)();
    }
    if (!lastEpub.generateCoverBmp(cropped)) {
      LOG_ERR("SLP", "Failed to generate cover bmp");
      return (this->*renderNoCoverSleepScreen)();
    }
  } else {
    return (this->*renderNoCoverSleepScreen)();
  }

  if (tryRenderCachedCover(coverBmpPath)) {
    return;
  }
  return (this->*renderNoCoverSleepScreen)();
}

void SleepActivity::renderBitmapSleepScreen(const Bitmap& bitmap) const {
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int bmpWidth = bitmap.getWidth();
  const int bmpHeight = bitmap.getHeight();

  int x = 0;
  int y = 0;
  float cropX = 0.0f;
  float cropY = 0.0f;

  LOG_DBG("SLP", "bitmap %d x %d, screen %d x %d", bmpWidth, bmpHeight, pageWidth, pageHeight);

  if (bmpWidth > pageWidth || bmpHeight > pageHeight) {
    float ratio = static_cast<float>(bmpWidth) / static_cast<float>(bmpHeight);
    const float screenRatio = static_cast<float>(pageWidth) / static_cast<float>(pageHeight);

    if (ratio > screenRatio) {
      // Image wider than viewport ratio: scaled-down image needs to be centered vertically.
      if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP) {
        cropX = 1.0f - (screenRatio / ratio);
        ratio = (1.0f - cropX) * static_cast<float>(bmpWidth) / static_cast<float>(bmpHeight);
      }
      x = 0;
      y = static_cast<int>(std::round(
          (static_cast<float>(pageHeight) - static_cast<float>(pageWidth) / ratio) / 2.0f));
    } else {
      // Image taller than viewport ratio: scaled-down image needs to be centered horizontally.
      if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP) {
        cropY = 1.0f - (ratio / screenRatio);
        ratio = static_cast<float>(bmpWidth) / ((1.0f - cropY) * static_cast<float>(bmpHeight));
      }
      x = static_cast<int>(std::round(
          (static_cast<float>(pageWidth) - static_cast<float>(pageHeight) * ratio) / 2.0f));
      y = 0;
    }
  } else {
    // Image fits without scaling — center it on screen.
    x = (pageWidth - bmpWidth) / 2;
    y = (pageHeight - bmpHeight) / 2;
  }

  const bool useGreyscale = bitmap.hasGreyscale() &&
                            SETTINGS.sleepScreenCoverFilter ==
                                CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::NO_FILTER;

  renderer.clearScreen();
  renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);

  if (SETTINGS.sleepScreenCoverFilter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::INVERTED_BLACK_AND_WHITE) {
    renderer.invertScreen();
  }

  renderer.displayBuffer(sleepRefreshMode());

  if (useGreyscale) {
    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    renderer.copyGrayscaleLsbBuffers();

    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    renderer.copyGrayscaleMsbBuffers();

    renderer.displayGrayBuffer();
    renderer.setRenderMode(GfxRenderer::BW);
  }
}

void SleepActivity::renderDefaultSleepScreen() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const int modelFooterY = pageHeight - renderer.getLineHeight(SMALL_FONT_ID) - PokiBranding::kFooterBottomMargin;

  renderer.clearScreen();
  renderer.drawImage(PokiInkBrand320Rot, PokiBranding::logoX(pageWidth), PokiBranding::logoTop(pageHeight),
                     PokiBranding::kLogoWidth, PokiBranding::kLogoHeight);
  // Greeting line uses Poki's handwriting voice (Mali 18 — EN + TH native); the
  // "Halo - X3" footer stays in the UI sans-serif since it's a fixed brand mark.
  renderer.drawCenteredText(POKI_18_FONT_ID, PokiBranding::greetingLineY(pageHeight),
                            I18N.get(pickRandomSleepGreeting()));
  renderer.drawCenteredText(SMALL_FONT_ID, modelFooterY, PokiBranding::kModelName, true, EpdFontFamily::BOLD);

  if (SETTINGS.sleepScreen != CrossPointSettings::SLEEP_SCREEN_MODE::LIGHT) {
    renderer.invertScreen();
  }

  renderer.displayBuffer(sleepRefreshMode());
}

void SleepActivity::renderBlankSleepScreen() const {
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}
