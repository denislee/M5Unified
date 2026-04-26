#include "games.h"

namespace flappy {

constexpr float    kGravity      = 0.32f;
constexpr float    kFlapVelocity = -3.6f;
constexpr float    kMaxFallSpeed = 5.0f;
constexpr float    kPipeSpeed    = 1.6f;
constexpr int      kPipeWidth    = 18;
constexpr int      kPipeGap      = 46;
constexpr int      kPipeSpacing  = 78;
constexpr int      kPipeCount    = 4;
constexpr int      kBirdRadius   = 5;

constexpr uint16_t kSky      = 0x6D9F;
constexpr uint16_t kGround   = 0xDE0B;
constexpr uint16_t kGrass    = 0x6E45;
constexpr uint16_t kPipe     = 0x4644;
constexpr uint16_t kPipeEdge = 0x2322;
constexpr uint16_t kBird     = 0xFEC0;
constexpr uint16_t kBirdEdge = 0x0000;
constexpr uint16_t kBeak     = 0xF800;

struct Pipe {
  float x;
  int   gap_y;
  bool  scored;
};

enum class State : uint8_t { Title, Playing, Dead };

namespace {
State    state;
float    bird_y, bird_vy;
int      bird_x;
int      ground_y;
Pipe     pipes[kPipeCount];
int      score, best;

bool flapPressed() {
  return M5.BtnA.wasPressed() || M5.BtnB.wasPressed();
}

void resetPipes() {
  for (int i = 0; i < kPipeCount; ++i) {
    pipes[i].x      = gfx::screen_w + i * kPipeSpacing;
    pipes[i].gap_y  = 20 + (esp_random() % (ground_y - kPipeGap - 40));
    pipes[i].scored = false;
  }
}

void resetGame() {
  bird_x  = gfx::screen_w / 3;
  bird_y  = gfx::screen_h / 2.0f;
  bird_vy = 0.0f;
  score   = 0;
  resetPipes();
}

void drawBird(int x, int y, float vy) {
  gfx::canvas.fillCircle(x, y, kBirdRadius, kBird);
  gfx::canvas.drawCircle(x, y, kBirdRadius, kBirdEdge);
  gfx::canvas.fillCircle(x + 2, y - 1, 1, kBirdEdge);
  int beak_dy = (vy > 0) ? 1 : (vy < -1.5f ? -1 : 0);
  gfx::canvas.fillTriangle(x + kBirdRadius - 1, y - 1 + beak_dy,
                           x + kBirdRadius - 1, y + 1 + beak_dy,
                           x + kBirdRadius + 3, y     + beak_dy,
                           kBeak);
  gfx::canvas.fillRect(x - 3, y, 4, 2, kBirdEdge);
}

void drawPipe(const Pipe& p) {
  int x = (int)p.x;
  if (x + kPipeWidth < 0 || x > gfx::screen_w) return;

  gfx::canvas.fillRect(x, 0, kPipeWidth, p.gap_y, kPipe);
  gfx::canvas.drawRect(x, 0, kPipeWidth, p.gap_y, kPipeEdge);
  gfx::canvas.fillRect(x - 2, p.gap_y - 6, kPipeWidth + 4, 6, kPipe);
  gfx::canvas.drawRect(x - 2, p.gap_y - 6, kPipeWidth + 4, 6, kPipeEdge);

  int by = p.gap_y + kPipeGap;
  int bh = ground_y - by;
  gfx::canvas.fillRect(x, by, kPipeWidth, bh, kPipe);
  gfx::canvas.drawRect(x, by, kPipeWidth, bh, kPipeEdge);
  gfx::canvas.fillRect(x - 2, by, kPipeWidth + 4, 6, kPipe);
  gfx::canvas.drawRect(x - 2, by, kPipeWidth + 4, 6, kPipeEdge);
}

bool collides(const Pipe& p) {
  int bx0 = bird_x - kBirdRadius, bx1 = bird_x + kBirdRadius;
  int by0 = (int)bird_y - kBirdRadius, by1 = (int)bird_y + kBirdRadius;
  int px  = (int)p.x;
  if (bx1 < px || bx0 > px + kPipeWidth) return false;
  if (by0 < p.gap_y) return true;
  if (by1 > p.gap_y + kPipeGap) return true;
  return false;
}

void stepPlaying() {
  if (flapPressed()) { bird_vy = kFlapVelocity; M5.Speaker.tone(880, 40); }

  bird_vy += kGravity;
  if (bird_vy > kMaxFallSpeed) bird_vy = kMaxFallSpeed;
  bird_y  += bird_vy;

  if (bird_y + kBirdRadius >= ground_y) {
    bird_y = ground_y - kBirdRadius;
    state  = State::Dead;
    M5.Speaker.tone(160, 200);
    if (score > best) best = score;
    return;
  }
  if (bird_y - kBirdRadius < 0) {
    bird_y  = kBirdRadius;
    bird_vy = 0;
  }

  for (int i = 0; i < kPipeCount; ++i) {
    pipes[i].x -= kPipeSpeed;
    if (pipes[i].x + kPipeWidth < 0) {
      float max_x = pipes[0].x;
      for (int j = 1; j < kPipeCount; ++j) if (pipes[j].x > max_x) max_x = pipes[j].x;
      pipes[i].x      = max_x + kPipeSpacing;
      pipes[i].gap_y  = 20 + (esp_random() % (ground_y - kPipeGap - 40));
      pipes[i].scored = false;
    }
    if (!pipes[i].scored && pipes[i].x + kPipeWidth < bird_x) {
      pipes[i].scored = true;
      ++score;
      M5.Speaker.tone(1320, 80);
    }
    if (collides(pipes[i])) {
      state = State::Dead;
      M5.Speaker.tone(160, 200);
      if (score > best) best = score;
      return;
    }
  }
}

void drawScene() {
  gfx::canvas.fillRect(0, 0, gfx::screen_w, ground_y, kSky);
  for (int i = 0; i < kPipeCount; ++i) drawPipe(pipes[i]);
  gfx::canvas.fillRect(0, ground_y, gfx::screen_w, 4, kGrass);
  gfx::canvas.fillRect(0, ground_y + 4, gfx::screen_w, gfx::screen_h - ground_y - 4, kGround);
  drawBird(bird_x, (int)bird_y, bird_vy);

  gfx::canvas.setTextColor(TFT_WHITE, kSky);
  gfx::canvas.setTextDatum(top_center);
  gfx::canvas.setTextSize(2);
  gfx::canvas.drawNumber(score, gfx::screen_w / 2, 4);
}
}  // namespace

void enter() {
  ground_y = gfx::screen_h - 15;
  state    = State::Title;
  resetGame();
}

bool tick() {
  // Hold BtnB on title/dead screens to exit to menu.
  if (state != State::Playing && M5.BtnB.pressedFor(600)) return false;

  switch (state) {
    case State::Title:
      if (flapPressed()) {
        resetGame();
        bird_vy = kFlapVelocity;
        state   = State::Playing;
        M5.Speaker.tone(880, 40);
      }
      break;
    case State::Playing:
      stepPlaying();
      break;
    case State::Dead:
      if (flapPressed()) {
        resetGame();
        state = State::Title;
      }
      break;
  }

  drawScene();
  if (state == State::Title)
    gfx::drawCenteredText("FLAPPY", "press to start - hold B to exit", kSky);
  else if (state == State::Dead)
    gfx::drawCenteredText("GAME OVER", "press to retry - hold B to exit", kSky);

  return true;
}

}  // namespace flappy
