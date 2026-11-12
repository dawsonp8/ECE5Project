// Pure Minesweeper game logic (spec sections 5 and 9.3).
// No Arduino or display includes, so this compiles and is unit tested on a PC.
#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>
#include "config.h"

#define CELLS (COLS * ROWS)
#define NO_CELL 0xFF

#if CELLS > 255
#error "Cell indices are uint8_t; the board must have at most 255 cells"
#endif

// One byte per cell (spec 5.2).
#define CELL_COUNT_MASK 0x0F  // bits 0-3: adjacent mine count
#define CELL_MINE       0x10  // bit 4
#define CELL_REVEALED   0x20  // bit 5
#define CELL_FLAGGED    0x40  // bit 6
#define CELL_QUEUED     0x80  // bit 7: flood-fill scratch

extern uint8_t cells[CELLS];

inline uint8_t cellCount(uint8_t i) { return cells[i] & CELL_COUNT_MASK; }
inline bool isMine(uint8_t i) { return cells[i] & CELL_MINE; }
inline bool isRevealed(uint8_t i) { return cells[i] & CELL_REVEALED; }
inline bool isFlagged(uint8_t i) { return cells[i] & CELL_FLAGGED; }
inline uint8_t cellIndex(uint8_t row, uint8_t col) { return row * COLS + col; }
inline uint8_t cellRow(uint8_t i) { return i / COLS; }
inline uint8_t cellCol(uint8_t i) { return i % COLS; }

// Called once for every cell whose visible state changed, so the display can
// redraw just that cell. May be null.
typedef void (*CellChangedFn)(uint8_t idx);

enum RevealResult : uint8_t {
  REVEAL_NONE,  // nothing changed
  REVEAL_OK,    // one or more safe cells revealed
  REVEAL_MINE   // a mine was revealed (loss)
};

// Clears every cell (no mines, nothing revealed or flagged).
void boardReset();

// Places mineCount mines, avoiding safeIdx and its 8 neighbors when there is
// room (otherwise only safeIdx), then computes adjacent counts. The seed drives
// a deterministic PRNG so tests are repeatable.
void boardPlaceMines(uint8_t safeIdx, uint8_t mineCount, uint32_t seed);

// Recomputes every adjacent count from the mine bits (for hand-built boards).
void boardRecount();

// Reveals idx. Does nothing on a flagged or revealed cell. A 0 cell floods
// outward. On a mine, *lostIdx (if non-null) is set to the mine's index.
RevealResult boardReveal(uint8_t idx, CellChangedFn onChange, uint8_t *lostIdx);

// Chord: on a revealed number whose adjacent flag count equals its number,
// reveals all unflagged neighbors. Otherwise does nothing.
RevealResult boardChord(uint8_t idx, CellChangedFn onChange, uint8_t *lostIdx);

// Toggles a flag on an unrevealed cell. Returns true if the cell changed.
bool boardToggleFlag(uint8_t idx);

uint8_t boardFlagCount();
uint8_t boardMineCount();

// Mines left = mines - flags; may go negative.
int16_t boardMinesLeft();

// True when every non-mine cell is revealed.
bool boardIsWon();

// On a loss: reveals every mine. Wrong flags stay flagged; the display can
// spot them with isFlagged(i) && !isMine(i).
void boardRevealAllMines(CellChangedFn onChange);

// On a win: flags every mine that is not yet flagged.
void boardFlagAllMines(CellChangedFn onChange);

#endif
