#include "games.h"
#include <math.h>

namespace pong {

constexpr int kHudHeight = 12;

constexpr int   kPaddleW       = 3;
constexpr int   kPaddleH       = 22;
constexpr int   kPaddleMargin  = 6;
constexpr float kPlayerSpeed   = 2.6f;
constexpr float kAiMaxSpeed    = 1.8f;

constexpr float kBallRadius    = 2.0f;
constexpr float kBallStartVx   = 1.6f;
constexpr float kBallSpeedup   = 1.06f;   // applied on each paddle hit
constexpr float kBallMaxSpeed  = 4.5f;

constexpr int   kServeFrames   = 50;      // pause before each serve
constexpr int   kWinScore      = 5;

constexpr uint16_t kBg       = 0x0000;
constexpr uint16_t kCenter   = 0x4208;
constexpr uint16_t kPaddleC  = 0xFFFF;
constexpr uint16_t kBallC    = 0xFFE0;
constexpr uint16_t kHud      = 0x0841;
constexpr uint16_t kHudText  = 0xFFFF;

enum class State : uint8_t { Title, Playing, GameOver };

namespace {

float ply, aiy;             // top-left y of paddles
float bx, by, vx, vy;
int   player_score, ai_score;
int   serve_timer;          // counts down; ball is parked while > 0
int   serve_dir;            // -1 toward AI (left), +1 toward player (right)
State state;
bool  player_won;
int   field_top, field_bot; // y bounds for ball/paddle play area

void resetServe(int dir) {
  serve_dir = dir;
  serve_timer = kServeFrames;
  bx = gfx::screen_w / 2.0f;
  by = (field_top + field_bot) / 2.0f;
  vx = 0.0f;
  vy = 0.0f;
}

void launchBall() {
  vx = kBallStartVx * serve_dir;
  // Random initial vertical angle: small but nonzero so it isn't pure horizontal.
  float vy_mag = 0.4f + (esp_random() % 100) / 140.0f;  // 0.4 .. ~1.1
  vy = (esp_random() & 1) ? vy_mag : -vy_mag;
}

void resetMatch() {
  field_top = kHudHeight + 2;
  field_bot = gfx::screen_h - 2;
  ply = aiy = (field_top + field_bot - kPaddleH) / 2.0f;
  player_score = ai_score = 0;
  // First serve: random direction.
  resetServe((esp_random() & 1) ? +1 : -1);
}

void clampPaddle(float& y) {
  if (y < field_top)              y = field_top;
  if (y > field_bot - kPaddleH)   y = field_bot - kPaddleH;
}

void updatePlayer() {
  if (M5.BtnA.isPressed()) ply -= kPlayerSpeed;
  if (M5.BtnB.isPressed()) ply += kPlayerSpeed;
  clampPaddle(ply);
}

void updateAi() {
  // Track the ball's y, but only when it's moving toward us, with capped speed
  // so the AI is beatable. When the ball goes the other way, drift to center.
  float target;
  if (vx < 0) {
    target = by - kPaddleH / 2.0f;
  } else {
    target = (field_top + field_bot - kPaddleH) / 2.0f;
  }
  float dy = target - aiy;
  if (dy >  kAiMaxSpeed) dy =  kAiMaxSpeed;
  if (dy < -kAiMaxSpeed) dy = -kAiMaxSpeed;
  aiy += dy;
  clampPaddle(aiy);
}

void bounceOffPaddle(float paddle_y) {
  // Add english based on where the ball hit the paddle: top -> up, bottom -> down.
  float rel = (by - (paddle_y + kPaddleH / 2.0f)) / (kPaddleH / 2.0f);
  if (rel < -1.0f) rel = -1.0f;
  if (rel >  1.0f) rel =  1.0f;

  vx = -vx * kBallSpeedup;
  vy += rel * 1.1f;

  // Clamp total speed so the ball doesn't get unplayable.
  float s = sqrtf(vx * vx + vy * vy);
  if (s > kBallMaxSpeed) {
    vx = vx / s * kBallMaxSpeed;
    vy = vy / s * kBallMaxSpeed;
  }
  M5.Speaker.tone(880, 30);
}

void stepBall() {
  if (serve_timer > 0) {
    if (--serve_timer == 0) launchBall();
    return;
  }

  bx += vx;
  by += vy;

  // Top/bottom walls
  if (by < field_top + kBallRadius) {
    by = field_top + kBallRadius;
    vy = -vy;
    M5.Speaker.tone(660, 25);
  } else if (by > field_bot - kBallRadius) {
    by = field_bot - kBallRadius;
    vy = -vy;
    M5.Speaker.tone(660, 25);
  }

  // AI paddle (left)
  float ai_x = kPaddleMargin;
  if (vx < 0 &&
      bx - kBallRadius <= ai_x + kPaddleW &&
      bx - kBallRadius >= ai_x - 2 &&
      by >= aiy && by <= aiy + kPaddleH) {
    bx = ai_x + kPaddleW + kBallRadius;
    bounceOffPaddle(aiy);
  }

  // Player paddle (right)
  float pl_x = gfx::screen_w - kPaddleMargin - kPaddleW;
  if (vx > 0 &&
      bx + kBallRadius >= pl_x &&
      bx + kBallRadius <= pl_x + kPaddleW + 2 &&
      by >= ply && by <= ply + kPaddleH) {
    bx = pl_x - kBallRadius;
    bounceOffPaddle(ply);
  }

  // Goals
  if (bx < -4) {
    ++player_score;
    M5.Speaker.tone(1320, 80);
    if (player_score >= kWinScore) { state = State::GameOver; player_won = true; }
    else                            resetServe(+1);  // serve toward player
  } else if (bx > gfx::screen_w + 4) {
    ++ai_score;
    M5.Speaker.tone(220, 120);
    if (ai_score >= kWinScore) { state = State::GameOver; player_won = false; }
    else                        resetServe(-1);  // serve toward AI
  }
}

void drawHud() {
  auto& c = gfx::canvas;
  c.fillRect(0, 0, gfx::screen_w, kHudHeight, kHud);
  c.setTextColor(kHudText);
  c.setTextSize(1);
  c.setTextDatum(top_left);
  char buf[8];
  snprintf(buf, sizeof(buf), "CPU %d", ai_score);
  c.drawString(buf, 4, 2);
  c.setTextDatum(top_right);
  snprintf(buf, sizeof(buf), "%d YOU", player_score);
  c.drawString(buf, gfx::screen_w - 4, 2);
}

void drawScene() {
  auto& c = gfx::canvas;
  c.fillScreen(kBg);
  drawHud();

  // Dashed center line
  for (int y = field_top; y < field_bot; y += 6) {
    c.drawFastVLine(gfx::screen_w / 2, y, 3, kCenter);
  }

  // Paddles
  c.fillRect(kPaddleMargin, (int)aiy, kPaddleW, kPaddleH, kPaddleC);
  c.fillRect(gfx::screen_w - kPaddleMargin - kPaddleW, (int)ply,
             kPaddleW, kPaddleH, kPaddleC);

  // Ball
  c.fillCircle((int)bx, (int)by, (int)kBallRadius, kBallC);
}

}  // namespace

void enter() {
  resetMatch();
  state = State::Title;
}

bool tick() {
  // Hold BtnB to exit when not actively playing.
  if (state != State::Playing && M5.BtnB.pressedFor(600)) return false;

  switch (state) {
    case State::Title:
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
        resetMatch();
        state = State::Playing;
      }
      break;
    case State::Playing:
      updatePlayer();
      updateAi();
      stepBall();
      break;
    case State::GameOver:
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
        resetMatch();
        state = State::Title;
      }
      break;
  }

  drawScene();
  if (state == State::Title) {
    gfx::drawCenteredText("PONG", "A:up  B:down", kBg);
  } else if (state == State::GameOver) {
    gfx::drawCenteredText(player_won ? "YOU WIN!" : "CPU WINS",
                          "press to retry - hold B to exit", kBg);
  }
  return true;
}

}  // namespace pong
