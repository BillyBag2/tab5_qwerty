// Orientation manager for M5Unified devices.
// Uses accelerometer to keep the display upright (no animation).
// Call orientation_begin() in setup with a redraw callback, then
// call orientation_update() frequently (e.g., in loop()).

#pragma once

#include <stdint.h>

// Callback signature to redraw the full UI when rotation changes
typedef void (*RedrawCallback)();

// rotation_offset: 0..3 to align device axes with display rotations
// If the screen appears rotated incorrectly, try 1, 2, or 3.
void orientation_begin(RedrawCallback cb, uint8_t rotation_offset);

// Poll IMU and update rotation if needed; invokes redraw callback on change
void orientation_update();

