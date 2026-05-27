#include "XthViewerActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Xtc/XtcTypes.h>

#include <cstdlib>

#include "fontIds.h"

namespace {

size_t getStandaloneBitmapSize(const xtc::XtgPageHeader& header, uint8_t bitDepth) {
  if (bitDepth == 2) {
    return static_cast<size_t>(header.width) * ((header.height + 7) / 8) * 2;
  }
  return ((header.width + 7) / 8) * header.height;
}

void drawClippedPixel(GfxRenderer& renderer, int x, int y, bool value) {
  if (x < 0 || y < 0 || x >= renderer.getScreenWidth() || y >= renderer.getScreenHeight()) {
    return;
  }
  renderer.drawPixel(x, y, value);
}

bool readExact(FsFile& file, uint8_t* buffer, size_t size) { return file.read(buffer, size) == size; }

bool renderStandaloneXtg(GfxRenderer& renderer, FsFile& file, const xtc::XtgPageHeader& header, int xOffset, int yOffset) {
  const size_t rowBytes = (header.width + 7) / 8;
  uint8_t* rowBuffer = static_cast<uint8_t*>(malloc(rowBytes));
  if (!rowBuffer) {
    return false;
  }

  const bool seekOk = file.seek(sizeof(xtc::XtgPageHeader));
  if (!seekOk) {
    free(rowBuffer);
    return false;
  }

  renderer.clearScreen();
  for (uint16_t y = 0; y < header.height; y++) {
    if (!readExact(file, rowBuffer, rowBytes)) {
      free(rowBuffer);
      return false;
    }

    for (uint16_t x = 0; x < header.width; x++) {
      const size_t srcByte = x / 8;
      const size_t srcBit = 7 - (x % 8);
      const bool isBlack = !((rowBuffer[srcByte] >> srcBit) & 1);
      if (isBlack) {
        drawClippedPixel(renderer, xOffset + x, yOffset + y, true);
      }
    }
  }

  free(rowBuffer);
  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
  return true;
}

bool renderStandaloneXth(GfxRenderer& renderer, const std::string& filePath, const xtc::XtgPageHeader& header, int xOffset,
                         int yOffset) {
  FsFile plane1File;
  FsFile plane2File;
  if (!Storage.openFileForRead("XTH", filePath, plane1File) || !Storage.openFileForRead("XTH", filePath, plane2File)) {
    if (plane1File) plane1File.close();
    if (plane2File) plane2File.close();
    return false;
  }

  const size_t colBytes = (header.height + 7) / 8;
  const size_t planeSize = static_cast<size_t>(header.width) * colBytes;
  const size_t plane1Offset = sizeof(xtc::XtgPageHeader);
  const size_t plane2Offset = plane1Offset + planeSize;

  uint8_t* plane1Column = static_cast<uint8_t*>(malloc(colBytes));
  uint8_t* plane2Column = static_cast<uint8_t*>(malloc(colBytes));
  if (!plane1Column || !plane2Column) {
    free(plane1Column);
    free(plane2Column);
    plane1File.close();
    plane2File.close();
    return false;
  }

  auto cleanup = [&]() {
    free(plane1Column);
    free(plane2Column);
    plane1File.close();
    plane2File.close();
  };

  auto rewindPlanes = [&]() -> bool { return plane1File.seek(plane1Offset) && plane2File.seek(plane2Offset); };

  auto renderPass = [&](uint8_t pass) -> bool {
    if (!rewindPlanes()) {
      return false;
    }

    for (size_t colIndex = 0; colIndex < header.width; colIndex++) {
      if (!readExact(plane1File, plane1Column, colBytes) || !readExact(plane2File, plane2Column, colBytes)) {
        return false;
      }

      const int x = xOffset + static_cast<int>(header.width - 1 - colIndex);
      for (uint16_t y = 0; y < header.height; y++) {
        const size_t byteInCol = y / 8;
        const size_t bitInByte = 7 - (y % 8);
        const uint8_t bit1 = (plane1Column[byteInCol] >> bitInByte) & 1;
        const uint8_t bit2 = (plane2Column[byteInCol] >> bitInByte) & 1;
        const uint8_t pixelValue = (bit1 << 1) | bit2;

        if (pass == 0) {
          if (pixelValue >= 1) {
            drawClippedPixel(renderer, x, yOffset + y, true);
          }
        } else if (pass == 1) {
          if (pixelValue == 1) {
            drawClippedPixel(renderer, x, yOffset + y, false);
          }
        } else if (pixelValue == 1 || pixelValue == 2) {
          drawClippedPixel(renderer, x, yOffset + y, false);
        }
      }
    }

    return true;
  };

  renderer.clearScreen();
  if (!renderPass(0)) {
    cleanup();
    return false;
  }
  renderer.displayBuffer(HalDisplay::FULL_REFRESH);

  renderer.clearScreen(0x00);
  if (!renderPass(1)) {
    cleanup();
    return false;
  }
  renderer.copyGrayscaleLsbBuffers();

  renderer.clearScreen(0x00);
  if (!renderPass(2)) {
    cleanup();
    return false;
  }
  renderer.copyGrayscaleMsbBuffers();
  renderer.displayGrayBuffer();

  renderer.clearScreen();
  if (!renderPass(0)) {
    cleanup();
    return false;
  }
  renderer.cleanupGrayscaleWithFrameBuffer();

  cleanup();
  return true;
}

}  // namespace

XthViewerActivity::XthViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string path)
    : Activity("XthViewer", renderer, mappedInput), filePath(std::move(path)) {}

void XthViewerActivity::showError(const char* message, HalDisplay::RefreshMode refreshMode) {
  renderer.clearScreen();
  renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2, message);
  renderer.displayBuffer(refreshMode);
}

void XthViewerActivity::onEnter() {
  Activity::onEnter();

  FsFile file;
  if (!Storage.openFileForRead("XTH", filePath, file)) {
    LOG_ERR("XTH", "Could not open file: %s", filePath.c_str());
    showError("Could not open file", HalDisplay::FULL_REFRESH);
    return;
  }

  const size_t fileSize = file.size();
  xtc::XtgPageHeader header{};
  const size_t headerRead = file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header));
  if (headerRead != sizeof(header)) {
    file.close();
    LOG_ERR("XTH", "Failed to read header: %s", filePath.c_str());
    showError("Invalid XTH/XTG file", HalDisplay::FAST_REFRESH);
    return;
  }

  uint8_t bitDepth = 0;
  if (header.magic == xtc::XTH_MAGIC) {
    bitDepth = 2;
  } else if (header.magic == xtc::XTG_MAGIC) {
    bitDepth = 1;
  } else {
    file.close();
    LOG_ERR("XTH", "Unsupported magic 0x%08lx in %s", static_cast<unsigned long>(header.magic), filePath.c_str());
    showError("Invalid XTH/XTG file", HalDisplay::FAST_REFRESH);
    return;
  }

  LOG_DBG("XTH", "Header %s magic=0x%08lx width=%u height=%u color=%u compression=%u dataSize=%lu fileSize=%lu",
          filePath.c_str(), static_cast<unsigned long>(header.magic), header.width, header.height, header.colorMode,
          header.compression, static_cast<unsigned long>(header.dataSize), static_cast<unsigned long>(fileSize));

  if (header.width == 0 || header.height == 0 || header.compression != 0) {
    file.close();
    LOG_ERR("XTH", "Unsupported header in %s (w=%u h=%u compression=%u)", filePath.c_str(), header.width,
            header.height, header.compression);
    showError("Unsupported XTH/XTG file", HalDisplay::FAST_REFRESH);
    return;
  }

  const size_t bitmapSize = getStandaloneBitmapSize(header, bitDepth);
  if (header.dataSize < bitmapSize || fileSize < sizeof(header) + bitmapSize) {
    file.close();
    LOG_ERR("XTH", "Bitmap payload invalid in %s (header=%lu expected=%lu file=%lu)", filePath.c_str(),
            static_cast<unsigned long>(header.dataSize), static_cast<unsigned long>(bitmapSize),
            static_cast<unsigned long>(fileSize));
    showError("Corrupted XTH/XTG file", HalDisplay::FAST_REFRESH);
    return;
  }

  const int xOffset = (renderer.getScreenWidth() - static_cast<int>(header.width)) / 2;
  const int yOffset = (renderer.getScreenHeight() - static_cast<int>(header.height)) / 2;

  bool rendered = false;
  if (bitDepth == 2) {
    file.close();
    rendered = renderStandaloneXth(renderer, filePath, header, xOffset, yOffset);
  } else {
    rendered = renderStandaloneXtg(renderer, file, header, xOffset, yOffset);
    file.close();
  }

  if (!rendered) {
    LOG_ERR("XTH", "Failed to stream-render %s", filePath.c_str());
    showError("Read error", HalDisplay::FAST_REFRESH);
    return;
  }

  LOG_DBG("XTH", "Rendered %s (%ux%u, %u-bit)", filePath.c_str(), header.width, header.height, bitDepth);
}

void XthViewerActivity::onExit() {
  Activity::onExit();
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

void XthViewerActivity::loop() {
  Activity::loop();

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
  }
}
