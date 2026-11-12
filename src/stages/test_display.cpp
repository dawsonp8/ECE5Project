// Stage 1 display test: pio run -e test_display -t upload
//
// Loops through every screen and cell type with no input needed. Scene names
// print to Serial at 115200 (pio device monitor) so you know what to look at.
//   1. Title (1.5 s)
//   2. Calibration screen with fake BPM and a filling progress bar (5 s)
//   3. Calibration-failed screen (3 s)
//   4. Gallery: every cell type, numbers 1-8, flag, mine, wrong flag (X),
//      exploded mine (red), corner cells, cursor, status bar (8 s)
//   5. Cursor sweep across the whole board on a real game (~11 s)
//   6. Timer bar draining (green), then fallback mode (orange, "HR --")
//   7. Explosion at a mine near the top edge, all mines revealed, "BOOM"
//   8. Banners "TOO SLOW" and "CLEARED 3:42"
#include <Arduino.h>

#include "board.h"
#include "config.h"
#include "display.h"

enum Scene : uint8_t {
  S_TITLE, S_CAL, S_CALFAIL, S_GALLERY, S_CURSOR, S_TIMER, S_EXPLODE, S_BANNERS, S_COUNT
};

static uint8_t scene;
static uint32_t sceneStart;
static uint32_t lastStep;
static uint16_t step;
static uint8_t blastIdx;

static bool elapsed(uint32_t since, uint32_t ms) { return millis() - since >= ms; }

static void buildGallery() {
  boardReset();
  for (uint8_t n = 0; n <= 8; n++) cells[cellIndex(1, 1 + n)] = CELL_REVEALED | n;
  cells[cellIndex(3, 1)] = CELL_FLAGGED | CELL_MINE;   // correct flag
  cells[cellIndex(3, 3)] = CELL_REVEALED | CELL_MINE;  // revealed mine
  cells[cellIndex(3, 5)] = CELL_FLAGGED;               // wrong flag: X
  cells[cellIndex(3, 7)] = CELL_REVEALED | CELL_MINE;  // the mine that was hit
  // Corners, to check nothing is cut off at the screen edges.
  cells[cellIndex(0, 0)] = CELL_REVEALED | 1;
  cells[cellIndex(0, COLS - 1)] = CELL_REVEALED | 2;
  cells[cellIndex(ROWS - 1, 0)] = CELL_REVEALED | 3;
  cells[cellIndex(ROWS - 1, COLS - 1)] = CELL_REVEALED | 8;
  cells[cellIndex(ROWS - 1, COLS - 2)] = CELL_FLAGGED | CELL_MINE;
}

static uint8_t snake(uint16_t s) {
  uint8_t row = s / COLS, col = s % COLS;
  if (row & 1) col = COLS - 1 - col;
  return cellIndex(row, col);
}

static void enterScene(uint8_t s) {
  scene = s;
  sceneStart = lastStep = millis();
  step = 0;

  switch (scene) {
    case S_TITLE:
      Serial.println(F("1. Title"));
      displayTitleScreen();
      break;
    case S_CAL:
      Serial.println(F("2. Calibration screen"));
      displayCalibrateScreen();
      displayCalibrateUpdate(-1, 0, 0);
      break;
    case S_CALFAIL:
      Serial.println(F("3. Calibration failed screen"));
      displayCalFailScreen();
      break;
    case S_GALLERY:
      Serial.println(F("4. Gallery: rows 1 and 3, corners, cursor at center"));
      buildGallery();
      displayNewGame(cellIndex(ROWS / 2, COLS / 2));
      displayGameOver(cellIndex(3, 7));
      displayStatusMines(28);
      displayStatusHeart(72);
      displayTimerBar(10000, 10000, false);
      break;
    case S_CURSOR: {
      Serial.println(F("5. Cursor sweep on a real board"));
      uint8_t start = cellIndex(ROWS / 2, COLS / 2);
      boardReset();
      boardPlaceMines(start, MINE_COUNT, 12345);
      boardReveal(start, NULL, NULL);
      displayNewGame(snake(0));
      displayStatusMines(boardMinesLeft());
      displayStatusHeart(68);
      displayTimerBarEmpty();
      break;
    }
    case S_TIMER:
      Serial.println(F("6. Timer bar: green 10 s, then orange fallback 5 s"));
      displaySetCursor(NO_CELL);
      break;
    case S_EXPLODE: {
      Serial.println(F("7. Explosion near the top edge, then all mines"));
      blastIdx = NO_CELL;
      for (uint8_t i = 0; i < CELLS && blastIdx == NO_CELL; i++) {
        if (isMine(i)) blastIdx = i;  // first mine: top rows, tests status repaint
      }
      cells[blastIdx] |= CELL_REVEALED;
      break;
    }
    case S_BANNERS:
      Serial.println(F("8. Banners"));
      displayBanner(F("TOO SLOW"), -1);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("Stage 1 display test"));
  displayInit();
  enterScene(S_TITLE);
}

void loop() {
  switch (scene) {
    case S_TITLE:
      if (elapsed(sceneStart, 1500)) enterScene(S_CAL);
      break;

    case S_CAL:
      if (elapsed(lastStep, 250)) {
        lastStep = millis();
        step++;
        int16_t bpm = step < 4 ? -1 : 70 + (step % 5);
        uint16_t permille = (uint32_t)(millis() - sceneStart) * 1000 / 5000;
        displayCalibrateUpdate(bpm, permille, step / 3);
      }
      if (elapsed(sceneStart, 5000)) enterScene(S_CALFAIL);
      break;

    case S_CALFAIL:
      if (elapsed(sceneStart, 3000)) enterScene(S_GALLERY);
      break;

    case S_GALLERY:
      if (elapsed(sceneStart, 8000)) enterScene(S_CURSOR);
      break;

    case S_CURSOR:
      if (elapsed(lastStep, 60)) {
        lastStep = millis();
        step++;
        if (step >= CELLS) {
          enterScene(S_TIMER);
          break;
        }
        displaySetCursor(snake(step));
        if (step % 16 == 0) displayStatusMines(boardMinesLeft() - step / 16);
      }
      break;

    case S_TIMER: {
      uint32_t t = millis() - sceneStart;
      if (elapsed(lastStep, 100)) {
        lastStep = millis();
        if (t < 10000) {
          displayTimerBar(10000 - t, 10000, false);
        } else if (t < 15000) {
          displayStatusHeart(-1);
          displayStatusMines(-3);
          displayTimerBar(15000 - t, 5000, true);
        } else {
          enterScene(S_EXPLODE);
        }
      }
      break;
    }

    case S_EXPLODE:
      if (step < EXPLOSION_FRAMES) {
        if (step == 0 || elapsed(lastStep, EXPLOSION_FRAME_MS)) {
          lastStep = millis();
          if (displayExplosionFrame(blastIdx, step)) {
            displayGameOver(blastIdx);
            boardRevealAllMines(displayDrawCell);
            displayBanner(F("BOOM"), -1);
          }
          step++;
        }
      } else if (elapsed(lastStep, 3000)) {
        enterScene(S_BANNERS);
      }
      break;

    case S_BANNERS:
      if (step == 0 && elapsed(sceneStart, 2500)) {
        displayBanner(F("CLEARED"), 222);  // 3:42
        step = 1;
      } else if (elapsed(sceneStart, 5500)) {
        enterScene(S_TITLE);
      }
      break;
  }
}
