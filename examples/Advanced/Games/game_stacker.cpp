#include "games.h"

namespace stacker {

constexpr int kCols  = 16;
constexpr int kRows  = 8;
constexpr int kHudH  = 14;
constexpr int kCellW = 13;
constexpr int kCellH = 14;

constexpr uint16_t kBg      = 0x0000;
constexpr uint16_t kGrid    = 0x10A2;
constexpr uint16_t kStack   = 0x07E0;
constexpr uint16_t kPiece   = 0xFFE0;
constexpr uint16_t kHudBg   = 0x0841;
constexpr uint16_t kHudText = 0xFFFF;

enum class State : uint8_t { Title, Playing, Won, Lost };

namespace {

int   field_x, field_y;
bool  placed[kRows][kCols];
int   cur_col;
int   piece_top;
int   piece_dir;
int   piece_size;
int   step_frames;
int   step_counter;
int   level;
int   score;
State state;

void clearBoard() {
  for (int r = 0; r < kRows; ++r)
    for (int c = 0; c < kCols; ++c) placed[r][c] = false;
}

void resetGame() {
  clearBoard();
  cur_col      = 0;
  piece_size   = 3;
  piece_top    = 0;
  piece_dir    = +1;
  step_frames  = 12;
  step_counter = 0;
  level        = 1;
  score        = 0;
  state        = State::Playing;
}

void nextLevel() {
  ++level;
  clearBoard();
  cur_col      = 0;
  piece_size   = (level >= 5) ? 1 : (level >= 3) ? 2 : 3;
  piece_top    = 0;
  piece_dir    = +1;
  step_counter = 0;
  if (step_frames > 3) --step_frames;
  state = State::Playing;
}

void stepPiece() {
  if (++step_counter < step_frames) return;
  step_counter = 0;
  piece_top += piece_dir;
  if (piece_top + piece_size > kRows) {
    piece_top = kRows - piece_size;
    piece_dir = -1;
  } else if (piece_top < 0) {
    piece_top = 0;
    piece_dir = +1;
  }
}

void lockPiece() {
  int kept = 0;
  for (int r = piece_top; r < piece_top + piece_size; ++r) {
    bool keep = (cur_col == 0) || placed[r][cur_col - 1];
    if (keep) {
      placed[r][cur_col] = true;
      ++kept;
    }
  }
  if (kept == 0) {
    state = State::Lost;
    M5.Speaker.tone(180, 200);
    return;
  }
  M5.Speaker.tone(660 + kept * 60, 40);
  score += kept * level;
  ++cur_col;
  if (cur_col >= kCols) {
    state = State::Won;
    M5.Speaker.tone(1320, 220);
    return;
  }
  piece_size = kept;

  int top_of_prev = 0;
  while (top_of_prev < kRows && !placed[top_of_prev][cur_col - 1]) ++top_of_prev;
  if (top_of_prev > kRows - piece_size) top_of_prev = kRows - piece_size;
  piece_top = top_of_prev;
  piece_dir = +1;
}

void drawCell(int r, int c, uint16_t color) {
  int x = field_x + (kCols - 1 - c) * kCellW;
  int y = field_y + r * kCellH;
  gfx::canvas.fillRect(x + 1, y + 1, kCellW - 2, kCellH - 2, color);
}

void drawHud() {
  auto& c = gfx::canvas;
  c.fillRect(0, 0, gfx::screen_w, kHudH, kHudBg);
  c.setTextColor(kHudText);
  c.setTextSize(1);
  c.setTextDatum(top_left);
  char buf[24];
  snprintf(buf, sizeof(buf), "L%d  %d", level, score);
  c.drawString(buf, 4, 2);
  c.setTextDatum(top_right);
  snprintf(buf, sizeof(buf), "%d/%d", cur_col, kCols);
  c.drawString(buf, gfx::screen_w - 4, 2);
}

void drawScene() {
  auto& c = gfx::canvas;
  c.fillScreen(kBg);
  drawHud();

  c.drawRect(field_x - 1, field_y - 1,
             kCols * kCellW + 2, kRows * kCellH + 2, kGrid);

  for (int r = 0; r < kRows; ++r)
    for (int cc = 0; cc < kCols; ++cc)
      if (placed[r][cc]) drawCell(r, cc, kStack);

  if (state == State::Playing && cur_col < kCols) {
    for (int r = piece_top; r < piece_top + piece_size; ++r) {
      drawCell(r, cur_col, kPiece);
    }
  }
}

}  // namespace

void enter() {
  field_x = (gfx::screen_w - kCols * kCellW) / 2;
  field_y = kHudH + (gfx::screen_h - kHudH - kRows * kCellH) / 2;
  resetGame();
  state = State::Title;
}

bool tick() {
  if (state != State::Playing && M5.BtnB.pressedFor(600)) return false;

  switch (state) {
    case State::Title:
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
        resetGame();
      }
      break;
    case State::Playing:
      stepPiece();
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) lockPiece();
      break;
    case State::Won:
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) nextLevel();
      break;
    case State::Lost:
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) resetGame();
      break;
  }

  drawScene();
  if (state == State::Title) {
    gfx::drawCenteredText("STACKER", "A/B: drop  hold B: exit", kBg);
  } else if (state == State::Won) {
    gfx::drawCenteredText("CLEARED!", "press to advance", kBg);
  } else if (state == State::Lost) {
    gfx::drawCenteredText("TOWER FELL", "press to retry", kBg);
  }
  return true;
}

}  // namespace stacker
