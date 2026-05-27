#pragma once

// CloudLoop
#include <builtinFonts/cloudloop_8_regular.h>
#include <builtinFonts/cloudloop_10_regular.h>
#include <builtinFonts/cloudloop_12_regular.h>
#include <builtinFonts/cloudloop_14_regular.h>
#include <builtinFonts/cloudloop_16_regular.h>
#include <builtinFonts/cloudloop_18_regular.h>
#include <builtinFonts/cloudloop_20_regular.h>
#include <builtinFonts/cloudloop_22_regular.h>
#include <builtinFonts/cloudloop_36_regular.h>

// Bookerly (Latin serif). Italic is wired only for body-text sizes 14/16/18 to
// stay under the flash budget (~80KB per font). Sizes 12, 20, 22 and bold-italic
// fall back to regular/bold via EpdFontFamily's nullptr fallback.
#include <builtinFonts/bookerly_12_regular.h>
#include <builtinFonts/bookerly_12_bold.h>
#include <builtinFonts/bookerly_14_regular.h>
#include <builtinFonts/bookerly_14_bold.h>
#include <builtinFonts/bookerly_14_italic.h>
#include <builtinFonts/bookerly_16_regular.h>
#include <builtinFonts/bookerly_16_bold.h>
#include <builtinFonts/bookerly_16_italic.h>
#include <builtinFonts/bookerly_18_regular.h>
#include <builtinFonts/bookerly_18_bold.h>
#include <builtinFonts/bookerly_18_italic.h>
#include <builtinFonts/bookerly_20_regular.h>
#include <builtinFonts/bookerly_20_bold.h>
#include <builtinFonts/bookerly_22_regular.h>
#include <builtinFonts/bookerly_22_bold.h>

// Noto Serif (Latin serif + Thai via NotoSansThaiLooped font stack)
#include <builtinFonts/notoserif_12_regular.h>
#include <builtinFonts/notoserif_12_bold.h>
#include <builtinFonts/notoserif_14_regular.h>
#include <builtinFonts/notoserif_14_bold.h>
#include <builtinFonts/notoserif_16_regular.h>
#include <builtinFonts/notoserif_16_bold.h>
#include <builtinFonts/notoserif_18_regular.h>
#include <builtinFonts/notoserif_18_bold.h>
#include <builtinFonts/notoserif_20_regular.h>
#include <builtinFonts/notoserif_20_bold.h>
#include <builtinFonts/notoserif_22_regular.h>
#include <builtinFonts/notoserif_22_bold.h>

// Noto Sans SC (CJK)
#include <builtinFonts/notosanssc_8_regular.h>
#include <builtinFonts/notosanssc_10_regular.h>
#include <builtinFonts/notosanssc_10_bold.h>
#include <builtinFonts/notosanssc_12_regular.h>
#include <builtinFonts/notosanssc_12_bold.h>

// Bai Jamjuree (Thai + Latin sans-serif)
#include <builtinFonts/baijamjuree_8_regular.h>
#include <builtinFonts/baijamjuree_10_regular.h>
#include <builtinFonts/baijamjuree_12_regular.h>
#include <builtinFonts/baijamjuree_12_bold.h>
#include <builtinFonts/baijamjuree_14_regular.h>
#include <builtinFonts/baijamjuree_14_bold.h>
#include <builtinFonts/baijamjuree_16_regular.h>
#include <builtinFonts/baijamjuree_16_bold.h>
#include <builtinFonts/baijamjuree_18_regular.h>
#include <builtinFonts/baijamjuree_18_bold.h>
#include <builtinFonts/baijamjuree_20_regular.h>
#include <builtinFonts/baijamjuree_20_bold.h>
#include <builtinFonts/baijamjuree_22_regular.h>
#include <builtinFonts/baijamjuree_22_bold.h>

// Mali (Poki's handwriting voice — boot/sleep popups and greeting lines; EN + TH).
// Mali 14 is carried alongside Mali 18 as a shrink-to-fit fallback for popups
// where long EN/TH lines would overflow the bubble's inner text window.
#include <builtinFonts/mali_14_regular.h>
#include <builtinFonts/mali_18_regular.h>
