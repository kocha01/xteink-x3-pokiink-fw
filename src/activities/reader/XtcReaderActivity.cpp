/**
 * XtcReaderActivity.cpp
 *
 * XTC ebook reader activity implementation
 * Displays pre-rendered XTC pages on e-ink display
 */

#include "XtcReaderActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <cstring>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "ReaderUtils.h"
#include "RecentBooksStore.h"
#include "XtcReaderChapterSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr unsigned long skipPageMs = 700;
constexpr unsigned long goHomeMs = 1000;

size_t getXthPlaneSize(const uint16_t pageWidth, const uint16_t pageHeight) {
  return static_cast<size_t>(pageWidth) * ((pageHeight + 7) / 8);
}

bool getColumnBit(const uint8_t* columnData, const uint16_t y) {
  const size_t byteInCol = y / 8;
  const size_t bitInByte = 7 - (y % 8);
  return ((columnData[byteInCol] >> bitInByte) & 1U) != 0;
}

void drawMonochromeColumn(GfxRenderer& renderer, const uint8_t* columnData, const uint16_t x, const uint16_t pageHeight) {
  for (uint16_t y = 0; y < pageHeight; y++) {
    if (getColumnBit(columnData, y)) {
      renderer.drawPixel(x, y, true);
    }
  }
}

bool render1BitPageStreaming(GfxRenderer& renderer, const Xtc& xtc, const uint32_t pageIndex, const uint16_t pageWidth,
                             const uint16_t pageHeight) {
  const size_t rowBytes = (pageWidth + 7) / 8;
  bool callbackOk = true;

  renderer.clearScreen();
  const auto error = xtc.loadPageStreaming(
      pageIndex,
      [&](const uint8_t* data, const size_t size, const size_t offset) {
        if (!callbackOk) {
          return;
        }

        for (size_t i = 0; i < size; i++) {
          const size_t globalOffset = offset + i;
          const uint16_t y = static_cast<uint16_t>(globalOffset / rowBytes);
          if (y >= pageHeight) {
            callbackOk = false;
            return;
          }

          const uint16_t byteInRow = static_cast<uint16_t>(globalOffset % rowBytes);
          const uint8_t value = data[i];
          const uint16_t xBase = byteInRow * 8;

          for (uint8_t bit = 0; bit < 8; bit++) {
            const uint16_t x = xBase + bit;
            if (x >= pageWidth) {
              break;
            }
            const bool isBlack = ((value >> (7 - bit)) & 1U) == 0;
            if (isBlack) {
              renderer.drawPixel(x, y, true);
            }
          }
        }
      },
      1024);

  return callbackOk && error == xtc::XtcError::OK;
}

bool loadPlane1Buffer(const Xtc& xtc, const uint32_t pageIndex, uint8_t* plane1Buffer, const size_t planeSize) {
  size_t copiedBytes = 0;
  const auto error = xtc.loadPageStreaming(
      pageIndex,
      [&](const uint8_t* data, const size_t size, const size_t offset) {
        if (offset >= planeSize) {
          return;
        }
        const size_t toCopy = std::min(size, planeSize - offset);
        std::memcpy(plane1Buffer + offset, data, toCopy);
        copiedBytes += toCopy;
      },
      1024);

  return error == xtc::XtcError::OK && copiedBytes == planeSize;
}

template <typename ColumnCallback>
bool streamPlane2Columns(const Xtc& xtc, const uint32_t pageIndex, const uint16_t pageWidth, const uint16_t pageHeight,
                         ColumnCallback&& callback) {
  const size_t colBytes = (pageHeight + 7) / 8;
  const size_t planeSize = static_cast<size_t>(pageWidth) * colBytes;
  bool callbackOk = true;

  const auto error = xtc.loadPageStreaming(
      pageIndex,
      [&](const uint8_t* data, const size_t size, const size_t offset) {
        if (!callbackOk || offset < planeSize) {
          return;
        }

        const size_t plane2Offset = offset - planeSize;
        if (size != colBytes || (plane2Offset % colBytes) != 0) {
          callbackOk = false;
          return;
        }

        const uint16_t colIndex = static_cast<uint16_t>(plane2Offset / colBytes);
        if (colIndex >= pageWidth) {
          callbackOk = false;
          return;
        }

        callback(colIndex, data);
      },
      colBytes);

  return callbackOk && error == xtc::XtcError::OK;
}

bool render2BitPageStreaming(GfxRenderer& renderer, const Xtc& xtc, const uint32_t pageIndex, const uint16_t pageWidth,
                             const uint16_t pageHeight, const bool darkMode, int& pagesUntilFullRefresh,
                             bool& firstDisplayAfterEnter) {
  const size_t colBytes = (pageHeight + 7) / 8;
  const size_t planeSize = getXthPlaneSize(pageWidth, pageHeight);
  uint8_t* plane1Buffer = static_cast<uint8_t*>(malloc(planeSize));
  if (!plane1Buffer) {
    return false;
  }

  auto cleanup = [&]() { free(plane1Buffer); };

  if (!loadPlane1Buffer(xtc, pageIndex, plane1Buffer, planeSize)) {
    cleanup();
    return false;
  }

  // Pass 1: BW buffer - any set bit in either plane becomes black.
  renderer.clearScreen();
  for (uint16_t colIndex = 0; colIndex < pageWidth; colIndex++) {
    const uint8_t* plane1Column = plane1Buffer + static_cast<size_t>(colIndex) * colBytes;
    const uint16_t x = pageWidth - 1 - colIndex;
    drawMonochromeColumn(renderer, plane1Column, x, pageHeight);
  }

  if (!streamPlane2Columns(xtc, pageIndex, pageWidth, pageHeight,
                           [&](const uint16_t colIndex, const uint8_t* plane2Column) {
                             const uint16_t x = pageWidth - 1 - colIndex;
                             drawMonochromeColumn(renderer, plane2Column, x, pageHeight);
                           })) {
    cleanup();
    return false;
  }

  if (darkMode) {
    renderer.invertScreen();
  }
  ReaderUtils::displayWithReaderEntryRefreshCycle(renderer, pagesUntilFullRefresh, firstDisplayAfterEnter);

  // Pass 2: LSB buffer - dark grey only (pixel value 1 => bit2=1 && bit1=0).
  renderer.clearScreen(0x00);
  if (!streamPlane2Columns(xtc, pageIndex, pageWidth, pageHeight,
                           [&](const uint16_t colIndex, const uint8_t* plane2Column) {
                             const uint8_t* plane1Column = plane1Buffer + static_cast<size_t>(colIndex) * colBytes;
                             const uint16_t x = pageWidth - 1 - colIndex;

                             for (uint16_t y = 0; y < pageHeight; y++) {
                               const bool bit1 = getColumnBit(plane1Column, y);
                               const bool bit2 = getColumnBit(plane2Column, y);
                               if (bit2 && !bit1) {
                                 renderer.drawPixel(x, y, false);
                               }
                             }
                           })) {
    cleanup();
    return false;
  }
  renderer.copyGrayscaleLsbBuffers();

  // Pass 3: MSB buffer - light or dark grey (pixel values 1 or 2 => bit1 XOR bit2).
  renderer.clearScreen(0x00);
  if (!streamPlane2Columns(xtc, pageIndex, pageWidth, pageHeight,
                           [&](const uint16_t colIndex, const uint8_t* plane2Column) {
                             const uint8_t* plane1Column = plane1Buffer + static_cast<size_t>(colIndex) * colBytes;
                             const uint16_t x = pageWidth - 1 - colIndex;

                             for (uint16_t y = 0; y < pageHeight; y++) {
                               const bool bit1 = getColumnBit(plane1Column, y);
                               const bool bit2 = getColumnBit(plane2Column, y);
                               if (bit1 != bit2) {
                                 renderer.drawPixel(x, y, false);
                               }
                             }
                           })) {
    cleanup();
    return false;
  }
  renderer.copyGrayscaleMsbBuffers();
  renderer.displayGrayBuffer();

  // Pass 4: Restore BW framebuffer for the next differential refresh.
  renderer.clearScreen();
  for (uint16_t colIndex = 0; colIndex < pageWidth; colIndex++) {
    const uint8_t* plane1Column = plane1Buffer + static_cast<size_t>(colIndex) * colBytes;
    const uint16_t x = pageWidth - 1 - colIndex;
    drawMonochromeColumn(renderer, plane1Column, x, pageHeight);
  }

  if (!streamPlane2Columns(xtc, pageIndex, pageWidth, pageHeight,
                           [&](const uint16_t colIndex, const uint8_t* plane2Column) {
                             const uint16_t x = pageWidth - 1 - colIndex;
                             drawMonochromeColumn(renderer, plane2Column, x, pageHeight);
                           })) {
    cleanup();
    return false;
  }

  renderer.cleanupGrayscaleWithFrameBuffer();
  cleanup();
  return true;
}
}  // namespace

void XtcReaderActivity::onEnter() {
  Activity::onEnter();
#if defined(CROSSPOINT_BOARD_X3)
  motionSensor.resetGestureState();
#endif

  if (!xtc) {
    return;
  }

  xtc->setupCacheDir();

  // Load saved progress
  loadProgress();

  // Save current XTC as last opened book and add to recent books
  APP_STATE.openEpubPath = xtc->getPath();
  APP_STATE.saveToFile();
  RECENT_BOOKS.addBook(xtc->getPath(), xtc->getTitle(), xtc->getAuthor(), xtc->getThumbBmpPath());

  // Trigger first update
  requestUpdate();
}

void XtcReaderActivity::onExit() {
  Activity::onExit();

  APP_STATE.readerActivityLoadCount = 0;
  APP_STATE.saveToFile();
  xtc.reset();
}

void XtcReaderActivity::loop() {
  // Enter chapter selection activity
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (xtc && xtc->hasChapters() && !xtc->getChapters().empty()) {
      startActivityForResult(
          std::make_unique<XtcReaderChapterSelectionActivity>(renderer, mappedInput, xtc, currentPage),
          [this](const ActivityResult& result) {
            if (!result.isCancelled) {
              currentPage = std::get<PageResult>(result.data).page;
            }
          });
    }
  }

  // Long press BACK (1s+) goes to file selection
  if (mappedInput.isPressed(MappedInputManager::Button::Back) && mappedInput.getHeldTime() >= goHomeMs) {
    activityManager.goToFileBrowser(xtc ? xtc->getPath() : "");
    return;
  }

  // Short press BACK goes directly to home
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) && mappedInput.getHeldTime() < goHomeMs) {
    onGoHome();
    return;
  }

  auto [prevTriggered, nextTriggered] = ReaderUtils::detectPageTurn(mappedInput);

  if (!prevTriggered && !nextTriggered) {
    return;
  }

  // Handle end of book
  if (currentPage >= xtc->getPageCount()) {
    currentPage = xtc->getPageCount() - 1;
    requestUpdate();
    return;
  }

  const bool skipPages = SETTINGS.longPressChapterSkip && mappedInput.getHeldTime() > skipPageMs;
  const int skipAmount = skipPages ? 10 : 1;

  if (prevTriggered) {
    if (currentPage >= static_cast<uint32_t>(skipAmount)) {
      currentPage -= skipAmount;
    } else {
      currentPage = 0;
    }
    requestUpdate();
  } else if (nextTriggered) {
    currentPage += skipAmount;
    if (currentPage >= xtc->getPageCount()) {
      currentPage = xtc->getPageCount();  // Allow showing "End of book"
    }
    requestUpdate();
  }
}

void XtcReaderActivity::render(RenderLock&&) {
  if (!xtc) {
    return;
  }

  // Bounds check
  if (currentPage >= xtc->getPageCount()) {
    // Show end of book screen
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_END_OF_BOOK), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  renderPage();
  saveProgress();
}

void XtcReaderActivity::renderPage() {
  const uint16_t pageWidth = xtc->getPageWidth();
  const uint16_t pageHeight = xtc->getPageHeight();
  const uint8_t bitDepth = xtc->getBitDepth();
  bool rendered = false;

  if (bitDepth == 2) {
    rendered = render2BitPageStreaming(renderer, *xtc, currentPage, pageWidth, pageHeight, SETTINGS.readerDarkMode,
                                       pagesUntilFullRefresh, firstDisplayAfterEnter);
    if (!rendered) {
      LOG_ERR("XTR", "Failed to stream-render 2-bit page %lu", currentPage);
      renderer.clearScreen();
      renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_MEMORY_ERROR), true, EpdFontFamily::BOLD);
      renderer.displayBuffer();
      return;
    }

    LOG_DBG("XTR", "Rendered page %lu/%lu (2-bit streaming)", currentPage + 1, xtc->getPageCount());
    return;
  }

  rendered = render1BitPageStreaming(renderer, *xtc, currentPage, pageWidth, pageHeight);
  if (!rendered) {
    LOG_ERR("XTR", "Failed to stream-render 1-bit page %lu", currentPage);
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_PAGE_LOAD_ERROR), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  if (SETTINGS.readerDarkMode) {
    renderer.invertScreen();
  }

  ReaderUtils::displayWithReaderEntryRefreshCycle(renderer, pagesUntilFullRefresh, firstDisplayAfterEnter);

  LOG_DBG("XTR", "Rendered page %lu/%lu (1-bit streaming)", currentPage + 1, xtc->getPageCount());
}

void XtcReaderActivity::saveProgress() const {
  FsFile f;
  if (Storage.openFileForWrite("XTR", xtc->getCachePath() + "/progress.bin", f)) {
    uint8_t data[4];
    data[0] = currentPage & 0xFF;
    data[1] = (currentPage >> 8) & 0xFF;
    data[2] = (currentPage >> 16) & 0xFF;
    data[3] = (currentPage >> 24) & 0xFF;
    f.write(data, 4);
    f.close();
  }
}

void XtcReaderActivity::loadProgress() {
  FsFile f;
  if (Storage.openFileForRead("XTR", xtc->getCachePath() + "/progress.bin", f)) {
    uint8_t data[4];
    if (f.read(data, 4) == 4) {
      currentPage = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
      LOG_DBG("XTR", "Loaded progress: page %lu", currentPage);

      // Validate page number
      if (currentPage >= xtc->getPageCount()) {
        currentPage = 0;
      }
    }
    f.close();
  }
}
