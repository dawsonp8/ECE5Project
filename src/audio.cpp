#include "audio.h"

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

#include "config.h"

static SoftwareSerial dfSerial(PIN_DF_RX, PIN_DF_TX);
static DFRobotDFPlayerMini dfPlayer;

static bool ok;
static uint32_t lastSentAt;
static bool sentAny;

static uint8_t queuedTrack;  // 0 = nothing queued
static uint32_t queuedAt;

bool audioInit() {
  dfSerial.begin(9600);
  // isACK=false: the library then never waits for replies during play. With
  // ACK off, begin() always returns true, so check the player's startup
  // message ourselves to see whether the module and SD card are there.
  dfPlayer.begin(dfSerial, /*isACK=*/false, /*doReset=*/true);
  uint8_t type = dfPlayer.readType();
  ok = type == DFPlayerCardOnline || type == DFPlayerUSBOnline || type == DFPlayerCardUSBOnline;
  if (ok) dfPlayer.volume(DF_VOLUME);
  return ok;
}

bool audioOk() { return ok; }

static bool important(uint8_t track) {
  return track == TRK_EXPLODE || track == TRK_LOSE || track == TRK_WIN;
}

void audioPlay(uint8_t track) {
  if (!ok) return;
  uint32_t now = millis();
  if (sentAny && now - lastSentAt < AUDIO_MIN_GAP_MS && !important(track)) return;
  // Each command is 10 bytes over SoftwareSerial (~10 ms with interrupts
  // off) plus the library's 10 ms pause; acceptable per spec 9.5.
  dfPlayer.playMp3Folder(track);
  lastSentAt = now;
  sentAny = true;
}

void audioPlayAfter(uint8_t track, uint32_t ms) {
  queuedTrack = track;
  queuedAt = millis() + ms;
}

void audioCancelQueued() { queuedTrack = 0; }

void audioUpdate() {
  if (queuedTrack && (int32_t)(millis() - queuedAt) >= 0) {
    uint8_t t = queuedTrack;
    queuedTrack = 0;
    audioPlay(t);
  }
}

void audioSetVolume(uint8_t volume) {
  if (!ok) return;
  if (volume > 30) volume = 30;
  dfPlayer.volume(volume);
}
