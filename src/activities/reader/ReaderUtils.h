#pragma once

#include <BoardConfig.h>
#include <CrossPointSettings.h>
#include <GfxRenderer.h>
#if defined(CROSSPOINT_BOARD_X3)
#include <HalMotionSensor.h>
#endif
#include <Logging.h>

#include "MappedInputManager.h"

namespace ReaderUtils {

constexpr unsigned long GO_HOME_MS = 1000;

inline void applyOrientation(GfxRenderer& renderer, const uint8_t orientation) {
  switch (orientation) {
    case CrossPointSettings::ORIENTATION::PORTRAIT:
      renderer.setOrientation(GfxRenderer::Orientation::Portrait);
      break;
    case CrossPointSettings::ORIENTATION::LANDSCAPE_CW:
      renderer.setOrientation(GfxRenderer::Orientation::LandscapeClockwise);
      break;
    case CrossPointSettings::ORIENTATION::INVERTED:
      renderer.setOrientation(GfxRenderer::Orientation::PortraitInverted);
      break;
    case CrossPointSettings::ORIENTATION::LANDSCAPE_CCW:
      renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
      break;
    default:
      break;
  }
}

struct PageTurnResult {
  bool prev;
  bool next;
};

inline PageTurnResult detectPageTurn(const MappedInputManager& input) {
  const bool usePress = !SETTINGS.longPressChapterSkip;
  bool prev = usePress ? (input.wasPressed(MappedInputManager::Button::PageBack) ||
                          input.wasPressed(MappedInputManager::Button::Left))
                       : (input.wasReleased(MappedInputManager::Button::PageBack) ||
                          input.wasReleased(MappedInputManager::Button::Left));
  const bool powerTurn = SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::PAGE_TURN &&
                         input.wasReleased(MappedInputManager::Button::Power);
  bool next = usePress ? (input.wasPressed(MappedInputManager::Button::PageForward) || powerTurn ||
                          input.wasPressed(MappedInputManager::Button::Right))
                       : (input.wasReleased(MappedInputManager::Button::PageForward) || powerTurn ||
                          input.wasReleased(MappedInputManager::Button::Right));
#if defined(CROSSPOINT_BOARD_X3)
  if (!prev && !next && SETTINGS.motionPageTurn != CrossPointSettings::MOTION_OFF) {
    const auto motionTurn = motionSensor.pollPageTurn(SETTINGS.motionPageTurn, SETTINGS.orientation);
    prev = motionTurn == HalMotionSensor::PageTurnDirection::Previous;
    next = motionTurn == HalMotionSensor::PageTurnDirection::Next;
  }
#endif
  return {prev, next};
}

inline void displayWithRefreshCycle(const GfxRenderer& renderer, int& pagesUntilFullRefresh) {
  if (pagesUntilFullRefresh <= 1) {
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    pagesUntilFullRefresh = SETTINGS.getRefreshFrequency();
  } else {
    renderer.displayBuffer();
    pagesUntilFullRefresh--;
  }
}

inline void displayWithReaderEntryRefreshCycle(const GfxRenderer& renderer, int& pagesUntilFullRefresh,
                                               bool& firstDisplayAfterEnter) {
#if defined(CROSSPOINT_BOARD_X3)
  if (firstDisplayAfterEnter) {
    firstDisplayAfterEnter = false;
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    pagesUntilFullRefresh = SETTINGS.getRefreshFrequency();
    return;
  }
#endif

  firstDisplayAfterEnter = false;
  displayWithRefreshCycle(renderer, pagesUntilFullRefresh);
}

inline bool shouldUseTextAntiAliasing() {
  // X3 hardware: AA grayscale path triggers 3 refreshes per page turn
  // (grayscaleRevert from previous page + BW pass + displayGrayBuffer for
  // the grayscale overlay).  Users perceive this as a full refresh on EVERY
  // page — the screen visibly winks 3 times before settling.  The original
  // X3 short-circuit was removed for "testing" without hardware verification
  // and never restored; user-confirmed regression — restoring the gate so X3
  // page turns are a single partial refresh again.  Non-X3 boards continue
  // to honor SETTINGS.textAntiAliasing.
#if defined(CROSSPOINT_BOARD_X3)
  return false;
#else
  return SETTINGS.textAntiAliasing != 0;
#endif
}

// Grayscale anti-aliasing pass. Renders content twice (LSB + MSB) to build
// the grayscale buffer. Only the content callback is re-rendered — status bars
// and other overlays should be drawn before calling this.
// Kept as a template to avoid std::function overhead; instantiated once per reader type.
template <typename RenderFn>
void renderAntiAliased(GfxRenderer& renderer, RenderFn&& renderFn) {
  if (!renderer.storeBwBuffer()) {
    LOG_ERR("READER", "Failed to store BW buffer for anti-aliasing");
    return;
  }

  renderer.clearScreen(0x00);
  renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
  renderFn();
  renderer.copyGrayscaleLsbBuffers();

  renderer.clearScreen(0x00);
  renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
  renderFn();
  renderer.copyGrayscaleMsbBuffers();

  renderer.displayGrayBuffer();
  renderer.setRenderMode(GfxRenderer::BW);

  renderer.restoreBwBuffer();
}

}  // namespace ReaderUtils
