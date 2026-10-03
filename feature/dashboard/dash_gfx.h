// Patternflow Dashboard - drawing: orientation, Patternflow's own pixel fonts,
// the moon and icon blits. Everything goes through PFCanvas.
#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "../../src/core_canvas.h"
#include "../../src/fonts/MatrixLight6.h"
#include "../../src/fonts/MatrixLight8X.h"

namespace DashGfx {

struct RGB {
  uint8_t r, g, b;
};

constexpr RGB BLACK{0, 0, 0}, WHITE{240, 240, 240}, GREY{125, 125, 135}, DIM{45, 45, 55};
constexpr RGB YELLOW{255, 200, 40}, ORANGE{255, 120, 30}, LIGHTBLUE{90, 170, 255};
constexpr RGB BLUE{40, 110, 230}, CYAN{0, 210, 210}, MOONSHADE{176, 164, 136};

const GFXfont* const SMALL = &MatrixLight6;
const GFXfont* const LARGE = &MatrixLight8X;

// Orientation: 0 landscape, 1 portrait, 2 landscape upside down, 3 portrait upside down
inline int W = 128, H = 64;
inline bool flipped = false;

inline void begin(int orientation) {
  const bool portrait = orientation & 1;
  flipped = orientation >= 2;
  W = portrait ? 64 : 128;
  H = portrait ? 128 : 64;
  PFCanvas::setFrame(W, H);  // a portrait frame is rotated onto the panel by the core
  PFCanvas::clear();
}

inline void px(int x, int y, RGB c) {
  if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) return;
  if (flipped) {
    x = W - 1 - x;
    y = H - 1 - y;
  }
  PFCanvas::setPixel(x, y, c.r, c.g, c.b);
}

inline void fillRect(int x, int y, int w, int h, RGB c) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) px(xx, yy, c);
}

inline void dotsH(int y, int x0 = 1, int x1 = -1, int step = 2) {
  if (x1 < 0) x1 = W;
  for (int x = x0; x < x1; x += step) px(x, y, DIM);
}

inline void dotsV(int x, int y0 = 1, int y1 = -1, int step = 2) {
  if (y1 < 0) y1 = H;
  for (int y = y0; y < y1; y += step) px(x, y, DIM);
}

// ---------------------------------------------------------------- text
inline int ascent(const GFXfont* f) {
  int a = 0;
  for (int c = f->first; c <= f->last; c++) {
    const int up = -f->glyph[c - f->first].yOffset;
    if (up > a) a = up;
  }
  return a;
}

inline int textWidth(const GFXfont* f, const char* s, int k = 1) {
  int w = 0;
  for (; *s; s++) {
    const uint8_t c = (uint8_t)*s;
    if (c >= f->first && c <= f->last) w += f->glyph[c - f->first].xAdvance;
  }
  return w * k;
}

// Draws text with the top of the line at y. align: 'l', 'c' (x = center) or 'r' (x = right edge).
inline void text(const GFXfont* f, const char* s, int x, int y, RGB col, int k = 1, char align = 'l') {
  if (align == 'c') x -= textWidth(f, s, k) / 2;
  if (align == 'r') x -= textWidth(f, s, k);
  const int baseline = y + ascent(f) * k;
  for (; *s; s++) {
    const uint8_t c = (uint8_t)*s;
    if (c < f->first || c > f->last) continue;
    const GFXglyph& g = f->glyph[c - f->first];
    const uint8_t* bits = f->bitmap + g.bitmapOffset;
    int bit = 0;
    for (int gy = 0; gy < g.height; gy++) {
      for (int gx = 0; gx < g.width; gx++, bit++) {
        if (bits[bit >> 3] & (0x80 >> (bit & 7))) {
          fillRect(x + (g.xOffset + gx) * k, baseline + (g.yOffset + gy) * k, k, k, col);
        }
      }
    }
    x += g.xAdvance * k;
  }
}

// A clock face of radius r centred on pixel (cx, cy): warm by day, dark blue by night.
inline void dial(int cx, int cy, int r, int hour, int minute, bool day) {
  const RGB face = day ? RGB{70, 52, 12} : RGB{12, 20, 58}, rim = day ? YELLOW : BLUE;
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++) {
      const float d = sqrtf((float)(x * x + y * y));
      if (d <= r + 0.3f) px(cx + x, cy + y, d > r - 0.9f ? rim : face);
    }
  // marks at 12, 3, 6 and 9
  for (int q = 0; q < 4; q++) {
    const float a = q * (float)M_PI / 2;
    px(cx + (int)lroundf(sinf(a) * (r - 2)), cy - (int)lroundf(cosf(a) * (r - 2)), rim);
  }
  auto hand = [&](float turns, float len, RGB c) {
    const float a = turns * 2 * (float)M_PI;
    const int steps = (int)(len * 2) + 1;
    for (int i = 0; i <= steps; i++) {
      const float t = len * i / steps;
      px(cx + (int)lroundf(sinf(a) * t), cy - (int)lroundf(cosf(a) * t), c);
    }
  };
  hand(((hour % 12) + minute / 60.0f) / 12, r * 0.5f, WHITE);
  hand(minute / 60.0f, r * 0.8f, WHITE);
  px(cx, cy, WHITE);
}

// ---------------------------------------------------------------- icons
// An icon is a size*size RGB buffer; black pixels are transparent.
inline void blit(const uint8_t* rgb, int size, int x, int y) {
  if (!rgb) return;
  for (int yy = 0; yy < size; yy++)
    for (int xx = 0; xx < size; xx++) {
      const uint8_t* p = rgb + (yy * size + xx) * 3;
      if (p[0] | p[1] | p[2]) px(x + xx, y + yy, RGB{p[0], p[1], p[2]});
    }
}

// ---------------------------------------------------------------- moon
constexpr int64_t NEW_MOON = 947182440;  // 6 Jan 2000 18:14 UTC, a known new moon
constexpr double SYNODIC = 29.530588853;

// 0 = new, 0.25 = first quarter, 0.5 = full, 0.75 = last quarter
inline float moonPhase(int64_t unixUtc) {
  const double days = (double)(unixUtc - NEW_MOON) / 86400.0;
  double p = fmod(days, SYNODIC) / SYNODIC;
  return (float)(p < 0 ? p + 1 : p);
}

// Percentage of the disc that is lit.
inline int moonLit(float phase) { return (int)lroundf((1 - cosf(2 * (float)M_PI * phase)) * 50); }

// Moon with radius r; pixels cx-r .. cx+r-1. Waxing = lit on the right (northern hemisphere).
inline void moon(int cx, int cy, int r, float phase) {
  static const float craters[][3] = {{-0.35f, -0.25f, 0.24f}, {0.25f, 0.2f, 0.18f}, {-0.1f, 0.5f, 0.15f},
                                     {0.35f, -0.4f, 0.12f},  {-0.5f, 0.2f, 0.1f},  {0.1f, -0.1f, 0.08f}};
  const float k = cosf(2 * (float)M_PI * phase);
  const bool waxing = phase < 0.5f;
  for (int py = -r; py < r; py++) {
    const float ny = (py + 0.5f) / r;
    const float edge = sqrtf(fmaxf(0.0f, 1 - ny * ny)) * k;
    for (int qx = -r; qx < r; qx++) {
      const float nx = (qx + 0.5f) / r;
      const float rr = nx * nx + ny * ny;
      if (rr > 1) continue;
      const bool lit = waxing ? nx > edge : nx < -edge;
      RGB c{40, 40, 56};
      if (lit) {
        const float shade = 0.8f + 0.2f * sqrtf(fmaxf(0.0f, 1 - rr));  // a little roundness
        float base[3] = {255, 242, 200};
        for (auto& cr : craters)
          if ((nx - cr[0]) * (nx - cr[0]) + (ny - cr[1]) * (ny - cr[1]) < cr[2] * cr[2]) {
            base[0] = 176, base[1] = 164, base[2] = 136;
            break;
          }
        c = RGB{(uint8_t)(base[0] * shade), (uint8_t)(base[1] * shade), (uint8_t)(base[2] * shade)};
      }
      px(cx + qx, cy + py, c);
    }
  }
}

}  // namespace DashGfx
