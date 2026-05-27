#include <HalDisplay.h>
#include <HalGPIO.h>

HalDisplay::HalDisplay()
    : einkDisplay(BoardConfig::Pins::kEpdSclk, BoardConfig::Pins::kEpdMosi, BoardConfig::Pins::kEpdCs,
                  BoardConfig::Pins::kEpdDc, BoardConfig::Pins::kEpdRst, BoardConfig::Pins::kEpdBusy) {
#if defined(CROSSPOINT_BOARD_X3)
  einkDisplay.setDisplayX3();
#endif
}

HalDisplay::~HalDisplay() {}

void HalDisplay::begin() {
  einkDisplay.begin();

#if defined(CROSSPOINT_BOARD_X3)
  // Official CrossPoint X3 resyncs after wake/flash so the controller RAM
  // starts from a coherent baseline before the first visible refresh.
  const auto wakeupReason = gpio.getWakeupReason();
  if (wakeupReason == HalGPIO::WakeupReason::PowerButton || wakeupReason == HalGPIO::WakeupReason::AfterFlash ||
      wakeupReason == HalGPIO::WakeupReason::Other) {
    einkDisplay.requestResync();
  }
#endif
}

void HalDisplay::clearScreen(uint8_t color) const { einkDisplay.clearScreen(color); }

void HalDisplay::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           bool fromProgmem) const {
  einkDisplay.drawImage(imageData, x, y, w, h, fromProgmem);
}

void HalDisplay::drawImageTransparent(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                      bool fromProgmem) const {
  einkDisplay.drawImageTransparent(imageData, x, y, w, h, fromProgmem);
}

EInkDisplay::RefreshMode convertRefreshMode(HalDisplay::RefreshMode mode) {
  switch (mode) {
    case HalDisplay::FULL_REFRESH:
      return EInkDisplay::FULL_REFRESH;
    case HalDisplay::HALF_REFRESH:
      return EInkDisplay::HALF_REFRESH;
    case HalDisplay::FAST_REFRESH:
    default:
      return EInkDisplay::FAST_REFRESH;
  }
}

void HalDisplay::displayBuffer(HalDisplay::RefreshMode mode, bool turnOffScreen) {
#if defined(CROSSPOINT_BOARD_X3)
  if (mode == RefreshMode::HALF_REFRESH) {
    einkDisplay.requestResync(1);
  }
#endif

  einkDisplay.displayBuffer(convertRefreshMode(mode), turnOffScreen);
}

void HalDisplay::refreshDisplay(HalDisplay::RefreshMode mode, bool turnOffScreen) {
#if defined(CROSSPOINT_BOARD_X3)
  if (mode == RefreshMode::HALF_REFRESH) {
    einkDisplay.requestResync(1);
  }
#endif

  einkDisplay.refreshDisplay(convertRefreshMode(mode), turnOffScreen);
}

void HalDisplay::deepSleep() { einkDisplay.deepSleep(); }

uint8_t* HalDisplay::getFrameBuffer() const { return einkDisplay.getFrameBuffer(); }

uint16_t HalDisplay::getBackendWidth() const { return einkDisplay.getDisplayWidth(); }

uint16_t HalDisplay::getBackendHeight() const { return einkDisplay.getDisplayHeight(); }

uint16_t HalDisplay::getBackendWidthBytes() const { return einkDisplay.getDisplayWidthBytes(); }

uint32_t HalDisplay::getBackendBufferSize() const { return einkDisplay.getBufferSize(); }

bool HalDisplay::backendMatchesBoardPanel() const {
  return getBackendWidth() == EXPECTED_PANEL_WIDTH && getBackendHeight() == EXPECTED_PANEL_HEIGHT;
}

void HalDisplay::copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) {
  einkDisplay.copyGrayscaleBuffers(lsbBuffer, msbBuffer);
}

void HalDisplay::copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) { einkDisplay.copyGrayscaleLsbBuffers(lsbBuffer); }

void HalDisplay::copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) { einkDisplay.copyGrayscaleMsbBuffers(msbBuffer); }

void HalDisplay::cleanupGrayscaleBuffers(const uint8_t* bwBuffer) { einkDisplay.cleanupGrayscaleBuffers(bwBuffer); }

void HalDisplay::displayGrayBuffer(bool turnOffScreen) { einkDisplay.displayGrayBuffer(turnOffScreen); }
