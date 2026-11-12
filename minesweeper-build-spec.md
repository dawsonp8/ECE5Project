# Build Spec: Heart-Rate Minesweeper (Arduino Uno)

> **For Claude Code.** Generate the complete Arduino codebase described below. Follow the module layout, the pin map, and the Uno constraints exactly. Build and verify it in the stages listed in section 11, and stop after each stage so the hardware can be tested before moving on. Where this spec says "tunable", put the value in `config.h`.

---

## 1. Project summary

A handheld Minesweeper game for ECE 5, built on an Arduino Uno. The player moves a cursor with a joystick, presses the joystick to reveal a cell, and presses a separate button to flag a cell. The twist: every move has a countdown timer, and **the time allowed shrinks as the player's heart rate rises**. Heart rate comes from an analog pulse sensor that the player rests a free finger on. If the timer runs out, the player loses with the same explosion as hitting a mine. Sound effects play through a DFPlayer Mini.

---

## 2. Hardware and pin map

| Part | Notes | Uno pins |
|---|---|---|
| ILI9341 2.8" SPI TFT, 320×240 | Hardware SPI through a 74HC4050 level shifter (3.3 V logic) | SCK **13**, MOSI **11**, MISO **12**, CS **10**, DC **9**, RST **8** |
| DFPlayer Mini + 8 Ω speaker | SoftwareSerial at 9600 baud. Powered from the Uno 5 V pin for now, 470 µF cap at the module | Uno **2** ← module TX; Uno **3** → module RX through 1 kΩ |
| Analog joystick | X/Y analog, push switch active-low | X **A0**, Y **A1**, press **4** |
| Flag push button | To GND, active-low | **5** |
| Pulse sensor (Comimark analog clone) | Mounted face-up as a finger pad | Signal **A2** |
| Power | Power bank → switch → Uno USB | |

Free pins: 6, 7, A3, A4, A5. Leave **A3 unconnected**: it is read as a floating input to seed the random number generator.

**Pin warnings for the code:**
- Pin 13 is the SPI clock. **Never** use `LED_BUILTIN`, `pinMode(13, …)`, or PulseSensor Playground's `blinkOnPulse(13)` / `fadeOnPulse(…)`. Those would break the display.
- Pins 10 to 13 belong to the screen. Do not put SoftwareSerial on them.
- Use `INPUT_PULLUP` for pins 4 and 5.

---

## 3. Libraries

Install with the Arduino Library Manager (or `arduino-cli lib install`):

| Library | Use |
|---|---|
| `Adafruit GFX Library` | Drawing primitives and text |
| `Adafruit ILI9341` | Display driver: `Adafruit_ILI9341 tft(10, 9, 8);` (hardware SPI) |
| `PulseSensor Playground` | Beat detection with interrupts |
| `DFRobotDFPlayerMini` | DFPlayer control |
| `SoftwareSerial` | Built in |

Do **not** use `tone()`, `Servo`, or any library that takes over Timer2 (see section 9).

---

## 4. File and module structure

Arduino sketch folder named `Minesweeper/` (the folder name must match the `.ino`):

```
Minesweeper/
  Minesweeper.ino   // setup() and loop() only: init modules, then call update functions
  config.h          // every pin, tunable constant, color, and track number
  board.h / .cpp    // pure game logic: cells, mine placement, counts, flood fill, win check.
                    //   NO Arduino or display includes, so it can be unit tested on a PC.
  display.h / .cpp  // all drawing: cells, cursor, status bar, screens, explosion animation
  input.h / .cpp    // joystick reading with dead zone and auto-repeat, button debouncing
  heart.h / .cpp    // PulseSensor Playground wrapper, calibration, 8-beat average, move-time formula
  audio.h / .cpp    // DFPlayer wrapper, non-blocking sound queue
  game.h / .cpp     // state machine; ties the modules together
tests/
  host/board_test.cpp        // g++ unit tests for board.cpp, run on a PC
  01_display/01_display.ino  // stage test sketches (section 11)
  02_input/02_input.ino
  03_audio/03_audio.ino
  04_heart/04_heart.ino
README.md                    // wiring table, library list, SD card setup, how to build and test
```

Module rules:
- Each module exposes a small set of plain functions (`displayInit()`, `inputUpdate()`, `heartUpdate()`, …). Classes are fine but avoid dynamic allocation (`new`, `malloc`, `String`) entirely.
- `board.cpp` only knows about cells and indices. The game layer tells `display` which cells changed.
- **PulseSensor Playground must be included in exactly one `.cpp` file** (`heart.cpp`), with `#define USE_ARDUINO_INTERRUPTS true` placed directly before `#include <PulseSensorPlayground.h>`. Including it in more than one file causes duplicate ISR link errors.

---

## 5. Board and game rules

### 5.1 Layout
- Screen in landscape: `tft.setRotation(1)` → 320×240.
- **Status bar:** top 20 px (y = 0 to 19).
- **Board:** 16 columns × 11 rows of 20×20 px cells, from y = 20 to 239. 16 × 20 = 320, 11 × 20 = 220.
- Mine count: **28** (tunable `MINE_COUNT`). Grid size and cell size are also in `config.h`, but the code may assume they fill the screen.

### 5.2 Cell storage (RAM critical)
One byte per cell, `uint8_t cells[176]`, packed as bit fields:

| Bits | Meaning |
|---|---|
| 0 to 3 | Adjacent mine count (0 to 8) |
| 4 | Mine |
| 5 | Revealed |
| 6 | Flagged |
| 7 | Scratch bit used by flood fill ("queued") |

Use `uint8_t` cell indices (`idx = row * COLS + col`). Provide inline helpers (`isMine(i)`, `isRevealed(i)`, `setFlag(i)`, …).

### 5.3 Rules
- **First reveal is always safe.** Mines are placed only after the first reveal, avoiding the chosen cell and its 8 neighbors (falls back to avoiding only the cell if the board is too small). Use `randomSeed(analogRead(A3) ^ micros())` at that moment.
- **Reveal (joystick press):** no effect on a flagged or already revealed cell. Revealing a mine is a loss. Revealing a 0 floods outward (section 9.3).
- **Chord (optional, behind `ENABLE_CHORD` in config, default on):** pressing reveal on a revealed number whose adjacent flag count equals its number reveals all unflagged neighbors.
- **Flag (flag button):** toggles a flag on an unrevealed cell. Flags count as a move for the timer.
- **Mines-left counter** = `MINE_COUNT − flags placed` (may go negative; display it as is).
- **Win:** every non-mine cell is revealed. Auto-flag remaining mines on win.
- **Loss:** revealing a mine, or the move timer reaching 0.

---

## 6. Game state machine

```
BOOT → CALIBRATE → (CAL_FAIL) → READY → PLAYING → EXPLODING → LOST
                                           │                    │
                                           └──────→ WON ────────┴──→ READY (on press)
```

| State | Enters when | Behavior | Leaves when |
|---|---|---|---|
| `BOOT` | Power on | Init display, input, audio, heart. Show title for ~1.5 s (non-blocking). | Timer done → `CALIBRATE` |
| `CALIBRATE` | After boot, or retry | Screen: "Rest a finger on the sensor". Live BPM and a 15 s progress bar. Collect beats. | 15 s elapsed: enough good beats → `READY`; else → `CAL_FAIL` |
| `CAL_FAIL` | Not enough beats | Screen: "No pulse found. Press = retry, Flag = play with fixed timer." | Press → `CALIBRATE`; flag → `READY` in fixed-timer mode |
| `READY` | New game | Draw an empty board and the status bar. Cursor at center. No timer yet. | First reveal → place mines, reveal, → `PLAYING` |
| `PLAYING` | First reveal | Cursor, reveal, flag, timer. | Mine or timeout → `EXPLODING`; win → `WON` |
| `EXPLODING` | Loss | Play track 3, run the explosion animation at the losing cell (or the cursor, on timeout), reveal all mines, mark wrong flags. | Animation done (~1.5 s) → `LOST` |
| `LOST` | After explosion | Play track 4. Banner "BOOM" or "TOO SLOW" in the status bar. | Press → `READY` (new game, keep calibration) |
| `WON` | All safe cells revealed | Play track 5. Banner "CLEARED" with the elapsed time. | Press → `READY` |

Holding **both** buttons for 2 s in `READY`, `LOST`, or `WON` reruns calibration.

All states are driven from `loop()`; no state may block with `delay()` except the one-time hardware init in `setup()`.

---

## 7. Heart-rate move timer

### 7.1 Beat detection (`heart.cpp`)
- `PulseSensorPlayground pulse;` with `analogInput(A2)`, `setThreshold(PULSE_THRESHOLD)` (tunable, start at 550), interrupts on.
- In `heartUpdate()`, when `pulse.sawStartOfBeat()` is true, read `pulse.getInterBeatIntervalMs()`.
- Reject an interbeat interval (IBI) outside **300 to 1500 ms** (40 to 200 BPM).
- Keep a ring buffer of the last **8** good IBIs. `bpm = 60000 / average(IBIs)`.
- Signal lost: no good beat for **3 s** → `heartValid() == false`.

### 7.2 Calibration
- 15 s window (`CAL_MS`). Need at least **10** good beats (`CAL_MIN_BEATS`).
- `BPM_rest` = average of all good IBIs in the window, converted to BPM.
- Store `BPM_rest` in RAM only (no EEPROM).

### 7.3 Move time formula

$$T = \operatorname{clamp}\big(10 - 0.2\,(\text{BPM} - \text{BPM}_{\text{rest}}),\ 3,\ 10\big)\ \text{seconds}$$

- Implement in integer milliseconds: `T_ms = 10000 − 200 × (bpm − bpmRest)`, clamped to `[3000, 10000]`. Constants `T_MAX_MS`, `T_MIN_MS`, `T_SLOPE_MS_PER_BPM` in config.
- A heart rate at or below rest gives the full 10 s. +35 BPM above rest reaches the 3 s floor.
- The deadline is set at the **start** of each move: `deadline = millis() + T_ms` using the current BPM when the move begins. It is reset on every reveal or flag (including flag removal). Cursor movement alone does not reset it.
- The timer starts after the first reveal (no timer in `READY`).
- Use `(long)(millis() - deadline) >= 0` style comparisons so `millis()` rollover is safe.

### 7.4 Fixed-timer fallback
Use `FIXED_MOVE_MS` (tunable, default **8000 ms**) when either:
- the player chose fixed mode after a failed calibration, or
- during play, `heartValid()` is false (finger lifted, sensor noise). Switch back to the formula automatically once a valid average returns.

The status bar shows `HR --` and a different color for the timer bar while in fallback, so the player knows.

---

## 8. Sound

SD card layout (FAT32): folder `/mp3/` with files named `0001.mp3` … `0005.mp3`. Use `playMp3Folder(n)`, which plays by file name, not by copy order.

| Track | Event |
|---|---|
| 0001 | Reveal (also chord, flood fill; one sound per action) |
| 0002 | Flag placed or removed |
| 0003 | Explosion (mine or timeout) |
| 0004 | Lose jingle, played after the explosion finishes (`EXPLOSION_SOUND_MS`, tunable ~1500 ms) |
| 0005 | Win |

`audio.cpp`:
- `SoftwareSerial dfSerial(2, 3);` (RX = 2, TX = 3), `dfSerial.begin(9600)`.
- `dfPlayer.begin(dfSerial, /*isACK=*/false, /*doReset=*/true)`. If it returns false, set `audioOk = false` and keep running silently. The game must never hang on missing audio.
- `dfPlayer.volume(DF_VOLUME)` with `DF_VOLUME = 20` (tunable; keep 18 to 22 while powered from the Uno 5 V).
- The code never reads from the DFPlayer after init (no busy-pin or reply polling).
- `audioPlay(track)` sends at once. `audioPlayAfter(track, ms)` queues one delayed track, fired by `audioUpdate()`. A new sound simply cuts off the old one.
- Rate limit: ignore a request if the previous command was sent less than 60 ms ago, except for tracks 3, 4, 5.

---

## 9. Uno constraints (must follow)

### 9.1 RAM (2 KB)
- **No frame buffer.** Draw straight to the TFT.
- All string literals in `Serial.print` / `tft.print` use `F("…")`. Constant tables go in `PROGMEM`.
- No `String`, no `new`/`malloc`, no recursion.
- Target: global variables under **1300 bytes** as reported by the compiler, leaving room for the stack. Report the number after each compile.

### 9.2 Partial redraws
- Draw the full board once per game. After that, redraw only cells whose state changed, plus the old and new cursor cells.
- Cursor = a 2 px colored rectangle inside the cell. Moving it redraws the old cell, then draws the cursor on the new one.
- Status bar: redraw a field only when its value changes. Draw text with `setTextColor(fg, bg)` so it overwrites without a separate clear.
- Timer bar: a horizontal bar that shrinks. Update it at most every 100 ms and only draw the changed slice (fill the strip that just emptied).
- Cell look: unrevealed = raised gray with a lighter top-left edge; revealed = flat darker gray; numbers 1 to 8 in classic distinct colors, size-2 text centered; flag = small red triangle on a black pole; mine = black circle with spikes.

### 9.3 Iterative flood fill
- Use an explicit stack `uint8_t stack[CELLS]` (176 bytes, can be a static local in `board.cpp`). Use bit 7 of each cell as "already queued" so no index is pushed twice, then clear bit 7 at the end.
- The fill reveals cells and records which ones changed. Report them to the display through a callback or by redrawing from a changed list. Do not redraw the whole board.

### 9.4 Non-blocking loop
- `loop()` calls `inputUpdate()`, `heartUpdate()`, `audioUpdate()`, `gameUpdate()` every pass, all timed with `millis()`.
- No `delay()` after `setup()`. Animations are frame steps advanced by time.

### 9.5 Timers and interrupts
- PulseSensor Playground on the Uno samples every 2 ms from a **Timer2** interrupt. So: no `tone()` and nothing else that uses Timer2. Timer0 (`millis`) and Timer1 are untouched.
- SoftwareSerial turns interrupts off while it sends a byte (~1 ms per byte). A 10-byte DFPlayer command can delay a few pulse samples. This is acceptable; do not try to fix it, but do not send audio commands in a tight loop either (rate limit in section 8).
- Because the DFPlayer is not read after init, SoftwareSerial receive conflicts do not matter.

---

## 10. Input

- Joystick axes read with `analogRead`. Center ~512, dead zone ±200 (tunable `JOY_DEADZONE`). The dominant axis wins, so only one direction per step (no diagonal moves).
- Check the axis directions on hardware; provide `JOY_INVERT_X`, `JOY_INVERT_Y`, `JOY_SWAP_XY` in config.
- Auto-repeat: first step at once, then repeat after 350 ms, then every 120 ms while held (tunable).
- Cursor stops at board edges (no wrap).
- Buttons: 25 ms debounce, events fire on the press edge. Expose `inputRevealPressed()`, `inputFlagPressed()`, `inputDir()`, `inputBothHeldMs()`.

---

## 11. Build and test in stages

Compile check at every stage:
```
arduino-cli compile --fqbn arduino:avr:uno Minesweeper
```
Report flash and RAM usage. Stop after each stage so the person can upload and confirm on hardware.

| Stage | What to build | How to verify |
|---|---|---|
| **0. Logic on PC** | `board.cpp` + `tests/host/board_test.cpp` | `g++ -std=c++11 -I Minesweeper tests/host/board_test.cpp Minesweeper/board.cpp -o board_test && ./board_test`. Tests: exact mine count after placement; first cell and its neighbors are mine-free; neighbor counts match a brute-force recount; flood fill on a hand-made board reveals the right set; win check; flag toggling; chord logic. Run with many seeds. |
| **1. Display** | `display.cpp` + `tests/01_display` | Draws every cell type, all 8 numbers, flag, mine, cursor, status bar, and the explosion animation in a loop. Check colors, alignment, and that no 20 px row is cut off. |
| **2. Input** | `input.cpp` + `tests/02_input` | Prints direction and button events to Serial at 115200 and moves a cursor on screen. Check dead zone, repeat feel, axis directions, debounce. |
| **3. Audio** | `audio.cpp` + `tests/03_audio` | Plays tracks 1 to 5 on a button press, prints whether init worked. Raise the volume step by step and watch for Uno resets (brown-out from shared 5 V). Note the highest safe volume. |
| **4. Heart** | `heart.cpp` + `tests/04_heart` | Prints raw signal, BPM, valid flag, and the computed `T_ms` to Serial (use Serial Plotter). Run the 15 s calibration. Tune `PULSE_THRESHOLD`. Do 20 jumping jacks and confirm T drops. Lift the finger and confirm the fallback kicks in within 3 s. |
| **5. Full game** | `game.cpp` + `Minesweeper.ino` | Play full games: win, mine loss, timeout loss, fixed-timer mode, recalibration, restart. Confirm the display never full-redraws mid-game and the game keeps running with the DFPlayer unplugged. |
| **6. Balance** | Config only | Adjust `MINE_COUNT`, `FIXED_MOVE_MS`, slope, and floor from playtests. |

Debug output: `#define DEBUG_SERIAL 1` in config enables `Serial` logging (115200) through a `DBG(x)` macro that compiles to nothing when off.

---

## 12. `config.h` contents (starting values)

```cpp
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
```

---

## 13. Definition of done

- Compiles for `arduino:avr:uno` with no warnings at default level, globals under 1300 bytes.
- Host board tests pass.
- All stage test sketches compile.
- A full game plays from power-on with no `delay()` in the game loop, no full-screen redraws mid-game, and correct sounds.
- Game works with the pulse sensor unplugged (fixed timer) and with the DFPlayer unplugged (silent).
- `README.md` covers wiring, libraries, SD card file names, build commands, and the tunables.
