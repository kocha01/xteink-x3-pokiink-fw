#include "HomeActivity.h"

#include <Arduino.h>  // millis()
#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalMotionSensor.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Utf8.h>
#include <Xtc.h>

#include <array>
#include <cstring>
#include <vector>

#include <NtpClock.h>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

int HomeActivity::getMenuItemCount() const {
  int count = 4;  // File Browser, Recents, File transfer, Settings
  if (!recentBooks.empty()) {
    count += recentBooks.size();
  }
  if (hasOpdsUrl) {
    count++;
  }
  return count;
}

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(std::min(static_cast<int>(books.size()), maxBooks));

  for (const RecentBook& book : books) {
    // Limit to maximum number of recent books
    if (recentBooks.size() >= maxBooks) {
      break;
    }
    // onEnter() already prunes missing files from the backing store, so this copy path can stay I/O-free.
    recentBooks.push_back(book);
  }
}

void HomeActivity::loadRecentCovers(int coverHeight) {
  recentsLoading = true;
  bool showingLoading = false;
  Rect popupRect;

  const auto& metrics = UITheme::getInstance().getMetrics();
  // Collect all thumbnail heights needed (center + side + far).
  // Modern home now renders covers at their natural aspect ratio, so exact display-height thumbs
  // look sharper than oversized thumbs that get scaled down again at draw time.
  int thumbHeights[4] = {coverHeight, 0, 0, 0};
  int thumbCount = 1;
  if (metrics.homeSideCoverHeight > 0 && metrics.homeSideCoverHeight != coverHeight) {
    thumbHeights[thumbCount++] = metrics.homeSideCoverHeight;
  }
  if (metrics.homeFarCoverHeight > 0 && metrics.homeFarCoverHeight != coverHeight) {
    thumbHeights[thumbCount++] = metrics.homeFarCoverHeight;
  }

  int progress = 0;
  for (RecentBook& book : recentBooks) {
    if (!book.coverBmpPath.empty()) {
      // Check if any needed thumbnail size is missing
      bool needsGeneration = false;
      for (int t = 0; t < thumbCount; t++) {
        std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, thumbHeights[t]);
        if (!Storage.exists(coverPath.c_str())) {
          needsGeneration = true;
          break;
        }
      }

      if (needsGeneration) {
        // If epub, try to load the metadata for title/author and cover
        if (FsHelpers::hasEpubExtension(book.path)) {
          Epub epub(book.path, "/.crosspoint");
          // Skip loading css since we only need metadata here
          epub.load(false, true);

          // Try to generate thumbnail images at all needed sizes
          if (!showingLoading) {
            showingLoading = true;
            popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
          }
          GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
          bool anySuccess = false;
          for (int t = 0; t < thumbCount; t++) {
            if (epub.generateThumbBmp(thumbHeights[t])) {
              anySuccess = true;
            }
          }
          if (!anySuccess) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
            book.coverBmpPath = "";
          }
        } else if (FsHelpers::hasXtcExtension(book.path)) {
          // Handle XTC file
          Xtc xtc(book.path, "/.crosspoint");
          if (xtc.load()) {
            if (!showingLoading) {
              showingLoading = true;
              popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
            }
            GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
            bool anySuccess = false;
            for (int t = 0; t < thumbCount; t++) {
              if (xtc.generateThumbBmp(thumbHeights[t])) {
                anySuccess = true;
              }
            }
            if (!anySuccess) {
              RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
              book.coverBmpPath = "";
            }
          }
        }
      }
    }
    progress++;
  }

  recentsLoaded = true;
  recentsLoading = false;
}

void HomeActivity::onEnter() {
  Activity::onEnter();
  motionSensor.resetGestureState();

  // Check if OPDS browser URL is configured
  hasOpdsUrl = strlen(SETTINGS.opdsServerUrl) > 0;

  // If books were deleted from SD while the device stayed on, keep Home in sync.
  RECENT_BOOKS.pruneMissingBooks();

  selectorIndex = 0;

  const auto& metrics = UITheme::getInstance().getMetrics();
  loadRecentBooks(metrics.homeRecentBooksCount);

  // Generate any missing cover thumbnails BEFORE the first render
  loadRecentCovers(metrics.homeCoverHeight);

  // Trigger single update — render() uses fast refresh for minimal flicker
  requestUpdate();
}

void HomeActivity::onExit() {
  Activity::onExit();
  motionSensor.resetGestureState();

  // Free the stored cover buffer if any
  freeCoverBuffer();
}

bool HomeActivity::storeCoverBuffer() {
  uint8_t* frameBuffer = renderer.getFrameBuffer();
  if (!frameBuffer) {
    return false;
  }

  const size_t bufferSize = GfxRenderer::getBufferSize();
  if (!coverBuffer) {
    coverBuffer = static_cast<uint8_t*>(malloc(bufferSize));
    if (!coverBuffer) {
      return false;
    }
  }

  memcpy(coverBuffer, frameBuffer, bufferSize);
  return true;
}

void HomeActivity::invalidateCoverBuffer() { coverBufferStored = false; }

bool HomeActivity::restoreCoverBuffer() {
  if (!coverBuffer) {
    return false;
  }

  uint8_t* frameBuffer = renderer.getFrameBuffer();
  if (!frameBuffer) {
    return false;
  }

  const size_t bufferSize = GfxRenderer::getBufferSize();
  memcpy(frameBuffer, coverBuffer, bufferSize);
  return true;
}

void HomeActivity::freeCoverBuffer() {
  if (coverBuffer) {
    free(coverBuffer);
    coverBuffer = nullptr;
  }
  coverBufferStored = false;
}

void HomeActivity::loop() {
  const int bookCount = static_cast<int>(recentBooks.size());
  const int menuItemCount = getMenuItemCount() - bookCount;
  const int totalSelectableCount = bookCount + menuItemCount;
  const bool useModernHomeControls = SETTINGS.uiTheme == CrossPointSettings::MODERN;
  constexpr int cols = 2;  // 2-column menu grid

  auto moveToSelection = [this, bookCount](const int newIndex) {
    const bool movedIntoBooks = newIndex < bookCount;
    const bool wasInBooks = selectorIndex < bookCount;
    selectorIndex = newIndex;
    if (movedIntoBooks || wasInBooks) {
      coverRendered = false;
      invalidateCoverBuffer();
    }
    requestUpdate();
  };

  auto moveHorizontal = [this, bookCount, menuItemCount, cols](const bool moveLeft) {
    if (selectorIndex < bookCount) {
      const bool previewsRight = SETTINGS.previewDirection == CrossPointSettings::PREVIEW_RIGHT;
      if (moveLeft) {
        selectorIndex = previewsRight ? (selectorIndex + 1) % bookCount : (selectorIndex - 1 + bookCount) % bookCount;
      } else {
        selectorIndex = previewsRight ? (selectorIndex - 1 + bookCount) % bookCount : (selectorIndex + 1) % bookCount;
      }
      coverRendered = false;
      invalidateCoverBuffer();
    } else {
      const int menuIdx = selectorIndex - bookCount;
      const int col = menuIdx % cols;
      const int row = menuIdx / cols;
      const int newCol = (col == 0) ? 1 : 0;
      const int newIdx = row * cols + newCol;
      if (newIdx < menuItemCount) {
        selectorIndex = bookCount + newIdx;
        lastMenuCol = newCol;
      }
    }
    requestUpdate();
  };

  if (!useModernHomeControls) {
    auto movePrevious = [this, totalSelectableCount, &moveToSelection]() {
      if (totalSelectableCount <= 0) return;
      const int newIndex = (selectorIndex - 1 + totalSelectableCount) % totalSelectableCount;
      moveToSelection(newIndex);
    };
    auto moveNext = [this, totalSelectableCount, &moveToSelection]() {
      if (totalSelectableCount <= 0) return;
      const int newIndex = (selectorIndex + 1) % totalSelectableCount;
      moveToSelection(newIndex);
    };

    if (mappedInput.wasPressed(MappedInputManager::Button::Left) ||
        mappedInput.wasPressed(MappedInputManager::Button::Up)) {
      movePrevious();
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Right) ||
        mappedInput.wasPressed(MappedInputManager::Button::Down)) {
      moveNext();
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      if (bookCount > 0 && selectorIndex >= bookCount) {
        moveToSelection(0);
      }
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      int idx = 0;
      int menuSelectedIndex = selectorIndex - static_cast<int>(recentBooks.size());
      const int fileBrowserIdx = idx++;
      const int recentsIdx = idx++;
      const int opdsLibraryIdx = hasOpdsUrl ? idx++ : -1;
      const int fileTransferIdx = idx++;
      const int settingsIdx = idx;

      if (selectorIndex < recentBooks.size()) {
        onSelectBook(recentBooks[selectorIndex].path);
      } else if (menuSelectedIndex == fileBrowserIdx) {
        onFileBrowserOpen();
      } else if (menuSelectedIndex == recentsIdx) {
        onRecentsOpen();
      } else if (menuSelectedIndex == opdsLibraryIdx) {
        onOpdsBrowserOpen();
      } else if (menuSelectedIndex == fileTransferIdx) {
        onFileTransferOpen();
      } else if (menuSelectedIndex == settingsIdx) {
        onSettingsOpen();
      }
    }
    return;
  }

  // --- 4-directional navigation ---

  // LEFT or tilt-prev: previous book or toggle menu column
  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    moveHorizontal(true);
  }

  // RIGHT or tilt-next: next book or toggle menu column
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    moveHorizontal(false);
  }

  // DOWN: books → menu first row, or menu → next row
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    if (selectorIndex < bookCount) {
      // From books → menu grid first row — remember which book was selected
      lastBookIndex = selectorIndex;
      const int targetIdx = lastMenuCol < menuItemCount ? lastMenuCol : 0;
      selectorIndex = bookCount + targetIdx;
      coverRendered = false;
      invalidateCoverBuffer();
    } else {
      // In menu grid: go down one row, same column
      const int menuIdx = selectorIndex - bookCount;
      const int col = menuIdx % cols;
      const int row = menuIdx / cols;
      int newIdx = (row + 1) * cols + col;
      // If target cell doesn't exist (odd item count), try col 0
      if (newIdx >= menuItemCount) {
        newIdx = (row + 1) * cols;
      }
      if (newIdx < menuItemCount) {
        selectorIndex = bookCount + newIdx;
        lastMenuCol = newIdx % cols;
      }
    }
    requestUpdate();
  }

  // UP: menu first row → books, or menu → previous row
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    if (selectorIndex < bookCount) {
      // Already in books: no-op
    } else {
      const int menuIdx = selectorIndex - bookCount;
      const int col = menuIdx % cols;
      const int row = menuIdx / cols;
      if (row > 0) {
        // Go up one row, same column
        selectorIndex = bookCount + (row - 1) * cols + col;
        lastMenuCol = col;
      } else if (bookCount > 0) {
        // Top menu row → go back to books (return to last selected book)
        lastMenuCol = col;
        selectorIndex = lastBookIndex;
        coverRendered = false;  // Force cover re-render when returning to carousel
        invalidateCoverBuffer();
      }
    }
    requestUpdate();
  }

  // BACK: jump to cover carousel (from menu only)
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (bookCount > 0 && selectorIndex >= bookCount) {
      // In menu: jump to last selected book cover
      lastMenuCol = (selectorIndex - bookCount) % cols;
      selectorIndex = lastBookIndex;
      coverRendered = false;
      invalidateCoverBuffer();
      requestUpdate();
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    // Calculate dynamic indices based on which options are available
    int idx = 0;
    int menuSelectedIndex = selectorIndex - static_cast<int>(recentBooks.size());
    const int fileBrowserIdx = idx++;
    const int recentsIdx = idx++;
    const int opdsLibraryIdx = hasOpdsUrl ? idx++ : -1;
    const int fileTransferIdx = idx++;
    const int settingsIdx = idx;

    if (selectorIndex < recentBooks.size()) {
      onSelectBook(recentBooks[selectorIndex].path);
    } else if (menuSelectedIndex == fileBrowserIdx) {
      onFileBrowserOpen();
    } else if (menuSelectedIndex == recentsIdx) {
      onRecentsOpen();
    } else if (menuSelectedIndex == opdsLibraryIdx) {
      onOpdsBrowserOpen();
    } else if (menuSelectedIndex == fileTransferIdx) {
      onFileTransferOpen();
    } else if (menuSelectedIndex == settingsIdx) {
      onSettingsOpen();
    }
  }
}

void HomeActivity::render(RenderLock&&) {
  // ─────────────────────────────────────────────────────────────────────────
  // Home render timing — leaves a one-line breakdown in serial that lets us
  // see exactly which phase eats the latency when carousel navigation feels
  // slow.  `clear` + `cover` are CPU/SD-bound; `display` is the e-ink
  // refresh and is mostly hardware-bound (FAST_REFRESH ≈ 400 ms on X3).
  // If `cover` jumps when bufferRestored=false but is near zero otherwise,
  // the bottleneck is BMP decoding rather than the framebuffer copy.
  // ─────────────────────────────────────────────────────────────────────────
  const unsigned long tStart = millis();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  const unsigned long tClear = millis();

  bool bufferRestored = coverBufferStored && restoreCoverBuffer();
  const unsigned long tBufRestore = millis();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.homeTopPadding}, nullptr);

  GUI.drawRecentBookCover(renderer, Rect{0, metrics.homeTopPadding, pageWidth, metrics.homeCoverTileHeight},
                          recentBooks, selectorIndex, coverRendered, coverBufferStored, bufferRestored,
                          std::bind(&HomeActivity::storeCoverBuffer, this));
  const unsigned long tCover = millis();

  // Build menu items dynamically
  std::array<const char*, 5> menuItems{};
  std::array<UIIcon, 5> menuIcons{};
  int menuItemCount = 0;
  menuItems[menuItemCount] = tr(STR_BROWSE_FILES);
  menuIcons[menuItemCount++] = Folder;
  menuItems[menuItemCount] = tr(STR_MENU_RECENT_BOOKS);
  menuIcons[menuItemCount++] = Recent;
  if (hasOpdsUrl) {
    menuItems[menuItemCount] = tr(STR_OPDS_BROWSER);
    menuIcons[menuItemCount++] = Library;
  }
  menuItems[menuItemCount] = tr(STR_SYNC_APP);
  menuIcons[menuItemCount++] = Transfer;
  menuItems[menuItemCount] = tr(STR_SETTINGS_TITLE);
  menuIcons[menuItemCount++] = Settings;

  const int menuY = metrics.homeTopPadding + metrics.homeCoverTileHeight + metrics.verticalSpacing;
  const int menuHeight = pageHeight - menuY - metrics.verticalSpacing - metrics.buttonHintsHeight;
  GUI.drawButtonMenu(renderer, Rect{0, menuY, pageWidth, menuHeight}, menuItemCount, selectorIndex - recentBooks.size(),
                     [&menuItems](int index) { return std::string(menuItems[index]); },
                     [&menuIcons](int index) { return menuIcons[index]; });

  const bool useModernHomeControls = SETTINGS.uiTheme == CrossPointSettings::MODERN;
  const auto labels = mappedInput.mapLabels(tr(STR_BOOKS), tr(STR_SELECT),
                                            useModernHomeControls ? tr(STR_DIR_LEFT) : tr(STR_DIR_UP),
                                            useModernHomeControls ? tr(STR_DIR_RIGHT) : tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (useModernHomeControls) {
    GUI.drawSideButtonHints(renderer, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  }

  const unsigned long tMenu = millis();

  // Always use fast refresh for minimal flicker (1 flash only).
  renderer.displayBuffer();
  const unsigned long tDisplay = millis();

  LOG_INF("HOME",
          "render: clear=%lums bufRestore=%lu(%s) cover=%lums menu+hints=%lums display=%lums total=%lums "
          "(coverRendered=%d coverBufStored=%d bookCount=%d sel=%d)",
          tClear - tStart, tBufRestore - tClear, bufferRestored ? "restored" : "skipped", tCover - tBufRestore,
          tMenu - tCover, tDisplay - tMenu, tDisplay - tStart, coverRendered ? 1 : 0, coverBufferStored ? 1 : 0,
          static_cast<int>(recentBooks.size()), selectorIndex);
}

void HomeActivity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }

void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }

void HomeActivity::onRecentsOpen() { activityManager.goToRecentBooks(); }

void HomeActivity::onSettingsOpen() { activityManager.goToSettings(); }

void HomeActivity::onFileTransferOpen() { activityManager.goToFileTransfer(); }

void HomeActivity::onOpdsBrowserOpen() { activityManager.goToBrowser(); }
