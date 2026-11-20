#include "heart.h"

#include <Arduino.h>

#include "config.h"
#include "heart_math.h"

#if !HR_SOURCE_BLE
// PulseSensor Playground must be included in exactly one .cpp file: its header
// defines the sampling interrupt handler (Timer2, or Timer1 when the Servo
// library is not on the include path). The define is kept for older library
// versions that read it.
#define USE_ARDUINO_INTERRUPTS true
#include <PulseSensorPlayground.h>

static PulseSensorPlayground pulse;

// Ring buffer of the last BEAT_AVG_N good interbeat intervals.
static uint16_t ibis[BEAT_AVG_N];
static uint8_t ibiCount;
static uint8_t ibiHead;
static uint32_t lastGoodBeat;
#else
// Garmin watch via the ESP32 bridge: one "HR <bpm>" line per watch update
// (about 1 per second) arrives on Serial (D0). The watch already smooths its
// BPM, so the latest reading is used directly. "HR 0" means the bridge has
// lost the watch.
static char line[12];
static uint8_t lineLen;
static bool lineOverflow;
static int16_t lastBpm = -1;
static uint32_t lastReading;
#endif

// Calibration accumulates IBIs (pulse sensor) or BPM readings (watch).
static bool calibrating;
static bool calEvaluated;
static uint32_t calStart;
static uint32_t calSum;
static uint8_t calCount;
static int16_t restBpm = -1;

static bool fixedTimer;

static void calAdd(uint16_t value) {
  if (!calibrating) return;
  calSum += value;
  if (calCount < 255) calCount++;
}

#if !HR_SOURCE_BLE

void heartInit() {
  pulse.analogInput(PIN_PULSE);
  pulse.setThreshold(PULSE_THRESHOLD);
  // Never call blinkOnPulse/fadeOnPulse: pin 13 is the display's SPI clock.
  pulse.begin();
}

static void addBeat(uint16_t ibi, uint32_t now) {
  ibis[ibiHead] = ibi;
  ibiHead = (ibiHead + 1) % BEAT_AVG_N;
  if (ibiCount < BEAT_AVG_N) ibiCount++;
  lastGoodBeat = now;
  calAdd(ibi);
}

static void pollSource(uint32_t now) {
  if (pulse.sawStartOfBeat()) {
    int ibi = pulse.getInterBeatIntervalMs();
    if (ibiValid(ibi)) addBeat((uint16_t)ibi, now);
  }

  // Signal lost: forget stale beats so the next average starts fresh.
  if (ibiCount && now - lastGoodBeat >= HR_LOST_MS) {
    ibiCount = 0;
    ibiHead = 0;
  }
}

bool heartValid() {
  return ibiCount >= HR_MIN_BEATS && millis() - lastGoodBeat < HR_LOST_MS;
}

int16_t heartBpm() {
  if (!heartValid()) return -1;
  uint32_t sum = 0;
  for (uint8_t i = 0; i < ibiCount; i++) sum += ibis[i];
  return bpmFromIbi(sum / ibiCount);
}

static int16_t calRestBpm() { return bpmFromIbi(calSum / calCount); }

int16_t heartRawSignal() { return pulse.getLatestSample(); }

#else  // HR_SOURCE_BLE

void heartInit() {
  // The sketch normally starts Serial already; the bridge needs this baud.
  Serial.begin(HR_SERIAL_BAUD);
}

// Parses one complete line in `line`. Accepts exactly "HR <digits>".
static void parseLine(uint32_t now) {
  if (lineLen < 4 || line[0] != 'H' || line[1] != 'R' || line[2] != ' ') return;
  int16_t bpm = 0;
  for (uint8_t i = 3; i < lineLen; i++) {
    char c = line[i];
    if (c < '0' || c > '9' || bpm > 999) return;
    bpm = bpm * 10 + (c - '0');
  }
  if (bpm < BLE_BPM_MIN || bpm > BLE_BPM_MAX) {
    lastBpm = -1;  // "HR 0" or nonsense: signal lost
    return;
  }
  lastBpm = bpm;
  lastReading = now;
  calAdd((uint16_t)bpm);
}

static void pollSource(uint32_t now) {
  // Non-blocking: take only what has already arrived.
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n') {
      if (!lineOverflow) parseLine(now);
      lineLen = 0;
      lineOverflow = false;
    } else if (c != '\r') {
      if (lineLen < sizeof line) {
        line[lineLen++] = c;
      } else {
        lineOverflow = true;  // drop the rest of an over-long line
      }
    }
  }
}

bool heartValid() {
  return lastBpm >= 0 && millis() - lastReading < HR_LOST_MS;
}

int16_t heartBpm() { return heartValid() ? lastBpm : -1; }

static int16_t calRestBpm() { return (int16_t)((calSum + calCount / 2) / calCount); }

// No raw waveform from a watch: report the latest BPM for the plotter.
int16_t heartRawSignal() { return lastBpm < 0 ? 0 : lastBpm; }

#endif

void heartUpdate() {
  uint32_t now = millis();
  pollSource(now);

  if (calibrating && now - calStart >= CAL_MS) {
    calibrating = false;
    calEvaluated = false;
  }
}

void heartCalStart() {
  calibrating = true;
  calEvaluated = false;
  calStart = millis();
  calSum = 0;
  calCount = 0;
}

bool heartCalibrating() { return calibrating; }

uint16_t heartCalProgress() {
  if (!calibrating) return 1000;
  uint32_t t = millis() - calStart;
  return t >= CAL_MS ? 1000 : (uint16_t)(t * 1000 / CAL_MS);
}

uint8_t heartCalBeats() { return calCount; }

bool heartCalSucceeded() {
  bool ok = calCount >= HEART_CAL_MIN;
  if (ok && !calEvaluated) restBpm = calRestBpm();
  calEvaluated = true;
  return ok;
}

int16_t heartRestBpm() { return restBpm; }

void heartSetFixedTimer(bool fixed) { fixedTimer = fixed; }

bool heartUsingFallback() {
  return fixedTimer || restBpm < 0 || !heartValid();
}

uint32_t heartMoveTimeMs() {
  if (heartUsingFallback()) return FIXED_MOVE_MS;
  return (uint32_t)moveTimeMs(heartBpm(), restBpm);
}
