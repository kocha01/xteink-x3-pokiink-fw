#include "BootActivity.h"

#include <Arduino.h>
#include <array>
#include <esp_system.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include "PokiBranding.h"
#include "fontIds.h"
#include "images/PokiInkBrand320Rot.h"

namespace {
constexpr std::array<StrId, 10> kBootGreetings = {
    StrId::STR_BOOT_GREETING_1,
    StrId::STR_BOOT_GREETING_2,
    StrId::STR_BOOT_GREETING_3,
    StrId::STR_BOOT_GREETING_4,
    StrId::STR_BOOT_GREETING_5,
    StrId::STR_BOOT_GREETING_6,
    StrId::STR_BOOT_GREETING_7,
    StrId::STR_BOOT_GREETING_8,
    StrId::STR_BOOT_GREETING_9,
    StrId::STR_BOOT_GREETING_10,
};

StrId pickRandomBootGreeting() {
  return kBootGreetings[esp_random() % kBootGreetings.size()];
}
}  // namespace

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const int modelFooterY = pageHeight - renderer.getLineHeight(SMALL_FONT_ID) - PokiBranding::kFooterBottomMargin;

  // Show boot screen so the user has clear visual feedback while the device loads.
  // Ghost-clearing from the previous activity (sleep wallpaper on wake) is handled
  // centrally by ActivityManager's HALF_REFRESH transition override on X3 — no
  // separate invert-refresh pass is needed here, and no cold-boot hold either.
  renderer.clearScreen();
  renderer.drawImage(PokiInkBrand320Rot, PokiBranding::logoX(pageWidth), PokiBranding::logoTop(pageHeight),
                     PokiBranding::kLogoWidth, PokiBranding::kLogoHeight);
  // Greeting line uses Poki's handwriting voice (Mali 18 — EN + TH native); the
  // "Halo - X3" footer stays in the UI sans-serif since it's a fixed brand mark.
  renderer.drawCenteredText(POKI_18_FONT_ID, PokiBranding::greetingLineY(pageHeight),
                            I18N.get(pickRandomBootGreeting()));
  renderer.drawCenteredText(SMALL_FONT_ID, modelFooterY, PokiBranding::kModelName, true, EpdFontFamily::BOLD);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
