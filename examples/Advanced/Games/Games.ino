// Mini-arcade for M5Unified — multiple games in one sketch.
//
// Currently includes:
//   - Flappy Bird (BtnA/BtnB to flap)
//   - Snake       (BtnA = turn right, BtnB = turn left)
//   - Tilt Maze   (tilt the device to roll the ball to the exit)
//   - Breakout    (BtnB = paddle left, BtnA = paddle right)
//   - T-Rex Run   (press any button to jump over cacti)
//   - Pong        (BtnA = paddle up, BtnB = paddle down; first to 5)
//   - Stacker     (BtnA/BtnB = drop the moving block onto the tower)
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
namespace pong     { void enter(); bool tick(); }
namespace stacker  { void enter(); bool tick(); }

const Game kGames[] = {
  { "Flappy Bird", flappy::enter,   flappy::tick   },
  { "Snake",       snake::enter,    snake::tick    },
  { "Tilt Maze",   tilt::enter,     tilt::tick     },
  { "Breakout",    breakout::enter, breakout::tick },
  { "T-Rex Run",   dino::enter,     dino::tick     },
  { "Pong",        pong::enter,     pong::tick     },
  { "Stacker",     stacker::enter,  stacker::tick  },
};
constexpr int kGameCount = sizeof(kGames) / sizeof(kGames[0]);

// ---- Menu ------------------------------------------------------------------
namespace menu {

int  selected = 0;
const Game* picked = nullptr;

// Per-game accent colors.
constexpr uint16_t kAccent[] = {
  0xFFE0,  // Flappy Bird : yellow
  0x07E0,  // Snake       : green
  0x07FF,  // Tilt Maze   : cyan
  0xFD20,  // Breakout    : orange
  0xCE59,  // T-Rex Run   : tan
  0xFFFF,  // Pong        : white
  0x067F,  // Stacker     : blue
};
static_assert(sizeof(kAccent) / sizeof(kAccent[0]) >= kGameCount,
              "kAccent must cover every game");

// Halve each RGB565 component (subtle tint).
static inline uint16_t dim(uint16_t c, int shift = 1) {
  switch (shift) {
    case 1: return (c & 0xF7DEu) >> 1;
    case 2: return (c & 0xE79Cu) >> 2;
    default: return (c & 0xC718u) >> 3;
  }
}

// Draw a small illustration for the given game centered at (cx, cy).
void drawGameIcon(int idx, int cx, int cy, uint32_t t_ms) {
  auto& c = gfx::canvas;
  switch (idx) {
    case 0: {  // Flappy Bird
      int bob = (int)((t_ms / 120) % 4) - 2;
      cy += bob;
      c.fillCircle(cx - 1, cy, 12, 0xFFE0);                 // body
      c.fillCircle(cx + 5, cy + 4, 6, 0xFD20);              // wing
      c.fillCircle(cx + 5, cy - 3, 4, TFT_WHITE);           // eye
      c.fillCircle(cx + 6, cy - 3, 2, TFT_BLACK);
      c.fillTriangle(cx + 9, cy, cx + 16, cy - 2,
                     cx + 16, cy + 3, 0xF800);              // beak
      break;
    }
    case 1: {  // Snake
      uint16_t body = 0x07E0, head = 0x07FF;
      for (int i = 0; i < 4; ++i) {
        c.fillRoundRect(cx - 16 + i * 6, cy + 2, 8, 8, 2, body);
      }
      c.fillRoundRect(cx + 9, cy - 4, 10, 10, 3, head);     // head
      c.fillCircle(cx + 17, cy - 1, 1, TFT_BLACK);          // eye
      c.fillRect(cx + 18, cy + 1, 3, 1, 0xF800);            // tongue
      break;
    }
    case 2: {  // Tilt Maze
      int r = 16;
      c.drawRoundRect(cx - r, cy - r + 2, r * 2, r * 2 - 4, 3, 0x07FF);
      c.drawFastHLine(cx - r + 6, cy - 2, 14, 0x07FF);
      c.drawFastVLine(cx + 4, cy - r + 4, 12, 0x07FF);
      c.fillCircle(cx - 9, cy - 8, 3, 0xFFE0);              // ball
      c.drawRect(cx + 8, cy + 8, 6, 6, 0x07E0);             // exit
      break;
    }
    case 3: {  // Breakout
      static const uint16_t row_col[] = {0xF800, 0xFD20, 0xFFE0, 0x07E0};
      for (int r = 0; r < 4; ++r) {
        for (int b = 0; b < 5; ++b) {
          c.fillRect(cx - 17 + b * 7, cy - 14 + r * 4, 6, 3, row_col[r]);
        }
      }
      c.fillRect(cx - 7, cy + 10, 14, 2, TFT_WHITE);        // paddle
      c.fillCircle(cx + 4, cy + 4, 2, TFT_WHITE);           // ball
      break;
    }
    case 4: {  // T-Rex Run
      uint16_t dino = 0x8C71;
      c.fillRect(cx - 10, cy - 2, 11, 9, dino);             // body
      c.fillRect(cx - 1, cy - 8, 10, 9, dino);              // head
      c.fillRect(cx + 7, cy - 4, 3, 1, TFT_BLACK);          // mouth
      c.drawPixel(cx + 6, cy - 6, 0xFFFF);                  // eye
      c.fillTriangle(cx - 10, cy + 1, cx - 16, cy - 3,
                     cx - 10, cy + 6, dino);                // tail
      c.fillRect(cx - 7, cy + 7, 2, 5, dino);
      c.fillRect(cx - 1, cy + 7, 2, 5, dino);
      c.drawFastHLine(cx - 18, cy + 13, 36, 0x9CD3);        // ground
      // little cactus
      c.fillRect(cx + 13, cy + 7, 2, 6, 0x07E0);
      c.drawPixel(cx + 12, cy + 9, 0x07E0);
      c.drawPixel(cx + 16, cy + 10, 0x07E0);
      break;
    }
    case 5: {  // Pong
      c.fillRoundRect(cx - 17, cy - 8, 3, 16, 1, TFT_WHITE);
      c.fillRoundRect(cx + 14, cy - 4, 3, 16, 1, TFT_WHITE);
      for (int dy = -14; dy <= 14; dy += 4) {
        c.drawPixel(cx, cy + dy, 0x52AA);
      }
      int bx = (int)((t_ms / 30) % 24) - 12;
      c.fillCircle(cx + bx, cy - 2, 2, TFT_WHITE);
      break;
    }
    case 6: {  // Stacker
      static const uint16_t stk_col[] = {0xF81F, 0xFD20, 0x07FF, 0x07E0, 0xFFE0};
      int y = cy + 14;
      int wpx = 22;
      for (int r = 0; r < 5; ++r) {
        c.fillRoundRect(cx - wpx / 2, y - r * 5, wpx, 4, 1, stk_col[r]);
        if (r >= 2) wpx -= 4;
      }
      // moving block on top
      int mx = (int)((t_ms / 60) % 22) - 11;
      c.fillRoundRect(cx + mx - 3, cy - 14, 6, 4, 1, 0xFFFF);
      break;
    }
  }
}

void enter() {
  picked = nullptr;
}

bool tick() {
  if (M5.BtnB.wasPressed()) {
    selected = (selected + 1) % kGameCount;
  }
  if (M5.BtnA.wasPressed()) {
    picked = &kGames[selected];
    return false;
  }

  auto& c = gfx::canvas;
  const int W = gfx::screen_w;
  const int H = gfx::screen_h;
  const uint32_t t = millis();
  const uint16_t accent = kAccent[selected];

  // ---- background gradient (dark indigo -> deep violet) -----------
  for (int y = 0; y < H; ++y) {
    uint8_t r = 1 + (y * 4) / H;       // 1..5
    uint8_t g = (y * 2) / H;           // 0..2
    uint8_t b = 6 + (y * 10) / H;      // 6..16
    uint16_t col = (uint16_t)((r << 11) | (g << 5) | b);
    c.drawFastHLine(0, y, W, col);
  }

  // ---- header ------------------------------------------------------
  c.setTextDatum(top_left);
  c.setTextSize(1);
  c.setTextColor(0x18C3);  c.drawString("M5",     7, 5);
  c.setTextColor(0xFFE0);  c.drawString("M5",     6, 4);
  c.setTextColor(0x18C3);  c.drawString("ARCADE", 23, 5);
  c.setTextColor(0xFFFF);  c.drawString("ARCADE", 22, 4);

  // battery indicator
  int bat = M5.Power.getBatteryLevel();
  if (bat >= 0) {
    bool charging = (M5.Power.isCharging() == 1);
    uint16_t bcol = (bat <= 20) ? TFT_RED
                  : (bat <= 50) ? TFT_YELLOW
                                : 0x07E0;
    const int bw = 22, bh = 10;
    const int bx = W - bw - 6, by = 7;
    c.drawRoundRect(bx, by, bw, bh, 2, 0xC618);
    c.fillRect(bx + bw, by + 3, 2, 4, 0xC618);
    int fillw = (bat * (bw - 4)) / 100;
    if (fillw > 0) c.fillRect(bx + 2, by + 2, fillw, bh - 4, bcol);
    if (charging) {
      int lx = bx + bw / 2 - 1;
      int ly = by + 1;
      c.fillTriangle(lx + 2, ly,     lx - 1, ly + 4, lx + 2, ly + 4, TFT_WHITE);
      c.fillTriangle(lx + 1, ly + 4, lx + 4, ly + 4, lx + 1, ly + 8, TFT_WHITE);
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", bat);
    c.setTextSize(1);
    c.setTextColor(bcol);
    c.setTextDatum(middle_right);
    c.drawString(buf, bx - 4, by + bh / 2);
  }

  // separator
  c.drawFastHLine(0, 24, W, 0x2945);
  c.drawFastHLine(0, 25, W, 0x10A2);

  // ---- hero panel (left) ------------------------------------------
  const int hx = 5, hy = 29, hw = 124, hh = 82;
  c.fillRoundRect(hx, hy, hw, hh, 6, 0x10A2);
  c.fillRoundRect(hx + 2, hy + 2, hw - 4, hh - 4, 5, 0x0841);
  // pulsing accent border
  uint8_t glow = 180 + (uint8_t)(60 * ((t % 1600) < 800
                                        ? (t % 800) / 800.0f
                                        : 1.0f - (t % 800) / 800.0f));
  uint16_t border = ((accent & 0xF7DEu) >> 1)
                  + ((accent & 0xF7DEu) >> 1) * (glow > 220 ? 1 : 0);
  c.drawRoundRect(hx,     hy,     hw,     hh,     6, accent);
  c.drawRoundRect(hx + 1, hy + 1, hw - 2, hh - 2, 5, dim(accent, 1));
  (void)border;

  // game illustration
  drawGameIcon(selected, hx + hw / 2, hy + 18, t);

  // game name (split on space if it has one, drawn 2x)
  c.setTextDatum(top_center);
  const char* name = kGames[selected].name;
  const char* sp = strchr(name, ' ');
  int name_top = hy + 38;
  const int max_name_w = hw - 10;  // keep clear of rounded border

  auto fitSize = [&](const char* s) -> uint8_t {
    c.setTextSize(2);
    if (c.textWidth(s) <= max_name_w) return 2;
    return 1;
  };

  if (sp && (sp - name) > 0 && (sp - name) < 12 && strlen(sp + 1) < 12) {
    // Two-line layout: size 1 only — size 2 (~32px/line) overflows the panel.
    char buf[16];
    int n = (int)(sp - name);
    if (n > 15) n = 15;
    memcpy(buf, name, n);
    buf[n] = 0;
    c.setTextSize(1);
    const int line_h = 17;
    const int top1 = name_top + 4;
    c.setTextColor(0x18C3);    c.drawString(buf,    hx + hw / 2 + 1, top1 + 1);
    c.setTextColor(accent);    c.drawString(buf,    hx + hw / 2,     top1);
    c.setTextColor(0x18C3);    c.drawString(sp + 1, hx + hw / 2 + 1, top1 + line_h + 1);
    c.setTextColor(TFT_WHITE); c.drawString(sp + 1, hx + hw / 2,     top1 + line_h);
  } else {
    uint8_t sz = fitSize(name);
    c.setTextSize(sz);
    int top = (sz == 2) ? name_top + 4 : name_top + 12;
    c.setTextColor(0x18C3); c.drawString(name, hx + hw / 2 + 1, top + 1);
    c.setTextColor(accent); c.drawString(name, hx + hw / 2,     top);
  }

  // ---- list panel (right) -----------------------------------------
  const int lx = 134, ly = 29, lw = W - lx - 4, lh = 82;
  c.fillRoundRect(lx, ly, lw, lh, 4, 0x0820);
  c.drawRoundRect(lx, ly, lw, lh, 4, 0x2945);

  c.setTextFont(&fonts::Font0);  // small 6x8 font for the list
  c.setTextSize(1);
  c.setTextDatum(middle_left);

  const int row_h = 11;
  const int rows_top = ly + 4;
  for (int i = 0; i < kGameCount; ++i) {
    int y = rows_top + i * row_h;
    bool sel = (i == selected);
    uint16_t a = kAccent[i];
    if (sel) {
      c.fillRoundRect(lx + 2, y - 1, lw - 4, row_h, 2, dim(a, 2));
      c.fillRect    (lx + 2, y - 1, 3,        row_h, a);
      // pulsing chevron
      bool blink = ((t / 250) & 1) != 0;
      if (blink) {
        int cxr = lx + lw - 6;
        int cyr = y + row_h / 2 - 1;
        c.fillTriangle(cxr - 3, cyr - 3, cxr + 1, cyr,
                       cxr - 3, cyr + 3, a);
      }
    } else {
      // tiny color tick
      c.fillRect(lx + 3, y + row_h / 2 - 1, 2, 2, a);
    }
    c.setTextColor(sel ? TFT_WHITE : 0x9CD3);
    c.drawString(kGames[i].name, lx + 8, y + row_h / 2 - 1);
  }

  // ---- footer ------------------------------------------------------
  // (still on Font0 from the list panel, which fits the 13px footer band)
  const int help_top = H - 11;
  c.drawFastHLine(0, help_top - 1, W, 0x10A2);
  c.setTextSize(1);
  c.setTextDatum(bottom_center);
  c.setTextColor(0x9492);
  c.drawString("A play   B next   PWR off", W / 2, H - 2);

  // restore the global font for games
  c.setTextFont(&fonts::Font2);

  return true;
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
