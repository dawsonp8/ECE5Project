#include "heart.h"

#include <Arduino.h>

#include "config.h"
#include "heart_math.h"

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

static bool calibrating;
static bool calEvaluated;
static uint32_t calStart;
static uint32_t calSum;
static uint8_t calCount;
static int16_t restBpm = -1;

static bool fixedTimer;

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

  if (calibrating) {
    calSum += ibi;
    if (calCount < 255) calCount++;
  }
}

void heartUpdate() {
  uint32_t now = millis();

  if (pulse.sawStartOfBeat()) {
    int ibi = pulse.getInterBeatIntervalMs();
    if (ibiValid(ibi)) addBeat((uint16_t)ibi, now);
  }

  // Signal lost: forget stale beats so the next average starts fresh.
  if (ibiCount && now - lastGoodBeat >= HR_LOST_MS) {
    ibiCount = 0;
    ibiHead = 0;
  }

  if (calibrating && now - calStart >= CAL_MS) {
    calibrating = false;
    calEvaluated = false;
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
  bool ok = calCount >= CAL_MIN_BEATS;
  if (ok && !calEvaluated) restBpm = bpmFromIbi(calSum / calCount);
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

int16_t heartRawSignal() { return pulse.getLatestSample(); }
