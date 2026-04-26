#include "games.h"

namespace flappy {

constexpr float    kGravity      = 0.32f;
constexpr float    kFlapVelocity = -3.6f;
constexpr float    kMaxFallSpeed = 5.0f;
constexpr float    kPipeSpeed    = 1.6f;
constexpr int      kPipeWidth    = 24;
constexpr int      kPipeGap      = 54;
constexpr int      kPipeSpacing  = 86;
constexpr int      kPipeCount    = 4;
constexpr int      kBirdRadius   = 6;

// Enhanced Color Palette
constexpr uint16_t kSky        = 0x6D9F;
constexpr uint16_t kGround     = 0xDE0B;
constexpr uint16_t kGroundDark = 0xBA08;
constexpr uint16_t kGrass      = 0x6E45;
constexpr uint16_t kGrassDark  = 0x4D03;
constexpr uint16_t kPipe       = 0x4644;
constexpr uint16_t kPipeHigh   = 0x6706;
constexpr uint16_t kPipeEdge   = 0x2322;
constexpr uint16_t kBird       = 0xFEC0;
constexpr uint16_t kBirdShadow = 0xFA00;
constexpr uint16_t kBirdEdge   = 0x0000;
constexpr uint16_t kBeak       = 0xFC00;
constexpr uint16_t kCloud      = 0xFFFF;
constexpr uint16_t kSun        = 0xFFE0;

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
float    ground_x = 0;

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
  bird_x   = gfx::screen_w / 3;
  bird_y   = gfx::screen_h / 2.0f;
  bird_vy  = 0.0f;
  score    = 0;
  ground_x = 0;
  resetPipes();
}

void drawBird(int x, int y, float vy) {
  // Shadow / bottom of body
  gfx::canvas.fillCircle(x, y, kBirdRadius, kBirdShadow);
  gfx::canvas.fillCircle(x, y - 1, kBirdRadius - 1, kBird);
  gfx::canvas.drawCircle(x, y, kBirdRadius, kBirdEdge);
  
  // Eye
  gfx::canvas.fillCircle(x + 2, y - 2, 2, TFT_WHITE);
  gfx::canvas.drawPixel(x + 3, y - 2, kBirdEdge); // Pupil
  
  // Beak
  int beak_dy = (vy > 0) ? 1 : (vy < -1.5f ? -1 : 0);
  gfx::canvas.fillTriangle(x + kBirdRadius - 1, y - 1 + beak_dy,
                           x + kBirdRadius - 1, y + 3 + beak_dy,
                           x + kBirdRadius + 5, y + 1 + beak_dy,
                           kBeak);
  gfx::canvas.drawTriangle(x + kBirdRadius - 1, y - 1 + beak_dy,
                           x + kBirdRadius - 1, y + 3 + beak_dy,
                           x + kBirdRadius + 5, y + 1 + beak_dy,
                           kBirdEdge);
                           
  // Wing (animates based on velocity)
  int wing_dy = (vy < -1.0f) ? -2 : ((vy > 1.5f) ? 2 : 0);
  gfx::canvas.fillRoundRect(x - 4, y + wing_dy, 6, 4, 2, TFT_WHITE);
  gfx::canvas.drawRoundRect(x - 4, y + wing_dy, 6, 4, 2, kBirdEdge);
}

void drawPipeBody(int x, int y, int w, int h) {
  if (h <= 0) return;
  gfx::canvas.fillRect(x, y, w, h, kPipe);
  gfx::canvas.fillRect(x + 2, y, w / 4, h, kPipeHigh); // Highlight
  gfx::canvas.drawRect(x, y, w, h, kPipeEdge);
}

void drawPipe(const Pipe& p) {
  int x = (int)p.x;
  if (x + kPipeWidth < -4 || x > gfx::screen_w) return;

  // Top pipe
  drawPipeBody(x, 0, kPipeWidth, p.gap_y - 8);
  // Top pipe cap
  drawPipeBody(x - 2, p.gap_y - 8, kPipeWidth + 4, 8);

  // Bottom pipe
  int by = p.gap_y + kPipeGap;
  int bh = ground_y - by;
  // Bottom pipe cap
  drawPipeBody(x - 2, by, kPipeWidth + 4, 8);
  // Bottom pipe body
  drawPipeBody(x, by + 8, kPipeWidth, bh - 8);
}

bool collides(const Pipe& p) {
  // Hitbox slightly smaller than visual bird bounds
  int bx0 = bird_x - kBirdRadius + 1, bx1 = bird_x + kBirdRadius - 1;
  int by0 = (int)bird_y - kBirdRadius + 1, by1 = (int)bird_y + kBirdRadius - 1;
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

  ground_x -= kPipeSpeed;
  if (ground_x <= -20) ground_x += 20;

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
    if (pipes[i].x + kPipeWidth < -4) {
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
  // Sky
  gfx::canvas.fillRect(0, 0, gfx::screen_w, ground_y, kSky);
  
  // Pipes
  for (int i = 0; i < kPipeCount; ++i) drawPipe(pipes[i]);
  
  // Ground
  gfx::canvas.fillRect(0, ground_y, gfx::screen_w, 4, kGrass);
  gfx::canvas.fillRect(0, ground_y + 4, gfx::screen_w, gfx::screen_h - ground_y - 4, kGround);
  
  // Ground scrolling pattern (stripes)
  for (int x = (int)ground_x; x < gfx::screen_w + 20; x += 20) {
    gfx::canvas.drawLine(x, ground_y + 4, x - 10, gfx::screen_h, kGroundDark);
    gfx::canvas.drawLine(x + 1, ground_y + 4, x - 9, gfx::screen_h, kGroundDark);
  }
  // Grass scrolling trim
  for (int x = (int)ground_x; x < gfx::screen_w + 20; x += 10) {
    gfx::canvas.drawLine(x, ground_y, x + 4, ground_y + 3, kGrassDark);
  }
  
  // Bird
  drawBird(bird_x, (int)bird_y, bird_vy);

  // Score
  gfx::canvas.setTextColor(TFT_WHITE, kSky);
  gfx::canvas.setTextDatum(top_center);
  gfx::canvas.setTextSize(2);
  gfx::canvas.drawNumber(score, gfx::screen_w / 2, 4);
}
}  // namespace

void enter() {
  ground_y = gfx::screen_h - 20;
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