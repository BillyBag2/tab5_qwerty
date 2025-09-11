// Simple on-screen keyboard for M5Unified/M5GFX
// - Compact full QWERTY layout at bottom
// - Portrait: mobile-style; Landscape: max half screen height
// - Touch input -> character callback; supports backspace

#pragma once

#include <Arduino.h>

// Callbacks for key input
typedef void (*KeyCharCallback)(char c);
typedef void (*KeyBackspaceCallback)();

void keyboard_begin(KeyCharCallback onChar, KeyBackspaceCallback onBackspace);
void keyboard_draw();
void keyboard_handleTouch();

// Returns keyboard rectangle in display coordinates
void keyboard_getRect(int16_t* x, int16_t* y, int16_t* w, int16_t* h);

