#include "games.h"

namespace dino {

constexpr int   kHudHeight = 12;
constexpr int   kGroundY   = 115;
constexpr int   kDinoX     = 24;
constexpr int   kDinoW     = 16;
constexpr int   kDinoH     = 17;

constexpr float kGravity    = 0.45f;
constexpr float kJumpV      = -6.5f;
constexpr float kBaseSpeed  = 2.0f;
constexpr float kSpeedAccel = 0.0008f;
constexpr float kMaxSpeed   = 5.5f;
constexpr float kMinSpawnPx = 90.0f;
constexpr float kMaxSpawnPx = 170.0f;

constexpr int   kMaxObstacles = 6;
constexpr int   kMaxClouds    = 3;

constexpr uint16_t kSky    = 0x10A2;  // dark blue-grey
constexpr uint16_t kFg     = 0xFFFF;  // white dino + ground
constexpr uint16_t kHud    = 0x0000;
constexpr uint16_t kHudTxt = 0xFFFF;

enum class State : uint8_t { Title, Playing, Dead };

struct Obstacle {
  bool    active;
  float   x;
  uint8_t w, h;
  uint8_t type;
  float   y_offset;
};

struct Cloud {
  bool  active;
  float x;
  int   y;
};

const uint16_t dino_stand_bmp[17] = {
  0x07FE, 0x07DF, 0x07FF, 0x07E0, 0x07FE, 0x43FE, 0x63FE, 0x73FE, 
  0x7FFF, 0x3FFF, 0x1FFF, 0x0FFF, 0x07FE, 0x03FC, 0x01B8, 0x01B8, 0x03EE
};

const uint16_t dino_run1_bmp[17] = {
  0x07FE, 0x07DF, 0x07FF, 0x07E0, 0x07FE, 0x43FE, 0x63FE, 0x73FE, 
  0x7FFF, 0x3FFF, 0x1FFF, 0x0FFF, 0x07FE, 0x03FC, 0x0180, 0x0188, 0x03C8
};

const uint16_t dino_run2_bmp[17] = {
  0x07FE, 0x07DF, 0x07FF, 0x07E0, 0x07FE, 0x43FE, 0x63FE, 0x73FE, 
  0x7FFF, 0x3FFF, 0x1FFF, 0x0FFF, 0x07FE, 0x03FC, 0x00B8, 0x00B0, 0x00E0
};

const uint16_t dino_dead_bmp[17] = {
  0x07FE, 0x075F, 0x07FF, 0x07E0, 0x07FE, 0x43FE, 0x63FE, 0x73FE, 
  0x7FFF, 0x3FFF, 0x1FFF, 0x0FFF, 0x07FE, 0x03FC, 0x01B8, 0x01B8, 0x03EE
};

const uint16_t cactus1_bmp[15] = {
  0x00C0, 0x00C0, 0x00C0, 0x0CC0, 0x0CC0, 0x0CC0, 0x0CC0, 0x0CCC, 
  0x0CCC, 0x0CCC, 0x0CFC, 0x0CFC, 0x0FFC, 0x0FFC, 0x00C0
}; // 12x15

const uint16_t cactus2_bmp[15] = {
  0x0C00, 0x0C00, 0x0C60, 0x0C60, 0x4C60, 0x4C60, 0x4C6C, 0x4C6C, 
  0x4C6C, 0x4CFC, 0x4CFC, 0x7FFC, 0x7FFC, 0x0C60, 0x0C60
}; // 16x15

const uint32_t cloud_bmp[8] = {
  0x00000780, 0x00000FC0, 0x0001EFE0, 0x0007FFFF, 
  0x000FFFFF, 0x001FFFFF, 0x001FFFFF, 0x000FFFFE
}; // 21x8

const uint16_t ptero1_bmp[11] = {
  0x0008, 0x0018, 0x0038, 0x0078, 0x00FC, 0x7FFF, 0x7FFF, 0x003E, 0x000C, 0x0008, 0x0000
}; // 16x11

const uint16_t ptero2_bmp[11] = {
  0x0000, 0x0000, 0x0180, 0x03C0, 0x07E0, 0x7FFF, 0x7FFF, 0x003E, 0x000C, 0x0008, 0x0000
};

namespace {
float    dy, vy;
bool     on_ground;
float    speed;
float    spawn_timer;
float    bg_offset;
float    distance;
Obstacle obstacles[kMaxObstacles];
Cloud    clouds_arr[kMaxClouds];
int      score, best;
State    state;
int      frame_tick = 0;

float frand(float lo, float hi) {
  return lo + (esp_random() & 0xffff) * (hi - lo) / 65536.0f;
}

void drawBitmap16(int x, int y, int w, int h, const uint16_t* bitmap, uint16_t color) {
  auto& c = gfx::canvas;
  for (int j = 0; j < h; ++j) {
    uint16_t row = bitmap[j];
    for (int i = 0; i < w; ++i) {
      if (row & (1 << (w - 1 - i))) {
        c.drawPixel(x + i, y + j, color);
      }
    }
  }
}

void drawBitmap32(int x, int y, int w, int h, const uint32_t* bitmap, uint16_t color) {
  auto& c = gfx::canvas;
  for (int j = 0; j < h; ++j) {
    uint32_t row = bitmap[j];
    for (int i = 0; i < w; ++i) {
      if (row & (1UL << (w - 1 - i))) {
        c.drawPixel(x + i, y + j, color);
      }
    }
  }
}

void spawnObstacle() {
  for (int i = 0; i < kMaxObstacles; ++i) {
    if (obstacles[i].active) continue;
    obstacles[i].active = true;
    obstacles[i].x = gfx::screen_w + 2;
    obstacles[i].y_offset = 0;
    
    int r = esp_random() % 3;
    if (score < 100 && r == 2) r = esp_random() % 2; // Pteros start later
    
    if (r == 0) {
      obstacles[i].w = 12; obstacles[i].h = 15; obstacles[i].type = 0;
    } else if (r == 1) {
      obstacles[i].w = 16; obstacles[i].h = 15; obstacles[i].type = 1;
    } else {
      obstacles[i].w = 16; obstacles[i].h = 11; obstacles[i].type = 2;
      obstacles[i].y_offset = (esp_random() % 3) * 12 + 10;
    }
    return;
  }
}

void spawnCloud() {
  for (int i = 0; i < kMaxClouds; ++i) {
    if (clouds_arr[i].active) continue;
    clouds_arr[i].active = true;
    clouds_arr[i].x = gfx::screen_w + esp_random() % 20;
    clouds_arr[i].y = 20 + esp_random() % 40;
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
  frame_tick = 0;
  for (int i = 0; i < kMaxObstacles; ++i) obstacles[i].active = false;
  for (int i = 0; i < kMaxClouds; ++i) clouds_arr[i].active = false;
}

bool overlap(int ax, int ay, int aw, int ah,
             int bx, int by, int bw, int bh) {
  // Relaxed hitbox for dino
  int hb_ax = ax + 2; int hb_ay = ay + 2;
  int hb_aw = aw - 4; int hb_ah = ah - 4;
  int hb_bx = bx + 2; int hb_by = by + 2;
  int hb_bw = bw - 4; int hb_bh = bh - 4;
  return hb_ax < hb_bx + hb_bw && hb_ax + hb_aw > hb_bx &&
         hb_ay < hb_by + hb_bh && hb_ay + hb_ah > hb_by;
}

void stepPhysics() {
  frame_tick++;
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

  for (int i = 0; i < kMaxClouds; ++i) {
    if (!clouds_arr[i].active) {
      if ((esp_random() % 100) == 0) spawnCloud();
    } else {
      clouds_arr[i].x -= (speed * 0.2f); // parallax
      if (clouds_arr[i].x + 21 < 0) clouds_arr[i].active = false;
    }
  }

  spawn_timer -= speed;
  if (spawn_timer <= 0) {
    spawnObstacle();
    spawn_timer = frand(kMinSpawnPx, kMaxSpawnPx);
  }

  bg_offset -= speed;
  while (bg_offset < -64) bg_offset += 64;

  int dino_top = kGroundY - kDinoH + (int)dy;
  for (int i = 0; i < kMaxObstacles; ++i) {
    if (!obstacles[i].active) continue;
    int ox = (int)obstacles[i].x;
    int oy = kGroundY - obstacles[i].h - obstacles[i].y_offset;
    if (overlap(kDinoX, dino_top, kDinoW, kDinoH,
                ox, oy, obstacles[i].w, obstacles[i].h)) {
      state = State::Dead;
      M5.Speaker.tone(140, 220);
      if (score > best) best = score;
      return;
    }
  }
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

  for (int i = 0; i < kMaxClouds; ++i) {
    if (clouds_arr[i].active) {
      drawBitmap32((int)clouds_arr[i].x, clouds_arr[i].y, 21, 8, cloud_bmp, kFg);
    }
  }

  c.drawFastHLine(0, kGroundY, gfx::screen_w, kFg);
  
  // Ground details
  for (int x = (int)bg_offset - 64; x < gfx::screen_w; x += 16) {
    int rx = x + 10000; // Keep positive
    int rnd = (rx * 137) % 7;
    if (rnd == 0) c.drawFastHLine(x, kGroundY + 2, 4, kFg);
    else if (rnd == 1) c.drawFastHLine(x, kGroundY + 1, 2, kFg);
    else if (rnd == 2) c.drawPixel(x, kGroundY + 3, kFg);
  }

  int dino_top = kGroundY - kDinoH + (int)dy;
  if (state == State::Dead) {
    drawBitmap16(kDinoX, dino_top, kDinoW, kDinoH, dino_dead_bmp, kFg);
  } else if (!on_ground || state == State::Title) {
    drawBitmap16(kDinoX, dino_top, kDinoW, kDinoH, dino_stand_bmp, kFg);
  } else {
    if ((frame_tick / 6) % 2 == 0) {
      drawBitmap16(kDinoX, dino_top, kDinoW, kDinoH, dino_run1_bmp, kFg);
    } else {
      drawBitmap16(kDinoX, dino_top, kDinoW, kDinoH, dino_run2_bmp, kFg);
    }
  }

  for (int i = 0; i < kMaxObstacles; ++i) {
    if (!obstacles[i].active) continue;
    int ox = (int)obstacles[i].x;
    int oy = kGroundY - obstacles[i].h - obstacles[i].y_offset;
    if (obstacles[i].type == 0) {
      drawBitmap16(ox, oy, obstacles[i].w, obstacles[i].h, cactus1_bmp, kFg);
    } else if (obstacles[i].type == 1) {
      drawBitmap16(ox, oy, obstacles[i].w, obstacles[i].h, cactus2_bmp, kFg);
    } else {
      if ((frame_tick / 10) % 2 == 0) {
        drawBitmap16(ox, oy, obstacles[i].w, obstacles[i].h, ptero1_bmp, kFg);
      } else {
        drawBitmap16(ox, oy, obstacles[i].w, obstacles[i].h, ptero2_bmp, kFg);
      }
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
