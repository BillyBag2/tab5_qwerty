// Implementation of a compact touch keyboard using M5Unified/M5GFX

#include "keyboard.h"
#include <M5Unified.h>
#include <ctype.h>

// Public input buffer owned by main sketch
String g_inputText;

namespace {
  KeyCharCallback g_onChar = nullptr;
  KeyBackspaceCallback g_onBackspace = nullptr;
  KeyDoneCallback g_onDone = nullptr;
  const char* g_doneLabel = "Done";
  bool g_caps = false;   // sticky shift
  bool g_sym  = false;   // symbols mode

  enum KeyType : uint8_t { KT_CHAR, KT_BACKSPACE, KT_DONE, KT_SHIFT, KT_SYM };

  struct Key {
    const char* label;   // what to draw (nullptr for dynamic like Done)
    char ch;             // character to emit (valid when type==KT_CHAR)
    uint8_t units;       // width units for layout
    KeyType type;        // special type
    // runtime
    int16_t x, y, w, h;  // bounding box
  };

  // Layout rows for alphabet mode (numbers always visible), with Shift and Sym keys
  // Units let us make wider keys (e.g., Shift, Done, Space)
  Key row0[] = {
    {"1", '1', 1, KT_CHAR}, {"2", '2', 1, KT_CHAR}, {"3", '3', 1, KT_CHAR}, {"4", '4', 1, KT_CHAR}, {"5", '5', 1, KT_CHAR},
    {"6", '6', 1, KT_CHAR}, {"7", '7', 1, KT_CHAR}, {"8", '8', 1, KT_CHAR}, {"9", '9', 1, KT_CHAR}, {"0", '0', 1, KT_CHAR},
  };
  Key row1_alpha[] = {
    {"q", 'q', 1, KT_CHAR}, {"w", 'w', 1, KT_CHAR}, {"e", 'e', 1, KT_CHAR}, {"r", 'r', 1, KT_CHAR}, {"t", 't', 1, KT_CHAR},
    {"y", 'y', 1, KT_CHAR}, {"u", 'u', 1, KT_CHAR}, {"i", 'i', 1, KT_CHAR}, {"o", 'o', 1, KT_CHAR}, {"p", 'p', 1, KT_CHAR},
  };
  Key row2_alpha[] = {
    {"a", 'a', 1, KT_CHAR}, {"s", 's', 1, KT_CHAR}, {"d", 'd', 1, KT_CHAR}, {"f", 'f', 1, KT_CHAR}, {"g", 'g', 1, KT_CHAR},
    {"h", 'h', 1, KT_CHAR}, {"j", 'j', 1, KT_CHAR}, {"k", 'k', 1, KT_CHAR}, {"l", 'l', 1, KT_CHAR},
  };
  Key row3_alpha[] = {
    {"Shift", 0, 2, KT_SHIFT}, {"z", 'z', 1, KT_CHAR}, {"x", 'x', 1, KT_CHAR}, {"c", 'c', 1, KT_CHAR}, {"v", 'v', 1, KT_CHAR}, {"b", 'b', 1, KT_CHAR},
    {"n", 'n', 1, KT_CHAR}, {"m", 'm', 1, KT_CHAR}, {"Sym", 0, 2, KT_SYM},
  };
  // Bottom action row: Del | [space] | Done
  Key row4[] = {
    {"Del", 0,   2, KT_BACKSPACE}, {"space", ' ', 8, KT_CHAR}, {nullptr, 0, 3, KT_DONE},
  };

  // Symbols layout (numbers preserved on row0)
  Key row1_sym[] = {
    {"@", '@', 1, KT_CHAR}, {"#", '#', 1, KT_CHAR}, {"$", '$', 1, KT_CHAR}, {"%", '%', 1, KT_CHAR}, {"&", '&', 1, KT_CHAR},
    {"*", '*', 1, KT_CHAR}, {"(", '(', 1, KT_CHAR}, {")", ')', 1, KT_CHAR}, {"-", '-', 1, KT_CHAR}, {"+", '+', 1, KT_CHAR},
  };
  Key row2_sym[] = {
    {"!", '!', 1, KT_CHAR}, {"?", '?', 1, KT_CHAR}, {"'", '\'', 1, KT_CHAR}, {"\"", '"', 1, KT_CHAR}, {":", ':', 1, KT_CHAR},
    {";", ';', 1, KT_CHAR}, {"/", '/', 1, KT_CHAR}, {"\\", '\\', 1, KT_CHAR}, {",", ',', 1, KT_CHAR}, {".", '.', 1, KT_CHAR},
  };
  Key row3_sym[] = {
    {"Shift", 0, 2, KT_SHIFT}, {"_", '_', 1, KT_CHAR}, {"=", '=', 1, KT_CHAR}, {"[", '[', 1, KT_CHAR}, {"]", ']', 1, KT_CHAR},
    {"{", '{', 1, KT_CHAR}, {"}", '}', 1, KT_CHAR}, {"<", '<', 1, KT_CHAR}, {">", '>', 1, KT_CHAR}, {"Sym", 0, 2, KT_SYM},
  };

  Key* rows_alpha[] = { row0, row1_alpha, row2_alpha, row3_alpha, row4 };
  const int lens_alpha[] = {
    (int)(sizeof(row0)/sizeof(row0[0])),
    (int)(sizeof(row1_alpha)/sizeof(row1_alpha[0])),
    (int)(sizeof(row2_alpha)/sizeof(row2_alpha[0])),
    (int)(sizeof(row3_alpha)/sizeof(row3_alpha[0])),
    (int)(sizeof(row4)/sizeof(row4[0]))
  };

  Key* rows_sym[] = { row0, row1_sym, row2_sym, row3_sym, row4 };
  const int lens_sym[] = {
    (int)(sizeof(row0)/sizeof(row0[0])),
    (int)(sizeof(row1_sym)/sizeof(row1_sym[0])),
    (int)(sizeof(row2_sym)/sizeof(row2_sym[0])),
    (int)(sizeof(row3_sym)/sizeof(row3_sym[0])),
    (int)(sizeof(row4)/sizeof(row4[0]))
  };

  // Cached keyboard rect
  int16_t kb_x = 0, kb_y = 0, kb_w = 0, kb_h = 0;

  // Number of keyboard rows including the action row
  constexpr int kRowsCount = 5;

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

    Key** rows = g_sym ? rows_sym : rows_alpha;
    const int* rowLens = g_sym ? lens_sym : lens_alpha;

    int16_t rowH = usableH / kRowsCount; // even rows for simplicity

    d.setTextSize(4); // ~2x bigger labels
    d.setTextColor(0x000000, 0xCFCFCF);

    for (int r = 0; r < kRowsCount; ++r) {
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

        // Determine key colors (active highlight for toggles)
        uint16_t bg = 0xCFCFCF;
        uint16_t fg = 0x000000;
        if ((k.type == KT_SHIFT && g_caps) || (k.type == KT_SYM && g_sym)) {
          bg = 0xAAAAAA;
        }

        // Draw key
        d.fillRoundRect(k.x, k.y, k.w, k.h, 6, bg);
        d.drawRoundRect(k.x, k.y, k.w, k.h, 6, 0x666666);

        // Center label
        char tmp[2] = {0, 0};
        const char* lbl;
        if (k.type == KT_DONE) {
          lbl = (g_doneLabel ? g_doneLabel : "Done");
        } else if (k.type == KT_SYM) {
          lbl = g_sym ? "ABC" : "Sym";
        } else if (k.type == KT_CHAR && !g_sym && ((k.ch >= 'a' && k.ch <= 'z') || (k.ch >= 'A' && k.ch <= 'Z'))) {
          char out = g_caps ? (char)toupper((int)k.ch) : (char)tolower((int)k.ch);
          tmp[0] = out;
          lbl = tmp;
        } else {
          lbl = k.label;
        }
        int32_t tw = d.textWidth(lbl);
        int32_t th = d.fontHeight();
        int16_t tx = k.x + (k.w - tw) / 2;
        int16_t ty = k.y + (k.h - th) / 2;
        d.setCursor(tx, ty);
        d.setTextColor(fg, bg);
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
    Key** rows = g_sym ? rows_sym : rows_alpha;
    const int* rowLens = g_sym ? lens_sym : lens_alpha;
    for (int r = 0; r < kRowsCount; ++r) {
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
      // Determine which set of rows is active for hit (must match layout)
      Key** rows = g_sym ? rows_sym : rows_alpha;
      const int* rowLens = g_sym ? lens_sym : lens_alpha;
      if (r < 0 || r >= kRowsCount || i < 0 || i >= rowLens[r]) { wasTouching = touching; return; }
      Key& k = rows[r][i];
      if (k.type == KT_BACKSPACE) {
        if (g_onBackspace) g_onBackspace();
      } else if (k.type == KT_DONE) {
        if (g_onDone) g_onDone();
      } else if (k.type == KT_SHIFT) {
        g_caps = !g_caps; // sticky toggle
      } else if (k.type == KT_SYM) {
        g_sym = !g_sym;
      } else { // KT_CHAR
        char out = k.ch;
        if (!g_sym && ((out >= 'a' && out <= 'z') || (out >= 'A' && out <= 'Z'))) {
          out = g_caps ? (char)toupper((int)out) : (char)tolower((int)out);
        }
        if (g_onChar) g_onChar(out);
      }
      // Redraw full keyboard to clear pressed color and reflect mode/shift state
      keyboard_draw();
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

// Masking support for passwords
bool g_maskInput = false;
void keyboard_setMask(bool masked)
{
  g_maskInput = masked;
}
