// Patternflow Dashboard - weather icons drawn from signed distance functions.
// Same shapes as tools/icon_mockup.py. An icon is rendered once into an RGB
// buffer (supersampled for smooth edges) and only re-rendered when it changes.
#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>

namespace DashIcons {

enum Kind : uint8_t {
  CLEAR, MAINLY_CLEAR, PARTLY, OVERCAST, FOG, DRIZZLE, RAIN, SHOWERS, SNOW, SLEET, THUNDER,
  CLEAR_NIGHT, PARTLY_NIGHT, KIND_COUNT
};

// WMO weather code (Open-Meteo) -> icon
inline Kind fromWmo(int code, bool day) {
  if (code <= 1) return day ? (code == 0 ? CLEAR : MAINLY_CLEAR) : CLEAR_NIGHT;
  if (code == 2) return day ? PARTLY : PARTLY_NIGHT;
  if (code == 3) return OVERCAST;
  if (code == 45 || code == 48) return FOG;
  if (code >= 51 && code <= 55) return DRIZZLE;
  if (code == 56 || code == 57 || code == 66 || code == 67) return SLEET;
  if (code >= 61 && code <= 65) return RAIN;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return SNOW;
  if (code >= 80 && code <= 82) return day ? SHOWERS : RAIN;
  if (code >= 95) return THUNDER;
  return OVERCAST;
}

// ---------------------------------------------------------------- shapes
inline float clampf(float v, float a = 0, float b = 1) { return v < a ? a : v > b ? b : v; }
inline float circle(float x, float y, float cx, float cy, float r) { return hypotf(x - cx, y - cy) - r; }
inline float segment(float x, float y, float ax, float ay, float bx, float by, float r) {
  const float px = x - ax, py = y - ay, vx = bx - ax, vy = by - ay;
  const float h = clampf((px * vx + py * vy) / (vx * vx + vy * vy));
  return hypotf(px - vx * h, py - vy * h) - r;
}

struct Px {
  bool on = false;
  float r = 0, g = 0, b = 0;
  void paint(float d, float rr, float gg, float bb) {
    if (d <= 0) on = true, r = rr, g = gg, b = bb;
  }
};

inline void sun(Px& p, float x, float y, float cx, float cy, float rad, bool rays = true) {
  if (rays) {
    float a = atan2f(y - cy, x - cx);
    const float k = roundf(a / ((float)M_PI / 4)) * ((float)M_PI / 4);
    const float d = segment(x, y, cx + cosf(k) * (rad + 0.08f), cy + sinf(k) * (rad + 0.08f),
                            cx + cosf(k) * (rad + 0.19f), cy + sinf(k) * (rad + 0.19f), 0.035f);
    p.paint(d, 255, 150, 20);
  }
  const float t = clampf(hypotf(x - cx + rad * 0.3f, y - cy + rad * 0.3f) / (rad * 1.6f));
  p.paint(circle(x, y, cx, cy, rad), 255 + (255 - 255) * t, 236 + (150 - 236) * t, 120 + (20 - 120) * t);
}

inline void moonShape(Px& p, float x, float y, float cx, float cy, float rad, bool stars) {
  const float d = fmaxf(circle(x, y, cx, cy, rad), -circle(x, y, cx + rad * 0.55f, cy - rad * 0.35f, rad * 0.85f));
  p.paint(d, 255, 238, 190);
  if (stars) {
    p.paint(circle(x, y, 0.80f, 0.22f, 0.035f), 255, 255, 230);
    p.paint(circle(x, y, 0.72f, 0.50f, 0.025f), 255, 255, 230);
    p.paint(circle(x, y, 0.88f, 0.40f, 0.020f), 255, 255, 230);
  }
}

inline void cloud(Px& p, float x, float y, float ox, float oy, float s, bool dark = false) {
  float d = fminf(fminf(circle(x, y, ox + 0.30f * s, oy + 0.58f * s, 0.20f * s),
                        circle(x, y, ox + 0.52f * s, oy + 0.44f * s, 0.26f * s)),
                  circle(x, y, ox + 0.74f * s, oy + 0.60f * s, 0.18f * s));
  const float box = fmaxf(fabsf(x - (ox + 0.52f * s)) - 0.32f * s, fabsf(y - (oy + 0.68f * s)) - 0.10f * s);
  d = fminf(d, box);
  const float t = clampf((y - oy - 0.25f * s) / (0.55f * s));
  if (dark) p.paint(d, 175 + (95 - 175) * t, 180 + (100 - 180) * t, 200 + (125 - 200) * t);
  else p.paint(d, 250 + (175 - 250) * t, 252 + (188 - 252) * t, 255 + (210 - 255) * t);
}

// mode: 0 rain, 1 snow, 2 mixed
inline void drops(Px& p, float x, float y, int n, bool heavy, int mode) {
  static const float xs[] = {0.28f, 0.50f, 0.72f, 0.39f, 0.61f};
  for (int i = 0; i < n; i++) {
    const float x0 = xs[i], y0 = 0.74f + (i >= 3 ? 0.08f : 0);
    const bool flake = mode == 1 || (mode == 2 && (i & 1));
    if (flake) p.paint(circle(x, y, x0, y0 + 0.04f, 0.04f), 235, 245, 255);
    else p.paint(segment(x, y, x0 + 0.03f, y0 - 0.02f, x0 - 0.02f, y0 + (heavy ? 0.13f : 0.08f), 0.022f), 80, 170, 255);
  }
}

inline void bolt(Px& p, float x, float y) {
  static const float pts[][2] = {{0.52f, 0.56f}, {0.40f, 0.76f}, {0.50f, 0.76f}, {0.43f, 0.95f},
                                 {0.64f, 0.70f}, {0.53f, 0.70f}, {0.60f, 0.56f}};
  constexpr int n = 7;
  float d = 1e9f;
  bool inside = false;
  for (int i = 0; i < n; i++) {
    const float* a = pts[i];
    const float* b = pts[(i + 1) % n];
    d = fminf(d, segment(x, y, a[0], a[1], b[0], b[1], 0));
    if ((a[1] > y) != (b[1] > y) && x < (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]) + a[0]) inside = !inside;
  }
  p.paint(inside ? -d : d, 255, 220, 40);
}

inline void fog(Px& p, float x, float y) {
  static const float rows[][3] = {{0.42f, 0.15f, 0.85f}, {0.56f, 0.08f, 0.70f}, {0.70f, 0.25f, 0.92f}, {0.84f, 0.12f, 0.78f}};
  for (auto& r : rows) p.paint(segment(x, y, r[1], r[0], r[2], r[0], 0.035f), 170, 175, 185);
}

inline Px sample(Kind k, float x, float y) {
  Px p;
  switch (k) {
    case CLEAR: sun(p, x, y, 0.5f, 0.5f, 0.22f); break;
    case MAINLY_CLEAR: sun(p, x, y, 0.44f, 0.44f, 0.22f); cloud(p, x, y, 0.40f, 0.48f, 0.52f); break;
    case PARTLY: sun(p, x, y, 0.38f, 0.38f, 0.17f); cloud(p, x, y, 0.10f, 0.24f, 0.86f); break;
    case OVERCAST: cloud(p, x, y, 0.24f, 0.00f, 0.74f, true); cloud(p, x, y, 0.02f, 0.20f, 0.82f); break;
    case FOG: cloud(p, x, y, 0.14f, -0.08f, 0.70f); fog(p, x, y); break;
    case DRIZZLE: cloud(p, x, y, 0.08f, -0.04f, 0.84f); drops(p, x, y, 3, false, 0); break;
    case RAIN: cloud(p, x, y, 0.08f, -0.04f, 0.84f, true); drops(p, x, y, 5, true, 0); break;
    case SHOWERS: sun(p, x, y, 0.32f, 0.30f, 0.13f); cloud(p, x, y, 0.12f, 0.06f, 0.80f); drops(p, x, y, 3, true, 0); break;
    case SNOW: cloud(p, x, y, 0.08f, -0.04f, 0.84f); drops(p, x, y, 5, false, 1); break;
    case SLEET: cloud(p, x, y, 0.08f, -0.04f, 0.84f); drops(p, x, y, 5, false, 2); break;
    case THUNDER: cloud(p, x, y, 0.08f, -0.04f, 0.84f, true); bolt(p, x, y); break;
    case CLEAR_NIGHT: moonShape(p, x, y, 0.5f, 0.48f, 0.26f, true); break;
    case PARTLY_NIGHT: moonShape(p, x, y, 0.36f, 0.34f, 0.20f, false); cloud(p, x, y, 0.10f, 0.24f, 0.86f); break;
    default: break;
  }
  return p;
}

// Renders an icon into out (size*size*3 bytes), with ss*ss samples per pixel.
inline void render(Kind k, int size, uint8_t* out, int ss) {
  for (int py = 0; py < size; py++)
    for (int px = 0; px < size; px++) {
      float r = 0, g = 0, b = 0;
      for (int sy = 0; sy < ss; sy++)
        for (int sx = 0; sx < ss; sx++) {
          const Px p = sample(k, (px + (sx + 0.5f) / ss) / size, (py + (sy + 0.5f) / ss) / size);
          if (p.on) r += p.r, g += p.g, b += p.b;
        }
      const float n = (float)(ss * ss);
      uint8_t* o = out + (py * size + px) * 3;
      o[0] = (uint8_t)(r / n), o[1] = (uint8_t)(g / n), o[2] = (uint8_t)(b / n);
    }
}

// A cache slot: re-renders only when the kind changes.
struct Slot {
  int size = 0;
  int kind = -1;
  uint8_t* rgb = nullptr;
  const uint8_t* get(Kind k, int sz, void* (*alloc)(size_t)) {
    if (!rgb || size != sz) {
      rgb = (uint8_t*)alloc((size_t)sz * sz * 3);
      size = sz;
      kind = -1;
    }
    if (!rgb) return nullptr;
    if (kind != k) {
      render(k, sz, rgb, sz >= 48 ? 2 : 3);
      kind = k;
    }
    return rgb;
  }
};

}  // namespace DashIcons
