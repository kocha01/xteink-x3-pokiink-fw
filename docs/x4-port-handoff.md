# X4 Port Handoff — Recent Reading-Experience Work

**Audience**: an agent/engineer porting recent PokiInk reading-experience and
font-management work from the X3 port branch (this repo) into an X4-targeting
codebase (e.g. `crosspoint-reader-halo-2-ui-x3-port` parent, an X4 fork, or
upstream `crosspoint-reader/crosspoint-reader`).

**Date**: 2026-05-12 (this doc), tracking work landed up to firmware
`firmware.bin` sha `78ee915147b9209a37286d913ab0d0f1c6bdb9d9f9ed5e06dade0ffda51e9478`
plus the `tools/web-font-builder/` standalone converter.

**TL;DR**: SD-card custom fonts, italic body text, line-spacing rework,
Home-perf cover cache, AA re-enable, page-turn idempotent-prewarm fix, WiFi
font upload, and a browser-based TTF→.cpfont builder.  Most of it ports
1:1 to X4; a few things are X3-specific by design (display waveform, board
defines, UI font enum).

If you have time for only one source-of-truth artefact, read
[`AI_COORDINATION.md`](../AI_COORDINATION.md) bottom-up — every change has a
"Files touched / Result / Risks" entry there.  This doc is the tour guide
on top of that log.

---

## 0. Repo orientation

| Path | What lives there |
| --- | --- |
| [`platformio.ini`](../platformio.ini) | Build environments.  X3 uses `[env:x3_preview]`; X4 should mirror with its own board flag (`CROSSPOINT_BOARD_X4` per upstream). |
| [`partitions.csv`](../partitions.csv) | OTA dual-slot layout — 6.4 MB each app slot, 3.4 MB spiffs.  Same shape on X4. |
| [`AI_COORDINATION.md`](../AI_COORDINATION.md) | Append-only change log — most recent feature work at the bottom.  **Read this**. |
| `lib/EpdFont/` | Font format, decompressor, SD font runtime, converter scripts. |
| `lib/GfxRenderer/` | Renderer, font cache manager, Bitmap class. |
| `lib/Epub/` | EPUB parser + ParsedText layout. |
| `src/activities/` | All activity classes — Home, Reader, Settings pickers. |
| `src/components/themes/modern/ModernTheme.cpp` | The default theme on X3.  X4 uses Modern too but with different metrics. |
| `src/network/CrossPointWebServer.{h,cpp}` | HTTP server with all the upload/edit endpoints. |
| `src/network/html/` | HTML pages bundled into flash (gzipped by `scripts/build_html.py`). |
| `tools/web-font-builder/` | Standalone browser tool for TTF→.cpfont conversion (no firmware involvement). |

---

## 1. Feature inventory — what's new since X4 baseline

Each row points at the canonical entry in `AI_COORDINATION.md` for full context.

| Feature | Files | Status on X3 | Portable to X4? |
| --- | --- | --- | --- |
| Cover-on-Sleep + random folder screensaver | `src/activities/boot_sleep/SleepActivity.{cpp,h}`, `src/CrossPointSettings.{cpp,h}` | ✅ shipped | ✅ portable (depends only on existing cover BMP pipeline) |
| EPUB italic for Bookerly body sizes (14/16/18) | `lib/EpdFont/builtinFonts/all.h`, `src/main.cpp` font registrations, `lib/EpdFont/EpdFontFamily.h` graceful fallback | ✅ shipped | ✅ trivially portable; just bring the italic glyph headers |
| Bai Jamjuree tightened `advanceY` to match CloudLoop leading | `lib/EpdFont/builtinFonts/baijamjuree_*_{regular,bold}.h`, `lib/EpdFont/scripts/fontconvert.py` `--advance-y`, `lib/EpdFont/scripts/convert-builtin-fonts.sh` | ✅ shipped | ⚠️ X4 doesn't ship BJ as a built-in family by default — feature is X3-specific.  Underlying `--advance-y` flag and `BAIJAMJUREE_ADVANCE_Y` table are portable. |
| Flash UX fix — single-flash uploads | `platformio.ini` (removed `board_upload.offset_address = 0x10000`) | ✅ shipped | ✅ same fix applies; just don't set `offset_address` so the default arduino-esp32 upload sequence writes `boot_app0.bin` to `0xe000` |
| AA re-enable on X3 | `src/activities/reader/ReaderUtils.h` (removed hard-coded `return false`) | ✅ shipped pending hardware verify | ⚠️ Was an X3-specific workaround.  On X4 the grayscale waveform is the original X4 LUT — AA was always on.  No action needed on X4. |
| **SD custom font `.cpfont` full stack** | `lib/EpdFont/SdCardFont*`, `src/SdCardFontSystem.{cpp,h}`, `src/SdCardFontGlobals.h`, picker activity, settings persistence, dynamic font-size cycling | ✅ shipped | ✅ **fully portable** — see §3 for porting notes |
| Page-turn idempotent prewarm (10s→1-2s) | `lib/EpdFont/SdCardFont.{h,cpp}` (`lastPrewarm*` fields), `lib/GfxRenderer/FontCacheManager.cpp` (skip SD in `clearCache`) | ✅ shipped, hardware-confirmed by user | ✅ portable |
| Home navigation perf — BMP byte cache + Bitmap memory ctor | `lib/GfxRenderer/Bitmap.{h,cpp}` (new memory-source ctor), `src/components/themes/modern/ModernTheme.cpp` (96 KB LRU cache) | ✅ shipped pending hardware verify | ✅ portable — adjust cache budget if X4 has different RAM headroom |
| Quick Settings rule: POWER no-op on SD font | `src/activities/reader/EpubReaderMenuActivity.cpp` | ✅ shipped | ✅ portable |
| WiFi font upload API + browser drop-zone | `src/network/CrossPointWebServer.{h,cpp}` (`POST /api/fonts/upload`, `GET /api/fonts/list`, `POST /api/fonts/delete`, `GET /fonts`), `src/network/html/FontsPage.html` | ✅ shipped | ✅ portable |
| Standalone browser TTF→.cpfont converter | `tools/web-font-builder/index.html` (no firmware involvement) | ✅ shipped | ✅ portable as-is — copy the file into X4's `tools/` |

---

## 2. Hardware differences X3 ↔ X4 — what to verify when porting

X3 panel: **528 × 792** @ 2-bit grayscale.  X4 panel: **480 × 800** @ 2-bit
grayscale.  Most code is dimension-agnostic via `renderer.getScreenWidth/Height`
and the `UITheme` metrics struct.  Watch these spots:

| Concern | X3 (this repo) | X4 (verify in target repo) |
| --- | --- | --- |
| **Board build flag** | `CROSSPOINT_BOARD_X3` (set in `[env:x3_preview]` of `platformio.ini`) | X4 likely uses no `CROSSPOINT_BOARD_X3` (default path) or `CROSSPOINT_BOARD_X4`. |
| **Display LUT / refresh waveforms** | X3-specific reverse-engineered grayscale LUT in `open-x4-sdk/libs/display/EInkDisplay/src/EInkDisplay.cpp` (gated by `_x3Mode`) | X4 has its own LUT — do **not** copy the `_x3Mode` branches. |
| **AA `shouldUseTextAntiAliasing()`** | We removed the X3 hard-coded false in `src/activities/reader/ReaderUtils.h:88` | X4 was always on; verify it stays that way. |
| **Cover thumbnail sizes** | `centerCoverSourceH=260`, `sideCoverSourceH=200`, `farCoverSourceH=150` in `ModernTheme.cpp:45-47` | X4's bigger viewport may want larger thumbs; verify against the upstream Modern theme constants. |
| **Carousel layout** | `centerCoverW=180`, `sideCoverW=140`, `farCoverW=100`, `carouselH=340` | X4 horizontal centering math is the same (uses `rect.x + rect.width/2`); just confirm the constants fit X4's reservation for the menu strip below. |
| **SD card pins** | `CROSSPOINT_SD_CS=12`, `CROSSPOINT_SD_SPI_FQ=40MHz`, `CROSSPOINT_SD_HAS_POWER_SWITCH=1`, `CROSSPOINT_SD_POWER_CONTROL_PIN=13` | X4 wiring is different — see upstream `platformio.ini` for the X4 env's flags. Don't copy these. |
| **Reader font enum** | `BAIJAMJUREE / CLOUDLOOP / BOOKERLY` (Thai-leaning fork) | X4 upstream uses `NOTOSERIF / NOTOSANS / OPENDYSLEXIC`. Settings persistence assumes numeric enum; the `getReaderFontId()` switch is the only place that names them. |
| **UI font IDs at boot** | On X3, `UI_10_FONT_ID = baijamjuree_12`, `UI_12_FONT_ID = baijamjuree_14`, `SMALL_FONT_ID = baijamjuree_10` (small panel needs bigger UI fonts).  See `src/main.cpp:345-360` | X4 uses `ui10/ui12/smallFontFamily` directly; verify nothing in the SD-font path depends on the X3-only mapping. |
| **Orientations** | Same four-state enum (PORTRAIT / LANDSCAPE_CW / INVERTED / LANDSCAPE_CCW) | Same on X4. |
| **Flash partition** | 6.4 MB app slot, current usage 94.5% | X4 may have different partition — check upstream `partitions.csv`. |

---

## 3. SD custom font pipeline — the biggest portable piece

This is the most valuable new feature: read `.cpfont` v4 files from
`/fonts/<Family>/<Family>_<size>.cpfont` on the SD card, dynamic size
selection in settings, web-based upload + delete.  Designed to be portable
across boards — the only board-specific thing is the SD I/O via `HalStorage`.

### 3.1 Architecture

```
        ┌──────────────────────────────────┐
        │   SD card: /fonts/Lexend/        │
        │     Lexend_14.cpfont  (~17 KB)   │
        │     Lexend_16.cpfont             │
        │     Lexend_18.cpfont             │
        │     Lexend_20.cpfont             │
        │     Lexend_22.cpfont             │
        └─────────────┬────────────────────┘
                      │
        ┌─────────────▼──────────────────────┐
        │  SdCardFontRegistry::discover()    │
        │  ─ scans /fonts/ AND               │
        │     /.crosspoint/fonts/ (legacy)   │
        │  ─ parses <Name>_<size>.cpfont     │
        │  ─ alphabetical, max 128 families  │
        └─────────────┬──────────────────────┘
                      │
        ┌─────────────▼──────────────────────┐
        │  SdCardFontSystem (facade)         │
        │  ─ begin(renderer)                 │
        │  ─ ensureLoaded(renderer)          │
        │  ─ rediscover()  ← wifi upload     │
        │  ─ installs SETTINGS.sdFontId-     │
        │    Resolver trampoline             │
        └─────────────┬──────────────────────┘
                      │
        ┌─────────────▼──────────────────────┐
        │  SdCardFontManager                 │
        │  ─ loadFamily(family, size)        │
        │  ─ unloadAll()                     │
        │  ─ owns ONE SdCardFont at a time   │
        │  ─ picks closest-size .cpfont file │
        └─────────────┬──────────────────────┘
                      │  EpdFont (per style) registered with GfxRenderer
                      │
        ┌─────────────▼──────────────────────┐
        │  SdCardFont (per-font instance)    │
        │  ─ load(path) — parses header +    │
        │    intervals only into RAM         │
        │  ─ prewarm(text) — reads JUST the  │
        │    glyphs needed for current page  │
        │  ─ overflow ring (8 entries) for   │
        │    glyphs the prewarm missed       │
        │  ─ lastPrewarmHash idempotency     │
        │    (skips redundant SD reads when  │
        │    the same text is asked again)   │
        └────────────────────────────────────┘
```

### 3.2 Files to copy

New files (drop in as-is):

```
lib/EpdFont/SdCardFont.{h,cpp}              # ~1,200 LOC — core
lib/EpdFont/SdCardFontManager.{h,cpp}       # ~150 LOC
lib/EpdFont/SdCardFontRegistry.{h,cpp}      # ~210 LOC
src/SdCardFontSystem.{h,cpp}                # ~140 LOC
src/SdCardFontGlobals.h                     # 12 LOC, externs
lib/EpdFont/scripts/fontconvert_sdcard.py   # 882 LOC, Python TTF→.cpfont
lib/EpdFont/scripts/generate-sd-fonts.sh    # batch driver
```

Modified files (review and merge carefully):

- `lib/EpdFont/EpdFontData.h` — added `glyphMissHandler` callback + `glyphMissCtx` to `EpdFontData` struct.  Existing C-style initializers in builtin font headers zero-init the new fields automatically; safe.
- `lib/EpdFont/EpdFont.cpp` — `getGlyph()` calls `glyphMissHandler` before falling back to `REPLACEMENT_GLYPH`.  Conflicts to expect: if X4 has Thai combining-mark work that diverged, the surrounding code may need careful merging.  The miss-handler addition itself is a clean ~8-line patch.
- `lib/GfxRenderer/GfxRenderer.{h,cpp}` — `class SdCardFont;` fwd decl, `mutable std::map<int, SdCardFont*> sdCardFonts_`, `register/unregister/clear/isSdCardFont/ensureSdCardFontReady` methods, `getGlyphBitmap` routes through `SdCardFont::isOverflowGlyph + getOverflowBitmap` for on-demand glyphs.
- `lib/GfxRenderer/FontCacheManager.{h,cpp}` — constructor takes `sdCardFonts_` reference, `prewarmCache` branches for SD fonts, `clearCache` deliberately **does not** clear SD font caches (perf-critical — see §5).
- `lib/Epub/Epub/ParsedText.cpp` — `layoutAndExtractLines()` joins page text + calls `renderer.ensureSdCardFontReady(fontId, text)` when the active font is SD-resident.  ~15 lines.
- `src/CrossPointSettings.{h,cpp}` — added `char sdFontFamilyName[32]`, `SdFontIdResolver` typedef + two trampoline fields, `getReaderFontId()` checks SD first.
- `src/main.cpp` — included `<SdCardFontSystem.h>`, defined global `sdFontSystem` and `ensureSdFontLoaded()` shim, `sdFontSystem.begin(renderer)` after `SETTINGS.loadFromFile()`.
- `src/SettingsList.h` — `SettingInfo::String` for `sdFontFamilyName` (for JsonSettingsIO persistence + web settings UI).
- `src/activities/settings/SettingsActivity.{cpp,h}` — `SettingAction::SelectSdFont`, picker dispatch, dynamic font-size cycling.
- `src/activities/settings/SdCardFontPickerActivity.{cpp,h}` — list-style picker activity, same pattern as `SleepWallpaperPickerActivity`.

### 3.3 The `.cpfont` v4 format — single source of truth

Definitive spec lives in two places:

1. **Reader**: `lib/EpdFont/SdCardFont.cpp` — `static_assert(sizeof(EpdGlyph) == 16, "...")` block and the `load()` function.
2. **Writer**: `lib/EpdFont/scripts/fontconvert_sdcard.py` — the `generate_cpfont_multistyle` function around line 660.  The format strings `<8sHHB19s` (header) and `<B3xIIBhhHHBBBI4x` (style TOC entry) ARE the format.

Quick spec recap for reference (little-endian):

```
[0..31]   Header:
            magic "CPFONT\0\0"     8 bytes
            version u16            2 bytes = 4
            flags u16              2 bytes = 1 (always 2-bit greyscale)
            styleCount u8          1 byte
            reserved               19 bytes
[32..]    Style TOC entries (one per style, 32 bytes each):
            styleId u8 + 3 pad
            intervalCount u32
            glyphCount u32
            advanceY u8
            ascender i16
            descender i16
            kernLeftEntryCount u16
            kernRightEntryCount u16
            kernLeftClassCount u8
            kernRightClassCount u8
            ligaturePairCount u8
            dataOffset u32         (absolute file offset to this style's data)
            reserved 4 bytes
[data]    Per style, in order:
            intervals[]            12 bytes each (first u32, last u32, glyphIdxOffset u32)
            glyphs[]               16 bytes each — matches EpdGlyph struct
            kernLeftClasses[]      3 bytes packed
            kernRightClasses[]     3 bytes packed
            kernMatrix[]           int8_t per (L × R) cell
            ligaturePairs[]        8 bytes packed
            bitmap[]               raw 2bpp packed glyph data
```

When porting, the **`static_assert` block in `SdCardFont.cpp` is the canary**:
if `sizeof(EpdGlyph) != 16` on X4's compiler/ABI, the format will silently
mis-decode.  The struct fields are designed for natural ESP32 alignment;
any unrelated layout change must keep this property.

### 3.4 Things X4 may need to tune

- `MAX_SD_FAMILIES` is 128 in `SdCardFontRegistry.h`.  Plenty headroom.
- `OVERFLOW_CAPACITY` is 8 in `SdCardFont.h` — glyphs missed by prewarm.  Larger panel → potentially more glyphs per page → consider 12 or 16 on X4 if you see overflow-eviction churn in `SDCF` logs.
- The 96 KB cover BMP cache in `ModernTheme.cpp` was tuned for X3's heap budget.  X4 has the same chip family (ESP32-C3) and same RAM size, so 96 KB is likely correct; verify by running `Sd card → Home navigation` and watching free heap.

---

## 4. Home navigation perf — Bitmap memory-source ctor

Independent of SD fonts.  X4 likely benefits identically.

**Problem**: ModernTheme rendered 5 cover BMPs per Home redraw (center + 2
side + 2 far), each one reopening the file and reading row-by-row from SD.
On book navigation that's ~5 × 100-150 ms = ~600-750 ms per nav.

**Fix**: two parts.

1. **`lib/GfxRenderer/Bitmap.{h,cpp}`** — added a memory-source ctor:
   ```cpp
   Bitmap(const uint8_t* data, size_t size, bool dithering = false);
   ```
   Internally branches on `file_ != nullptr` via `srcReadByte/srcRead/srcSeekSet/srcSeekCur/srcReadLE16/srcReadLE32` helpers; existing FsFile-backed call sites are unchanged.

2. **`src/components/themes/modern/ModernTheme.cpp`** — anonymous-namespace LRU cache keyed by full SD path, 96 KB budget.  `loadOrCacheBmp(path)` returns a `const std::vector<uint8_t>*` (cache pointer) or nullptr.  `drawCoverAt` lambda and the center cover render path both call this then construct `Bitmap(bytes.data(), bytes.size())`.

After-effect: first carousel cycle warms all 5 covers, subsequent navs go from ~1000 ms → ~450 ms (e-ink-bound).

**X4 porting**: copy as-is.  No board-specific assumptions.

---

## 5. Page-turn perf — idempotent prewarm

This was the user-reported `~10 s page turns` bug.  Three things conspired:

1. Layout-time `ensureSdCardFontReady` (metadata-only prewarm) — runs inside `ParsedText::layoutAndExtractLines`.
2. Render-scan-pass full prewarm via `FontCacheManager::PrewarmScope` — clears cache then re-prewarms.
3. Real render also touches the cache.

Each call was rebuilding the mini cache from scratch → 3× SD I/O per page.

**Fix**:

- **`SdCardFont.{h,cpp}`** — added `lastPrewarmHash_`, `lastPrewarmStyleMask_`, `lastPrewarmHadBitmaps_`.  `prewarm()` computes FNV-1a over the input UTF-8 text on entry; if hash + mode + style mask match the previous call, return 0 ms (no SD touched).  `clearCache()` invalidates.
- **`FontCacheManager::clearCache()`** — deliberately **does not** clear SD font caches anymore.  Mini-cache survives `PrewarmScope::ctor()` so cross-page text overlap (typical 50%+ in EPUB) preserves cache hits.  Built-in compressed-font cache wiping unchanged.

**X4 porting**: copy verbatim.  Same ESP32 family, same SPI SD, same I/O cost.

---

## 6. WiFi font upload — HTTP API

Endpoints added to `CrossPointWebServer`:

| Method | Path | Purpose |
| --- | --- | --- |
| `GET` | `/fonts` | Serves `FontsPageHtml.generated.h` — drop-zone web UI |
| `POST` | `/api/fonts/upload` | Multipart `.cpfont` upload; auto-creates `/fonts/<Family>/` from parsed filename; calls `sdFontSystem.rediscover()` + `ensureSdFontLoaded()` on success |
| `GET` | `/api/fonts/list` | JSON of `{ families: [{name, sizes: []}], selected }` |
| `POST` | `/api/fonts/delete?family=<name>` | Recursive `Storage.removeDir` of both visible + legacy paths; clears `sdFontFamilyName` if it was the active selection |

Implementation lives at:

- `src/network/CrossPointWebServer.cpp` — handlers `handleFontUpload`, `handleFontUploadPost`, `handleFontList`, `handleFontDelete`.
- `src/network/CrossPointWebServer.h` — method declarations + `UploadState fontUpload` instance.
- `src/network/html/FontsPage.html` — drop-zone UI with delete buttons.
- `src/SdCardFontSystem.h` — exposes `void rediscover()` so non-const callers (the upload handler) can trigger a registry rescan.

**Filename convention**: `<Family>_<size>.cpfont`.  Family directory in
`/fonts/<Family>/` is auto-created.

**X4 porting**: copy verbatim.  All filesystem ops go through `HalStorage`
which abstracts the SD layer.  Only adjustment: if X4's existing WebServer
routing macros are upstream-style (and X3's are too — same arduino-esp32
WebServer), no change needed.

---

## 7. Standalone browser TTF → .cpfont builder

[`tools/web-font-builder/index.html`](../tools/web-font-builder/index.html) —
single self-contained HTML file.  No firmware involvement.

- **Engine**: [opentype.js](https://github.com/opentypejs/opentype.js) v1.3.4 from jsDelivr CDN (cached after first load) + Canvas2D for rasterization (4× supersample → downsample → 2-bit quantize) + custom DataView packing for `.cpfont` v4 binary.
- **UX**: drop TTF/OTF, **5 fixed size slots** (structurally capped — no comma input that users can stuff too many values into), Unicode interval presets (Latin / Latin-1 / Latin-Ext / Thai / Punctuation) + custom hex ranges, build → auto-download per size, optional "Upload to device…" button that POSTs to `/api/fonts/upload` on a user-supplied IP.
- **Privacy**: 100% local — TTF never leaves the browser.
- **Browser support**: anything since ~2017 (Chrome 60+, Firefox 55+, Safari 11+).
- **MVP limitations** (documented at top of the file):
  - Single style: Regular only.  Bold/Italic etc require running the Python script.
  - Pair kerning from the font's `kern` / GPOS-1 table; no kerning-class extraction.
  - No ligature pairs.
  - Glyph quality close to FreeType for body sizes (14pt+); slightly softer at <12pt because opentype.js + Canvas2D doesn't do TrueType bytecode hinting.

**X4 porting**: copy the folder.  Already board-agnostic — runs in the user's
browser.

---

## 8. Settings UI patterns to mirror

The new SD-font UX adds three things that other ports may want to replicate:

1. **Action row that opens a list picker** — `SettingAction::SelectSdFont` + `SdCardFontPickerActivity`.  See `src/activities/settings/SettingsActivity.cpp` for how the action is wired (display value + dispatch).  Same pattern as `SleepWallpaperPickerActivity`.

2. **Inline Value cycling that adapts to data** — `Settings → Reader → Font Size` row uses `SettingType::VALUE` with the normal valueRange (built-in 14..22), but `toggleCurrentSetting()` has a special branch for `STR_FONT_SIZE`: when `sdFontFamilyName` is non-empty, it cycles through the SD font's actual `.cpfont` sizes (e.g. 18/20/22/24/26) instead of the built-in step list.  See `SettingsActivity.cpp:toggleCurrentSetting()` — the `bool handled` lambda pattern.

3. **Quick Settings (in-reader) POWER no-op rule** — when an SD font is active, POWER stops cycling family.  `EpubReaderMenuActivity.cpp` — the POWER handler returns early if `pendingSdFontFamilyName.empty()` is false.  Rationale: a user reading with their chosen custom font shouldn't accidentally swap fonts mid-page.

The size up/down cycle in Quick Settings already uses a shared `availableReaderFontSizes()` helper that walks the SD font's actual sizes — copy the function too.

---

## 9. Build / flash workflow

X3 uses PlatformIO with the `x3_preview` env.  X4 should have its own env.

Important note in `platformio.ini`: we **deliberately do not** set
`board_upload.offset_address`.  Without that override, `pio run -t upload`
follows arduino-esp32's default sequence and writes:

```
0x0000   bootloader.bin
0x8000   partitions.bin
0xe000   boot_app0.bin   ← otadata marker — without this, app0 changes
                            don't take effect when otadata points at app1
0x10000  firmware.bin
```

Setting `offset_address = 0x10000` (which the upstream pre-fork did) causes
single-flash uploads to write only firmware to app0 while leaving otadata
unchanged — if otadata pointed at app1 from a prior in-app OTA, the new
firmware just sits there dormant.  Two-flash UX = bad.

**X4 should keep the same posture: do NOT set `board_upload.offset_address`.**

If X4 has more partition flexibility (factory app + OTA slots), the default
arduino-esp32 sequence is still correct.

---

## 10. Build artifact reference (current X3)

| Property | Value |
| --- | --- |
| Branch | `pokiink-backup-fw-port/main` (this repo) |
| Latest firmware.bin | `78ee915147b9209a37286d913ab0d0f1c6bdb9d9f9ed5e06dade0ffda51e9478` (or later — check git log) |
| Flash usage | 94.5% (6,194,463 / 6,553,600 bytes), ~360 KB free |
| RAM (static) | 31.1% (102 KB / 320 KB) |
| RAM (peak heap on Home) | +96 KB cover cache |
| Env | `pio run -e x3_preview` |

---

## 11. Per-feature porting checklist

For an X4 agent picking these up, suggested order (least-risk first):

- [ ] **A. Standalone web font builder** — copy `tools/web-font-builder/` verbatim.  Zero firmware touch.  Validates the format spec end-to-end.
- [ ] **B. `.cpfont` format support (read-only)** — drop in `SdCardFont*` + `SdCardFontSystem*` + `SdCardFontGlobals.h`.  Add the `glyphMissHandler` + `glyphMissCtx` hooks to `EpdFontData`.  Patch `EpdFont::getGlyph()`.  Add the registry/manager to `GfxRenderer`.  Add `ensureSdCardFontReady`.  At this point you can `Storage`-place a `.cpfont` and hand-write a test that loads it.
- [ ] **C. Settings UI** — `SETTINGS.sdFontFamilyName` + resolver typedef + `getReaderFontId()` SD-first check + `SettingInfo::String` registration in `SettingsList.h` + persistence via JsonSettingsIO + `SdCardFontPickerActivity` + `SelectSdFont` action.  Now you can pick a family on the device.
- [ ] **D. Dynamic font-size cycling** — patch `SettingsActivity::toggleCurrentSetting()` and `EpubReaderMenuActivity` (the size cycle + POWER no-op rule).
- [ ] **E. Perf: idempotent prewarm + skip-clear in FontCacheManager** — already in `SdCardFont.{h,cpp}` if you copy verbatim.  Also change `FontCacheManager::clearCache()` to skip SD fonts.
- [ ] **F. ParsedText prewarm hook** — add the `renderer.isSdCardFont(fontId)` block in `layoutAndExtractLines`.
- [ ] **G. WiFi upload API** — copy the four endpoints into X4's web server.  Add `rediscover()` to `SdCardFontSystem`.
- [ ] **H. `/fonts` HTML page** — copy `FontsPage.html` and the route registration.  Add nav links in the other HTML pages.
- [ ] **I. Home perf** — `Bitmap` memory-source ctor + `ModernTheme` LRU cover cache.  Independent of SD fonts; can ship separately.
- [ ] **J. Flash UX** — remove `board_upload.offset_address` from `platformio.ini`.  Verify with a deliberately-stale otadata.

---

## 12. Known risks / open items (handed off as-is)

These are noted in `AI_COORDINATION.md` per-entry "Risks / blockers" sections.
Summary for X4 awareness:

1. **AA fade regression on hardware** — the X3 plane-order fix in `EInkDisplay.cpp` was Codex's work and was never empirically confirmed because the short-circuit was never removed.  On X4 the grayscale waveform is the original X4 LUT; AA was always on; this concern does not apply.
2. **Thai mark clipping at BJ tightened spacing** — only applies to X3 (BJ isn't built-in on X4).
3. **POWER no-op rule is silent UI** — a user on SD font may press POWER and see nothing happen.  Mitigation: the font row still shows the SD family name so they know it's custom.
4. **Path-traversal in font delete endpoint** — guarded by rejecting `/`, `\`, leading `.` in family names.  Audit any new endpoints that accept user input similarly.
5. **opentype.js vs FreeType quality** — the web builder doesn't do TrueType hinting; for small sizes the Python script's output is sharper.  Web builder is best for body sizes (14pt+).

---

## 13. How to verify ports without hardware

1. Build clean: `pio run -e <x4-env>` should pass with the new sources.
2. Static checks:
   - `static_assert(sizeof(EpdGlyph) == 16)` in `SdCardFont.cpp` must hold.
   - `static_assert(sizeof(EpdUnicodeInterval) == 12)`, `EpdKernClassEntry == 3`, `EpdLigaturePair == 8`.
3. Web tool round-trip: open `tools/web-font-builder/index.html`, build a small font (Latin only, size 16), download .cpfont, run `lib/EpdFont/scripts/verify_compression.py` (if applicable) or just inspect with `xxd` — first 8 bytes should be `43 50 46 4F 4E 54 00 00`, bytes 8-9 should be `04 00`.
4. Curl test against device: `curl -F "file=@Test_16.cpfont" http://<ip>/api/fonts/upload`.  Response should be JSON with `success: true`.
5. Read serial logs at boot for `[D][SDFS] SD font system ready (N families discovered)`.

---

## 14. Pointers

- **Append-only change log**: [`AI_COORDINATION.md`](../AI_COORDINATION.md) — every commit's intent, files, build artifact, risks.  Read bottom-up for chronological order.
- **PR #1327 upstream reference**: https://github.com/crosspoint-reader/crosspoint-reader/pull/1327 — original SD card fonts back-end PR by @adriancaruana.  Our work follows its `.cpfont` v4 format and Python converter spec; UI and several perf fixes are added on top.
- **Converter source**: `lib/EpdFont/scripts/fontconvert_sdcard.py` is the canonical TTF→.cpfont implementation.  The web builder in `tools/web-font-builder/index.html` mirrors its byte layout exactly.

Good luck.  When you land features on X4, please mirror the `AI_COORDINATION.md` log format so future ports between branches stay tractable.
