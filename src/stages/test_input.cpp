// Stage 2 input test: pio run -e test_input -t upload
//
// Moves a cursor on a blank board and prints every input event to Serial at
// 115200. Leave the joystick untouched while it powers on (center calibration).
//   Joystick       -> cursor moves; one step at once, repeat after 350 ms
//   Joystick push  -> toggles that cell revealed / hidden   (prints REVEAL)
//   Flag button    -> toggles a flag on that cell           (prints FLAG)
//   Both held 2 s  -> prints BOTH HELD 2s and shows "RECAL" (game will recalibrate)
// Raw X/Y readings print every 500 ms while the stick is off center.
#include <Arduino.h>

#include "board.h"
#include "config.h"
#include "display.h"
#include "input.h"

static uint8_t cursor = cellIndex(ROWS / 2, COLS / 2);
static uint32_t lastRaw;
static bool bothReported;

static const char *dirName(Dir d) {
  switch (d) {
    case DIR_UP: return "UP";
    case DIR_DOWN: return "DOWN";
    case DIR_LEFT: return "LEFT";
    case DIR_RIGHT: return "RIGHT";
    default: return "NONE";
  }
}

static void moveCursor(Dir d) {
  uint8_t r = cellRow(cursor), c = cellCol(cursor);
  if (d == DIR_UP && r > 0) r--;
  else if (d == DIR_DOWN && r < ROWS - 1) r++;
  else if (d == DIR_LEFT && c > 0) c--;
  else if (d == DIR_RIGHT && c < COLS - 1) c++;
  else {
    Serial.println(F("  (at edge)"));
    return;
  }
  cursor = cellIndex(r, c);
  displaySetCursor(cursor);
}

void setup() {
  Serial.begin(115200);
  inputInit();
  displayInit();
  boardReset();
  displayNewGame(cursor);
  displayStatusMines(0);
  displayStatusHeart(-1);
  displayTimerBarEmpty();

  Serial.println(F("Stage 2 input test"));
  Serial.print(F("Joystick center: x="));
  Serial.print(inputCenterX());
  Serial.print(F(" y="));
  Serial.println(inputCenterY());
}

void loop() {
  inputUpdate();

  Dir d = inputDir();
  if (d != DIR_NONE) {
    Serial.print(F("DIR "));
    Serial.print(dirName(d));
    Serial.print(F("  x="));
    Serial.print(inputRawX());
    Serial.print(F(" y="));
    Serial.println(inputRawY());
    moveCursor(d);
  }

  if (inputRevealPressed()) {
    Serial.println(F("REVEAL (joystick press)"));
    if (!isFlagged(cursor)) {
      cells[cursor] ^= CELL_REVEALED;
      displayDrawCell(cursor);
    }
  }

  if (inputFlagPressed()) {
    Serial.println(F("FLAG (button)"));
    if (boardToggleFlag(cursor)) {
      displayDrawCell(cursor);
      displayStatusMines(boardFlagCount());
    }
  }

  uint32_t both = inputBothHeldMs();
  if (both >= 2000 && !bothReported) {
    bothReported = true;
    Serial.println(F("BOTH HELD 2s"));
    displayBanner(F("RECAL"), -1);
  } else if (both == 0 && bothReported) {
    bothReported = false;
    displayTimerBarEmpty();
  }

  int16_t dx = inputRawX() - inputCenterX(), dy = inputRawY() - inputCenterY();
  bool offCenter = abs(dx) > 40 || abs(dy) > 40;
  if (offCenter && millis() - lastRaw >= 500) {
    lastRaw = millis();
    Serial.print(F("raw x="));
    Serial.print(inputRawX());
    Serial.print(F(" y="));
    Serial.println(inputRawY());
  }
}
