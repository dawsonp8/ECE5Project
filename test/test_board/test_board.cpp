// Host unit tests for board.cpp (stage 0). Run: pio test -e native
#include <unity.h>
#include <string.h>
#include "board.h"

static const uint32_t SEED_RUNS = 1000;

// ---------- helpers ----------

static uint8_t changedCount;
static uint8_t changedTimes[CELLS];
static void recordChange(uint8_t i) {
  changedCount++;
  changedTimes[i]++;
}
static void resetChanges() {
  changedCount = 0;
  memset(changedTimes, 0, sizeof changedTimes);
}

static bool adjacent(uint8_t a, uint8_t b) {
  int dr = (int)cellRow(a) - (int)cellRow(b);
  int dc = (int)cellCol(a) - (int)cellCol(b);
  return a != b && dr >= -1 && dr <= 1 && dc >= -1 && dc <= 1;
}

// Independent brute-force recount, written differently from board.cpp.
static uint8_t bruteCount(uint8_t i) {
  uint8_t n = 0;
  for (uint8_t j = 0; j < CELLS; j++) {
    if (adjacent(i, j) && isMine(j)) n++;
  }
  return n;
}

// Reference flood fill: repeat "reveal any hidden, unflagged, non-mine
// neighbor of a revealed 0" until nothing changes.
static void referenceReveal(uint8_t start, bool out[CELLS]) {
  for (uint8_t i = 0; i < CELLS; i++) out[i] = isRevealed(i);
  out[start] = true;
  bool grew = true;
  while (grew) {
    grew = false;
    for (uint8_t i = 0; i < CELLS; i++) {
      if (!out[i] || cellCount(i) != 0 || isMine(i)) continue;
      for (uint8_t j = 0; j < CELLS; j++) {
        if (adjacent(i, j) && !out[j] && !isFlagged(j) && !isMine(j)) {
          out[j] = true;
          grew = true;
        }
      }
    }
  }
}

static void placeMinesAt(const uint8_t *idx, uint8_t n) {
  boardReset();
  for (uint8_t k = 0; k < n; k++) cells[idx[k]] |= CELL_MINE;
  boardRecount();
}

static void assertNoQueuedBits() {
  for (uint8_t i = 0; i < CELLS; i++) {
    TEST_ASSERT_EQUAL_HEX8(0, cells[i] & CELL_QUEUED);
  }
}

void setUp() { boardReset(); resetChanges(); }
void tearDown() {}

// ---------- mine placement ----------

void test_exact_mine_count_many_seeds() {
  for (uint32_t s = 1; s <= SEED_RUNS; s++) {
    boardReset();
    boardPlaceMines((uint8_t)(s % CELLS), MINE_COUNT, s);
    TEST_ASSERT_EQUAL_UINT8(MINE_COUNT, boardMineCount());
  }
}

void test_first_cell_and_neighbors_mine_free() {
  const uint8_t starts[] = {
    cellIndex(0, 0), cellIndex(0, COLS - 1), cellIndex(ROWS - 1, 0),
    cellIndex(ROWS - 1, COLS - 1), cellIndex(0, 7), cellIndex(5, 0),
    cellIndex(ROWS / 2, COLS / 2)};
  for (uint32_t s = 1; s <= SEED_RUNS; s++) {
    uint8_t safe = starts[s % sizeof starts];
    boardReset();
    boardPlaceMines(safe, MINE_COUNT, s);
    TEST_ASSERT_FALSE(isMine(safe));
    for (uint8_t j = 0; j < CELLS; j++) {
      if (adjacent(safe, j)) TEST_ASSERT_FALSE(isMine(j));
    }
  }
}

void test_crowded_board_falls_back_to_safe_cell_only() {
  // Too many mines to keep the 3x3 clear: only the chosen cell stays safe.
  uint8_t safe = cellIndex(ROWS / 2, COLS / 2);
  boardPlaceMines(safe, CELLS - 3, 42);
  TEST_ASSERT_EQUAL_UINT8(CELLS - 3, boardMineCount());
  TEST_ASSERT_FALSE(isMine(safe));
}

void test_mine_count_clamped_to_fit() {
  uint8_t safe = cellIndex(0, 0);
  boardPlaceMines(safe, 255, 7);
  TEST_ASSERT_EQUAL_UINT8(CELLS - 1, boardMineCount());
  TEST_ASSERT_FALSE(isMine(safe));
}

void test_neighbor_counts_match_brute_force() {
  for (uint32_t s = 1; s <= SEED_RUNS; s++) {
    boardReset();
    boardPlaceMines((uint8_t)((s * 37) % CELLS), MINE_COUNT, s);
    for (uint8_t i = 0; i < CELLS; i++) {
      TEST_ASSERT_EQUAL_UINT8(bruteCount(i), cellCount(i));
    }
  }
}

void test_same_seed_same_board() {
  boardPlaceMines(10, MINE_COUNT, 1234);
  uint8_t first[CELLS];
  memcpy(first, cells, CELLS);
  boardReset();
  boardPlaceMines(10, MINE_COUNT, 1234);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(first, cells, CELLS);
}

// ---------- reveal and flood fill ----------

void test_flood_fill_stops_at_mine_wall() {
  // A full column of mines at col 5: revealing (0,0) floods cols 0-4 only.
  uint8_t wall[ROWS];
  for (uint8_t r = 0; r < ROWS; r++) wall[r] = cellIndex(r, 5);
  placeMinesAt(wall, ROWS);

  TEST_ASSERT_EQUAL(REVEAL_OK, boardReveal(cellIndex(0, 0), recordChange, NULL));
  for (uint8_t i = 0; i < CELLS; i++) {
    bool expected = cellCol(i) <= 4;
    TEST_ASSERT_EQUAL_MESSAGE(expected, isRevealed(i), "wrong reveal set");
    TEST_ASSERT_EQUAL_UINT8(expected ? 1 : 0, changedTimes[i]);
  }
  TEST_ASSERT_EQUAL_UINT8(ROWS * 5, changedCount);
  assertNoQueuedBits();
}

void test_flood_fill_skips_flagged_cells() {
  boardReset();  // no mines: every cell is 0
  uint8_t flagged = cellIndex(3, 3);
  boardToggleFlag(flagged);
  boardReveal(cellIndex(0, 0), recordChange, NULL);
  for (uint8_t i = 0; i < CELLS; i++) {
    TEST_ASSERT_EQUAL(i != flagged, isRevealed(i));
  }
  TEST_ASSERT_EQUAL_UINT8(CELLS - 1, changedCount);
  TEST_ASSERT_FALSE(boardIsWon());  // the flagged cell is safe but still hidden
}

void test_reveal_number_reveals_only_that_cell() {
  const uint8_t mines[] = {cellIndex(0, 1)};
  placeMinesAt(mines, 1);
  TEST_ASSERT_EQUAL(REVEAL_OK, boardReveal(cellIndex(0, 0), recordChange, NULL));
  TEST_ASSERT_EQUAL_UINT8(1, changedCount);
  TEST_ASSERT_TRUE(isRevealed(cellIndex(0, 0)));
}

void test_flood_fill_matches_reference_many_seeds() {
  bool expected[CELLS];
  for (uint32_t s = 1; s <= SEED_RUNS; s++) {
    uint8_t safe = (uint8_t)((s * 53) % CELLS);
    boardReset();
    boardPlaceMines(safe, MINE_COUNT, s);
    referenceReveal(safe, expected);
    resetChanges();
    TEST_ASSERT_EQUAL(REVEAL_OK, boardReveal(safe, recordChange, NULL));
    uint8_t revealed = 0;
    for (uint8_t i = 0; i < CELLS; i++) {
      TEST_ASSERT_EQUAL(expected[i], isRevealed(i));
      TEST_ASSERT_EQUAL_UINT8(expected[i] ? 1 : 0, changedTimes[i]);
      if (expected[i]) revealed++;
    }
    TEST_ASSERT_EQUAL_UINT8(revealed, changedCount);
    assertNoQueuedBits();
  }
}

void test_reveal_mine_is_loss() {
  const uint8_t mines[] = {cellIndex(4, 4)};
  placeMinesAt(mines, 1);
  uint8_t lost = NO_CELL;
  TEST_ASSERT_EQUAL(REVEAL_MINE, boardReveal(cellIndex(4, 4), recordChange, &lost));
  TEST_ASSERT_EQUAL_UINT8(cellIndex(4, 4), lost);
  TEST_ASSERT_TRUE(isRevealed(cellIndex(4, 4)));
  TEST_ASSERT_EQUAL_UINT8(1, changedCount);
}

void test_reveal_ignores_flagged_and_revealed() {
  const uint8_t mines[] = {cellIndex(4, 4)};
  placeMinesAt(mines, 1);
  boardToggleFlag(cellIndex(4, 4));
  TEST_ASSERT_EQUAL(REVEAL_NONE, boardReveal(cellIndex(4, 4), recordChange, NULL));
  TEST_ASSERT_FALSE(isRevealed(cellIndex(4, 4)));

  boardReveal(cellIndex(4, 5), NULL, NULL);
  resetChanges();
  TEST_ASSERT_EQUAL(REVEAL_NONE, boardReveal(cellIndex(4, 5), recordChange, NULL));
  TEST_ASSERT_EQUAL_UINT8(0, changedCount);
}

// ---------- win check ----------

void test_win_when_all_safe_cells_revealed() {
  const uint8_t mines[] = {cellIndex(0, 0), cellIndex(10, 15)};
  placeMinesAt(mines, 2);
  TEST_ASSERT_FALSE(boardIsWon());
  for (uint8_t i = 0; i < CELLS; i++) {
    if (!isMine(i) && i != cellIndex(5, 5)) cells[i] |= CELL_REVEALED;
  }
  TEST_ASSERT_FALSE(boardIsWon());
  boardReveal(cellIndex(5, 5), NULL, NULL);
  TEST_ASSERT_TRUE(boardIsWon());
}

void test_win_ignores_flags() {
  const uint8_t mines[] = {cellIndex(0, 0)};
  placeMinesAt(mines, 1);
  boardReveal(cellIndex(10, 15), NULL, NULL);  // floods everything but the mine
  TEST_ASSERT_TRUE(boardIsWon());               // mine need not be flagged
  boardFlagAllMines(recordChange);
  TEST_ASSERT_TRUE(isFlagged(cellIndex(0, 0)));
  TEST_ASSERT_EQUAL_UINT8(1, changedCount);
}

// ---------- flags ----------

void test_flag_toggles_and_counts() {
  boardPlaceMines(0, MINE_COUNT, 99);
  uint8_t a = cellIndex(10, 15), b = cellIndex(10, 14);
  TEST_ASSERT_TRUE(boardToggleFlag(a));
  TEST_ASSERT_TRUE(isFlagged(a));
  TEST_ASSERT_TRUE(boardToggleFlag(b));
  TEST_ASSERT_EQUAL_UINT8(2, boardFlagCount());
  TEST_ASSERT_EQUAL_INT16(MINE_COUNT - 2, boardMinesLeft());
  TEST_ASSERT_TRUE(boardToggleFlag(a));
  TEST_ASSERT_FALSE(isFlagged(a));
  TEST_ASSERT_EQUAL_UINT8(1, boardFlagCount());
}

void test_cannot_flag_revealed_cell() {
  const uint8_t mines[] = {cellIndex(0, 1)};
  placeMinesAt(mines, 1);
  boardReveal(cellIndex(0, 0), NULL, NULL);
  TEST_ASSERT_FALSE(boardToggleFlag(cellIndex(0, 0)));
  TEST_ASSERT_FALSE(isFlagged(cellIndex(0, 0)));
}

void test_mines_left_can_go_negative() {
  const uint8_t mines[] = {cellIndex(0, 0)};
  placeMinesAt(mines, 1);
  boardToggleFlag(cellIndex(5, 5));
  boardToggleFlag(cellIndex(5, 6));
  boardToggleFlag(cellIndex(5, 7));
  TEST_ASSERT_EQUAL_INT16(-2, boardMinesLeft());
}

// ---------- chord ----------

// Board for chord tests: mines at (1,1) and (1,3); target (2,2) has count 2.
static const uint8_t CHORD_MINES[] = {cellIndex(1, 1), cellIndex(1, 3)};
static const uint8_t TARGET = cellIndex(2, 2);

void test_chord_reveals_unflagged_neighbors_when_flags_match() {
  placeMinesAt(CHORD_MINES, 2);
  TEST_ASSERT_EQUAL_UINT8(2, cellCount(TARGET));
  boardReveal(TARGET, NULL, NULL);
  boardToggleFlag(cellIndex(1, 1));
  boardToggleFlag(cellIndex(1, 3));
  resetChanges();
  TEST_ASSERT_EQUAL(REVEAL_OK, boardChord(TARGET, recordChange, NULL));
  for (uint8_t j = 0; j < CELLS; j++) {
    if (adjacent(TARGET, j) && !isMine(j)) TEST_ASSERT_TRUE(isRevealed(j));
  }
  TEST_ASSERT_FALSE(isRevealed(cellIndex(1, 1)));
  TEST_ASSERT_FALSE(isRevealed(cellIndex(1, 3)));
  assertNoQueuedBits();
}

void test_chord_does_nothing_when_flags_do_not_match() {
  placeMinesAt(CHORD_MINES, 2);
  boardReveal(TARGET, NULL, NULL);
  boardToggleFlag(cellIndex(1, 1));  // only 1 of 2 flagged
  resetChanges();
  TEST_ASSERT_EQUAL(REVEAL_NONE, boardChord(TARGET, recordChange, NULL));
  TEST_ASSERT_EQUAL_UINT8(0, changedCount);
}

void test_chord_with_wrong_flag_hits_mine() {
  placeMinesAt(CHORD_MINES, 2);
  boardReveal(TARGET, NULL, NULL);
  boardToggleFlag(cellIndex(1, 1));
  boardToggleFlag(cellIndex(3, 3));  // wrong flag: (1,3) is the real mine
  uint8_t lost = NO_CELL;
  TEST_ASSERT_EQUAL(REVEAL_MINE, boardChord(TARGET, recordChange, &lost));
  TEST_ASSERT_EQUAL_UINT8(cellIndex(1, 3), lost);
}

void test_chord_ignores_unrevealed_and_zero_cells() {
  placeMinesAt(CHORD_MINES, 2);
  TEST_ASSERT_EQUAL(REVEAL_NONE, boardChord(TARGET, NULL, NULL));  // not revealed
  uint8_t zero = cellIndex(8, 10);
  boardReveal(zero, NULL, NULL);
  TEST_ASSERT_EQUAL(REVEAL_NONE, boardChord(zero, NULL, NULL));
}

// ---------- loss display ----------

void test_reveal_all_mines_keeps_flags() {
  const uint8_t mines[] = {cellIndex(0, 0), cellIndex(5, 5)};
  placeMinesAt(mines, 2);
  boardToggleFlag(cellIndex(0, 0));  // correct flag
  boardToggleFlag(cellIndex(9, 9));  // wrong flag
  boardRevealAllMines(recordChange);
  TEST_ASSERT_TRUE(isRevealed(cellIndex(5, 5)));
  TEST_ASSERT_TRUE(isFlagged(cellIndex(0, 0)));
  TEST_ASSERT_TRUE(isFlagged(cellIndex(9, 9)) && !isMine(cellIndex(9, 9)));
  TEST_ASSERT_EQUAL_UINT8(1, changedCount);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_exact_mine_count_many_seeds);
  RUN_TEST(test_first_cell_and_neighbors_mine_free);
  RUN_TEST(test_crowded_board_falls_back_to_safe_cell_only);
  RUN_TEST(test_mine_count_clamped_to_fit);
  RUN_TEST(test_neighbor_counts_match_brute_force);
  RUN_TEST(test_same_seed_same_board);
  RUN_TEST(test_flood_fill_stops_at_mine_wall);
  RUN_TEST(test_flood_fill_skips_flagged_cells);
  RUN_TEST(test_reveal_number_reveals_only_that_cell);
  RUN_TEST(test_flood_fill_matches_reference_many_seeds);
  RUN_TEST(test_reveal_mine_is_loss);
  RUN_TEST(test_reveal_ignores_flagged_and_revealed);
  RUN_TEST(test_win_when_all_safe_cells_revealed);
  RUN_TEST(test_win_ignores_flags);
  RUN_TEST(test_flag_toggles_and_counts);
  RUN_TEST(test_cannot_flag_revealed_cell);
  RUN_TEST(test_mines_left_can_go_negative);
  RUN_TEST(test_chord_reveals_unflagged_neighbors_when_flags_match);
  RUN_TEST(test_chord_does_nothing_when_flags_do_not_match);
  RUN_TEST(test_chord_with_wrong_flag_hits_mine);
  RUN_TEST(test_chord_ignores_unrevealed_and_zero_cells);
  RUN_TEST(test_reveal_all_mines_keeps_flags);
  return UNITY_END();
}
