// Stage 4 heart test: pio run -e test_heart -t upload
//
// Needs: pulse sensor + -> 5V, - -> GND, S -> A2, mounted face-up. The screen
// is used if connected. Serial at 115200.
//
// At power-on it runs the 15 s calibration (rest a finger on the sensor), then
// shows the result. After that, every 500 ms it prints BPM, validity, resting
// BPM, and the move time T_ms the game would give; the timer bar on screen
// shows T_ms (green = formula, orange = fixed-timer fallback).
//
//   Joystick press -> rerun calibration
//   Flag button    -> toggle Serial Plotter mode (raw signal, BPM, threshold
//                     every 20 ms; open Tools > Serial Plotter or any plotter)
//
// Checks (spec stage 4): tune PULSE_THRESHOLD so beats are caught reliably;
// do ~20 jumping jacks and confirm T_ms drops; lift the finger and confirm the
// fallback ("valid=0", orange bar) kicks in within 3 s.
#include <Arduino.h>

#include "board.h"
#include "config.h"
#include "display.h"
#include "heart.h"
#include "input.h"

static bool plotter;
static uint32_t lastPrint;
static bool wasCalibrating;

static void startCalibration() {
  Serial.println(F("Calibration: rest a finger on the sensor for 15 s"));
  displayCalibrateScreen();
  heartCalStart();
  wasCalibrating = true;
}

static void showResultScreen() {
  boardReset();
  displayNewGame(NO_CELL);
  displayStatusMines(0);
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("BOOT: stage 4 heart test"));
  inputInit();
  displayInit();
  heartInit();
  startCalibration();
}

void loop() {
  heartUpdate();
  inputUpdate();

  if (inputRevealPressed()) startCalibration();
  if (inputFlagPressed()) {
    plotter = !plotter;
    if (!plotter) Serial.println(F("Plotter mode off"));
  }

  uint32_t now = millis();

  if (heartCalibrating()) {
    if (now - lastPrint >= 250) {
      lastPrint = now;
      displayCalibrateUpdate(heartBpm(), heartCalProgress(), heartCalBeats());
      if (!plotter) {
        Serial.print(F("cal "));
        Serial.print(heartCalProgress() / 10);
        Serial.print(F("%  beats="));
        Serial.print(heartCalBeats());
        Serial.print(F("  bpm="));
        Serial.println(heartBpm());
      }
    }
  } else if (wasCalibrating) {
    wasCalibrating = false;
    bool ok = heartCalSucceeded();
    Serial.print(ok ? F("Calibration OK: ") : F("Calibration FAILED: "));
    Serial.print(heartCalBeats());
    Serial.print(F(" beats (need "));
    Serial.print(CAL_MIN_BEATS);
    Serial.print(F("), resting BPM = "));
    Serial.println(heartRestBpm());
    if (ok) {
      showResultScreen();
    } else {
      displayCalFailScreen();
    }
  }

  if (plotter) {
    if (now - lastPrint >= 20) {
      lastPrint = now;
      Serial.print(F("raw:"));
      Serial.print(heartRawSignal());
      Serial.print(F(",bpm:"));
      Serial.print(heartBpm() < 0 ? 0 : heartBpm());
      Serial.print(F(",threshold:"));
      Serial.println(PULSE_THRESHOLD);
    }
    return;
  }

  if (!heartCalibrating() && now - lastPrint >= 500) {
    lastPrint = now;
    uint32_t t = heartMoveTimeMs();
    displayStatusHeart(heartBpm());
    displayTimerBar(t, T_MAX_MS, heartUsingFallback());
    Serial.print(F("bpm="));
    Serial.print(heartBpm());
    Serial.print(F(" valid="));
    Serial.print(heartValid());
    Serial.print(F(" rest="));
    Serial.print(heartRestBpm());
    Serial.print(F(" T_ms="));
    Serial.print(t);
    Serial.println(heartUsingFallback() ? F(" (fallback)") : F(""));
  }
}
