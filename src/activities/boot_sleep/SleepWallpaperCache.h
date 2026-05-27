#pragma once

#include <HalDisplay.h>

#include <string>

class GfxRenderer;

namespace SleepWallpaperCache {

bool warmCache(GfxRenderer& renderer, const std::string& sourcePath);
bool renderFromCache(GfxRenderer& renderer, const std::string& sourcePath, HalDisplay::RefreshMode refreshMode);
bool renderAndCacheFromSource(GfxRenderer& renderer, const std::string& sourcePath,
                              HalDisplay::RefreshMode refreshMode);

}  // namespace SleepWallpaperCache
