// ═══════════════════════════════════════════════════════════
// Patternflow Dashboard - the "Dashboard" pattern
//
// Shows up in the K4 pattern browser like any other pattern. For now it
// draws a big clock; the other screens follow.
//
// License: MIT
// ═══════════════════════════════════════════════════════════
#pragma once
#include <stdio.h>
#include "../../src/core_canvas.h"
#include "../../src/core_clock.h"

namespace Dashboard {

const char* NAME = "Dashboard";
const char* KNOB_LABELS[4] = {"screen", "-", "-", "-"};
constexpr bool ABSOLUTE_READY = false;

// Tiny 3x5 font: per glyph 15 bits, 5 rows of 3 pixels, top left first.
inline uint16_t glyph(char c) {
  switch (c) {
    case '0': return 0b111101101101111;
    case '1': return 0b010110010010111;
    case '2': return 0b111001111100111;
    case '3': return 0b111001111001111;
    case '4': return 0b101101111001001;
    case '5': return 0b111100111001111;
    case '6': return 0b111100111101111;
    case '7': return 0b111001010010010;
    case '8': return 0b111101111101111;
    case '9': return 0b111101111001111;
    case ':': return 0b000010000010000;
    case '-': return 0b000000111000000;
    default: return 0;
  }
}

inline void fillRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) PFCanvas::setPixel(xx, yy, r, g, b);
}

// Draws text with its top left corner at (x, y); `s` = pixel size.
inline void drawText(const char* text, int x, int y, int s, uint8_t r, uint8_t g, uint8_t b) {
  for (const char* p = text; *p; ++p) {
    uint16_t bits = glyph(*p);
    for (int i = 0; i < 15; i++)
      if (bits >> (14 - i) & 1) fillRect(x + (i % 3) * s, y + (i / 3) * s, s, s, r, g, b);
    x += 4 * s;
  }
}

inline int textWidth(const char* text, int s) {
  int n = 0;
  for (const char* p = text; *p; ++p) n++;
  return n ? (n * 4 - 1) * s : 0;
}

void setup() {}

void update(float /*dt*/, const InputFrame& /*input*/) {}

void draw() {
  const int w = PANEL_RES_W, h = PANEL_RES_H;
  PFCanvas::clear();
  char text[8];
  if (PatternflowClock::synced()) {
    snprintf(text, sizeof text, "%02d:%02d", PatternflowClock::hour(), PatternflowClock::minute());
  } else {
    snprintf(text, sizeof text, "--:--");
  }
  const int s = 4;
  drawText(text, (w - textWidth(text, s)) / 2, (h - 5 * s) / 2, s, 255, 255, 255);
  PFCanvas::present();
}

}  // namespace Dashboard
