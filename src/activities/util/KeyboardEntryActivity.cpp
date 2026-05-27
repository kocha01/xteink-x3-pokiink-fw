#include "KeyboardEntryActivity.h"

#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

// --- English QWERTY layout (per-key UTF-8 strings) ---

const char* const KeyboardEntryActivity::englishKeys[NUM_ROWS][KEYS_PER_ROW] = {
    {"`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "="},
    {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\\"},
    {"a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'"},
    {"z", "x", "c", "v", "b", "n", "m", ",", ".", "/"},
    {}  // special row handled in code
};

const char* const KeyboardEntryActivity::englishKeysShift[NUM_ROWS][KEYS_PER_ROW] = {
    {"~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+"},
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "{", "}", "|"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "\""},
    {"Z", "X", "C", "V", "B", "N", "M", "<", ">", "?"},
    {}
};

const int KeyboardEntryActivity::englishRowLengths[NUM_ROWS] = {13, 13, 11, 10, 11};

// --- Thai layout: Alphabetical (ก-ฮ order) ---
// Page 1: Consonants in standard Thai dictionary order
// Page 2 (shift): Vowels, tone marks, numbers

const char* const KeyboardEntryActivity::thaiKeysAlpha[NUM_ROWS][KEYS_PER_ROW] = {
    {"\xe0\xb8\x81", "\xe0\xb8\x82", "\xe0\xb8\x83", "\xe0\xb8\x84", "\xe0\xb8\x85",  // ก ข ฃ ค ฅ
     "\xe0\xb8\x86", "\xe0\xb8\x87", "\xe0\xb8\x88", "\xe0\xb8\x89", "\xe0\xb8\x8a",   // ฆ ง จ ฉ ช
     "\xe0\xb8\x8b", "\xe0\xb8\x8c", "\xe0\xb8\x8d"},                                    // ซ ฌ ญ
    {"\xe0\xb8\x8e", "\xe0\xb8\x8f", "\xe0\xb8\x90", "\xe0\xb8\x91", "\xe0\xb8\x92",   // ฎ ฏ ฐ ฑ ฒ
     "\xe0\xb8\x93", "\xe0\xb8\x94", "\xe0\xb8\x95", "\xe0\xb8\x96", "\xe0\xb8\x97",   // ณ ด ต ถ ท
     "\xe0\xb8\x98", "\xe0\xb8\x99", "\xe0\xb8\x9a"},                                    // ธ น บ
    {"\xe0\xb8\x9b", "\xe0\xb8\x9c", "\xe0\xb8\x9d", "\xe0\xb8\x9e", "\xe0\xb8\x9f",   // ป ผ ฝ พ ฟ
     "\xe0\xb8\xa0", "\xe0\xb8\xa1", "\xe0\xb8\xa2", "\xe0\xb8\xa3", "\xe0\xb8\xa5",   // ภ ม ย ร ล
     "\xe0\xb8\xa7"},                                                                      // ว
    {"\xe0\xb8\xa8", "\xe0\xb8\xa9", "\xe0\xb8\xaa", "\xe0\xb8\xab", "\xe0\xb8\xac",   // ศ ษ ส ห ฬ
     "\xe0\xb8\xad", "\xe0\xb8\xae", ".", "-", ","},                                      // อ ฮ . - ,
    {}
};

const char* const KeyboardEntryActivity::thaiKeysAlphaShift[NUM_ROWS][KEYS_PER_ROW] = {
    {"\xe0\xb9\x80", "\xe0\xb9\x81", "\xe0\xb9\x82", "\xe0\xb9\x83", "\xe0\xb9\x84",   // เ แ โ ใ ไ
     "\xe0\xb8\xb0", "\xe0\xb8\xb2", "\xe0\xb8\xb3", "\xe0\xb8\xb4", "\xe0\xb8\xb5",   // ะ า ำ ิ ี
     "\xe0\xb8\xb6", "\xe0\xb8\xb7", "\xe0\xb8\xb8"},                                    // ึ ื ุ
    {"\xe0\xb8\xb9", "\xe0\xb8\xb1", "\xe0\xb9\x87", "\xe0\xb9\x88", "\xe0\xb9\x89",   // ู ั ็ ่ ้
     "\xe0\xb9\x8a", "\xe0\xb9\x8b", "\xe0\xb9\x8c", "\xe0\xb9\x8d", "\xe0\xb8\xba",   // ๊ ๋ ์ ํ ฺ
     "\xe0\xb9\x86", "\xe0\xb8\xaf", "\xe0\xb9\x85"},                                    // ๆ ฯ ๅ
    {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "+"},
    {"!", "?", "(", ")", "\"", ":", ";", "@", "#", "_"},
    {}
};

// --- Thai layout: Kedmanee (standard Thai computer keyboard) ---
// Page 1: Unshifted Kedmanee (number row, QWERTY Q/A/Z rows)
// Page 2 (shift): Shifted Kedmanee

const char* const KeyboardEntryActivity::thaiKeysKed[NUM_ROWS][KEYS_PER_ROW] = {
    // Number row: _ ๅ / - ภ ถ ุ ึ ค ต จ ข ช
    {"_", "\xe0\xb9\x85", "/", "-", "\xe0\xb8\xa0", "\xe0\xb8\x96", "\xe0\xb8\xb8",
     "\xe0\xb8\xb6", "\xe0\xb8\x84", "\xe0\xb8\x95", "\xe0\xb8\x88", "\xe0\xb8\x82",
     "\xe0\xb8\x8a"},
    // Q row: ๆ ไ ำ พ ะ ั ี ร น ย บ ล ฃ
    {"\xe0\xb9\x86", "\xe0\xb9\x84", "\xe0\xb8\xb3", "\xe0\xb8\x9e", "\xe0\xb8\xb0",
     "\xe0\xb8\xb1", "\xe0\xb8\xb5", "\xe0\xb8\xa3", "\xe0\xb8\x99", "\xe0\xb8\xa2",
     "\xe0\xb8\x9a", "\xe0\xb8\xa5", "\xe0\xb8\x83"},
    // A row: ฟ ห ก ด เ ้ ่ า ส ว ง
    {"\xe0\xb8\x9f", "\xe0\xb8\xab", "\xe0\xb8\x81", "\xe0\xb8\x94", "\xe0\xb9\x80",
     "\xe0\xb9\x89", "\xe0\xb9\x88", "\xe0\xb8\xb2", "\xe0\xb8\xaa", "\xe0\xb8\xa7",
     "\xe0\xb8\x87"},
    // Z row: ผ ป แ อ ิ ื ท ม ใ ฝ
    {"\xe0\xb8\x9c", "\xe0\xb8\x9b", "\xe0\xb9\x81", "\xe0\xb8\xad", "\xe0\xb8\xb4",
     "\xe0\xb8\xb7", "\xe0\xb8\x97", "\xe0\xb8\xa1", "\xe0\xb9\x83", "\xe0\xb8\x9d"},
    {}
};

const char* const KeyboardEntryActivity::thaiKeysKedShift[NUM_ROWS][KEYS_PER_ROW] = {
    // Shift number row: % + ๑ ๒ ๓ ๔ ู ฿ ๕ ๖ ๗ ๘ ๙
    {"%", "+", "\xe0\xb9\x91", "\xe0\xb9\x92", "\xe0\xb9\x93", "\xe0\xb9\x94",
     "\xe0\xb8\xb9", "\xe0\xb8\xbf", "\xe0\xb9\x95", "\xe0\xb9\x96", "\xe0\xb9\x97",
     "\xe0\xb9\x98", "\xe0\xb9\x99"},
    // Shift Q row: ๐ " ฎ ฑ ธ ํ ๊ ณ ฯ ญ ฐ , ฅ
    {"\xe0\xb9\x90", "\"", "\xe0\xb8\x8e", "\xe0\xb8\x91", "\xe0\xb8\x98",
     "\xe0\xb9\x8d", "\xe0\xb9\x8a", "\xe0\xb8\x93", "\xe0\xb8\xaf", "\xe0\xb8\x8d",
     "\xe0\xb8\x90", ",", "\xe0\xb8\x85"},
    // Shift A row: ฤ ฆ ฏ โ ฌ ็ ๋ ษ ศ ซ .
    {"\xe0\xb8\xa4", "\xe0\xb8\x86", "\xe0\xb8\x8f", "\xe0\xb9\x82", "\xe0\xb8\x8c",
     "\xe0\xb9\x87", "\xe0\xb9\x8b", "\xe0\xb8\xa9", "\xe0\xb8\xa8", "\xe0\xb8\x8b",
     "."},
    // Shift Z row: ( ) ฉ ฮ ฺ ์ ? ฒ ฬ ฦ
    {"(", ")", "\xe0\xb8\x89", "\xe0\xb8\xae", "\xe0\xb8\xba", "\xe0\xb9\x8c",
     "?", "\xe0\xb8\x92", "\xe0\xb8\xac", "\xe0\xb8\xa6"},
    {}
};

// --- Thai layout: Frequency-ordered (optimized for 5-button navigation) ---
// Page 1: Most common consonants + vowels + tone marks
// Page 2 (shift): Rare consonants + specials + numbers

const char* const KeyboardEntryActivity::thaiKeysFreq[NUM_ROWS][KEYS_PER_ROW] = {
    // Row 0: 13 most frequent consonants
    {"\xe0\xb8\x99", "\xe0\xb8\x81", "\xe0\xb8\xa1", "\xe0\xb8\xa3", "\xe0\xb8\x97",   // น ก ม ร ท
     "\xe0\xb8\xa5", "\xe0\xb8\x84", "\xe0\xb8\xa7", "\xe0\xb8\xa2", "\xe0\xb8\x9b",   // ล ค ว ย ป
     "\xe0\xb8\x88", "\xe0\xb8\x94", "\xe0\xb8\xad"},                                    // จ ด อ
    // Row 1: 8 consonants + 5 leading vowels
    {"\xe0\xb8\x8a", "\xe0\xb8\xaa", "\xe0\xb8\x9e", "\xe0\xb8\x95", "\xe0\xb8\xab",   // ช ส พ ต ห
     "\xe0\xb8\x87", "\xe0\xb8\x9a", "\xe0\xb8\x82", "\xe0\xb9\x80", "\xe0\xb9\x81",   // ง บ ข เ แ
     "\xe0\xb9\x82", "\xe0\xb9\x83", "\xe0\xb9\x84"},                                    // โ ใ ไ
    // Row 2: Vowels
    {"\xe0\xb8\xb0", "\xe0\xb8\xb2", "\xe0\xb8\xb3", "\xe0\xb8\xb4", "\xe0\xb8\xb5",   // ะ า ำ ิ ี
     "\xe0\xb8\xb6", "\xe0\xb8\xb7", "\xe0\xb8\xb8", "\xe0\xb8\xb9", "\xe0\xb8\xb1",   // ึ ื ุ ู ั
     "\xe0\xb9\x87"},                                                                      // ็
    // Row 3: Tone marks + punctuation
    {"\xe0\xb9\x88", "\xe0\xb9\x89", "\xe0\xb9\x8a", "\xe0\xb9\x8b", "\xe0\xb9\x8c",   // ่ ้ ๊ ๋ ์
     "\xe0\xb9\x86", "\xe0\xb8\xaf", ".", "-", ","},                                      // ๆ ฯ . - ,
    {}
};

const char* const KeyboardEntryActivity::thaiKeysFreqShift[NUM_ROWS][KEYS_PER_ROW] = {
    // Row 0: Less frequent consonants
    {"\xe0\xb8\xa8", "\xe0\xb8\x8b", "\xe0\xb8\xa0", "\xe0\xb8\x98", "\xe0\xb8\x96",   // ศ ซ ภ ธ ถ
     "\xe0\xb8\x9c", "\xe0\xb8\x9f", "\xe0\xb8\x8d", "\xe0\xb8\x93", "\xe0\xb8\x90",   // ผ ฟ ญ ณ ฐ
     "\xe0\xb8\x86", "\xe0\xb8\x8c", "\xe0\xb8\x8e"},                                    // ฆ ฌ ฎ
    // Row 1: Rare consonants + specials
    {"\xe0\xb8\x8f", "\xe0\xb8\x91", "\xe0\xb8\x92", "\xe0\xb8\x83", "\xe0\xb8\x85",   // ฏ ฑ ฒ ฃ ฅ
     "\xe0\xb8\xac", "\xe0\xb8\xae", "\xe0\xb8\xa4", "\xe0\xb8\xa6", "\xe0\xb9\x85",   // ฬ ฮ ฤ ฦ ๅ
     "\xe0\xb8\xba", "\xe0\xb9\x8d", "/"},                                                // ฺ ํ /
    // Row 2: Numbers
    {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "+"},
    // Row 3: Symbols
    {"!", "?", "(", ")", "\"", ":", ";", "@", "#", "_"},
    {}
};

const int KeyboardEntryActivity::thaiRowLengths[NUM_ROWS] = {13, 13, 11, 10, 11};

const char* const KeyboardEntryActivity::shiftString[3] = {"shift", "SHIFT", "LOCK"};

// --- Lifecycle ---

void KeyboardEntryActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void KeyboardEntryActivity::onExit() { Activity::onExit(); }

// --- Key access ---

const char* KeyboardEntryActivity::getKeyAt(const int row, const int col) const {
  if (row < 0 || row >= NUM_ROWS || col < 0 || col >= KEYS_PER_ROW) return nullptr;

  if (layout == KeyboardLayout::Thai) {
    return shiftState ? thaiKeysShift[row][col] : thaiKeys[row][col];
  }
  return shiftState ? englishKeysShift[row][col] : englishKeys[row][col];
}

int KeyboardEntryActivity::getRowLength(const int row) const {
  if (row < 0 || row >= NUM_ROWS) return 0;
  return layout == KeyboardLayout::Thai ? thaiRowLengths[row] : englishRowLengths[row];
}

// --- Text manipulation ---

void KeyboardEntryActivity::appendKey(const char* key) {
  if (!key || key[0] == '\0') return;
  if (maxLength > 0 && text.length() >= maxLength) return;
  text += key;
}

void KeyboardEntryActivity::backspaceUtf8() {
  if (text.empty()) return;
  // Walk backward from end to find start of last UTF-8 sequence.
  // Continuation bytes: 10xxxxxx (0x80-0xBF)
  size_t pos = text.size() - 1;
  while (pos > 0 && (static_cast<uint8_t>(text[pos]) & 0xC0) == 0x80) {
    --pos;
  }
  text.erase(pos);
}

// --- Input handling ---

bool KeyboardEntryActivity::handleKeyPress() {
  if (selectedRow == SPECIAL_ROW) {
    if (selectedCol >= SHIFT_COL && selectedCol < SPACE_COL) {
      shiftState = (shiftState + 1) % 3;
      return true;
    }
    if (selectedCol >= SPACE_COL && selectedCol < BACKSPACE_COL) {
      appendKey(" ");
      return true;
    }
    if (selectedCol >= BACKSPACE_COL && selectedCol < DONE_COL) {
      backspaceUtf8();
      return true;
    }
    if (selectedCol >= DONE_COL) {
      onComplete(text);
      return false;
    }
  }

  const char* key = getKeyAt(selectedRow, selectedCol);
  if (!key || key[0] == '\0') return true;

  appendKey(key);
  if (shiftState == 1) {
    shiftState = 0;
  }
  return true;
}

// --- Navigation ---

void KeyboardEntryActivity::loop() {
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this] {
    selectedRow = ButtonNavigator::previousIndex(selectedRow, NUM_ROWS);
    const int maxCol = getRowLength(selectedRow) - 1;
    if (selectedCol > maxCol) selectedCol = maxCol;
    requestUpdate();
  });

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [this] {
    selectedRow = ButtonNavigator::nextIndex(selectedRow, NUM_ROWS);
    const int maxCol = getRowLength(selectedRow) - 1;
    if (selectedCol > maxCol) selectedCol = maxCol;
    requestUpdate();
  });

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [this] {
    const int maxCol = getRowLength(selectedRow) - 1;
    if (selectedRow == SPECIAL_ROW) {
      if (selectedCol >= SHIFT_COL && selectedCol < SPACE_COL) {
        selectedCol = maxCol;
      } else if (selectedCol >= SPACE_COL && selectedCol < BACKSPACE_COL) {
        selectedCol = SHIFT_COL;
      } else if (selectedCol >= BACKSPACE_COL && selectedCol < DONE_COL) {
        selectedCol = SPACE_COL;
      } else if (selectedCol >= DONE_COL) {
        selectedCol = BACKSPACE_COL;
      }
    } else {
      selectedCol = ButtonNavigator::previousIndex(selectedCol, maxCol + 1);
    }
    requestUpdate();
  });

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [this] {
    const int maxCol = getRowLength(selectedRow) - 1;
    if (selectedRow == SPECIAL_ROW) {
      if (selectedCol >= SHIFT_COL && selectedCol < SPACE_COL) {
        selectedCol = SPACE_COL;
      } else if (selectedCol >= SPACE_COL && selectedCol < BACKSPACE_COL) {
        selectedCol = BACKSPACE_COL;
      } else if (selectedCol >= BACKSPACE_COL && selectedCol < DONE_COL) {
        selectedCol = DONE_COL;
      } else if (selectedCol >= DONE_COL) {
        selectedCol = SHIFT_COL;
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

// --- Rendering ---

void KeyboardEntryActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title.c_str());

  // Draw input field
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int inputStartY =
      metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + metrics.verticalSpacing * 4;
  int inputHeight = 0;

  std::string displayText;
  if (isPassword) {
    displayText = std::string(text.length(), '*');
  } else {
    displayText = text;
  }
  displayText += "_";

  // Render input text across multiple lines
  int lineStartIdx = 0;
  int lineEndIdx = displayText.length();
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
      lineEndIdx = displayText.length();
    } else {
      lineEndIdx -= 1;
    }
  }

  GUI.drawTextField(renderer, Rect{0, inputStartY, pageWidth, inputHeight}, textWidth);

  // Draw keyboard
  const int keyboardStartY = metrics.keyboardBottomAligned
                                 ? pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing -
                                       (metrics.keyboardKeyHeight + metrics.keyboardKeySpacing) * NUM_ROWS
                                 : inputStartY + inputHeight + metrics.verticalSpacing * 4;
  const int keyWidth = metrics.keyboardKeyWidth;
  const int keyHeight = metrics.keyboardKeyHeight;
  const int keySpacing = metrics.keyboardKeySpacing;

  const int maxRowWidth = KEYS_PER_ROW * (keyWidth + keySpacing);
  const int leftMargin = (pageWidth - maxRowWidth) / 2;

  for (int row = 0; row < NUM_ROWS; row++) {
    const int rowY = keyboardStartY + row * (keyHeight + keySpacing);
    const int startX = leftMargin;

    if (row == SPECIAL_ROW) {
      int currentX = startX;

      // SHIFT key
      const bool shiftSelected = (selectedRow == SPECIAL_ROW && selectedCol >= SHIFT_COL && selectedCol < SPACE_COL);
      const int shiftWidth = SPACE_COL - SHIFT_COL;
      const int shiftXWidth = shiftWidth * (keyWidth + keySpacing);
      GUI.drawKeyboardKey(renderer, Rect{currentX, rowY, shiftXWidth, keyHeight}, shiftString[shiftState],
                          shiftSelected);
      currentX += shiftXWidth;

      // Space bar
      const bool spaceSelected =
          (selectedRow == SPECIAL_ROW && selectedCol >= SPACE_COL && selectedCol < BACKSPACE_COL);
      const int spaceWidth = BACKSPACE_COL - SPACE_COL;
      const int spaceXWidth = spaceWidth * (keyWidth + keySpacing);
      GUI.drawKeyboardKey(renderer, Rect{currentX, rowY, spaceXWidth, keyHeight}, "_____", spaceSelected);
      currentX += spaceXWidth;

      // Backspace key
      const bool bsSelected = (selectedRow == SPECIAL_ROW && selectedCol >= BACKSPACE_COL && selectedCol < DONE_COL);
      const int backspaceWidth = DONE_COL - BACKSPACE_COL;
      const int backspaceXWidth = backspaceWidth * (keyWidth + keySpacing);
      GUI.drawKeyboardKey(renderer, Rect{currentX, rowY, backspaceXWidth, keyHeight}, "<-", bsSelected);
      currentX += backspaceXWidth;

      // OK button
      const bool okSelected = (selectedRow == SPECIAL_ROW && selectedCol >= DONE_COL);
      const int okWidth = getRowLength(row) - DONE_COL;
      const int okXWidth = okWidth * (keyWidth + keySpacing);
      GUI.drawKeyboardKey(renderer, Rect{currentX, rowY, okXWidth, keyHeight}, tr(STR_OK_BUTTON), okSelected);
    } else {
      for (int col = 0; col < getRowLength(row); col++) {
        const char* keyLabel = getKeyAt(row, col);
        if (!keyLabel) continue;

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

void KeyboardEntryActivity::onComplete(std::string text) {
  setResult(KeyboardResult{std::move(text)});
  finish();
}

void KeyboardEntryActivity::onCancel() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}
