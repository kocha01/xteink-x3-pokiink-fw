#ifdef CROSSPOINT_KEYBOARD_STUB

#include "KeyboardEntryActivity.h"

#include <I18n.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

constexpr int kKeyboardRows = 5;
constexpr int kSpecialRow = 4;
constexpr int kVisualRowOrder[kKeyboardRows] = {
    kSpecialRow,
    0,
    1,
    2,
    3,
};
constexpr int kLangCol = 0;
constexpr int kLangEndCol = 3;
constexpr int kShiftCol = 3;
constexpr int kShiftEndCol = 5;
constexpr int kSpaceCol = 5;
constexpr int kSpaceEndCol = 7;
constexpr int kBackspaceCol = 7;
constexpr int kBackspaceEndCol = 9;
constexpr int kDoneCol = 9;
constexpr int kDoneEndCol = 11;

int visualIndexForRow(const int logicalRow) {
  for (int index = 0; index < kKeyboardRows; ++index) {
    if (kVisualRowOrder[index] == logicalRow) {
      return index;
    }
  }
  return 0;
}

int clampToolbarCol(const int col) {
  if (col < kLangEndCol) return kLangCol;
  if (col < kShiftEndCol) return kShiftCol;
  if (col < kSpaceEndCol) return kSpaceCol;
  if (col < kBackspaceEndCol) return kBackspaceCol;
  return kDoneCol;
}

}  // namespace

const char* const KeyboardEntryActivity::englishKeys[NUM_ROWS][KEYS_PER_ROW] = {
    {"`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "="},
    {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\\"},
    {"a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'"},
    {"z", "x", "c", "v", "b", "n", "m", ",", ".", "/"},
    {}};

const char* const KeyboardEntryActivity::englishKeysShift[NUM_ROWS][KEYS_PER_ROW] = {
    {"~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+"},
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "{", "}", "|"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "\""},
    {"Z", "X", "C", "V", "B", "N", "M", "<", ">", "?"},
    {}};

const int KeyboardEntryActivity::englishRowLengths[NUM_ROWS] = {13, 13, 11, 10, 11};

const char* const KeyboardEntryActivity::thaiKeysAlpha[NUM_ROWS][KEYS_PER_ROW] = {
    {"\xe0\xb8\x81", "\xe0\xb8\x82", "\xe0\xb8\x83", "\xe0\xb8\x84", "\xe0\xb8\x85",
     "\xe0\xb8\x86", "\xe0\xb8\x87", "\xe0\xb8\x88", "\xe0\xb8\x89", "\xe0\xb8\x8a",
     "\xe0\xb8\x8b", "\xe0\xb8\x8c", "\xe0\xb8\x8d"},
    {"\xe0\xb8\x8e", "\xe0\xb8\x8f", "\xe0\xb8\x90", "\xe0\xb8\x91", "\xe0\xb8\x92",
     "\xe0\xb8\x93", "\xe0\xb8\x94", "\xe0\xb8\x95", "\xe0\xb8\x96", "\xe0\xb8\x97",
     "\xe0\xb8\x98", "\xe0\xb8\x99", "\xe0\xb8\x9a"},
    {"\xe0\xb8\x9b", "\xe0\xb8\x9c", "\xe0\xb8\x9d", "\xe0\xb8\x9e", "\xe0\xb8\x9f",
     "\xe0\xb8\xa0", "\xe0\xb8\xa1", "\xe0\xb8\xa2", "\xe0\xb8\xa3", "\xe0\xb8\xa5",
     "\xe0\xb8\xa7"},
    {"\xe0\xb8\xa8", "\xe0\xb8\xa9", "\xe0\xb8\xaa", "\xe0\xb8\xab", "\xe0\xb8\xac",
     "\xe0\xb8\xad", "\xe0\xb8\xae", ".", "-", ","},
    {}};

const char* const KeyboardEntryActivity::thaiKeysAlphaShift[NUM_ROWS][KEYS_PER_ROW] = {
    {"\xe0\xb9\x80", "\xe0\xb9\x81", "\xe0\xb9\x82", "\xe0\xb9\x83", "\xe0\xb9\x84",
     "\xe0\xb8\xb0", "\xe0\xb8\xb2", "\xe0\xb8\xb3", "\xe0\xb8\xb4", "\xe0\xb8\xb5",
     "\xe0\xb8\xb6", "\xe0\xb8\xb7", "\xe0\xb8\xb8"},
    {"\xe0\xb8\xb9", "\xe0\xb8\xb1", "\xe0\xb9\x87", "\xe0\xb9\x88", "\xe0\xb9\x89",
     "\xe0\xb9\x8a", "\xe0\xb9\x8b", "\xe0\xb9\x8c", "\xe0\xb9\x8d", "\xe0\xb8\xba",
     "\xe0\xb9\x86", "\xe0\xb8\xaf", "\xe0\xb9\x85"},
    {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "+"},
    {"!", "?", "(", ")", "\"", ":", ";", "@", "#", "_"},
    {}};

const char* const KeyboardEntryActivity::thaiKeysKed[NUM_ROWS][KEYS_PER_ROW] = {
    {"_", "\xe0\xb9\x85", "/", "-", "\xe0\xb8\xa0", "\xe0\xb8\x96", "\xe0\xb8\xb8",
     "\xe0\xb8\xb6", "\xe0\xb8\x84", "\xe0\xb8\x95", "\xe0\xb8\x88", "\xe0\xb8\x82",
     "\xe0\xb8\x8a"},
    {"\xe0\xb9\x86", "\xe0\xb9\x84", "\xe0\xb8\xb3", "\xe0\xb8\x9e", "\xe0\xb8\xb0",
     "\xe0\xb8\xb1", "\xe0\xb8\xb5", "\xe0\xb8\xa3", "\xe0\xb8\x99", "\xe0\xb8\xa2",
     "\xe0\xb8\x9a", "\xe0\xb8\xa5", "\xe0\xb8\x83"},
    {"\xe0\xb8\x9f", "\xe0\xb8\xab", "\xe0\xb8\x81", "\xe0\xb8\x94", "\xe0\xb9\x80",
     "\xe0\xb9\x89", "\xe0\xb9\x88", "\xe0\xb8\xb2", "\xe0\xb8\xaa", "\xe0\xb8\xa7",
     "\xe0\xb8\x87"},
    {"\xe0\xb8\x9c", "\xe0\xb8\x9b", "\xe0\xb9\x81", "\xe0\xb8\xad", "\xe0\xb8\xb4",
     "\xe0\xb8\xb7", "\xe0\xb8\x97", "\xe0\xb8\xa1", "\xe0\xb9\x83", "\xe0\xb8\x9d"},
    {}};

const char* const KeyboardEntryActivity::thaiKeysKedShift[NUM_ROWS][KEYS_PER_ROW] = {
    {"%", "+", "\xe0\xb9\x91", "\xe0\xb9\x92", "\xe0\xb9\x93", "\xe0\xb9\x94",
     "\xe0\xb8\xb9", "\xe0\xb8\xbf", "\xe0\xb9\x95", "\xe0\xb9\x96", "\xe0\xb9\x97",
     "\xe0\xb9\x98", "\xe0\xb9\x99"},
    {"\xe0\xb9\x90", "\"", "\xe0\xb8\x8e", "\xe0\xb8\x91", "\xe0\xb8\x98",
     "\xe0\xb9\x8d", "\xe0\xb9\x8a", "\xe0\xb8\x93", "\xe0\xb8\xaf", "\xe0\xb8\x8d",
     "\xe0\xb8\x90", ",", "\xe0\xb8\x85"},
    {"\xe0\xb8\xa4", "\xe0\xb8\x86", "\xe0\xb8\x8f", "\xe0\xb9\x82", "\xe0\xb8\x8c",
     "\xe0\xb9\x87", "\xe0\xb9\x8b", "\xe0\xb8\xa9", "\xe0\xb8\xa8", "\xe0\xb8\x8b",
     "."},
    {"(", ")", "\xe0\xb8\x89", "\xe0\xb8\xae", "\xe0\xb8\xba", "\xe0\xb9\x8c",
     "?", "\xe0\xb8\x92", "\xe0\xb8\xac", "\xe0\xb8\xa6"},
    {}};

const char* const KeyboardEntryActivity::thaiKeysFreq[NUM_ROWS][KEYS_PER_ROW] = {
    {"\xe0\xb8\x99", "\xe0\xb8\x81", "\xe0\xb8\xa1", "\xe0\xb8\xa3", "\xe0\xb8\x97",
     "\xe0\xb8\xa5", "\xe0\xb8\x84", "\xe0\xb8\xa7", "\xe0\xb8\xa2", "\xe0\xb8\x9b",
     "\xe0\xb8\x88", "\xe0\xb8\x94", "\xe0\xb8\xad"},
    {"\xe0\xb8\x8a", "\xe0\xb8\xaa", "\xe0\xb8\x9e", "\xe0\xb8\x95", "\xe0\xb8\xab",
     "\xe0\xb8\x87", "\xe0\xb8\x9a", "\xe0\xb8\x82", "\xe0\xb9\x80", "\xe0\xb9\x81",
     "\xe0\xb9\x82", "\xe0\xb9\x83", "\xe0\xb9\x84"},
    {"\xe0\xb8\xb0", "\xe0\xb8\xb2", "\xe0\xb8\xb3", "\xe0\xb8\xb4", "\xe0\xb8\xb5",
     "\xe0\xb8\xb6", "\xe0\xb8\xb7", "\xe0\xb8\xb8", "\xe0\xb8\xb9", "\xe0\xb8\xb1",
     "\xe0\xb9\x87"},
    {"\xe0\xb9\x88", "\xe0\xb9\x89", "\xe0\xb9\x8a", "\xe0\xb9\x8b", "\xe0\xb9\x8c",
     "\xe0\xb9\x86", "\xe0\xb8\xaf", ".", "-", ","},
    {}};

const char* const KeyboardEntryActivity::thaiKeysFreqShift[NUM_ROWS][KEYS_PER_ROW] = {
    {"\xe0\xb8\xa8", "\xe0\xb8\x8b", "\xe0\xb8\xa0", "\xe0\xb8\x98", "\xe0\xb8\x96",
     "\xe0\xb8\x9c", "\xe0\xb8\x9f", "\xe0\xb8\x8d", "\xe0\xb8\x93", "\xe0\xb8\x90",
     "\xe0\xb8\x86", "\xe0\xb8\x8c", "\xe0\xb8\x8e"},
    {"\xe0\xb8\x8f", "\xe0\xb8\x91", "\xe0\xb8\x92", "\xe0\xb8\x83", "\xe0\xb8\x85",
     "\xe0\xb8\xac", "\xe0\xb8\xae", "\xe0\xb8\xa4", "\xe0\xb8\xa6", "\xe0\xb9\x85",
     "\xe0\xb8\xba", "\xe0\xb9\x8d", "/"},
    {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "+"},
    {"!", "?", "(", ")", "\"", ":", ";", "@", "#", "_"},
    {}};
const int KeyboardEntryActivity::thaiRowLengths[NUM_ROWS] = {13, 13, 11, 10, 11};

const char* const KeyboardEntryActivity::shiftString[3] = {"shift", "SHIFT", "LOCK"};

void KeyboardEntryActivity::onEnter() {
  Activity::onEnter();
  if (selectedRow == kSpecialRow) {
    selectedCol = clampToolbarCol(selectedCol);
  }
  requestUpdate();
}

void KeyboardEntryActivity::onExit() { Activity::onExit(); }

const char* KeyboardEntryActivity::getKeyAt(const int row, const int col) const {
  if (row < 0 || row >= NUM_ROWS || col < 0 || col >= KEYS_PER_ROW) {
    return nullptr;
  }

  if (layout == KeyboardLayout::Thai) {
    const char* const(*thaiKeys)[KEYS_PER_ROW] = thaiKeysFreq;
    const char* const(*thaiKeysShift)[KEYS_PER_ROW] = thaiKeysFreqShift;
    switch (SETTINGS.thaiKeyboardLayout) {
      case CrossPointSettings::THAI_KB_ALPHABETICAL:
        thaiKeys = thaiKeysAlpha;
        thaiKeysShift = thaiKeysAlphaShift;
        break;
      case CrossPointSettings::THAI_KB_KEDMANEE:
        thaiKeys = thaiKeysKed;
        thaiKeysShift = thaiKeysKedShift;
        break;
      case CrossPointSettings::THAI_KB_FREQUENCY:
      default:
        thaiKeys = thaiKeysFreq;
        thaiKeysShift = thaiKeysFreqShift;
        break;
    }
    return shiftState ? thaiKeysShift[row][col] : thaiKeys[row][col];
  }

  return shiftState ? englishKeysShift[row][col] : englishKeys[row][col];
}

int KeyboardEntryActivity::getRowLength(const int row) const {
  if (row < 0 || row >= NUM_ROWS) {
    return 0;
  }
  return layout == KeyboardLayout::Thai ? thaiRowLengths[row] : englishRowLengths[row];
}

void KeyboardEntryActivity::appendKey(const char* key) {
  if (!key || key[0] == '\0') {
    return;
  }
  if (maxLength > 0 && text.length() >= maxLength) {
    return;
  }
  text += key;
}

void KeyboardEntryActivity::backspaceUtf8() {
  if (text.empty()) {
    return;
  }

  size_t pos = text.size() - 1;
  while (pos > 0 && (static_cast<uint8_t>(text[pos]) & 0xC0) == 0x80) {
    --pos;
  }
  text.erase(pos);
}

bool KeyboardEntryActivity::handleKeyPress() {
  if (selectedRow == kSpecialRow) {
    if (selectedCol >= kLangCol && selectedCol < kLangEndCol) {
      layout = layout == KeyboardLayout::Thai ? KeyboardLayout::English : KeyboardLayout::Thai;
      shiftState = 0;
      return true;
    }
    if (selectedCol >= kShiftCol && selectedCol < kShiftEndCol) {
      shiftState = (shiftState + 1) % 3;
      return true;
    }
    if (selectedCol >= kSpaceCol && selectedCol < kSpaceEndCol) {
      appendKey(" ");
      return true;
    }
    if (selectedCol >= kBackspaceCol && selectedCol < kBackspaceEndCol) {
      backspaceUtf8();
      return true;
    }
    if (selectedCol >= kDoneCol) {
      onComplete(text);
      return false;
    }
  }

  const char* key = getKeyAt(selectedRow, selectedCol);
  if (!key || key[0] == '\0') {
    return true;
  }

  appendKey(key);
  if (shiftState == 1) {
    shiftState = 0;
  }
  return true;
}

void KeyboardEntryActivity::loop() {
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this] {
    const int currentVisualIndex = visualIndexForRow(selectedRow);
    selectedRow = kVisualRowOrder[ButtonNavigator::previousIndex(currentVisualIndex, NUM_ROWS)];
    if (selectedRow == kSpecialRow) {
      selectedCol = clampToolbarCol(selectedCol);
    } else {
      const int maxCol = getRowLength(selectedRow) - 1;
      if (selectedCol > maxCol) {
        selectedCol = maxCol;
      }
    }
    requestUpdate();
  });

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [this] {
    const int currentVisualIndex = visualIndexForRow(selectedRow);
    selectedRow = kVisualRowOrder[ButtonNavigator::nextIndex(currentVisualIndex, NUM_ROWS)];
    if (selectedRow == kSpecialRow) {
      selectedCol = clampToolbarCol(selectedCol);
    } else {
      const int maxCol = getRowLength(selectedRow) - 1;
      if (selectedCol > maxCol) {
        selectedCol = maxCol;
      }
    }
    requestUpdate();
  });

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [this] {
    const int maxCol = getRowLength(selectedRow) - 1;
    if (selectedRow == kSpecialRow) {
      if (selectedCol >= kLangCol && selectedCol < kLangEndCol) {
        selectedCol = kDoneCol;
      } else if (selectedCol >= kShiftCol && selectedCol < kShiftEndCol) {
        selectedCol = kLangCol;
      } else if (selectedCol >= kSpaceCol && selectedCol < kSpaceEndCol) {
        selectedCol = kShiftCol;
      } else if (selectedCol >= kBackspaceCol && selectedCol < kBackspaceEndCol) {
        selectedCol = kSpaceCol;
      } else if (selectedCol >= kDoneCol) {
        selectedCol = kBackspaceCol;
      }
    } else {
      selectedCol = ButtonNavigator::previousIndex(selectedCol, maxCol + 1);
    }
    requestUpdate();
  });

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [this] {
    const int maxCol = getRowLength(selectedRow) - 1;
    if (selectedRow == kSpecialRow) {
      if (selectedCol >= kLangCol && selectedCol < kLangEndCol) {
        selectedCol = kShiftCol;
      } else if (selectedCol >= kShiftCol && selectedCol < kShiftEndCol) {
        selectedCol = kSpaceCol;
      } else if (selectedCol >= kSpaceCol && selectedCol < kSpaceEndCol) {
        selectedCol = kBackspaceCol;
      } else if (selectedCol >= kBackspaceCol && selectedCol < kBackspaceEndCol) {
        selectedCol = kDoneCol;
      } else if (selectedCol >= kDoneCol) {
        selectedCol = kLangCol;
      }
    } else {
      selectedCol = ButtonNavigator::nextIndex(selectedCol, maxCol + 1);
    }
    requestUpdate();
  });

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (handleKeyPress()) {
      requestUpdate();
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onCancel();
  }
}

void KeyboardEntryActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title.c_str());

  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int inputStartY =
      metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + metrics.verticalSpacing * 4;
  int inputHeight = 0;

  std::string displayText = isPassword ? std::string(text.length(), '*') : text;
  displayText += "_";

  int lineStartIdx = 0;
  int lineEndIdx = static_cast<int>(displayText.length());
  int textWidth = 0;
  while (true) {
    std::string lineText = displayText.substr(lineStartIdx, lineEndIdx - lineStartIdx);
    textWidth = renderer.getTextWidth(UI_12_FONT_ID, lineText.c_str());
    if (textWidth <= pageWidth - 2 * metrics.contentSidePadding) {
      if (metrics.keyboardCenteredText) {
        renderer.drawCenteredText(UI_12_FONT_ID, inputStartY + inputHeight, lineText.c_str());
      } else {
        renderer.drawText(UI_12_FONT_ID, metrics.contentSidePadding, inputStartY + inputHeight, lineText.c_str());
      }

      if (lineEndIdx == static_cast<int>(displayText.length())) {
        break;
      }

      inputHeight += lineHeight;
      lineStartIdx = lineEndIdx;
      lineEndIdx = static_cast<int>(displayText.length());
    } else {
      lineEndIdx -= 1;
    }
  }

  GUI.drawTextField(renderer, Rect{0, inputStartY, pageWidth, inputHeight}, textWidth);

  const int keyboardStartY = metrics.keyboardBottomAligned
                                 ? pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing -
                                       (metrics.keyboardKeyHeight + metrics.keyboardKeySpacing) * NUM_ROWS
                                 : inputStartY + inputHeight + metrics.verticalSpacing * 4;
  const int keyWidth = metrics.keyboardKeyWidth;
  const int keyHeight = metrics.keyboardKeyHeight;
  const int keySpacing = metrics.keyboardKeySpacing;
  const int maxRowWidth = KEYS_PER_ROW * (keyWidth + keySpacing);
  const int leftMargin = (pageWidth - maxRowWidth) / 2;

  for (int visualRow = 0; visualRow < NUM_ROWS; visualRow++) {
    const int row = kVisualRowOrder[visualRow];
    const int rowY = keyboardStartY + visualRow * (keyHeight + keySpacing);
    const int startX = leftMargin;

    if (row == kSpecialRow) {
      int currentX = startX;

      const char* languageLabel = layout == KeyboardLayout::Thai ? "abc123" : "กขค";
      const bool languageSelected = (selectedRow == kSpecialRow && selectedCol >= kLangCol && selectedCol < kLangEndCol);
      const int languagePixelWidth = (kLangEndCol - kLangCol) * (keyWidth + keySpacing);
      GUI.drawKeyboardKey(renderer, Rect{currentX, rowY, languagePixelWidth, keyHeight}, languageLabel,
                          languageSelected);
      currentX += languagePixelWidth;

      const bool shiftSelected = (selectedRow == kSpecialRow && selectedCol >= kShiftCol && selectedCol < kShiftEndCol);
      const int shiftPixelWidth = (kShiftEndCol - kShiftCol) * (keyWidth + keySpacing);
      GUI.drawKeyboardKey(renderer, Rect{currentX, rowY, shiftPixelWidth, keyHeight}, shiftString[shiftState],
                          shiftSelected);
      currentX += shiftPixelWidth;

      const bool spaceSelected = (selectedRow == kSpecialRow && selectedCol >= kSpaceCol && selectedCol < kSpaceEndCol);
      const int spacePixelWidth = (kSpaceEndCol - kSpaceCol) * (keyWidth + keySpacing);
      GUI.drawKeyboardKey(renderer, Rect{currentX, rowY, spacePixelWidth, keyHeight}, "_", spaceSelected);
      currentX += spacePixelWidth;

      const bool backspaceSelected =
          (selectedRow == kSpecialRow && selectedCol >= kBackspaceCol && selectedCol < kBackspaceEndCol);
      const int backspacePixelWidth = (kBackspaceEndCol - kBackspaceCol) * (keyWidth + keySpacing);
      GUI.drawKeyboardKey(renderer, Rect{currentX, rowY, backspacePixelWidth, keyHeight}, "<-", backspaceSelected);
      currentX += backspacePixelWidth;

      const bool doneSelected = (selectedRow == kSpecialRow && selectedCol >= kDoneCol && selectedCol < kDoneEndCol);
      const int donePixelWidth = (kDoneEndCol - kDoneCol) * (keyWidth + keySpacing);
      GUI.drawKeyboardKey(renderer, Rect{currentX, rowY, donePixelWidth, keyHeight}, tr(STR_OK_BUTTON), doneSelected);
    } else {
      for (int col = 0; col < getRowLength(row); col++) {
        const char* keyLabel = getKeyAt(row, col);
        if (!keyLabel) {
          continue;
        }

        const int keyX = startX + col * (keyWidth + keySpacing);
        const bool isSelected = row == selectedRow && col == selectedCol;
        GUI.drawKeyboardKey(renderer, Rect{keyX, rowY, keyWidth, keyHeight}, keyLabel, isSelected);
      }
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  GUI.drawSideButtonHints(renderer, ">", "<");

  renderer.displayBuffer();
}

void KeyboardEntryActivity::onComplete(std::string value) {
  setResult(KeyboardResult{std::move(value)});
  finish();
}

void KeyboardEntryActivity::onCancel() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}

#endif
