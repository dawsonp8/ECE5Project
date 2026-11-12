// All drawing: cells, cursor, status bar, screens, explosion (spec 9.2).
// Draws straight to the TFT; no frame buffer. After displayNewGame() only
// changed cells and changed status fields are redrawn.
#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <stdint.h>

void displayInit();

// ---- Board ----

// Clears the status bar and draws the whole board from the board state once.
// Resets game-over marks. cursorIdx may be NO_CELL.
void displayNewGame(uint8_t cursorIdx);

// Redraws one cell from the board state (plus the cursor if it is there).
// Matches CellChangedFn, so it can be passed straight to board functions.
void displayDrawCell(uint8_t idx);

// Moves the cursor: redraws the old cell, then draws the cursor on the new one.
// NO_CELL hides it.
void displaySetCursor(uint8_t idx);

// Enters game-over drawing: lostIdx (or NO_CELL on a timeout) gets a red
// background, and wrong flags are crossed out. Redraws the affected cells.
// Call boardRevealAllMines(displayDrawCell) as well to show the mines.
void displayGameOver(uint8_t lostIdx);

// ---- Status bar (each field redraws only when its value changes) ----

void displayStatusMines(int16_t minesLeft);

// bpm < 0 shows "HR --" (fixed-timer fallback).
void displayStatusHeart(int16_t bpm);

// Shrinking move-timer bar. Only the slice that changed is drawn. Fallback
// mode uses a different color. Call at most every 100 ms.
void displayTimerBar(uint32_t remainingMs, uint32_t totalMs, bool fallback);

// Empty timer bar (no timer running, e.g. READY).
void displayTimerBarEmpty();

// Replaces the timer bar with a banner, e.g. "BOOM". seconds >= 0 appends
// an m:ss time ("CLEARED 3:42").
void displayBanner(const __FlashStringHelper *text, int16_t seconds);

// ---- Explosion animation (non-blocking) ----

// Draws frame `frame` (0 .. EXPLOSION_FRAMES-1) of the explosion centered on
// cell idx. The last frame cleans up: it redraws the board cells and status
// bar the fireball covered, and returns true. Caller advances frames by time.
bool displayExplosionFrame(uint8_t idx, uint8_t frame);

// ---- Full screens ----

void displayTitleScreen();
void displayCalibrateScreen();
// bpm < 0 shows "--". progressPermille is 0..1000 of the calibration window.
void displayCalibrateUpdate(int16_t bpm, uint16_t progressPermille, uint8_t beats);
void displayCalFailScreen();

#endif
