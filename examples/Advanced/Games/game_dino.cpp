#include "games.h"

namespace dino {

constexpr int   kHudHeight = 12;
constexpr int   kGroundY   = 115;
constexpr int   kDinoX     = 24;
constexpr int   kDinoW     = 14;
constexpr int   kDinoH     = 16;

constexpr float kGravity    = 0.45f;
constexpr float kJumpV      = -6.5f;
constexpr float kBaseSpeed  = 2.0f;
constexpr float kSpeedAccel = 0.0008f;
constexpr float kMaxSpeed   = 5.5f;
constexpr float kMinSpawnPx = 90.0f;
constexpr float kMaxSpawnPx = 170.0f;

constexpr int   kMaxObstacles = 6;

constexpr uint16_t kSky    = 0x10A2;  // dark blue-grey
constexpr uint16_t kFg     = 0xFFFF;  // white dino + ground
constexpr uint16_t kHud    = 0x0000;
constexpr uint16_t kHudTxt = 0xFFFF;

enum class State : uint8_t { Title, Playing, Dead };

struct Obstacle {
  bool    active;
  float   x;
  uint8_t w, h;
};

namespace {
float    dy, vy;
bool     on_ground;
float    speed;
float    spawn_timer;
float    bg_offset;
float    distance;
Obstacle obstacles[kMaxObstacles];
int      score, best;
State    state;

float frand(float lo, float hi) {
  return lo + (esp_random() & 0xffff) * (hi - lo) / 65536.0f;
}

void spawnObstacle() {
  for (int i = 0; i < kMaxObstacles; ++i) {
    if (obstacles[i].active) continue;
    obstacles[i].active = true;
    obstacles[i].x = gfx::screen_w + 2;
    if (esp_random() & 1) {
      obstacles[i].w = 5;
      obstacles[i].h = 9;
    } else {
      obstacles[i].w = 7;
      obstacles[i].h = 14;
    }
    return;
  }
}

void resetGame() {
  dy = vy = 0;
  on_ground = true;
  speed = kBaseSpeed;
  spawn_timer = 80;
  bg_offset = 0;
  distance = 0;
  score = 0;
  for (int i = 0; i < kMaxObstacles; ++i) obstacles[i].active = false;
}

bool overlap(int ax, int ay, int aw, int ah,
             int bx, int by, int bw, int bh) {
  return ax < bx + bw && ax + aw > bx &&
         ay < by + bh && ay + ah > by;
}

void stepPhysics() {
  if (on_ground && (M5.BtnA.wasPressed() || M5.BtnB.wasPressed())) {
    vy = kJumpV;
    on_ground = false;
    M5.Speaker.tone(880, 30);
  }
  if (!on_ground) {
    vy += kGravity;
    dy += vy;
    if (dy >= 0) { dy = 0; vy = 0; on_ground = true; }
  }

  speed += kSpeedAccel;
  if (speed > kMaxSpeed) speed = kMaxSpeed;
  distance += speed;
  score = (int)(distance / 10);

  for (int i = 0; i < kMaxObstacles; ++i) {
    if (!obstacles[i].active) continue;
    obstacles[i].x -= speed;
    if (obstacles[i].x + obstacles[i].w < 0) obstacles[i].active = false;
  }

  spawn_timer -= speed;
  if (spawn_timer <= 0) {
    spawnObstacle();
    spawn_timer = frand(kMinSpawnPx, kMaxSpawnPx);
  }

  bg_offset -= speed;
  while (bg_offset < -8) bg_offset += 8;

  int dino_top = kGroundY - kDinoH + (int)dy;
  for (int i = 0; i < kMaxObstacles; ++i) {
    if (!obstacles[i].active) continue;
    int ox = (int)obstacles[i].x;
    int oy = kGroundY - obstacles[i].h;
    if (overlap(kDinoX + 2, dino_top + 2, kDinoW - 4, kDinoH - 4,
                ox, oy, obstacles[i].w, obstacles[i].h)) {
      state = State::Dead;
      M5.Speaker.tone(140, 220);
      if (score > best) best = score;
      return;
    }
  }
}

void drawDino(int top) {
  auto& c = gfx::canvas;
  c.fillRect(kDinoX,              top + 4,            kDinoW - 4, kDinoH - 4, kFg);
  c.fillRect(kDinoX + 4,          top,                kDinoW - 4, 6,          kFg);
  c.fillRect(kDinoX + 1,          top + kDinoH - 4,   3,          4,          kFg);
  c.fillRect(kDinoX + kDinoW - 6, top + kDinoH - 4,   3,          4,          kFg);
  c.drawPixel(kDinoX + kDinoW - 3, top + 2, kSky);
}

void drawScene() {
  auto& c = gfx::canvas;
  c.fillScreen(kSky);

  c.fillRect(0, 0, gfx::screen_w, kHudHeight, kHud);
  c.setTextColor(kHudTxt);
  c.setTextDatum(top_left);
  c.setTextSize(1);
  char buf[40];
  snprintf(buf, sizeof(buf), "score %d   best %d", score, best);
  c.drawString(buf, 4, 2);

  c.drawFastHLine(0, kGroundY, gfx::screen_w, kFg);
  for (int x = (int)bg_offset; x < gfx::screen_w; x += 8) {
    c.drawPixel(x,     kGroundY + 3, kFg);
    c.drawPixel(x + 4, kGroundY + 6, kFg);
  }

  drawDino(kGroundY - kDinoH + (int)dy);

  for (int i = 0; i < kMaxObstacles; ++i) {
    if (!obstacles[i].active) continue;
    int ox = (int)obstacles[i].x;
    int oy = kGroundY - obstacles[i].h;
    c.fillRect(ox, oy, obstacles[i].w, obstacles[i].h, kFg);
    if (obstacles[i].h >= 12) {
      c.fillRect(ox - 2,                  oy + 3, 2, 4, kFg);
      c.fillRect(ox + obstacles[i].w,     oy + 5, 2, 4, kFg);
    }
  }
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
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
        resetGame();
        state = State::Title;
      }
      break;
  }

  drawScene();

  if (state == State::Title)
    gfx::drawCenteredText("T-REX RUN", "press to jump", kSky);
  else if (state == State::Dead)
    gfx::drawCenteredText("GAME OVER", "press to retry - hold B to exit", kSky);

  return true;
}

}  // namespace dino
