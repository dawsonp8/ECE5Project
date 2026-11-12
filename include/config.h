// Every pin, tunable constant, and track number lives here (spec section 12).
#ifndef CONFIG_H
#define CONFIG_H

// Pins
#define PIN_TFT_CS 10
#define PIN_TFT_DC 9
#define PIN_TFT_RST 8
#define PIN_DF_RX 2   // Uno receives from DFPlayer TX
#define PIN_DF_TX 3   // Uno sends to DFPlayer RX (via 1k)
#define PIN_JOY_X A0
#define PIN_JOY_Y A1
#define PIN_JOY_SW 4
#define PIN_FLAG 5
#define PIN_PULSE A2
#define PIN_RANDOM_SEED A3  // leave unconnected

// Board
#define COLS 16
#define ROWS 11
#define CELL_PX 20
#define STATUS_H 20
#define MINE_COUNT 28
#define ENABLE_CHORD 1

// Heart timer
#define CAL_MS 15000UL
#define CAL_MIN_BEATS 10
#define BEAT_AVG_N 8
#define IBI_MIN_MS 300
#define IBI_MAX_MS 1500
#define HR_LOST_MS 3000UL
#define PULSE_THRESHOLD 550
#define T_MAX_MS 10000L
#define T_MIN_MS 3000L
#define T_SLOPE_MS_PER_BPM 200L
#define FIXED_MOVE_MS 8000L

// Audio
#define DF_VOLUME 20
#define TRK_REVEAL 1
#define TRK_FLAG 2
#define TRK_EXPLODE 3
#define TRK_LOSE 4
#define TRK_WIN 5
#define EXPLOSION_SOUND_MS 1500UL

// Input
#define JOY_DEADZONE 200
#define JOY_REPEAT_DELAY_MS 350
#define JOY_REPEAT_RATE_MS 120
#define BTN_DEBOUNCE_MS 25
#define JOY_INVERT_X 0
#define JOY_INVERT_Y 0
#define JOY_SWAP_XY 0

#define DEBUG_SERIAL 0

#endif
