// TAB5 Qwerty - M5Unified + M5GFX UI scaffold
// - White screen background
// - Top toolbar with centered title "TAB5 Qwerty"

#include <M5Unified.h>
#include "orientation.h"
#include "keyboard.h"

static constexpr const char* TITLE = "TAB5 Qwerty";
static constexpr uint16_t TOOLBAR_H = 48;  // toolbar height in pixels (about twice)

static void drawToolbarWithTitle(const char* title)
{
    auto& display = M5.Display;

    // Toolbar background (black)
    display.fillRect(0, 0, display.width(), TOOLBAR_H, 0x000000);

    // Configure text and measure for centering
    display.setTextWrap(false);
    display.setTextSize(4); // larger text ~2x
    display.setTextColor(0xFFFFFF, 0x000000);  // white on black

    int32_t w = display.textWidth(title);
    int32_t h = display.fontHeight();
    int32_t tx = (display.width() - w) / 2;
    int32_t ty = (TOOLBAR_H - h) / 2;  // center vertically within toolbar

    display.setCursor(tx, ty);
    display.print(title);
}

// Draw only the text box (to reduce flicker during typing)
static void drawTextBoxOnly()
{
    auto& display = M5.Display;

    int16_t kb_x, kb_y, kb_w, kb_h;
    keyboard_getRect(&kb_x, &kb_y, &kb_w, &kb_h);
    const int16_t margin = 6;

    display.setTextSize(4); // ~2x bigger textbox text
    int16_t tb_h = display.fontHeight() + 12; // padding
    int16_t tb_x = margin;
    int16_t tb_y = kb_y - tb_h - margin;
    int16_t tb_w = display.width() - margin * 2;

    display.startWrite();
    // Clear region (including border) before redraw to prevent artifacts
    display.fillRect(tb_x - 2, tb_y - 2, tb_w + 4, tb_h + 4, 0xFFFFFF);
    display.fillRoundRect(tb_x, tb_y, tb_w, tb_h, 6, 0xFFFFFF);
    display.drawRoundRect(tb_x, tb_y, tb_w, tb_h, 6, 0x000000);

    extern String g_inputText;
    extern bool g_maskInput;
    String toDraw;
    if (g_maskInput) {
        toDraw.reserve(g_inputText.length());
        for (size_t i = 0; i < g_inputText.length(); ++i) toDraw += '*';
    } else {
        toDraw = g_inputText;
    }
    display.setTextColor(0x000000, 0xFFFFFF);
    display.setCursor(tb_x + 8, tb_y + (tb_h - display.fontHeight()) / 2);
    display.print(toDraw);
    display.endWrite();
    display.waitDisplay();
}

// Redraw the entire UI for the current rotation
void drawUI()
{
    auto& display = M5.Display;
    display.startWrite();
    display.fillScreen(0xFFFFFF); // white background
    drawToolbarWithTitle(TITLE);
    display.endWrite();
    display.waitDisplay();

    // Draw text box and keyboard after background (batched per region)
    drawTextBoxOnly();
    keyboard_draw();
}

void setup()
{
    auto cfg = M5.config();
    // Configure features as needed (keep defaults for now)
    // cfg.output_power = true;  // enable if your device needs 5V boost
    M5.begin(cfg);

    auto& display = M5.Display;
    display.setRotation(1);  // adjust orientation to your device

    // Init keyboard with callbacks
    keyboard_begin(
        // onChar
        [](char c){
            extern String g_inputText;
            g_inputText += c;
            // Redraw only the text box region to reduce flicker
            drawTextBoxOnly();
        },
        // onBackspace
        [](){
            extern String g_inputText;
            if (g_inputText.length() > 0) {
                g_inputText.remove(g_inputText.length() - 1);
            }
            drawTextBoxOnly();
        },
        // onDone
        [](){
            // Placeholder: could trigger submit or hide keyboard
        },
        // doneLabel
        "Done"
    );

    // Initial draw
    drawUI();

    // Start orientation manager; adjust rotation offset if axes differ
    orientation_begin(drawUI /* callback */, 0 /* rotation_offset */);
}

void loop() {
    M5.update();
    orientation_update();
    keyboard_handleTouch();
}
