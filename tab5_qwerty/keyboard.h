// Simple on-screen keyboard for M5Unified/M5GFX
// - Compact full QWERTY layout at bottom
// - Portrait: mobile-style; Landscape: max half screen height
// - Touch input -> character callback; supports backspace

#pragma once

#include <Arduino.h>

// Callbacks for key input
typedef void (*KeyCharCallback)(char c);
typedef void (*KeyBackspaceCallback)();
typedef void (*KeyDoneCallback)();

// Initialize keyboard. Provide callbacks and the label for the Done button (e.g., "Done").
void keyboard_begin(KeyCharCallback onChar, KeyBackspaceCallback onBackspace, KeyDoneCallback onDone, const char* doneLabel);
void keyboard_draw();
void keyboard_handleTouch();

// Returns keyboard rectangle in display coordinates
void keyboard_getRect(int16_t* x, int16_t* y, int16_t* w, int16_t* h);

// Shared input buffer (rendered by the host UI)
extern String g_inputText;

// Optional: mask input for password entry (default false)
void keyboard_setMask(bool masked);
extern bool g_maskInput;
