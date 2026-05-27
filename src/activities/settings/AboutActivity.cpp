#include "AboutActivity.h"

#include <string>

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "activities/boot_sleep/PokiBranding.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "images/PokiInkBrand320Rot.h"

// Version is injected at compile time via platformio.ini
#ifndef CROSSPOINT_VERSION
#define CROSSPOINT_VERSION "dev"
#endif

void AboutActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void AboutActivity::loop() {
  // Any button press goes back
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    finish();
  }
}

void AboutActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const std::string buildLabel = std::string("Build ") + CROSSPOINT_VERSION;

  renderer.clearScreen();

  // Header
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_ABOUT));

  const int brandTop = metrics.topPadding + metrics.headerHeight + 20;
  renderer.drawImage(PokiInkBrand320Rot, PokiBranding::logoX(pageWidth), brandTop, PokiBranding::kLogoWidth,
                     PokiBranding::kLogoHeight);
  renderer.drawCenteredText(SMALL_FONT_ID, brandTop + PokiBranding::kLogoHeight + 18, PokiBranding::kModelName, true,
                            EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, brandTop + PokiBranding::kLogoHeight + 46, buildLabel.c_str());

  // Divider
  const int dividerY = brandTop + PokiBranding::kLogoHeight + 74;
  renderer.fillRect(metrics.contentSidePadding * 3, dividerY, pageWidth - metrics.contentSidePadding * 6, 1);

  // Credit
  const int creditY = dividerY + 16;
  renderer.drawCenteredText(SMALL_FONT_ID, creditY, tr(STR_ABOUT_BASED_ON));
  renderer.drawCenteredText(SMALL_FONT_ID, creditY + renderer.getLineHeight(SMALL_FONT_ID) + 4,
                            tr(STR_ABOUT_AUTHOR));

  // Button hint
  GUI.drawButtonHints(renderer, nullptr, tr(STR_BACK), nullptr, nullptr);

  renderer.displayBuffer();
}
