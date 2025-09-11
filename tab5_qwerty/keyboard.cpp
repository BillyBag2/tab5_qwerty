// Implementation of a compact touch keyboard using M5Unified/M5GFX

#include "keyboard.h"
#include <M5Unified.h>

// Public input buffer owned by main sketch
String g_inputText;

namespace {
  KeyCharCallback g_onChar = nullptr;
  KeyBackspaceCallback g_onBackspace = nullptr;

  struct Key {
    const char* label;   // what to draw
    char ch;             // character to emit (0 for special)
    uint8_t units;       // width units for layout
    // runtime
    int16_t x, y, w, h;  // bounding box
  };

  // Layout rows: digits, QWERTY rows, then specials with space and backspace
  // Units let us make space/backspace wider.
  // Note: lower-case for now; passwords are case-sensitive but users can still enter lower-case.
  Key row0[] = {
    {"1", '1', 1}, {"2", '2', 1}, {"3", '3', 1}, {"4", '4', 1}, {"5", '5', 1},
    {"6", '6', 1}, {"7", '7', 1}, {"8", '8', 1}, {"9", '9', 1}, {"0", '0', 1},
  };
  Key row1[] = {
    {"q", 'q', 1}, {"w", 'w', 1}, {"e", 'e', 1}, {"r", 'r', 1}, {"t", 't', 1},
    {"y", 'y', 1}, {"u", 'u', 1}, {"i", 'i', 1}, {"o", 'o', 1}, {"p", 'p', 1},
  };
  Key row2[] = {
    {"a", 'a', 1}, {"s", 's', 1}, {"d", 'd', 1}, {"f", 'f', 1}, {"g", 'g', 1},
    {"h", 'h', 1}, {"j", 'j', 1}, {"k", 'k', 1}, {"l", 'l', 1},
  };
  Key row3[] = {
    {"z", 'z', 1}, {"x", 'x', 1}, {"c", 'c', 1}, {"v", 'v', 1}, {"b", 'b', 1},
    {"n", 'n', 1}, {"m", 'm', 1}, {"⌫", 0, 2}, {"space", ' ', 5},
  };

  Key* rows[] = { row0, row1, row2, row3 };
  const int rowsCount = sizeof(rows) / sizeof(rows[0]);
  const int rowLens[] = { (int)(sizeof(row0)/sizeof(row0[0])), (int)(sizeof(row1)/sizeof(row1[0])), (int)(sizeof(row2)/sizeof(row2[0])), (int)(sizeof(row3)/sizeof(row3[0])) };

  // Cached keyboard rect
  int16_t kb_x = 0, kb_y = 0, kb_w = 0, kb_h = 0;

  // Touch state for simple edge-triggered taps
  bool wasTouching = false;

  void computeKeyboardRect()
  {
    auto& d = M5.Display;
    int16_t W = d.width();
    int16_t H = d.height();
    bool portrait = H >= W;
    int16_t maxH = portrait ? (H * 45) / 100 : (H / 2);  // portrait ~45%, landscape <= 50%
    kb_w = W;
    kb_h = maxH;
    kb_x = 0;
    kb_y = H - kb_h;
  }

  void layoutAndDrawKeys()
  {
    auto& d = M5.Display;
    computeKeyboardRect();

    // Background panel
    d.fillRect(kb_x, kb_y, kb_w, kb_h, 0xDDDDDD);
    d.drawRect(kb_x, kb_y, kb_w, kb_h, 0x999999);

    const int16_t padX = 6;
    const int16_t padY = 6;
    int16_t usableW = kb_w - padX * 2;
    int16_t usableH = kb_h - padY * 2;

    int16_t rowH = usableH / rowsCount; // even rows for simplicity

    d.setTextSize(2);
    d.setTextColor(0x000000, 0xCFCFCF);

    for (int r = 0; r < rowsCount; ++r) {
      int16_t y = kb_y + padY + r * rowH;
      int units = 0;
      for (int i = 0; i < rowLens[r]; ++i) units += rows[r][i].units;
      int16_t gap = 4;
      int16_t totalGap = gap * (rowLens[r] - 1);
      int16_t unitW = (usableW - totalGap) / units;
      int16_t x = kb_x + padX;

      for (int i = 0; i < rowLens[r]; ++i) {
        Key& k = rows[r][i];
        int16_t w = unitW * k.units;
        int16_t h = rowH - 4;  // small vertical padding
        int16_t keyY = y + (rowH - h) / 2;

        // Store rect for hit-testing
        k.x = x; k.y = keyY; k.w = w; k.h = h;

        // Draw key
        d.fillRoundRect(k.x, k.y, k.w, k.h, 6, 0xCFCFCF);
        d.drawRoundRect(k.x, k.y, k.w, k.h, 6, 0x666666);

        // Center label
        int32_t tw = d.textWidth(k.label);
        int32_t th = d.fontHeight();
        int16_t tx = k.x + (k.w - tw) / 2;
        int16_t ty = k.y + (k.h - th) / 2;
        d.setCursor(tx, ty);
        d.print(k.label);

        x += w + gap;
      }
    }
  }

  // Return key index pair for touch point, or (-1,-1)
  bool hitTest(int16_t px, int16_t py, int& outRow, int& outIdx)
  {
    if (px < kb_x || py < kb_y || px >= kb_x + kb_w || py >= kb_y + kb_h) return false;
    for (int r = 0; r < rowsCount; ++r) {
      for (int i = 0; i < rowLens[r]; ++i) {
        Key& k = rows[r][i];
        if (px >= k.x && px < k.x + k.w && py >= k.y && py < k.y + k.h) {
          outRow = r; outIdx = i; return true;
        }
      }
    }
    return false;
  }
}

void keyboard_begin(KeyCharCallback onChar, KeyBackspaceCallback onBackspace)
{
  g_onChar = onChar;
  g_onBackspace = onBackspace;
}

void keyboard_draw()
{
  layoutAndDrawKeys();
}

void keyboard_handleTouch()
{
  int32_t x, y;
  bool touching = M5.Display.getTouch(&x, &y);

  // Detect rising edge (tap)
  if (touching && !wasTouching) {
    int r, i;
    if (hitTest((int16_t)x, (int16_t)y, r, i)) {
      Key& k = rows[r][i];
      if (k.ch == 0) {
        // Special key (currently only backspace)
        if (g_onBackspace) g_onBackspace();
      } else {
        if (g_onChar) g_onChar(k.ch);
      }
      // Simple key feedback: invert key briefly
      auto& d = M5.Display;
      d.fillRoundRect(k.x, k.y, k.w, k.h, 6, 0xAAAAAA);
      d.drawRoundRect(k.x, k.y, k.w, k.h, 6, 0x444444);
      int32_t tw = d.textWidth(k.label);
      int32_t th = d.fontHeight();
      int16_t tx = k.x + (k.w - tw) / 2;
      int16_t ty = k.y + (k.h - th) / 2;
      d.setCursor(tx, ty);
      d.setTextColor(0x000000, 0xAAAAAA);
      d.print(k.label);
    }
  }

  wasTouching = touching;
}

void keyboard_getRect(int16_t* x, int16_t* y, int16_t* w, int16_t* h)
{
  computeKeyboardRect();
  if (x) *x = kb_x;
  if (y) *y = kb_y;
  if (w) *w = kb_w;
  if (h) *h = kb_h;
}

