// Screen diagnostic: pio run -e screen_diag -t upload
//
// Starts the ILI9341 at a slow 1 MHz SPI clock (rules out signal problems
// through the level shifter), prints the screen's status registers to Serial
// at 115200 (needs screen SDO wired to D12; all 0x00 or 0xFF means nothing is
// read back), then fills the screen red, green, blue, white in a loop.
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#include "config.h"

static Adafruit_ILI9341 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

static const uint16_t COLORS[] = {ILI9341_RED, ILI9341_GREEN, ILI9341_BLUE, ILI9341_WHITE, ILI9341_BLACK};
static const char *const NAMES[] = {"red", "green", "blue", "white", "black"};
static uint8_t k;
static uint32_t since;

static void reg(const __FlashStringHelper *name, uint8_t cmd) {
  Serial.print(name);
  Serial.print(F(": 0x"));
  Serial.println(tft.readcommand8(cmd), HEX);
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("Screen diagnostic, SPI at 1 MHz"));
  tft.begin(1000000);
  reg(F("Power mode   (expect 0x94 or 0x9C)"), ILI9341_RDMODE);
  reg(F("MADCTL       (expect 0x48)"), ILI9341_RDMADCTL);
  reg(F("Pixel format (expect 0x5)"), ILI9341_RDPIXFMT);
  reg(F("Image format (expect 0x0)"), ILI9341_RDIMGFMT);
  reg(F("Self diag    (expect 0xC0)"), ILI9341_RDSELFDIAG);
  tft.setRotation(1);
  since = millis() - 2000;
}

void loop() {
  if (millis() - since >= 2000) {
    since = millis();
    tft.fillScreen(COLORS[k]);
    tft.setTextSize(3);
    tft.setTextColor(k == 3 ? ILI9341_BLACK : ILI9341_WHITE);
    tft.setCursor(20, 100);
    tft.print(F("Screen OK: "));
    tft.print(NAMES[k]);
    Serial.print(F("Filled "));
    Serial.println(NAMES[k]);
    k = (k + 1) % (sizeof COLORS / sizeof COLORS[0]);
  }
}
