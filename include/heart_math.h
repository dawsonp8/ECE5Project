// Pure heart-rate math (spec 7.1 and 7.3), kept separate from heart.cpp so it
// can be unit tested on the PC without the PulseSensor library.
#ifndef HEART_MATH_H
#define HEART_MATH_H

#include <stdint.h>
#include "config.h"

// An interbeat interval outside 300-1500 ms (40-200 BPM) is rejected as noise.
inline bool ibiValid(int32_t ibiMs) {
  return ibiMs >= IBI_MIN_MS && ibiMs <= IBI_MAX_MS;
}

// BPM from an average interbeat interval, rounded to the nearest beat.
inline int16_t bpmFromIbi(uint32_t avgIbiMs) {
  if (avgIbiMs == 0) return 0;
  return (int16_t)((60000UL + avgIbiMs / 2) / avgIbiMs);
}

// Move time: T = clamp(10 - 0.2 * (BPM - BPM_rest), 3, 10) seconds, in ms.
// At or below resting heart rate gives the full T_MAX_MS.
inline int32_t moveTimeMs(int16_t bpm, int16_t bpmRest) {
  int32_t t = T_MAX_MS - T_SLOPE_MS_PER_BPM * (int32_t)(bpm - bpmRest);
  if (t > T_MAX_MS) t = T_MAX_MS;
  if (t < T_MIN_MS) t = T_MIN_MS;
  return t;
}

#endif
