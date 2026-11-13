// DFPlayer Mini wrapper with a non-blocking one-slot delay queue (spec 8).
// If the module or SD card is missing, every call becomes a no-op and the
// game keeps running silently.
#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

// Resets the DFPlayer and waits up to ~2.2 s for its "card online" message
// (setup only). Returns false if the module or SD card did not answer.
bool audioInit();
bool audioOk();

// Plays /mp3/000<track>.mp3 now, cutting off whatever is playing. Requests
// less than AUDIO_MIN_GAP_MS after the previous one are dropped, except the
// explosion, lose, and win tracks.
void audioPlay(uint8_t track);

// Queues one track to play `ms` from now (replaces any queued track).
void audioPlayAfter(uint8_t track, uint32_t ms);

// Cancels a queued track.
void audioCancelQueued();

// Call every loop(): fires the queued track when its time comes.
void audioUpdate();

// Changes the volume (0-30) at runtime; used by the stage 3 test.
void audioSetVolume(uint8_t volume);

#endif
