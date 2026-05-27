#pragma once

#include <SdCardFontManager.h>
#include <SdCardFontRegistry.h>

class GfxRenderer;

/// Facade that owns the SD card font registry, manager, and resolver logic.
/// Hides implementation details behind a single begin() + ensureLoaded() API.
class SdCardFontSystem {
 public:
  SdCardFontSystem() = default;
  SdCardFontSystem(const SdCardFontSystem&) = delete;
  SdCardFontSystem& operator=(const SdCardFontSystem&) = delete;
  /// Discover SD card fonts and load user's saved selection. Call once during setup.
  void begin(GfxRenderer& renderer);

  /// Ensure the correct SD font family is loaded for the current settings.
  /// Call before entering the reader or after settings change.
  void ensureLoaded(GfxRenderer& renderer);

  /// Resolve an SD card font ID from family name + fontSize enum.
  /// Returns 0 if not found. Used by CrossPointSettings::getReaderFontId().
  int resolveFontId(const char* familyName, uint8_t fontSizeEnum) const;

  /// Access the registry (e.g. for settings UI to enumerate available fonts).
  const SdCardFontRegistry& registry() const { return registry_; }

  /// Re-scan the SD card for font families.  Used after WiFi uploads of new
  /// `.cpfont` files so Settings → Custom Font (SD) reflects the change
  /// without a reboot.  Cheap — just walks the /fonts and /.crosspoint/fonts
  /// directories, no font-data I/O.
  void rediscover() { registry_.discover(); }

  /// Free per-page glyph caches on every loaded SD font without unloading the
  /// fonts themselves.  Call before transitioning out of a reader activity
  /// so the ~30 KB mini-cache stops competing for heap with Home's 96 KB
  /// cover BMP cache.  The mini-cache rebuilds transparently when the user
  /// re-enters a reader.  (See docs/x3-backport-handoff.md Bug 8.)
  void releaseReaderHeap() { manager_.clearAllMiniCaches(); }

 private:
  SdCardFontRegistry registry_;
  SdCardFontManager manager_;
};
