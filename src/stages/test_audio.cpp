// Stage 3 audio test: pio run -e test_audio -t upload
//
// Needs: DFPlayer wired (TX->D2, RX via 1k->D3, 5V, GND, 470 uF cap), speaker on
// SPK_1/SPK_2, and a FAT32 microSD card with /mp3/0001.mp3 ... 0005.mp3.
// Serial at 115200 prints every action. The screen is not used.
//
//   Flag button     -> play the next track (1, 2, 3, 4, 5, 1, ...)
//   Joystick press  -> replay the current track
//   Joystick up     -> volume +1        Joystick down -> volume -1
//   Joystick right  -> explosion (3), then lose jingle (4) after EXPLOSION_SOUND_MS
//   Joystick left   -> five reveal sounds in a row (rate limit: only the first plays)
//
// Brown-out check: raise the volume step by step while playing tracks 3 and 5.
// If the Uno resets, a new "BOOT" line appears in Serial: note the volume just
// before it and set DF_VOLUME in config.h below that.
#include <Arduino.h>

#include "audio.h"
#include "config.h"
#include "input.h"

static uint8_t track = 0;
static uint8_t volume = DF_VOLUME;

static const char *const NAMES[] = {"", "reveal", "flag", "explosion", "lose", "win"};

static void play(uint8_t t) {
  Serial.print(F("Play track "));
  Serial.print(t);
  Serial.print(F(" ("));
  Serial.print(NAMES[t]);
  Serial.println(F(")"));
  audioPlay(t);
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("BOOT: stage 3 audio test"));
  inputInit();

  Serial.println(F("Starting DFPlayer (takes ~2 s)..."));
  uint32_t t0 = millis();
  bool ok = audioInit();
  Serial.print(ok ? F("DFPlayer OK, SD card online") : F("DFPlayer NOT found (check wiring and SD card)"));
  Serial.print(F(" after "));
  Serial.print(millis() - t0);
  Serial.println(F(" ms"));
  Serial.print(F("Volume "));
  Serial.println(volume);
}

void loop() {
  inputUpdate();
  audioUpdate();

  if (inputFlagPressed()) {
    track = track % 5 + 1;
    play(track);
  }
  if (inputRevealPressed() && track) play(track);

  switch (inputDir()) {
    case DIR_UP:
    case DIR_DOWN: {
      bool up = inputDir() == DIR_UP;
      if (up && volume < 30) volume++;
      if (!up && volume > 0) volume--;
      audioSetVolume(volume);
      Serial.print(F("Volume "));
      Serial.println(volume);
      break;
    }
    case DIR_RIGHT:
      play(TRK_EXPLODE);
      audioPlayAfter(TRK_LOSE, EXPLOSION_SOUND_MS);
      Serial.println(F("  lose jingle queued"));
      break;
    case DIR_LEFT:
      Serial.println(F("Five reveal requests in a row (expect one sound)"));
      for (uint8_t i = 0; i < 5; i++) audioPlay(TRK_REVEAL);
      break;
    default:
      break;
  }
}
