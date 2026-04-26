#include "games.h"
#include <math.h>

namespace breakout {

constexpr int kHudHeight   = 12;
constexpr int kBrickCols   = 10;
constexpr int kBrickRows   = 4;
constexpr int kBrickW      = 24;   // 10 * 24 = 240, fills width
constexpr int kBrickH      = 7;
constexpr int kBricksTop   = 14;
constexpr int kBricksLeft  = 0;

constexpr int kPaddleW     = 36;
constexpr int kPaddleH     = 4;
constexpr int kPaddleY     = 126;

constexpr float kBallRadius  = 2.5f;
constexpr float kBallSpeed   = 1.6f;
constexpr float kBallSpdMax  = 2.6f;
constexpr float kPaddleSpeed = 3.0f;

constexpr int   kServeFrames = 60;   // ~1s at 60fps

constexpr uint16_t kBg      = 0x0841;
constexpr uint16_t kPaddleC = 0xFFFF;
constexpr uint16_t kBallC   = 0xFFE0;
constexpr uint16_t kHud     = 0x0000;
constexpr uint16_t kHudText = 0xFFFF;

const uint16_t kBrickColors[kBrickRows] = {
  0xF800,  // red    (top)
  0xFD20,  // orange
  0xFFE0,  // yellow
  0x07E0,  // green  (bottom)
};

enum class State : uint8_t { Title, Playing, Dead, Win };

namespace {

bool  alive[kBrickRows][kBrickCols];
int   bricks_left;
float bx, by, vx, vy;
float px;
int   score, lives, best;
int   serve_timer;
State state;

void resetBricks() {
  bricks_left = kBrickRows * kBrickCols;
  for (int r = 0; r < kBrickRows; ++r)
    for (int c = 0; c < kBrickCols; ++c)
      alive[r][c] = true;
}

void parkBallOnPaddle() {
  bx = px + kPaddleW / 2.0f;
  by = kPaddleY - kBallRadius - 1;
  vx = vy = 0.0f;
}

void launchBall() {
  vy = -kBallSpeed;
  // Random initial angle: ±0.5 to ±0.8 horizontal component.
  float h = 0.5f + (esp_random() % 100) / 333.0f;
  vx = (esp_random() & 1) ? h : -h;
  // Renormalize to base speed.
  float s = sqrtf(vx*vx + vy*vy);
  vx = vx / s * kBallSpeed;
  vy = vy / s * kBallSpeed;
}

void resetGame() {
  resetBricks();
  px = (gfx::screen_w - kPaddleW) / 2.0f;
  score = 0;
  lives = 3;
  serve_timer = kServeFrames;
  parkBallOnPaddle();
}

void respawnBall() {
  serve_timer = kServeFrames;
  parkBallOnPaddle();
}

bool brickHit(int x, int y, int& out_r, int& out_c) {
  if (y < kBricksTop) return false;
  int r = (y - kBricksTop) / kBrickH;
  if (r < 0 || r >= kBrickRows) return false;
  int c = (x - kBricksLeft) / kBrickW;
  if (c < 0 || c >= kBrickCols) return false;
  if (!alive[r][c]) return false;
  out_r = r; out_c = c;
  return true;
}

void killBrick(int r, int c) {
  alive[r][c] = false;
  ++score;
  --bricks_left;
  M5.Speaker.tone(660 + r * 110, 25);
}

void movePaddle() {
  if (M5.BtnB.isPressed()) px -= kPaddleSpeed;
  if (M5.BtnA.isPressed()) px += kPaddleSpeed;
  if (px < 0) px = 0;
  if (px > gfx::screen_w - kPaddleW) px = gfx::screen_w - kPaddleW;
}

void stepPhysics() {
  movePaddle();

  if (serve_timer > 0) {
    bx = px + kPaddleW / 2.0f;
    by = kPaddleY - kBallRadius - 1;
    if (--serve_timer == 0) launchBall();
    return;
  }

  // X movement + walls/bricks.
  bx += vx;
  if (bx - kBallRadius < 0)               { bx = kBallRadius;                 vx = -vx; }
  if (bx + kBallRadius > gfx::screen_w)   { bx = gfx::screen_w - kBallRadius; vx = -vx; }
  int r, c;
  int probe_x = (int)(bx + (vx > 0 ?  kBallRadius : -kBallRadius));
  if (brickHit(probe_x, (int)by, r, c)) {
    killBrick(r, c);
    vx = -vx;
  }

  // Y movement + ceiling/bricks.
  by += vy;
  if (by - kBallRadius < kHudHeight) { by = kHudHeight + kBallRadius; vy = -vy; }
  int probe_y = (int)(by + (vy > 0 ?  kBallRadius : -kBallRadius));
  if (brickHit((int)bx, probe_y, r, c)) {
    killBrick(r, c);
    vy = -vy;
  }

  // Paddle collision (only when moving down and overlapping the paddle band).
  if (vy > 0 &&
      by + kBallRadius >= kPaddleY &&
      by - kBallRadius <= kPaddleY + kPaddleH &&
      bx >= px && bx <= px + kPaddleW) {
    by = kPaddleY - kBallRadius;
    vy = -vy;
    // Steer by where the ball hit on the paddle: -1 (left edge) .. +1 (right).
    float rel = (bx - (px + kPaddleW / 2.0f)) / (kPaddleW / 2.0f);
    float target = kBallSpeed + score * 0.01f;
    if (target > kBallSpdMax) target = kBallSpdMax;
    vx = rel * target * 0.85f;
    // Maintain target speed magnitude.
    float vy_sq = target * target - vx * vx;
    vy = -sqrtf(vy_sq > 0.04f ? vy_sq : 0.04f);
    M5.Speaker.tone(440, 18);
  }

  // Below screen.
  if (by - kBallRadius > gfx::screen_h) {
    --lives;
    M5.Speaker.tone(120, 180);
    if (lives <= 0) {
      state = State::Dead;
      if (score > best) best = score;
    } else {
      respawnBall();
    }
  }

  if (bricks_left <= 0) {
    state = State::Win;
    if (score > best) best = score;
    M5.Speaker.tone(880, 60);
  }
}

void drawScene() {
  auto& c = gfx::canvas;
  c.fillScreen(kBg);

  c.fillRect(0, 0, gfx::screen_w, kHudHeight, kHud);
  c.setTextColor(kHudText);
  c.setTextDatum(top_left);
  c.setTextSize(1);
  char buf[40];
  snprintf(buf, sizeof(buf), "score %d  lives %d  best %d", score, lives, best);
  c.drawString(buf, 4, 2);

  for (int r = 0; r < kBrickRows; ++r) {
    int y = kBricksTop + r * kBrickH;
    for (int co = 0; co < kBrickCols; ++co) {
      if (!alive[r][co]) continue;
      int x = kBricksLeft + co * kBrickW;
      c.fillRect(x + 1, y + 1, kBrickW - 2, kBrickH - 2, kBrickColors[r]);
    }
  }

  c.fillRect((int)px, kPaddleY, kPaddleW, kPaddleH, kPaddleC);
  c.fillCircle((int)bx, (int)by, (int)kBallRadius, kBallC);
}

}  // namespace

void enter() {
  state = State::Title;
  resetGame();
}

bool tick() {
  if (state != State::Playing && M5.BtnB.pressedFor(600)) return false;

  switch (state) {
    case State::Title:
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
        resetGame();
        state = State::Playing;
      }
      break;
    case State::Playing:
      stepPhysics();
      break;
    case State::Dead:
    case State::Win:
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
        resetGame();
        state = State::Title;
      }
      break;
  }

  drawScene();

  if (state == State::Title)
    gfx::drawCenteredText("BREAKOUT", "B:left  A:right  press to play", kBg);
  else if (state == State::Playing && serve_timer > 0)
    gfx::drawCenteredText("READY", "aim with A/B", kBg);
  else if (state == State::Dead)
    gfx::drawCenteredText("GAME OVER", "press to retry - hold B to exit", kBg);
  else if (state == State::Win)
    gfx::drawCenteredText("CLEARED!", "press to play again - hold B to exit", kBg);

  return true;
}

}  // namespace breakout
