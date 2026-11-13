// Pulse sensor wrapper: beat detection, calibration, 8-beat average, and the
// move-time formula (spec section 7).
#ifndef HEART_H
#define HEART_H

#include <stdint.h>

void heartInit();

// Call every loop(): collects new beats, drops stale ones, runs calibration.
void heartUpdate();

// True while a recent, plausible average is available: at least HR_MIN_BEATS
// good beats, the latest less than HR_LOST_MS ago.
bool heartValid();

// Current BPM from the last BEAT_AVG_N good beats, or -1 if not valid.
int16_t heartBpm();

// ---- Calibration (spec 7.2) ----

// Starts a CAL_MS window that averages every good beat as BPM_rest.
void heartCalStart();
bool heartCalibrating();
uint16_t heartCalProgress();  // 0..1000 of the window
uint8_t heartCalBeats();      // good beats so far in this window

// After the window ends: true if at least CAL_MIN_BEATS good beats were seen.
// On success BPM_rest is updated; on failure the old value is kept.
bool heartCalSucceeded();

int16_t heartRestBpm();  // -1 until a calibration succeeds

// ---- Move timer (spec 7.3 and 7.4) ----

// Forces the fixed timer (player chose it after a failed calibration).
void heartSetFixedTimer(bool fixed);

// True when the move time comes from FIXED_MOVE_MS instead of the formula:
// fixed mode chosen, no calibration yet, or the signal is lost.
bool heartUsingFallback();

// Time allowed for the move starting now, in ms.
uint32_t heartMoveTimeMs();

// Latest raw sensor sample (0-1023), for the test sketch's plotter output.
int16_t heartRawSignal();

#endif
