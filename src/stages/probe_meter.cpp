// Uno-as-multimeter wiring check (debug aid): pio run -e probe_meter -t upload
//
// Put a jumper in A5 and touch its other end to a point on the circuit (or plug
// it into the same breadboard row as a screen pin). The sketch toggles each
// screen control pin (D8-D13) one at a time and measures A5, then prints a
// line whenever the verdict changes, e.g.
//   A5: follows D10 (CS)  low 0.00 V  high 3.29 V
//   A5: steady 4.98 V
// A5 must never touch anything above 5 V.
#include <Arduino.h>

struct Line {
  uint8_t pin;
  const char *name;
};

static const Line LINES[] = {
  {13, "D13 (SCK)"}, {11, "D11 (SDI/MOSI)"}, {10, "D10 (CS)"},
  {9, "D9 (DC)"},    {8, "D8 (RESET)"},
};
static const uint8_t N = sizeof LINES / sizeof LINES[0];

static uint16_t readMv() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 16; i++) sum += analogRead(A5);
  return (uint32_t)sum * 5000 / 16 / 1023;
}

static void allLow() {
  for (uint8_t i = 0; i < N; i++) digitalWrite(LINES[i].pin, LOW);
}

static void printV(uint16_t mv) {
  Serial.print(mv / 1000);
  Serial.print('.');
  uint16_t frac = (mv % 1000) / 10;
  if (frac < 10) Serial.print('0');
  Serial.print(frac);
  Serial.print(F(" V"));
}

static char lastVerdict[64];
static uint32_t lastPrint;

void setup() {
  Serial.begin(115200);
  for (uint8_t i = 0; i < N; i++) pinMode(LINES[i].pin, OUTPUT);
  allLow();
  Serial.println(F("A5 wiring meter ready"));
}

void loop() {
  allLow();
  delayMicroseconds(300);
  uint16_t base = readMv();

  int8_t follows = -1;
  uint8_t followCount = 0;
  uint16_t highMv = 0;
  for (uint8_t i = 0; i < N; i++) {
    allLow();
    digitalWrite(LINES[i].pin, HIGH);
    delayMicroseconds(300);
    uint16_t mv = readMv();
    if (mv > base + 1000) {
      followCount++;
      if (follows < 0) {
        follows = i;
        highMv = mv;
      }
    }
  }
  allLow();

  // Build a verdict; print it only when it changes (or every 3 s).
  char verdict[64];
  uint16_t b10 = base / 100;  // 0.1 V buckets so noise doesn't spam
  if (followCount == 0) {
    snprintf(verdict, sizeof verdict, "s%u", b10);
  } else {
    snprintf(verdict, sizeof verdict, "f%d,%u,%u,%u", follows, followCount, b10, highMv / 100);
  }
  if (strcmp(verdict, lastVerdict) == 0 && millis() - lastPrint < 3000) return;
  strcpy(lastVerdict, verdict);
  lastPrint = millis();

  Serial.print(F("A5: "));
  if (followCount == 0) {
    Serial.print(F("steady "));
    printV(base);
  } else {
    Serial.print(F("follows "));
    Serial.print(LINES[follows].name);
    if (followCount > 1) {
      Serial.print(F(" AND "));
      Serial.print(followCount - 1);
      Serial.print(F(" other line(s): SHORT"));
    }
    Serial.print(F("  low "));
    printV(base);
    Serial.print(F("  high "));
    printV(highMv);
  }
  Serial.println();
}
