#include "display.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#include "board.h"
#include "config.h"

static Adafruit_ILI9341 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

// ---- Colors (RGB565) ----
#define C_BLACK      0x0000
#define C_WHITE      0xFFFF
#define C_UNREV      0xC618  // raised cell face
#define C_UNREV_HI   0xFFFF  // raised top-left edge
#define C_UNREV_LO   0x7BEF  // raised bottom-right edge
#define C_REV        0x9CD3  // flat revealed cell
#define C_GRID       0x7BEF  // revealed cell grid lines
#define C_CURSOR     0xFFE0  // yellow
#define C_FLAG       0xF800
#define C_LOST       0xF800  // background of the mine that was hit
#define C_WRONG      0xF800  // X over a wrong flag
#define C_STATUS_BG  0x0000
#define C_STATUS_FG  0xFFFF
#define C_FALLBACK   0xFD20  // orange: fixed-timer mode
#define C_BAR        0x07E0  // green timer bar
#define C_BAR_EMPTY  0x2104
#define C_BANNER     0xFFE0

static const uint16_t NUMBER_COLORS[9] PROGMEM = {
  0,       // 0: unused
  0x001F,  // 1 blue
  0x03E0,  // 2 green
  0xF800,  // 3 red
  0x0010,  // 4 navy
  0x8000,  // 5 maroon
  0x0410,  // 6 teal
  0x0000,  // 7 black
  0xFFFF,  // 8 white (classic gray would vanish on the gray cell)
};

// ---- Status bar layout ----
#define TEXT_W 12  // size-2 character cell
#define MINES_X 4
#define HEART_X 64
#define BAR_X 144
#define BAR_Y 4
#define BAR_W (320 - BAR_X - 4)
#define BAR_H 12

static uint8_t cursorIdx = NO_CELL;
static uint8_t lostIdx = NO_CELL;
static bool gameOver = false;

static int16_t shownMines;
static int16_t shownBpm;
static int16_t barW;    // -1: bar not drawn (empty or banner)
static uint8_t barMode; // 1 normal, 2 fallback
static const __FlashStringHelper *bannerText;
static int16_t bannerSeconds;

static const int16_t UNSET = -32768;

static uint16_t calBarW;

// ---------- helpers ----------

static void cellXY(uint8_t i, int16_t &x, int16_t &y) {
  x = (int16_t)cellCol(i) * CELL_PX;
  y = STATUS_H + (int16_t)cellRow(i) * CELL_PX;
}

// Prints v right-aligned in `width` characters at the current cursor.
static void printPadded(int16_t v, uint8_t width) {
  char buf[7];
  uint8_t n = 0;
  bool neg = v < 0;
  uint16_t u = neg ? (uint16_t)(-(int32_t)v) : (uint16_t)v;
  do {
    buf[n++] = '0' + u % 10;
    u /= 10;
  } while (u && n < 5);
  if (neg) buf[n++] = '-';
  while (n < width && n < sizeof buf) buf[n++] = ' ';
  while (n > 0) tft.print(buf[--n]);
}

static void printCentered(const __FlashStringHelper *s, int16_t y, uint8_t size, uint16_t color) {
  int16_t w = (int16_t)strlen_P((const char *)s) * 6 * size;
  tft.setTextSize(size);
  tft.setTextColor(color, C_BLACK);
  tft.setCursor((320 - w) / 2, y);
  tft.print(s);
}

// ---------- cells ----------

static void drawRaised(int16_t x, int16_t y) {
  tft.fillRect(x, y, CELL_PX, CELL_PX, C_UNREV);
  tft.drawFastHLine(x, y, CELL_PX, C_UNREV_HI);
  tft.drawFastHLine(x, y + 1, CELL_PX - 1, C_UNREV_HI);
  tft.drawFastVLine(x, y, CELL_PX, C_UNREV_HI);
  tft.drawFastVLine(x + 1, y, CELL_PX - 1, C_UNREV_HI);
  tft.drawFastHLine(x + 1, y + CELL_PX - 1, CELL_PX - 1, C_UNREV_LO);
  tft.drawFastHLine(x + 2, y + CELL_PX - 2, CELL_PX - 2, C_UNREV_LO);
  tft.drawFastVLine(x + CELL_PX - 1, y + 1, CELL_PX - 1, C_UNREV_LO);
  tft.drawFastVLine(x + CELL_PX - 2, y + 2, CELL_PX - 2, C_UNREV_LO);
}

static void drawFlat(int16_t x, int16_t y, uint16_t bg) {
  tft.fillRect(x, y, CELL_PX, CELL_PX, bg);
  tft.drawFastHLine(x, y, CELL_PX, C_GRID);
  tft.drawFastVLine(x, y, CELL_PX, C_GRID);
}

static void drawFlag(int16_t x, int16_t y) {
  tft.fillRect(x + 10, y + 4, 2, 12, C_BLACK);      // pole
  tft.fillRect(x + 6, y + 15, 10, 2, C_BLACK);      // base
  tft.fillTriangle(x + 10, y + 4, x + 10, y + 11, x + 4, y + 7, C_FLAG);
}

static void drawMine(int16_t x, int16_t y) {
  int16_t cx = x + 10, cy = y + 10;
  tft.drawFastHLine(cx - 7, cy, 15, C_BLACK);
  tft.drawFastVLine(cx, cy - 7, 15, C_BLACK);
  tft.drawLine(cx - 5, cy - 5, cx + 5, cy + 5, C_BLACK);
  tft.drawLine(cx - 5, cy + 5, cx + 5, cy - 5, C_BLACK);
  tft.fillCircle(cx, cy, 4, C_BLACK);
  tft.fillRect(cx - 2, cy - 2, 2, 2, C_WHITE);
}

static void drawCross(int16_t x, int16_t y) {
  for (int8_t d = 0; d < 2; d++) {
    tft.drawLine(x + 3 + d, y + 3, x + 16 + d, y + 16, C_WRONG);
    tft.drawLine(x + 16 + d, y + 3, x + 3 + d, y + 16, C_WRONG);
  }
}

static void drawCursorAt(uint8_t i) {
  int16_t x, y;
  cellXY(i, x, y);
  tft.drawRect(x, y, CELL_PX, CELL_PX, C_CURSOR);
  tft.drawRect(x + 1, y + 1, CELL_PX - 2, CELL_PX - 2, C_CURSOR);
}

void displayDrawCell(uint8_t i) {
  if (i >= CELLS) return;
  int16_t x, y;
  cellXY(i, x, y);

  if (isRevealed(i)) {
    drawFlat(x, y, i == lostIdx ? C_LOST : C_REV);
    if (isMine(i)) {
      drawMine(x, y);
    } else if (cellCount(i)) {
      uint8_t n = cellCount(i);
      tft.setTextSize(2);
      tft.setTextColor(pgm_read_word(&NUMBER_COLORS[n]));
      tft.setCursor(x + 5, y + 3);
      tft.print((char)('0' + n));
    }
  } else {
    drawRaised(x, y);
    if (isFlagged(i)) {
      drawFlag(x, y);
      if (gameOver && !isMine(i)) drawCross(x, y);
    }
  }

  if (i == cursorIdx) drawCursorAt(i);
}

// ---------- status bar ----------

static void drawMinesField() {
  tft.setTextSize(2);
  tft.setTextColor(C_STATUS_FG, C_STATUS_BG);
  tft.setCursor(MINES_X, 2);
  tft.print('M');
  printPadded(shownMines, 3);
}

static void drawHeartField() {
  tft.setTextSize(2);
  tft.setTextColor(shownBpm < 0 ? C_FALLBACK : C_STATUS_FG, C_STATUS_BG);
  tft.setCursor(HEART_X, 2);
  tft.print(F("HR "));
  if (shownBpm < 0) {
    tft.print(F(" --"));
  } else {
    printPadded(shownBpm, 3);
  }
}

static void drawWholeBar() {
  uint16_t color = barMode == 2 ? C_FALLBACK : C_BAR;
  tft.fillRect(BAR_X, BAR_Y, barW, BAR_H, color);
  tft.fillRect(BAR_X + barW, BAR_Y, BAR_W - barW, BAR_H, C_BAR_EMPTY);
}

static void drawBannerField() {
  tft.fillRect(BAR_X - 4, 0, 320 - (BAR_X - 4), STATUS_H, C_STATUS_BG);
  tft.setTextSize(2);
  tft.setTextColor(C_BANNER, C_STATUS_BG);
  tft.setCursor(BAR_X, 2);
  tft.print(bannerText);
  if (bannerSeconds >= 0) {
    tft.print(' ');
    tft.print(bannerSeconds / 60);
    tft.print(':');
    uint8_t s = bannerSeconds % 60;
    if (s < 10) tft.print('0');
    tft.print(s);
  }
}

void displayStatusMines(int16_t minesLeft) {
  if (minesLeft == shownMines) return;
  shownMines = minesLeft;
  drawMinesField();
}

void displayStatusHeart(int16_t bpm) {
  if (bpm < 0) bpm = -1;
  if (bpm == shownBpm) return;
  shownBpm = bpm;
  drawHeartField();
}

void displayTimerBar(uint32_t remainingMs, uint32_t totalMs, bool fallback) {
  int16_t w = totalMs ? (int16_t)((int32_t)min(remainingMs, totalMs) * BAR_W / (int32_t)totalMs) : 0;
  uint8_t mode = fallback ? 2 : 1;

  if (barW < 0 || mode != barMode) {
    bannerText = NULL;
    barW = w;
    barMode = mode;
    drawWholeBar();
    return;
  }
  if (w < barW) {
    tft.fillRect(BAR_X + w, BAR_Y, barW - w, BAR_H, C_BAR_EMPTY);
  } else if (w > barW) {
    tft.fillRect(BAR_X + barW, BAR_Y, w - barW, BAR_H, mode == 2 ? C_FALLBACK : C_BAR);
  }
  barW = w;
}

void displayTimerBarEmpty() {
  bannerText = NULL;
  barW = 0;
  barMode = 1;
  drawWholeBar();
}

void displayBanner(const __FlashStringHelper *text, int16_t seconds) {
  bannerText = text;
  bannerSeconds = seconds;
  barW = -1;
  drawBannerField();
}

static void redrawStatusBar() {
  tft.fillRect(0, 0, 320, STATUS_H, C_STATUS_BG);
  if (shownMines != UNSET) drawMinesField();
  if (shownBpm != UNSET) drawHeartField();
  if (bannerText) {
    drawBannerField();
  } else if (barW >= 0) {
    drawWholeBar();
  }
}

// ---------- board-level ----------

void displayInit() {
  tft.begin();
  tft.setRotation(1);  // landscape 320x240
  tft.fillScreen(C_BLACK);
}

void displayNewGame(uint8_t cursor) {
  cursorIdx = cursor;
  lostIdx = NO_CELL;
  gameOver = false;
  shownMines = UNSET;
  shownBpm = UNSET;
  bannerText = NULL;
  barW = -1;
  tft.fillRect(0, 0, 320, STATUS_H, C_STATUS_BG);
  for (uint8_t i = 0; i < CELLS; i++) displayDrawCell(i);
}

void displaySetCursor(uint8_t idx) {
  if (idx == cursorIdx) return;
  uint8_t old = cursorIdx;
  cursorIdx = idx;
  if (old != NO_CELL) displayDrawCell(old);
  if (idx != NO_CELL) displayDrawCell(idx);
}

void displayGameOver(uint8_t lost) {
  gameOver = true;
  lostIdx = lost;
  for (uint8_t i = 0; i < CELLS; i++) {
    if (i == lost || (isFlagged(i) && !isMine(i))) displayDrawCell(i);
  }
}

// ---------- explosion ----------

#define EXPLOSION_R0 4
#define EXPLOSION_DR 7
#define EXPLOSION_RMAX (EXPLOSION_R0 + EXPLOSION_DR * (EXPLOSION_FRAMES - 2))

static const uint16_t BLAST_OUTER[] PROGMEM = {0xFFE0, 0xFD20, 0xF800, 0xFD20, 0xF800, 0x8000, 0x4208};
static const uint16_t BLAST_INNER[] PROGMEM = {0xFFFF, 0xFFE0, 0xFD20, 0xFFE0, 0xFD20, 0xF800, 0x2104};
#define BLAST_COLORS (sizeof BLAST_OUTER / sizeof BLAST_OUTER[0])

bool displayExplosionFrame(uint8_t idx, uint8_t frame) {
  int16_t x, y;
  cellXY(idx, x, y);
  int16_t cx = x + CELL_PX / 2, cy = y + CELL_PX / 2;

  if (frame < EXPLOSION_FRAMES - 1) {
    int16_t r = EXPLOSION_R0 + EXPLOSION_DR * frame;
    uint8_t c = frame < BLAST_COLORS ? frame : BLAST_COLORS - 1;
    tft.fillCircle(cx, cy, r, pgm_read_word(&BLAST_OUTER[c]));
    tft.fillCircle(cx, cy, r * 2 / 3, pgm_read_word(&BLAST_INNER[c]));
    return false;
  }

  // Clean up: redraw every cell the fireball could have touched.
  int16_t r = EXPLOSION_RMAX + 1;
  int16_t c0 = max(0, (cx - r) / CELL_PX), c1 = min(COLS - 1, (cx + r) / CELL_PX);
  int16_t r0 = max(0, (cy - r - STATUS_H) / CELL_PX), r1 = min(ROWS - 1, (cy + r - STATUS_H) / CELL_PX);
  for (int16_t row = r0; row <= r1; row++) {
    for (int16_t col = c0; col <= c1; col++) displayDrawCell(cellIndex(row, col));
  }
  if (cy - r < STATUS_H) redrawStatusBar();
  return true;
}

// ---------- full screens ----------

#define CAL_BAR_X 40
#define CAL_BAR_Y 190
#define CAL_BAR_W 240
#define CAL_BAR_H 16

void displayTitleScreen() {
  tft.fillScreen(C_BLACK);
  printCentered(F("HEART-RATE"), 70, 3, C_FLAG);
  printCentered(F("MINESWEEPER"), 105, 3, C_WHITE);
  printCentered(F("ECE 5"), 160, 2, C_UNREV);
}

void displayCalibrateScreen() {
  tft.fillScreen(C_BLACK);
  printCentered(F("Rest a finger"), 20, 2, C_WHITE);
  printCentered(F("on the sensor"), 44, 2, C_WHITE);
  printCentered(F("BPM"), 150, 2, C_UNREV);
  tft.drawRect(CAL_BAR_X - 2, CAL_BAR_Y - 2, CAL_BAR_W + 4, CAL_BAR_H + 4, C_WHITE);
  calBarW = 0;
}

void displayCalibrateUpdate(int16_t bpm, uint16_t progressPermille, uint8_t beats) {
  tft.setTextSize(5);
  tft.setTextColor(C_FLAG, C_BLACK);
  tft.setCursor((320 - 3 * 30) / 2, 90);
  if (bpm < 0) {
    tft.print(F(" --"));
  } else {
    printPadded(bpm, 3);
  }

  tft.setTextSize(1);
  tft.setTextColor(C_UNREV, C_BLACK);
  tft.setCursor(CAL_BAR_X, CAL_BAR_Y + CAL_BAR_H + 8);
  tft.print(F("beats: "));
  printPadded(beats, 3);

  if (progressPermille > 1000) progressPermille = 1000;
  uint16_t w = (uint32_t)progressPermille * CAL_BAR_W / 1000;
  if (w > calBarW) {
    tft.fillRect(CAL_BAR_X + calBarW, CAL_BAR_Y, w - calBarW, CAL_BAR_H, C_BAR);
    calBarW = w;
  }
}

void displayCalFailScreen() {
  tft.fillScreen(C_BLACK);
  printCentered(F("No pulse found."), 60, 2, C_FLAG);
  printCentered(F("Press = retry"), 120, 2, C_WHITE);
  printCentered(F("Flag = fixed timer"), 150, 2, C_WHITE);
}
