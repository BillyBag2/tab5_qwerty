// Orientation manager implementation (M5Unified)
// Determines orientation from accelerometer and applies display rotation.
// On change, triggers a full UI redraw via callback.

#include "orientation.h"
#include <M5Unified.h>
#include <math.h>

static RedrawCallback s_redraw = nullptr;
static uint8_t s_rot_offset = 0;  // 0..3
static int8_t s_last_rotation = -1;    // last applied logical rotation (0..3)
static int8_t s_last_candidate = -1;   // last candidate orientation
static uint32_t s_change_ms = 0;       // last time candidate changed

// Determine a 0..3 logical rotation from accelerometer axes.
// Base mapping (may vary per device, offset handles differences):
//   ay > 0 => 0 (portrait)
//   ay < 0 => 2 (portrait inverted)
//   ax < 0 => 1 (landscape)
//   ax > 0 => 3 (landscape inverted)
static int8_t compute_rotation_from_accel(float ax, float ay)
{
  int8_t rot;
  if (fabsf(ax) > fabsf(ay)) {
    rot = (ax > 0) ? 3 : 1;
  } else {
    rot = (ay > 0) ? 0 : 2;
  }
  return rot;
}

void orientation_begin(RedrawCallback cb, uint8_t rotation_offset)
{
  s_redraw = cb;
  s_rot_offset = rotation_offset & 3;
  // Set baseline from current display rotation if available
  s_last_rotation = M5.Display.getRotation();
  s_last_candidate = s_last_rotation;
  s_change_ms = millis();
}

void orientation_update()
{
  // If IMU is unavailable, do nothing
  // Note: Some M5 devices lack IMU; code should compile anyway.
#if defined(ARDUINO)
  if (!M5.Imu.isEnabled()) { return; }
#endif

  float ax = 0, ay = 0, az = 0;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) {
    return; // IMU not ready / unavailable
  }

  int8_t cand = compute_rotation_from_accel(ax, ay);

  if (cand != s_last_candidate) {
    s_last_candidate = cand;
    s_change_ms = millis();
  }

  // Require stability for a short period to avoid flicker
  const uint32_t STABLE_MS = 200;
  if (cand != s_last_rotation && (millis() - s_change_ms) > STABLE_MS) {
    int8_t apply_rot = (cand + s_rot_offset) & 3;
    M5.Display.setRotation(apply_rot);
    s_last_rotation = cand;
    if (s_redraw) { s_redraw(); }
  }
}
