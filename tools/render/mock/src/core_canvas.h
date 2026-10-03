#pragma once
#include <stdint.h>
#include <string.h>
namespace PFCanvas {
inline int W = 128, H = 64;
inline uint8_t rgb[128 * 128 * 3];
inline void setFrame(int w, int h) { W = w; H = h; }
inline void clear() { memset(rgb, 0, sizeof rgb); }
inline void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (x < 0 || y < 0 || x >= W || y >= H) return;
  uint8_t* p = rgb + (y * W + x) * 3; p[0] = r; p[1] = g; p[2] = b;
}
inline void present() {}
}
