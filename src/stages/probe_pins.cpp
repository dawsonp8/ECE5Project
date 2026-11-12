// Wiring probe for the screen: pio run -e probe_pins -t upload
//
// Drives one screen control pin HIGH at a time (all others LOW) for 5 s each,
// printing which one to Serial at 115200. Measure with a multimeter (black
// probe on GND) at the SCREEN's pin: the active one should read ~3.3 V and
// the others ~0 V. A pin stuck at 0 V, or the wrong pin going high, shows
// where the path through the 74HC4050 is broken.
#include <Arduino.h>

struct Probe {
  uint8_t pin;
  const char *name;
};

static const Probe PROBES[] = {
  {13, "D13 -> 4050 pin 3 -> pin 2 -> screen SCK"},
  {11, "D11 -> 4050 pin 5 -> pin 4 -> screen SDI (MOSI)"},
  {10, "D10 -> 4050 pin 7 -> pin 6 -> screen CS"},
  {9,  "D9  -> 4050 pin 9 -> pin 10 -> screen DC"},
  {8,  "D8  -> 4050 pin 11 -> pin 12 -> screen RESET"},
};
static const uint8_t N = sizeof PROBES / sizeof PROBES[0];

static uint8_t current;
static uint32_t since;

static void activate(uint8_t k) {
  for (uint8_t i = 0; i < N; i++) digitalWrite(PROBES[i].pin, LOW);
  digitalWrite(PROBES[k].pin, HIGH);
  Serial.print(F("HIGH now: "));
  Serial.println(PROBES[k].name);
}

void setup() {
  Serial.begin(115200);
  for (uint8_t i = 0; i < N; i++) pinMode(PROBES[i].pin, OUTPUT);
  Serial.println(F("Screen wiring probe: one pin HIGH for 5 s each"));
  current = 0;
  since = millis();
  activate(current);
}

void loop() {
  if (millis() - since >= 5000) {
    since = millis();
    current = (current + 1) % N;
    activate(current);
  }
}
