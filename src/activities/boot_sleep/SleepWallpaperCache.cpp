#include "SleepWallpaperCache.h"

#include <Bitmap.h>
#include <BoardConfig.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace {
constexpr const char* kModule = "SLP";
constexpr const char* kCacheDir = "/.crosspoint";
constexpr const char* kMetaPath = "/.crosspoint/sleep_wallpaper.meta";
constexpr const char* kBwPath = "/.crosspoint/sleep_wallpaper.bw";
constexpr const char* kGrayLsbPath = "/.crosspoint/sleep_wallpaper.gray_lsb";
constexpr const char* kGrayMsbPath = "/.crosspoint/sleep_wallpaper.gray_msb";
constexpr uint32_t kCacheMagic = 0x43505753;  // SWPC
constexpr uint16_t kCacheVersion = 1;
constexpr size_t kHashSampleBytes = 256;
constexpr int kX4PortraitWidth = 480;
constexpr int kX4PortraitHeight = 800;

struct BitmapLayout {
  int x = 0;
  int y = 0;
  float cropX = 0.0f;
  float cropY = 0.0f;
  bool allowUpscale = false;
};

struct CacheHeader {
  uint32_t magic = kCacheMagic;
  uint16_t version = kCacheVersion;
  uint16_t screenWidth = 0;
  uint16_t screenHeight = 0;
  uint32_t bufferSize = 0;
  uint32_t sourceSize = 0;
  uint32_t sourceSignature = 0;
  uint8_t hasGreyscale = 0;
  uint8_t reserved[3] = {0, 0, 0};
};

struct SourceIdentity {
  uint32_t size = 0;
  uint32_t signature = 0;
};

bool isLegacyX4PortraitBitmap(const Bitmap& bitmap) {
  return BoardConfig::kTargetBoard == BoardConfig::TargetBoard::X3 &&
         bitmap.getWidth() == kX4PortraitWidth && bitmap.getHeight() == kX4PortraitHeight;
}

BitmapLayout computeSleepBitmapLayout(const Bitmap& bitmap, const int pageWidth, const int pageHeight,
                                      const bool cropToFill) {
  BitmapLayout layout;
  layout.allowUpscale = bitmap.getWidth() != pageWidth || bitmap.getHeight() != pageHeight;

  if (bitmap.getWidth() == pageWidth && bitmap.getHeight() == pageHeight) {
    return layout;
  }

  float ratio = static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
  const float screenRatio = static_cast<float>(pageWidth) / static_cast<float>(pageHeight);

  if (ratio > screenRatio) {
    if (cropToFill) {
      layout.cropX = 1.0f - (screenRatio / ratio);
      ratio = (1.0f - layout.cropX) * static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
    }
    layout.x = 0;
    layout.y = std::round((static_cast<float>(pageHeight) - static_cast<float>(pageWidth) / ratio) / 2);
  } else {
    if (cropToFill) {
      layout.cropY = 1.0f - (ratio / screenRatio);
      ratio = static_cast<float>(bitmap.getWidth()) / ((1.0f - layout.cropY) * static_cast<float>(bitmap.getHeight()));
    }
    layout.x = std::round((static_cast<float>(pageWidth) - static_cast<float>(pageHeight) * ratio) / 2);
    layout.y = 0;
  }

  return layout;
}

void updateFnv1a(uint32_t& hash, const void* data, const size_t size) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < size; i++) {
    hash ^= bytes[i];
    hash *= 16777619u;
  }
}

bool hashFileRegion(FsFile& file, const size_t offset, const size_t length, uint32_t& hash) {
  if (length == 0) {
    return true;
  }

  if (!file.seek(offset)) {
    return false;
  }

  uint8_t buffer[128];
  size_t remaining = length;
  while (remaining > 0) {
    const size_t chunkSize = std::min(remaining, sizeof(buffer));
    const int bytesRead = file.read(buffer, chunkSize);
    if (bytesRead != static_cast<int>(chunkSize)) {
      return false;
    }
    updateFnv1a(hash, buffer, chunkSize);
    remaining -= chunkSize;
  }

  return true;
}

bool computeSourceIdentity(const std::string& sourcePath, SourceIdentity& identity) {
  FsFile file;
  if (!Storage.openFileForRead(kModule, sourcePath, file)) {
    LOG_ERR(kModule, "Failed to open sleep wallpaper for cache validation: %s", sourcePath.c_str());
    return false;
  }

  identity.size = static_cast<uint32_t>(file.size());
  uint32_t hash = 2166136261u;
  updateFnv1a(hash, sourcePath.data(), sourcePath.size());
  updateFnv1a(hash, &identity.size, sizeof(identity.size));

  const size_t fileSize = identity.size;
  const size_t headBytes = std::min(fileSize, kHashSampleBytes);
  const size_t tailBytes = fileSize > kHashSampleBytes ? std::min(fileSize - headBytes, kHashSampleBytes) : 0;
  const size_t middleBytes =
      fileSize > (headBytes + tailBytes) ? std::min(fileSize - headBytes - tailBytes, kHashSampleBytes) : 0;

  bool ok = hashFileRegion(file, 0, headBytes, hash);
  if (ok && middleBytes > 0) {
    const size_t middleOffset = (fileSize - middleBytes) / 2;
    ok = hashFileRegion(file, middleOffset, middleBytes, hash);
  }
  if (ok && tailBytes > 0) {
    ok = hashFileRegion(file, fileSize - tailBytes, tailBytes, hash);
  }

  file.close();

  if (!ok) {
    LOG_ERR(kModule, "Failed to hash sleep wallpaper for cache validation: %s", sourcePath.c_str());
    return false;
  }

  identity.signature = hash;
  return true;
}

bool writeBufferToFile(const char* path, const uint8_t* buffer, const size_t size) {
  FsFile file;
  if (!Storage.openFileForWrite(kModule, path, file)) {
    LOG_ERR(kModule, "Failed to open cache file for write: %s", path);
    return false;
  }

  const bool ok = file.write(buffer, size) == size;
  file.close();

  if (!ok) {
    LOG_ERR(kModule, "Short write while saving cache file: %s", path);
  }

  return ok;
}

bool readBufferFromFile(const char* path, uint8_t* buffer, const size_t size) {
  FsFile file;
  if (!Storage.openFileForRead(kModule, path, file)) {
    LOG_ERR(kModule, "Failed to open cache file for read: %s", path);
    return false;
  }

  const bool ok = file.size() == size && file.read(buffer, size) == static_cast<int>(size);
  file.close();

  if (!ok) {
    LOG_ERR(kModule, "Invalid sleep wallpaper cache file: %s", path);
  }

  return ok;
}

bool readCacheHeader(CacheHeader& header) {
  FsFile file;
  if (!Storage.openFileForRead(kModule, kMetaPath, file)) {
    return false;
  }

  const bool ok = file.size() == sizeof(CacheHeader) && file.read(&header, sizeof(CacheHeader)) == sizeof(CacheHeader);
  file.close();
  return ok;
}

bool writeCacheHeader(const CacheHeader& header) {
  FsFile file;
  if (!Storage.openFileForWrite(kModule, kMetaPath, file)) {
    LOG_ERR(kModule, "Failed to open cache header for write");
    return false;
  }

  const bool ok = file.write(&header, sizeof(CacheHeader)) == sizeof(CacheHeader);
  file.close();

  if (!ok) {
    LOG_ERR(kModule, "Failed to finalize sleep wallpaper cache header");
  }

  return ok;
}

bool isCacheHeaderValid(const CacheHeader& header, const SourceIdentity& identity, const GfxRenderer& renderer) {
  return header.magic == kCacheMagic && header.version == kCacheVersion &&
         header.screenWidth == renderer.getScreenWidth() && header.screenHeight == renderer.getScreenHeight() &&
         header.bufferSize == GfxRenderer::getBufferSize() && header.sourceSize == identity.size &&
         header.sourceSignature == identity.signature;
}

bool hasUsableCacheFiles(const CacheHeader& header) {
  FsFile bwFile;
  if (!Storage.openFileForRead(kModule, kBwPath, bwFile)) {
    return false;
  }
  const bool bwOk = bwFile.size() == header.bufferSize;
  bwFile.close();
  if (!bwOk) {
    return false;
  }

  if (!header.hasGreyscale) {
    return true;
  }

  FsFile lsbFile;
  FsFile msbFile;
  const bool lsbOk = Storage.openFileForRead(kModule, kGrayLsbPath, lsbFile) && lsbFile.size() == header.bufferSize;
  const bool msbOk = Storage.openFileForRead(kModule, kGrayMsbPath, msbFile) && msbFile.size() == header.bufferSize;
  if (lsbFile) lsbFile.close();
  if (msbFile) msbFile.close();
  return lsbOk && msbOk;
}

bool loadValidCacheHeader(const GfxRenderer& renderer, const std::string& sourcePath, CacheHeader& header,
                          SourceIdentity& identity) {
  if (!computeSourceIdentity(sourcePath, identity)) {
    return false;
  }

  if (!readCacheHeader(header)) {
    return false;
  }

  return isCacheHeaderValid(header, identity, renderer) && hasUsableCacheFiles(header);
}

bool renderBitmapToCacheFiles(GfxRenderer& renderer, const std::string& sourcePath, const SourceIdentity& identity,
                              const bool displayResult, const HalDisplay::RefreshMode refreshMode) {
  FsFile file;
  if (!Storage.openFileForRead(kModule, sourcePath, file)) {
    LOG_ERR(kModule, "Failed to open sleep wallpaper source: %s", sourcePath.c_str());
    return false;
  }

  Bitmap bitmap(file, true);
  const auto parseResult = bitmap.parseHeaders();
  if (parseResult != BmpReaderError::Ok) {
    LOG_ERR(kModule, "Failed to parse sleep wallpaper %s: %s", sourcePath.c_str(),
            Bitmap::errorToString(parseResult));
    file.close();
    return false;
  }

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const bool legacyBitmapCompat = isLegacyX4PortraitBitmap(bitmap);
  const BitmapLayout layout = computeSleepBitmapLayout(bitmap, pageWidth, pageHeight, legacyBitmapCompat);
  const size_t bufferSize = GfxRenderer::getBufferSize();
  uint8_t* frameBuffer = renderer.getFrameBuffer();

  CacheHeader header;
  header.screenWidth = static_cast<uint16_t>(pageWidth);
  header.screenHeight = static_cast<uint16_t>(pageHeight);
  header.bufferSize = static_cast<uint32_t>(bufferSize);
  header.sourceSize = identity.size;
  header.sourceSignature = identity.signature;
  header.hasGreyscale = bitmap.hasGreyscale() ? 1 : 0;

  bool cacheReady = Storage.ensureDirectoryExists(kCacheDir);
  if (!cacheReady) {
    LOG_ERR(kModule, "Failed to ensure sleep wallpaper cache directory exists");
  } else {
    Storage.remove(kMetaPath);
  }

  renderer.setRenderMode(GfxRenderer::BW);
  renderer.clearScreen();
  renderer.drawBitmap(bitmap, layout.x, layout.y, pageWidth, pageHeight, layout.cropX, layout.cropY,
                      layout.allowUpscale);

  if (cacheReady) {
    cacheReady = writeBufferToFile(kBwPath, frameBuffer, bufferSize);
  }
  if (displayResult) {
    renderer.displayBuffer(refreshMode);
  }

  if (bitmap.hasGreyscale()) {
    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    renderer.drawBitmap(bitmap, layout.x, layout.y, pageWidth, pageHeight, layout.cropX, layout.cropY,
                        layout.allowUpscale);
    if (cacheReady) {
      cacheReady = writeBufferToFile(kGrayLsbPath, frameBuffer, bufferSize);
    }
    if (displayResult) {
      renderer.copyGrayscaleLsbBuffers();
    }

    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    renderer.drawBitmap(bitmap, layout.x, layout.y, pageWidth, pageHeight, layout.cropX, layout.cropY,
                        layout.allowUpscale);
    if (cacheReady) {
      cacheReady = writeBufferToFile(kGrayMsbPath, frameBuffer, bufferSize);
    }
    if (displayResult) {
      renderer.copyGrayscaleMsbBuffers();
      renderer.displayGrayBuffer();
    }
  } else if (cacheReady) {
    Storage.remove(kGrayLsbPath);
    Storage.remove(kGrayMsbPath);
  }

  renderer.setRenderMode(GfxRenderer::BW);
  file.close();

  if (cacheReady && !writeCacheHeader(header)) {
    cacheReady = false;
  }

  if (cacheReady) {
    LOG_DBG(kModule, "Updated sleep wallpaper cache for %s", sourcePath.c_str());
  } else {
    LOG_ERR(kModule, "Sleep wallpaper rendered but cache update failed for %s", sourcePath.c_str());
  }

  return true;
}
}  // namespace

namespace SleepWallpaperCache {

bool warmCache(GfxRenderer& renderer, const std::string& sourcePath) {
  SourceIdentity identity;
  if (!computeSourceIdentity(sourcePath, identity)) {
    return false;
  }

  CacheHeader header;
  if (readCacheHeader(header) && isCacheHeaderValid(header, identity, renderer) && hasUsableCacheFiles(header)) {
    return true;
  }

  return renderBitmapToCacheFiles(renderer, sourcePath, identity, false, HalDisplay::FAST_REFRESH);
}

bool renderFromCache(GfxRenderer& renderer, const std::string& sourcePath, const HalDisplay::RefreshMode refreshMode) {
  CacheHeader header;
  SourceIdentity identity;
  if (!loadValidCacheHeader(renderer, sourcePath, header, identity)) {
    return false;
  }

  uint8_t* frameBuffer = renderer.getFrameBuffer();
  renderer.setRenderMode(GfxRenderer::BW);

  if (!readBufferFromFile(kBwPath, frameBuffer, header.bufferSize)) {
    return false;
  }
  renderer.displayBuffer(refreshMode);

  if (header.hasGreyscale) {
    if (!readBufferFromFile(kGrayLsbPath, frameBuffer, header.bufferSize)) {
      return false;
    }
    renderer.copyGrayscaleLsbBuffers();

    if (!readBufferFromFile(kGrayMsbPath, frameBuffer, header.bufferSize)) {
      return false;
    }
    renderer.copyGrayscaleMsbBuffers();
    renderer.displayGrayBuffer();
  }

  renderer.setRenderMode(GfxRenderer::BW);
  LOG_DBG(kModule, "Rendered sleep wallpaper from cache: %s", sourcePath.c_str());
  return true;
}

bool renderAndCacheFromSource(GfxRenderer& renderer, const std::string& sourcePath,
                              const HalDisplay::RefreshMode refreshMode) {
  SourceIdentity identity;
  if (!computeSourceIdentity(sourcePath, identity)) {
    return false;
  }

  return renderBitmapToCacheFiles(renderer, sourcePath, identity, true, refreshMode);
}

}  // namespace SleepWallpaperCache
