#pragma once

namespace PokiBranding {

// Keep the portrait branding asset below the panel-safe height range so it
// still renders correctly on X3/X4 without bitmap rotation support.
inline constexpr int kLogoWidth = 320;
inline constexpr int kLogoHeight = 336;
inline constexpr int kLogoTopMargin = 122;
// Optical-balance nudge: the artwork has extra breathing room on the right
// (sparkle/arm), so shifting the logo 10px left of true center keeps the bunny
// feeling centered while nudging the whole mark slightly right.
inline constexpr int kLogoHorizontalShift = -10;
inline constexpr int kGreetingLineGap = 18;
inline constexpr int kFooterBottomMargin = 28;
inline constexpr char kModelName[] = "Halo - X3";

inline int logoX(const int pageWidth) { return (pageWidth - kLogoWidth) / 2 + kLogoHorizontalShift; }

inline int logoTop(const int pageHeight) {
  (void)pageHeight;
  return kLogoTopMargin;
}

inline int greetingLineY(const int pageHeight) { return logoTop(pageHeight) + kLogoHeight + kGreetingLineGap; }

}  // namespace PokiBranding
