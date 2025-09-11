// TAB5 Qwerty - M5Unified + M5GFX UI scaffold
// - White screen background
// - Top toolbar with centered title "TAB5 Qwerty"

#include <M5Unified.h>
#include "orientation.h"

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

// Redraw the entire UI for the current rotation
void drawUI()
{
    auto& display = M5.Display;
    display.fillScreen(0xFFFFFF); // white background
    drawToolbarWithTitle(TITLE);
}

void setup()
{
    auto cfg = M5.config();
    // Configure features as needed (keep defaults for now)
    // cfg.output_power = true;  // enable if your device needs 5V boost
    M5.begin(cfg);

    auto& display = M5.Display;
    display.setRotation(1);  // adjust orientation to your device

    // Initial draw
    drawUI();

    // Start orientation manager; adjust rotation offset if axes differ
    orientation_begin(drawUI /* callback */, 0 /* rotation_offset */);
}

void loop() {
    M5.update();
    orientation_update();
}
