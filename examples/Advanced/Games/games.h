#pragma once
#include <M5Unified.h>

// Shared graphics globals defined in Games.ino.
namespace gfx {
extern M5Canvas canvas;
extern int      screen_w;
extern int      screen_h;
void drawCenteredText(const char* line1, const char* line2,
                      uint16_t bg = TFT_BLACK);
}

// Game module entry points. Each game returns false from tick() when it
// wants to exit back to the menu.
namespace flappy   { void enter(); bool tick(); }
namespace snake    { void enter(); bool tick(); }
namespace tilt     { void enter(); bool tick(); }
namespace breakout { void enter(); bool tick(); }
namespace dino     { void enter(); bool tick(); }
namespace pong     { void enter(); bool tick(); }
namespace stacker  { void enter(); bool tick(); }
