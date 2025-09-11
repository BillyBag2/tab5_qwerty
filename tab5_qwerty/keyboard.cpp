// Implementation of a compact touch keyboard using M5Unified/M5GFX

#include "keyboard.h"
#include <M5Unified.h>

// Public input buffer owned by main sketch
String g_inputText;

namespace {
  KeyCharCallback g_onChar = nullptr;
  KeyBackspaceCallback g_onBackspace = nullptr;
  KeyDoneCallback g_onDone = nullptr;
  const char* g_doneLabel = "Done";

  enum KeyType : uint8_t { KT_CHAR, KT_BACKSPACE, KT_DONE };

  struct Key {
    const char* label;   // what to draw (nullptr for dynamic like Done)
    char ch;             // character to emit (valid when type==KT_CHAR)
    uint8_t units;       // width units for layout
    KeyType type;        // special type
    // runtime
    int16_t x, y, w, h;  // bounding box
  };

  // Layout rows: digits, QWERTY rows, then specials with space and backspace
  // Units let us make space/backspace wider.
  // Note: lower-case for now; passwords are case-sensitive but users can still enter lower-case.
  Key row0[] = {
    {"1", '1', 1, KT_CHAR}, {"2", '2', 1, KT_CHAR}, {"3", '3', 1, KT_CHAR}, {"4", '4', 1, KT_CHAR}, {"5", '5', 1, KT_CHAR},
    {"6", '6', 1, KT_CHAR}, {"7", '7', 1, KT_CHAR}, {"8", '8', 1, KT_CHAR}, {"9", '9', 1, KT_CHAR}, {"0", '0', 1, KT_CHAR},
  };
  Key row1[] = {
    {"q", 'q', 1, KT_CHAR}, {"w", 'w', 1, KT_CHAR}, {"e", 'e', 1, KT_CHAR}, {"r", 'r', 1, KT_CHAR}, {"t", 't', 1, KT_CHAR},
    {"y", 'y', 1, KT_CHAR}, {"u", 'u', 1, KT_CHAR}, {"i", 'i', 1, KT_CHAR}, {"o", 'o', 1, KT_CHAR}, {"p", 'p', 1, KT_CHAR},
  };
  Key row2[] = {
    {"a", 'a', 1, KT_CHAR}, {"s", 's', 1, KT_CHAR}, {"d", 'd', 1, KT_CHAR}, {"f", 'f', 1, KT_CHAR}, {"g", 'g', 1, KT_CHAR},
    {"h", 'h', 1, KT_CHAR}, {"j", 'j', 1, KT_CHAR}, {"k", 'k', 1, KT_CHAR}, {"l", 'l', 1, KT_CHAR},
  };
  Key row3[] = {
    {"z", 'z', 1, KT_CHAR}, {"x", 'x', 1, KT_CHAR}, {"c", 'c', 1, KT_CHAR}, {"v", 'v', 1, KT_CHAR}, {"b", 'b', 1, KT_CHAR},
    {"n", 'n', 1, KT_CHAR}, {"m", 'm', 1, KT_CHAR},
  };
  // Bottom action row: Del | [space] | Done
  Key row4[] = {
    {"Del", 0,   2, KT_BACKSPACE}, {"space", ' ', 8, KT_CHAR}, {nullptr, 0, 3, KT_DONE},
  };

  Key* rows[] = { row0, row1, row2, row3, row4 };
  const int rowsCount = sizeof(rows) / sizeof(rows[0]);
  const int rowLens[] = {
    (int)(sizeof(row0)/sizeof(row0[0])),
    (int)(sizeof(row1)/sizeof(row1[0])),
    (int)(sizeof(row2)/sizeof(row2[0])),
    (int)(sizeof(row3)/sizeof(row3[0])),
    (int)(sizeof(row4)/sizeof(row4[0]))
  };

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
    // Landscape target height ~ (W * 4.5 / 16) ~= H/2 for 16:9
    int16_t targetLandscape = (int32_t)W * 45 / 160;
    int16_t maxH = portrait ? (H / 4) : ((targetLandscape < (H / 2)) ? targetLandscape : (H / 2));
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
    d.startWrite();
    d.fillRect(kb_x, kb_y, kb_w, kb_h, 0xDDDDDD);
    d.drawRect(kb_x, kb_y, kb_w, kb_h, 0x999999);

    const int16_t padX = 6;
    const int16_t padY = 6;
    int16_t usableW = kb_w - padX * 2;
    int16_t usableH = kb_h - padY * 2;

    int16_t rowH = usableH / rowsCount; // even rows for simplicity

    d.setTextSize(4); // ~2x bigger labels
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
        const char* lbl = (k.type == KT_DONE ? (g_doneLabel ? g_doneLabel : "Done") : k.label);
        int32_t tw = d.textWidth(lbl);
        int32_t th = d.fontHeight();
        int16_t tx = k.x + (k.w - tw) / 2;
        int16_t ty = k.y + (k.h - th) / 2;
        d.setCursor(tx, ty);
        d.print(lbl);

        x += w + gap;
      }
    }
    d.endWrite();
    d.waitDisplay();
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

void keyboard_begin(KeyCharCallback onChar, KeyBackspaceCallback onBackspace, KeyDoneCallback onDone, const char* doneLabel)
{
  g_onChar = onChar;
  g_onBackspace = onBackspace;
  g_onDone = onDone;
  if (doneLabel && *doneLabel) { g_doneLabel = doneLabel; }
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
      if (k.type == KT_BACKSPACE) {
        if (g_onBackspace) g_onBackspace();
      } else if (k.type == KT_DONE) {
        if (g_onDone) g_onDone();
      } else { // KT_CHAR
        if (g_onChar) g_onChar(k.ch);
      }
      // Simple key feedback: invert key briefly
      auto& d = M5.Display;
      d.fillRoundRect(k.x, k.y, k.w, k.h, 6, 0xAAAAAA);
      d.drawRoundRect(k.x, k.y, k.w, k.h, 6, 0x444444);
      const char* lbl = (k.type == KT_DONE ? (g_doneLabel ? g_doneLabel : "Done") : k.label);
      int32_t tw = d.textWidth(lbl);
      int32_t th = d.fontHeight();
      int16_t tx = k.x + (k.w - tw) / 2;
      int16_t ty = k.y + (k.h - th) / 2;
      d.setCursor(tx, ty);
      d.setTextColor(0x000000, 0xAAAAAA);
      d.print(lbl);
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
