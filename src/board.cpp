#include "board.h"

uint8_t cells[CELLS];

// Fills out[] with the indices of idx's in-bounds neighbors; returns how many.
static uint8_t neighbors(uint8_t idx, uint8_t out[8]) {
  uint8_t r = cellRow(idx), c = cellCol(idx), n = 0;
  for (int8_t dr = -1; dr <= 1; dr++) {
    for (int8_t dc = -1; dc <= 1; dc++) {
      if (dr == 0 && dc == 0) continue;
      int8_t nr = (int8_t)r + dr, nc = (int8_t)c + dc;
      if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS) continue;
      out[n++] = cellIndex((uint8_t)nr, (uint8_t)nc);
    }
  }
  return n;
}

static bool isNeighborOrSelf(uint8_t a, uint8_t b) {
  int8_t dr = (int8_t)cellRow(a) - (int8_t)cellRow(b);
  int8_t dc = (int8_t)cellCol(a) - (int8_t)cellCol(b);
  return dr >= -1 && dr <= 1 && dc >= -1 && dc <= 1;
}

// xorshift32: small, deterministic, no library dependency.
static uint32_t rngState;
static uint32_t rngNext() {
  uint32_t x = rngState;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rngState = x;
  return x;
}

void boardReset() {
  for (uint8_t i = 0; i < CELLS; i++) cells[i] = 0;
}

void boardRecount() {
  uint8_t nb[8];
  for (uint8_t i = 0; i < CELLS; i++) {
    uint8_t n = neighbors(i, nb), count = 0;
    for (uint8_t k = 0; k < n; k++) {
      if (isMine(nb[k])) count++;
    }
    cells[i] = (cells[i] & ~CELL_COUNT_MASK) | count;
  }
}

void boardPlaceMines(uint8_t safeIdx, uint8_t mineCount, uint32_t seed) {
  rngState = seed ? seed : 0x9E3779B9UL;  // xorshift must not start at 0

  uint8_t nb[8];
  uint8_t excluded = 1 + neighbors(safeIdx, nb);
  bool avoidNeighbors = (uint16_t)mineCount + excluded <= CELLS;
  if (!avoidNeighbors) excluded = 1;
  if (mineCount > CELLS - excluded) mineCount = CELLS - excluded;

  uint8_t placed = 0;
  while (placed < mineCount) {
    uint8_t i = rngNext() % CELLS;
    if (isMine(i)) continue;
    if (avoidNeighbors ? isNeighborOrSelf(i, safeIdx) : i == safeIdx) continue;
    cells[i] |= CELL_MINE;
    placed++;
  }
  boardRecount();
}

static void notify(CellChangedFn onChange, uint8_t idx) {
  if (onChange) onChange(idx);
}

RevealResult boardReveal(uint8_t idx, CellChangedFn onChange, uint8_t *lostIdx) {
  if (isFlagged(idx) || isRevealed(idx)) return REVEAL_NONE;

  if (isMine(idx)) {
    cells[idx] |= CELL_REVEALED;
    notify(onChange, idx);
    if (lostIdx) *lostIdx = idx;
    return REVEAL_MINE;
  }

  // Iterative flood fill (spec 9.3): explicit stack, bit 7 marks "queued".
  static uint8_t stack[CELLS];
  uint8_t top = 0;
  stack[top++] = idx;
  cells[idx] |= CELL_QUEUED;

  uint8_t nb[8];
  while (top > 0) {
    uint8_t i = stack[--top];
    cells[i] |= CELL_REVEALED;
    notify(onChange, i);
    if (cellCount(i) != 0) continue;
    uint8_t n = neighbors(i, nb);
    for (uint8_t k = 0; k < n; k++) {
      uint8_t j = nb[k];
      if (cells[j] & (CELL_REVEALED | CELL_FLAGGED | CELL_QUEUED | CELL_MINE)) continue;
      cells[j] |= CELL_QUEUED;
      stack[top++] = j;
    }
  }

  for (uint8_t i = 0; i < CELLS; i++) cells[i] &= ~CELL_QUEUED;
  return REVEAL_OK;
}

RevealResult boardChord(uint8_t idx, CellChangedFn onChange, uint8_t *lostIdx) {
  if (!isRevealed(idx) || cellCount(idx) == 0) return REVEAL_NONE;

  uint8_t nb[8];
  uint8_t n = neighbors(idx, nb), flags = 0;
  for (uint8_t k = 0; k < n; k++) {
    if (isFlagged(nb[k])) flags++;
  }
  if (flags != cellCount(idx)) return REVEAL_NONE;

  RevealResult result = REVEAL_NONE;
  for (uint8_t k = 0; k < n; k++) {
    uint8_t lost = NO_CELL;
    RevealResult r = boardReveal(nb[k], onChange, &lost);
    if (r == REVEAL_MINE) {
      // Report the first mine hit, but keep revealing the rest of the chord.
      if (result != REVEAL_MINE && lostIdx) *lostIdx = lost;
      result = REVEAL_MINE;
    } else if (r == REVEAL_OK && result == REVEAL_NONE) {
      result = REVEAL_OK;
    }
  }
  return result;
}

bool boardToggleFlag(uint8_t idx) {
  if (isRevealed(idx)) return false;
  cells[idx] ^= CELL_FLAGGED;
  return true;
}

uint8_t boardFlagCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < CELLS; i++) {
    if (isFlagged(i)) n++;
  }
  return n;
}

uint8_t boardMineCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < CELLS; i++) {
    if (isMine(i)) n++;
  }
  return n;
}

int16_t boardMinesLeft() {
  return (int16_t)boardMineCount() - (int16_t)boardFlagCount();
}

bool boardIsWon() {
  for (uint8_t i = 0; i < CELLS; i++) {
    if (!isMine(i) && !isRevealed(i)) return false;
  }
  return true;
}

void boardRevealAllMines(CellChangedFn onChange) {
  for (uint8_t i = 0; i < CELLS; i++) {
    if (isMine(i) && !isRevealed(i) && !isFlagged(i)) {
      cells[i] |= CELL_REVEALED;
      notify(onChange, i);
    }
  }
}

void boardFlagAllMines(CellChangedFn onChange) {
  for (uint8_t i = 0; i < CELLS; i++) {
    if (isMine(i) && !isFlagged(i)) {
      cells[i] |= CELL_FLAGGED;
      notify(onChange, i);
    }
  }
}
