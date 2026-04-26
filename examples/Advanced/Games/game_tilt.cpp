#include "games.h"

namespace tilt {

constexpr int   kCell        = 8;
constexpr int   kHudHeight   = 12;
constexpr float kBallRadius  = 3.0f;
constexpr float kAccelGain   = 0.55f;
constexpr float kDamping     = 0.93f;
constexpr float kMaxSpeed    = 3.2f;
constexpr float kStuckEps    = 0.001f;

constexpr uint16_t kBg      = 0x10A2;
constexpr uint16_t kWall    = 0x6B6D;
constexpr uint16_t kBall    = 0xFFE0;
constexpr uint16_t kStart   = 0x528A;
constexpr uint16_t kExit    = 0x07E0;
constexpr uint16_t kHud     = 0x0000;
constexpr uint16_t kHudText = 0xFFFF;

enum class State : uint8_t { Title, Playing, Win };

namespace {

// 30 cols x 15 rows = 240x120 px game area below the 12px HUD.
// '#' wall, '.' open, 'S' start, 'E' exit. Each stage uses the same
// dimensions; corridors are 1 cell (8 px) wide, just larger than the
// 6 px ball.
constexpr int kRows = 15;
constexpr int kCols = 30;

const char* kStage1[kRows] = {
  "##############################",
  "#S...........................#",
  "#..###########################",
  "#............................#",
  "###########################..#",
  "#............................#",
  "#..###########################",
  "#............................#",
  "###########################..#",
  "#............................#",
  "#..###########################",
  "#............................#",
  "###########################..#",
  "#...........................E#",
  "##############################",
};

// Comb teeth: short walls with 1-cell gaps offset row-to-row, forcing
// the ball to zigzag through the open corridors between teeth rows.
const char* kStage2[kRows] = {
  "##############################",
  "#S...........................#",
  "##.##.##.##.##.##.##.##.##.###",
  "#............................#",
  "###.##.##.##.##.##.##.##.##.##",
  "#............................#",
  "##.##.##.##.##.##.##.##.##.###",
  "#............................#",
  "###.##.##.##.##.##.##.##.##.##",
  "#............................#",
  "##.##.##.##.##.##.##.##.##.###",
  "#............................#",
  "###.##.##.##.##.##.##.##.##.##",
  "#...........................E#",
  "##############################",
};

// Reverse serpentine: starts top-right and snakes down to bottom-left.
const char* kStage3[kRows] = {
  "##############################",
  "#...........................S#",
  "###########################..#",
  "#............................#",
  "#..###########################",
  "#............................#",
  "###########################..#",
  "#............................#",
  "#..###########################",
  "#............................#",
  "###########################..#",
  "#............................#",
  "#..###########################",
  "#E...........................#",
  "##############################",
};

constexpr int kStageCount = 3;
const char* const* const kStages[kStageCount] = {kStage1, kStage2, kStage3};

const char* const* maze;
int      current_stage;
float    bx, by, vx, vy;
int      start_cx, start_cy;
int      exit_cx,  exit_cy;
uint32_t start_ms, finish_ms;
State    state;
bool     imu_ok;

bool isWall(int cx, int cy) {
  if (cx < 0 || cy < 0 || cx >= kCols || cy >= kRows) return true;
  return maze[cy][cx] == '#';
}

void resetGame() {
  maze = kStages[current_stage];
  start_cx = exit_cx = 1;
  start_cy = 1;
  exit_cy  = kRows - 2;
  for (int y = 0; y < kRows; ++y) {
    for (int x = 0; x < kCols; ++x) {
      char c = maze[y][x];
      if (c == 'S') { start_cx = x; start_cy = y; }
      else if (c == 'E') { exit_cx = x; exit_cy = y; }
    }
  }
  bx = (start_cx + 0.5f) * kCell;
  by = (start_cy + 0.5f) * kCell;
  vx = vy = 0.0f;
  start_ms  = millis();
  finish_ms = 0;
}

void readTilt(float& ax, float& ay) {
  ax = ay = 0.0f;
  if (!imu_ok) return;
  M5.Imu.update();
  auto data = M5.Imu.getImuData();
  // Map physical accel axes to screen axes for landscape orientation
  // on M5StickC Plus. If the ball moves the wrong way on your unit,
  // flip the signs below or swap the two axes.
  ax =  data.accel.y;
  ay =  data.accel.x;
}

void stepPhysics() {
  float ax, ay;
  readTilt(ax, ay);

  vx = (vx + ax * kAccelGain) * kDamping;
  vy = (vy + ay * kAccelGain) * kDamping;
  if (vx >  kMaxSpeed) vx =  kMaxSpeed;
  if (vx < -kMaxSpeed) vx = -kMaxSpeed;
  if (vy >  kMaxSpeed) vy =  kMaxSpeed;
  if (vy < -kMaxSpeed) vy = -kMaxSpeed;

  const float r = kBallRadius;

  // Resolve x against the two cells the ball's bounding box overlaps.
  float new_x = bx + vx;
  int top = (int)((by - r) / kCell);
  int bot = (int)((by + r) / kCell);
  if (vx > 0) {
    int rc = (int)((new_x + r) / kCell);
    if (isWall(rc, top) || isWall(rc, bot)) {
      new_x = rc * kCell - r - kStuckEps;
      vx = 0;
    }
  } else if (vx < 0) {
    int lc = (int)((new_x - r) / kCell);
    if (isWall(lc, top) || isWall(lc, bot)) {
      new_x = (lc + 1) * kCell + r + kStuckEps;
      vx = 0;
    }
  }
  bx = new_x;

  float new_y = by + vy;
  int left  = (int)((bx - r) / kCell);
  int right = (int)((bx + r) / kCell);
  if (vy > 0) {
    int bc = (int)((new_y + r) / kCell);
    if (isWall(left, bc) || isWall(right, bc)) {
      new_y = bc * kCell - r - kStuckEps;
      vy = 0;
    }
  } else if (vy < 0) {
    int tc = (int)((new_y - r) / kCell);
    if (isWall(left, tc) || isWall(right, tc)) {
      new_y = (tc + 1) * kCell + r + kStuckEps;
      vy = 0;
    }
  }
  by = new_y;

  int cx = (int)(bx / kCell);
  int cy = (int)(by / kCell);
  if (cx == exit_cx && cy == exit_cy && state == State::Playing) {
    finish_ms = millis();
    state = State::Win;
    M5.Speaker.tone(880, 80);
    M5.delay(90);
    M5.Speaker.tone(1320, 140);
  }
}

void drawScene() {
  auto& c = gfx::canvas;
  c.fillScreen(kBg);

  c.fillRect(0, 0, gfx::screen_w, kHudHeight, kHud);
  c.setTextColor(kHudText);
  c.setTextDatum(top_left);
  c.setTextSize(1);
  uint32_t elapsed = (finish_ms ? finish_ms : millis()) - start_ms;
  char buf[32];
  snprintf(buf, sizeof(buf), "TILT MAZE  L%d  %lu.%02lus",
           current_stage + 1,
           (unsigned long)(elapsed / 1000),
           (unsigned long)((elapsed / 10) % 100));
  c.drawString(buf, 4, 2);

  for (int y = 0; y < kRows; ++y) {
    for (int x = 0; x < kCols; ++x) {
      char ch = maze[y][x];
      int px = x * kCell;
      int py = kHudHeight + y * kCell;
      if (ch == '#') {
        c.fillRect(px, py, kCell, kCell, kWall);
      } else if (ch == 'S') {
        c.fillRect(px + 1, py + 1, kCell - 2, kCell - 2, kStart);
      } else if (ch == 'E') {
        c.fillRect(px + 1, py + 1, kCell - 2, kCell - 2, kExit);
      }
    }
  }

  c.fillCircle((int)bx, kHudHeight + (int)by, (int)kBallRadius, kBall);
}

}  // namespace

void enter() {
  imu_ok = (M5.Imu.getType() != m5::imu_none);
  current_stage = 0;
  state = State::Title;
  resetGame();
}

bool tick() {
  if (state != State::Playing && M5.BtnB.pressedFor(600)) return false;

  if (!imu_ok) {
    gfx::canvas.fillScreen(kBg);
    gfx::drawCenteredText("NO IMU", "press B to return to menu", kBg);
    if (M5.BtnB.wasPressed()) return false;
    return true;
  }

  switch (state) {
    case State::Title:
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
        current_stage = 0;
        resetGame();
        state = State::Playing;
      }
      break;
    case State::Playing:
      stepPhysics();
      break;
    case State::Win:
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
        if (current_stage + 1 >= kStageCount) {
          current_stage = 0;
          resetGame();
          state = State::Title;
        } else {
          current_stage++;
          resetGame();
          state = State::Playing;
        }
      }
      break;
  }

  drawScene();

  if (state == State::Title)
    gfx::drawCenteredText("TILT MAZE", "tilt to roll - press to start", kBg);
  else if (state == State::Win) {
    if (current_stage + 1 >= kStageCount)
      gfx::drawCenteredText("ALL CLEAR!", "press to restart - hold B to exit", kBg);
    else
      gfx::drawCenteredText("STAGE CLEAR!", "press for next stage", kBg);
  }

  return true;
}

}  // namespace tilt
