#include "RecentBooksActivity.h"

#include <BoardConfig.h>
#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/themes/modern/ModernTheme.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr unsigned long GO_HOME_MS = 1000;

#if defined(CROSSPOINT_BOARD_X3)
// X3 power button is on the TOP edge of the device, not the right side like
// X4.  Draw the "Remove" affordance as a HORIZONTAL pill hanging from the
// top edge (flat-top, rounded-bottom) so users intuitively map the visual
// hint to the physical button.  Positioned at the top-right corner because
// that's where the battery icon used to live — moved out by
// drawBatteryLeftForRecent() below to leave a clean slot for this pill.
void drawPowerRemoveHint(const GfxRenderer& renderer, const ThemeMetrics& /*metrics*/, const int pageWidth,
                         const char* label) {
  if (label == nullptr || label[0] == '\0') {
    return;
  }

  constexpr int hintRadius = 8;
  constexpr int verticalPadding = 6;
  constexpr int horizontalPadding = 14;
  constexpr int rightInset = 24;        // gap from the screen's right edge
  constexpr int topOverhang = 4;        // pill top extends "above" screen 0,0 so the flat
                                        // top edge isn't visible — only rounded bottom shows.
  constexpr int minHintWidth = 60;

  const int textWidth = renderer.getTextWidth(SMALL_FONT_ID, label);
  const int textHeight = renderer.getTextHeight(SMALL_FONT_ID);
  const int hintHeight = textHeight + verticalPadding * 2 + topOverhang;
  const int hintWidth = std::max(minHintWidth, textWidth + horizontalPadding * 2);
  const int hintX = pageWidth - hintWidth - rightInset;
  const int hintY = -topOverhang;
  const int textX = hintX + (hintWidth - textWidth) / 2;
  const int textY = topOverhang + verticalPadding;

  renderer.fillRoundedRect(hintX, hintY, hintWidth, hintHeight, hintRadius,
                           false, false, true, true, Color::White);
  renderer.drawRoundedRect(hintX, hintY, hintWidth, hintHeight, 1, hintRadius,
                           false, false, true, true, true);
  renderer.drawText(SMALL_FONT_ID, textX, textY, label);
}

// On X3 the new top "Remove" pill occupies the same top-right slot that
// drawHeader's drawBatteryRight uses, so the % text + battery icon would
// collide with the pill.  Clear the right-side battery area and re-draw the
// battery on the left immediately after the time string.  Only applies to
// this page — the rest of the UI continues to use drawBatteryRight().
void drawBatteryLeftForRecent(const GfxRenderer& renderer, const ThemeMetrics& metrics, const int pageWidth) {
  const bool showBatteryPercentage =
      SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS;

  // 1) Erase the right-side battery area that drawHeader already drew.  Width
  //    must cover both the icon and the "100%"/"NN%" text that sits to its left.
  constexpr int rightClearWidth = 80;
  renderer.fillRect(pageWidth - rightClearWidth, metrics.topPadding + 3, rightClearWidth,
                    metrics.batteryHeight + 6, false);

  // 2) Compute the left slot just after the time string ("HH:MM" ≈ 36 px wide
  //    in SMALL_FONT_ID).  We always reserve the same gap regardless of NTP
  //    sync state so the battery never shifts when the clock appears.
  const int timeSlotWidth = renderer.getTextWidth(SMALL_FONT_ID, "00:00");
  const int battX = metrics.contentSidePadding + timeSlotWidth + 12;
  const uint16_t percentage = powerManager.getBatteryPercentage();
  const int battY = metrics.topPadding + 5;
  const int y = battY + 6;
  const int battWidth = metrics.batteryWidth;
  const int battHeight = metrics.batteryHeight;

  // Percentage text first (drawn to the right of where the icon will be).
  if (showBatteryPercentage) {
    const auto percentageText = std::to_string(percentage) + "%";
    renderer.drawText(SMALL_FONT_ID, battX + battWidth + 4, battY, percentageText.c_str(), true,
                      EpdFontFamily::BOLD);
  }

  // Battery glyph (same outline as ModernTheme::drawBatteryLeft).
  renderer.drawLine(battX + 1, y, battX + battWidth - 3, y);
  renderer.drawLine(battX + 1, y + battHeight - 1, battX + battWidth - 3, y + battHeight - 1);
  renderer.drawLine(battX, y + 1, battX, y + battHeight - 2);
  renderer.drawLine(battX + battWidth - 2, y + 1, battX + battWidth - 2, y + battHeight - 2);
  renderer.drawPixel(battX + battWidth - 1, y + 3);
  renderer.drawPixel(battX + battWidth - 1, y + battHeight - 4);
  renderer.drawLine(battX + battWidth - 0, y + 4, battX + battWidth - 0, y + battHeight - 5);
  if (percentage > 10) renderer.fillRect(battX + 2, y + 2, 3, battHeight - 4);
  if (percentage > 40) renderer.fillRect(battX + 6, y + 2, 3, battHeight - 4);
  if (percentage > 70) renderer.fillRect(battX + 10, y + 2, 3, battHeight - 4);
}
#else
// X4 (and any non-X3 board): power button is on the SIDE, so draw the
// "Remove" pill on the right edge of the screen with vertically rotated text
// — keeps the visual hint adjacent to the physical button.
void drawPowerRemoveHint(const GfxRenderer& renderer, const ThemeMetrics& metrics, const int pageWidth,
                         const char* label) {
  if (label == nullptr || label[0] == '\0') {
    return;
  }

  constexpr int hintInset = 8;
  constexpr int hintRadius = 8;
  constexpr int minHintHeight = 58;
  constexpr int verticalPadding = 10;
  constexpr int horizontalPadding = 7;
  constexpr int rightInset = -6;
  constexpr int downOffset = 19;

  const int halo2HintWidth = std::max(ModernMetrics::values.sideButtonHintsWidth + 10, 30);
  const int hintWidth =
      (SETTINGS.uiTheme == CrossPointSettings::MODERN) ? std::max(metrics.sideButtonHintsWidth + 10, 30) : halo2HintWidth;
  const int textWidth = renderer.getTextWidth(SMALL_FONT_ID, label);
  const int textHeight = renderer.getTextHeight(SMALL_FONT_ID);
  const int hintHeight = std::max(minHintHeight, textWidth + verticalPadding * 2);
  const int hintX = pageWidth - hintWidth - rightInset;
  const int hintY = metrics.topPadding + metrics.headerHeight + hintInset + downOffset;
  const int preferredTextX = hintX + (hintWidth - textHeight) / 2;
  const int maxVisibleTextX = pageWidth - textHeight - horizontalPadding;
  const int textX = std::min(preferredTextX, maxVisibleTextX);
  const int textY = hintY + (hintHeight + textWidth) / 2;

  renderer.fillRoundedRect(hintX, hintY, hintWidth, hintHeight, hintRadius, true, false, true, false, Color::White);
  renderer.drawRoundedRect(hintX, hintY, hintWidth, hintHeight, 1, hintRadius, true, false, true, false, true);
  renderer.drawTextRotated90CW(SMALL_FONT_ID, textX, textY, label);
}
#endif
}  // namespace

void RecentBooksActivity::loadRecentBooks() {
  RECENT_BOOKS.pruneMissingBooks();

  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(books.size());

  for (const auto& book : books) {
    // Skip if file no longer exists
    if (!Storage.exists(book.path.c_str())) {
      continue;
    }
    recentBooks.push_back(book);
  }
}

void RecentBooksActivity::onEnter() {
  Activity::onEnter();

  // Load data
  loadRecentBooks();

  selectorIndex = 0;
  requestUpdate();
}

void RecentBooksActivity::onExit() {
  Activity::onExit();
  recentBooks.clear();
}

void RecentBooksActivity::removeSelectedRecentBook() {
  if (recentBooks.empty() || selectorIndex >= recentBooks.size()) {
    return;
  }

  const auto selectedBook = recentBooks[selectorIndex];
  auto handler = [this, selectedBook](const ActivityResult& res) {
    if (res.isCancelled) {
      return;
    }

    if (RECENT_BOOKS.removeBook(selectedBook.path)) {
      loadRecentBooks();
      if (recentBooks.empty()) {
        selectorIndex = 0;
      } else if (selectorIndex >= recentBooks.size()) {
        selectorIndex = recentBooks.size() - 1;
      }
      requestUpdate(true);
    }
  };

  std::string heading = tr(STR_REMOVE_FROM_RECENT);
  startActivityForResult(std::make_unique<ConfirmationActivity>(renderer, mappedInput, heading, selectedBook.title),
                         handler);
}

void RecentBooksActivity::loop() {
  const int pageItems = UITheme::getInstance().getNumberOfItemsPerPage(renderer, true, false, true, true);

  if (mappedInput.wasReleased(MappedInputManager::Button::Power)) {
    if (!recentBooks.empty() && selectorIndex < static_cast<int>(recentBooks.size())) {
      removeSelectedRecentBook();
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!recentBooks.empty() && selectorIndex < static_cast<int>(recentBooks.size())) {
      LOG_DBG("RBA", "Selected recent book: %s", recentBooks[selectorIndex].path.c_str());
      onSelectBook(recentBooks[selectorIndex].path);
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
  }

  int listSize = static_cast<int>(recentBooks.size());

  buttonNavigator.onNextRelease([this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });
}

void RecentBooksActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_MENU_RECENT_BOOKS));

#if defined(CROSSPOINT_BOARD_X3)
  // X3-only: relocate the battery from the header's right-side default to the
  // left, freeing the top-right slot for the new top "Remove" hint pill.
  // Other activities continue to use drawBatteryRight; this rewrite is scoped
  // to the Recent Books page only.
  drawBatteryLeftForRecent(renderer, metrics, pageWidth);
#endif

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  // Recent tab
  if (recentBooks.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_RECENT_BOOKS));
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, recentBooks.size(), selectorIndex,
        [this](int index) { return recentBooks[index].title; }, [this](int index) { return recentBooks[index].author; },
        [this](int index) { return UITheme::getFileIcon(recentBooks[index].path); });
  }

  // Help text
  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (!recentBooks.empty()) {
    drawPowerRemoveHint(renderer, metrics, pageWidth, "Remove");
  }

  renderer.displayBuffer();
}
