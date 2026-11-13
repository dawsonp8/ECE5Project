#include "input.h"

#include <Arduino.h>

#include "config.h"

#define JOY_SAMPLE_MS 10

struct Button {
  uint8_t pin;
  bool rawDown;       // last raw reading
  bool down;          // debounced state
  uint32_t changedAt; // when rawDown last changed
  bool pressed;       // press edge this update
};

static Button reveal = {PIN_JOY_SW, false, false, 0, false};
static Button flag = {PIN_FLAG, false, false, 0, false};

static int16_t centerX = 512, centerY = 512;
static int16_t rawX = 512, rawY = 512;
static uint32_t lastSample;

static Dir heldDir = DIR_NONE;
static Dir stepDir = DIR_NONE;
static uint32_t nextRepeat;

static uint32_t bothSince;
static bool bothDown;

static void readAxes(int16_t &x, int16_t &y) {
  int16_t a = analogRead(PIN_JOY_X);
  int16_t b = analogRead(PIN_JOY_Y);
#if JOY_SWAP_XY
  int16_t t = a; a = b; b = t;
#endif
#if JOY_INVERT_X
  a = 1023 - a;
#endif
#if JOY_INVERT_Y
  b = 1023 - b;
#endif
  x = a;
  y = b;
}

static void updateButton(Button &b, uint32_t now) {
  b.pressed = false;
  bool raw = digitalRead(b.pin) == LOW;  // active-low with pull-up
  if (raw != b.rawDown) {
    b.rawDown = raw;
    b.changedAt = now;
  }
  if (raw != b.down && now - b.changedAt >= BTN_DEBOUNCE_MS) {
    b.down = raw;
    if (raw) b.pressed = true;
  }
}

static Dir dominantDir(int16_t x, int16_t y) {
  int16_t dx = x - centerX, dy = y - centerY;
  int16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
  if (ax <= JOY_DEADZONE && ay <= JOY_DEADZONE) return DIR_NONE;
  if (ax >= ay) return dx > 0 ? DIR_RIGHT : DIR_LEFT;
  return dy > 0 ? DIR_DOWN : DIR_UP;
}

void inputInit() {
  pinMode(PIN_JOY_SW, INPUT_PULLUP);
  pinMode(PIN_FLAG, INPUT_PULLUP);

  int32_t sx = 0, sy = 0;
  for (uint8_t i = 0; i < 8; i++) {
    int16_t x, y;
    readAxes(x, y);
    sx += x;
    sy += y;
  }
  centerX = sx / 8;
  centerY = sy / 8;
  // If the stick was held at power-on, fall back to the nominal center.
  if (centerX < 312 || centerX > 712) centerX = 512;
  if (centerY < 312 || centerY > 712) centerY = 512;
  rawX = centerX;
  rawY = centerY;
}

void inputUpdate() {
  uint32_t now = millis();
  stepDir = DIR_NONE;

  updateButton(reveal, now);
  updateButton(flag, now);

  bool both = reveal.down && flag.down;
  if (both && !bothDown) bothSince = now;
  bothDown = both;

  if (now - lastSample < JOY_SAMPLE_MS) return;
  lastSample = now;

  readAxes(rawX, rawY);
  Dir d = dominantDir(rawX, rawY);

  if (d != heldDir) {
    heldDir = d;
    if (d != DIR_NONE) {
      stepDir = d;  // first step at once
      nextRepeat = now + JOY_REPEAT_DELAY_MS;
    }
  } else if (d != DIR_NONE && (int32_t)(now - nextRepeat) >= 0) {
    stepDir = d;
    nextRepeat = now + JOY_REPEAT_RATE_MS;
  }
}

bool inputRevealPressed() { return reveal.pressed; }
bool inputFlagPressed() { return flag.pressed; }
Dir inputDir() { return stepDir; }

uint32_t inputBothHeldMs() {
  return bothDown ? millis() - bothSince : 0;
}

int16_t inputRawX() { return rawX; }
int16_t inputRawY() { return rawY; }
int16_t inputCenterX() { return centerX; }
int16_t inputCenterY() { return centerY; }
