#include "games.h"

namespace snake {

constexpr int kCell      = 6;
constexpr int kMaxLen    = 200;
constexpr int kStartLen  = 5;
constexpr int kHudHeight = 12;
constexpr int kStartStep = 9;   // frames between moves at start (~6.6 cells/s @60fps)
constexpr int kMinStep   = 3;

constexpr uint16_t kBg       = 0x0224;
constexpr uint16_t kGrid     = 0x10A2;
constexpr uint16_t kHead     = 0xFFE0;
constexpr uint16_t kBody     = 0x07E0;
constexpr uint16_t kFood     = 0xF800;
constexpr uint16_t kHud      = 0x0000;
constexpr uint16_t kHudText  = 0xFFFF;

enum Dir : uint8_t { Right = 0, Down = 1, Left = 2, Up = 3 };
enum class State : uint8_t { Title, Playing, Dead };

namespace {
struct Cell { int8_t x, y; };

int   cols, rows;
Cell  body[kMaxLen];
int   length;
int   head_idx;       // body[head_idx] is the head; ring buffer
Dir   dir, queued_dir;
Cell  food;
int   score, best;
int   step_frames;
int   frame_counter;
State state;

bool isOnSnake(int x, int y, bool include_tail = true) {
  // Body is laid out forward from head_idx: head at head_idx,
  // next segment at head_idx+1, ..., tail at head_idx+length-1 (mod kMaxLen).
  // Must match the traversal used in stepSnake/drawScene, otherwise we
  // sample uninitialized/stale slots and the snake "dies" on phantom cells.
  int n = include_tail ? length : (length - 1);
  for (int i = 0; i < n; ++i) {
    int idx = (head_idx + i) % kMaxLen;
    if (body[idx].x == x && body[idx].y == y) return true;
  }
  return false;
}

void placeFood() {
  for (int tries = 0; tries < 200; ++tries) {
    int x = esp_random() % cols;
    int y = esp_random() % rows;
    if (!isOnSnake(x, y)) { food = { (int8_t)x, (int8_t)y }; return; }
  }
  food = { 0, 0 };  // fallback (board nearly full)
}

void resetGame() {
  cols = gfx::screen_w / kCell;
  rows = (gfx::screen_h - kHudHeight) / kCell;
  length = kStartLen;
  head_idx = 0;
  int sx = cols / 2, sy = rows / 2;
  for (int i = 0; i < length; ++i) body[i] = { (int8_t)(sx - i), (int8_t)sy };
  // head at body[0] (index 0); tail at body[length-1]
  dir = queued_dir = Right;
  score = 0;
  step_frames = kStartStep;
  frame_counter = 0;
  placeFood();
}

void turnLeft()  { queued_dir = (Dir)((dir + 3) & 3); }
void turnRight() { queued_dir = (Dir)((dir + 1) & 3); }

void stepSnake() {
  dir = queued_dir;
  Cell head = body[head_idx];
  int8_t nx = head.x, ny = head.y;
  switch (dir) {
    case Right: nx++; break;
    case Down:  ny++; break;
    case Left:  nx--; break;
    case Up:    ny--; break;
  }

  // Wall collision
  if (nx < 0 || nx >= cols || ny < 0 || ny >= rows) {
    state = State::Dead;
    M5.Speaker.tone(160, 200);
    if (score > best) best = score;
    return;
  }

  bool ate = (nx == food.x && ny == food.y);

  // Self-collision: ignore the tail cell because it'll vacate this frame
  // (unless we just ate, in which case it stays).
  if (isOnSnake(nx, ny, /*include_tail=*/ate)) {
    state = State::Dead;
    M5.Speaker.tone(160, 200);
    if (score > best) best = score;
    return;
  }

  // Advance ring buffer: new head goes one step "back" in the ring.
  head_idx = (head_idx - 1 + kMaxLen) % kMaxLen;
  body[head_idx] = { nx, ny };

  if (ate) {
    if (length < kMaxLen) ++length;
    ++score;
    M5.Speaker.tone(1320, 60);
    if (score % 5 == 0 && step_frames > kMinStep) --step_frames;
    placeFood();
  }
  // If !ate, the tail at (head_idx + length) % kMaxLen is now outside the
  // valid range, effectively dropped — no copy needed.
}

void drawCell(int cx, int cy, uint16_t color) {
  gfx::canvas.fillRect(cx * kCell + 1, kHudHeight + cy * kCell + 1,
                       kCell - 1, kCell - 1, color);
}

void drawScene() {
  auto& c = gfx::canvas;
  c.fillScreen(kBg);
  c.fillRect(0, 0, gfx::screen_w, kHudHeight, kHud);
  c.setTextColor(kHudText);
  c.setTextDatum(top_left);
  c.setTextSize(1);
  char buf[32];
  snprintf(buf, sizeof(buf), "score %d   best %d", score, best);
  c.drawString(buf, 4, 2);

  // Subtle grid border
  c.drawRect(0, kHudHeight, cols * kCell + 1, rows * kCell + 1, kGrid);

  drawCell(food.x, food.y, kFood);
  for (int i = 0; i < length; ++i) {
    int idx = (head_idx + i) % kMaxLen;
    drawCell(body[idx].x, body[idx].y, i == 0 ? kHead : kBody);
  }
}
}  // namespace

void enter() {
  state = State::Title;
  resetGame();
}

bool tick() {
  // Hold BtnB on title/dead to exit to menu.
  if (state != State::Playing && M5.BtnB.pressedFor(600)) return false;

  switch (state) {
    case State::Title:
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
        resetGame();
        state = State::Playing;
      }
      break;
    case State::Playing:
      if (M5.BtnA.wasPressed()) turnRight();
      else if (M5.BtnB.wasPressed()) turnLeft();

      if (++frame_counter >= step_frames) {
        frame_counter = 0;
        stepSnake();
      }
      break;
    case State::Dead:
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
        resetGame();
        state = State::Title;
      }
      break;
  }

  drawScene();
  if (state == State::Title)
    gfx::drawCenteredText("SNAKE", "A:left  B:right", kBg);
  else if (state == State::Dead)
    gfx::drawCenteredText("GAME OVER", "press to retry - hold B to exit", kBg);

  return true;
}

}  // namespace snake
