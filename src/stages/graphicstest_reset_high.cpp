// Helper for graphicstest: Adafruit's example has no reset pin, but our wiring
// sends D8 to the screen's RESET through the 74HC4050. Drive D8 HIGH before
// setup() runs so the screen is not held in reset by a floating pin.
#include <Arduino.h>

static struct HoldResetHigh {
  HoldResetHigh() {
    pinMode(8, OUTPUT);
    digitalWrite(8, HIGH);
  }
} holdResetHigh;
