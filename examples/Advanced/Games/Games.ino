// Mini-arcade for M5Unified — multiple games in one sketch.
//
// Currently includes:
//   - Flappy Bird (BtnA/BtnB to flap)
//   - Snake       (BtnA = turn left, BtnB = turn right)
//   - Tilt Maze   (tilt the device to roll the ball to the exit)
//   - Breakout    (BtnB = paddle left, BtnA = paddle right)
//   - T-Rex Run   (press any button to jump over cacti)
//
// Global controls (work in any game and in the menu):
//   BtnPWR tap  : power off the device (AXP192/AXP2101 PMU)
//
// Tested target: M5StickC Plus / Plus2 (135x240 -> rotated to 240x135).

#include <M5Unified.h>

// ---- Shared framework ------------------------------------------------------
namespace gfx {

M5Canvas canvas(&M5.Display);
int screen_w = 240;
int screen_h = 135;

constexpr uint32_t kFrameMicros = 16000;  // ~60 FPS

void drawCenteredText(const char* line1, const char* line2,
                      uint16_t bg = TFT_BLACK) {
  canvas.setTextColor(TFT_WHITE, bg);
  canvas.setTextDatum(middle_center);
  canvas.setTextSize(2);
  canvas.drawString(line1, screen_w / 2, screen_h / 2 - 14);
  canvas.setTextSize(1);
  canvas.drawString(line2, screen_w / 2, screen_h / 2 + 8);
}

}  // namespace gfx

// Each game implements these hooks.
struct Game {
  const char* name;
  void (*enter)();   // called once when entering this game
  bool (*tick)();    // called every frame; return false to exit back to menu
};

// ---- Forward declarations from the game modules ---------------------------
namespace flappy   { void enter(); bool tick(); }
namespace snake    { void enter(); bool tick(); }
namespace tilt     { void enter(); bool tick(); }
namespace breakout { void enter(); bool tick(); }
namespace dino     { void enter(); bool tick(); }

const Game kGames[] = {
  { "Flappy Bird", flappy::enter,   flappy::tick   },
  { "Snake",       snake::enter,    snake::tick    },
  { "Tilt Maze",   tilt::enter,     tilt::tick     },
  { "Breakout",    breakout::enter, breakout::tick },
  { "T-Rex Run",   dino::enter,     dino::tick     },
};
constexpr int kGameCount = sizeof(kGames) / sizeof(kGames[0]);

// ---- Menu ------------------------------------------------------------------
namespace menu {

int  selected = 0;
const Game* picked = nullptr;

void enter() {
  picked = nullptr;
}

bool tick() {
  if (M5.BtnB.wasPressed()) {
    selected = (selected + 1) % kGameCount;
  }
  if (M5.BtnA.wasPressed()) {
    picked = &kGames[selected];
    return false;  // signal "leave menu, dispatcher will enter the game"
  }

  auto& c = gfx::canvas;
  c.fillScreen(0x0008);  // very dark blue
  c.setTextColor(TFT_WHITE);
  c.setTextDatum(top_center);
  c.setTextSize(2);
  c.drawString("M5 Arcade", gfx::screen_w / 2, 6);

  c.setTextSize(1);
  c.setTextDatum(top_left);
  int y = 36;
  for (int i = 0; i < kGameCount; ++i) {
    if (i == selected) {
      c.fillRect(10, y - 2, gfx::screen_w - 20, 14, 0x4208);
      c.setTextColor(TFT_YELLOW);
    } else {
      c.setTextColor(TFT_WHITE);
    }
    c.drawString(kGames[i].name, 18, y);
    y += 16;
  }

  c.setTextColor(0x8410);
  c.setTextDatum(bottom_center);
  c.drawString("A: select   B: next   PWR: off",
               gfx::screen_w / 2, gfx::screen_h - 4);
  return true;  // stay in menu
}

}  // namespace menu

// ---- Power-off shortcut ----------------------------------------------------
void shutdownIfRequested() {
  if (M5.BtnPWR.wasClicked()) {
    gfx::canvas.fillScreen(TFT_BLACK);
    gfx::canvas.setTextColor(TFT_WHITE);
    gfx::canvas.setTextDatum(middle_center);
    gfx::canvas.setTextSize(2);
    gfx::canvas.drawString("bye!", gfx::screen_w / 2, gfx::screen_h / 2);
    gfx::canvas.pushSprite(0, 0);
    M5.Speaker.tone(440, 80);
    M5.delay(400);
    M5.Power.powerOff();
  }
}

// ---- Top-level dispatcher --------------------------------------------------
const Game* current_game = nullptr;
uint32_t next_frame_us;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  if (M5.Display.width() < M5.Display.height()) {
    M5.Display.setRotation(M5.Display.getRotation() ^ 1);
  }
  gfx::screen_w = M5.Display.width();
  gfx::screen_h = M5.Display.height();

  gfx::canvas.setColorDepth(M5.Display.getColorDepth());
  if (!gfx::canvas.createSprite(gfx::screen_w, gfx::screen_h)) {
    gfx::canvas.setColorDepth(8);
    gfx::canvas.createSprite(gfx::screen_w, gfx::screen_h);
  }
  gfx::canvas.setTextFont(&fonts::Font2);

  M5.Speaker.setVolume(160);

  menu::enter();
  current_game = nullptr;  // null = in menu
  next_frame_us = micros();
}

void loop() {
  M5.update();
  shutdownIfRequested();

  bool keep;
  if (current_game == nullptr) {
    keep = menu::tick();
    if (!keep && menu::picked) {
      current_game = menu::picked;
      current_game->enter();
    }
  } else {
    keep = current_game->tick();
    if (!keep) {
      current_game = nullptr;
      menu::enter();
    }
  }

  gfx::canvas.pushSprite(0, 0);

  next_frame_us += gfx::kFrameMicros;
  int32_t wait = (int32_t)(next_frame_us - micros());
  if (wait > 0) M5.delay(wait / 1000);
  else          next_frame_us = micros();
}

#if !defined(ARDUINO) && defined(ESP_PLATFORM)
extern "C" {
void loopTask(void*) {
  setup();
  for (;;) loop();
  vTaskDelete(NULL);
}
void app_main() {
  xTaskCreatePinnedToCore(loopTask, "loopTask", 8192, NULL, 1, NULL, 1);
}
}
#endif
