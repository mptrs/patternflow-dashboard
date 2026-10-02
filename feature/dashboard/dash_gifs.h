// Patternflow Dashboard - GIF clips between the dashboard screens.
//
// The browser does all the GIF work (decode, scale, convert) on the settings
// page and uploads two clips per GIF: one for portrait (64x128) and one for
// landscape (128x64). The panel only streams frames from flash.
//
// Clip file (/dashgif/<name>.p.dgf or .l.dgf), little-endian:
//   "DGF1"  uint16 width  uint16 height  uint16 frames  uint16 0
//   uint16 delay_ms[frames]
//   uint8  pixels[frames][height][width]   index into the fixed palette below
// Fixed palette: 6 red x 7 green x 6 blue levels, index = r*42 + g*6 + b.
#pragma once
#include <Arduino.h>
#include <FFat.h>
#include <string.h>
#include "../../src/core_mem.h"
#include "dash_gfx.h"

namespace DashGifs {

constexpr const char* DIR = "/dashgif";
constexpr int MAX_FRAMES = 120;
constexpr int MAX_CLIPS = 32;
constexpr int NAME_LEN = 24;

inline char names[MAX_CLIPS][NAME_LEN + 1];
inline int count = 0;
inline volatile bool listDirty = true;
inline int nextClip = 0;

inline DashGfx::RGB color(uint8_t i) {
  static const uint8_t g7[] = {0, 43, 85, 128, 170, 213, 255};
  if (i >= 252) return DashGfx::BLACK;
  return DashGfx::RGB{(uint8_t)(i / 42 * 51), g7[i / 6 % 7], (uint8_t)(i % 6 * 51)};
}

inline bool validName(const char* s) {
  const size_t n = strlen(s);
  if (n == 0 || n > NAME_LEN) return false;
  for (size_t i = 0; i < n; i++) {
    const char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  return true;
}

inline void path(char* out, size_t n, const char* name, bool portrait) {
  snprintf(out, n, "%s/%s.%c.dgf", DIR, name, portrait ? 'p' : 'l');
}

inline bool ensureDir() { return FFat.exists(DIR) || FFat.mkdir(DIR); }

// Collect the clip names (each GIF has a .p and an .l file).
inline void rescan() {
  listDirty = false;
  count = 0;
  File dir = FFat.open(DIR);
  if (!dir || !dir.isDirectory()) return;
  for (File f = dir.openNextFile(); f && count < MAX_CLIPS; f = dir.openNextFile()) {
    const char* full = f.name();
    const char* base = strrchr(full, '/');
    base = base ? base + 1 : full;
    const size_t n = strlen(base);
    if (n < 7 || strcmp(base + n - 6, ".p.dgf") != 0) continue;  // count each GIF once
    char name[NAME_LEN + 1] = {};
    strncpy(name, base, n - 6 < NAME_LEN ? n - 6 : NAME_LEN);
    if (!validName(name)) continue;
    strcpy(names[count++], name);
  }
  // simple sort, so the order is predictable
  for (int i = 1; i < count; i++)
    for (int j = i; j > 0 && strcmp(names[j - 1], names[j]) > 0; j--) {
      char t[NAME_LEN + 1];
      strcpy(t, names[j]);
      strcpy(names[j], names[j - 1]);
      strcpy(names[j - 1], t);
    }
}

inline bool any() {
  if (listDirty) rescan();
  return count > 0;
}

// ---------------------------------------------------------------- player
struct Player {
  File file;
  char name[NAME_LEN + 1] = "";
  int w = 0, h = 0, frames = 0, frame = -1, shown = -1, loops = 0;
  uint16_t delays[MAX_FRAMES];
  uint32_t dataStart = 0;
  float msInFrame = 0;
  uint8_t* pixels = nullptr;

  bool start(bool portrait) {
    stop();
    if (!any()) return false;
    const char* pick = names[nextClip++ % count];
    char p[64];
    path(p, sizeof p, pick, portrait);
    file = FFat.open(p, FILE_READ);
    uint8_t head[12];
    if (!file || file.read(head, 12) != 12 || memcmp(head, "DGF1", 4) != 0) return fail();
    w = head[4] | head[5] << 8;
    h = head[6] | head[7] << 8;
    frames = head[8] | head[9] << 8;
    if (w != (portrait ? 64 : 128) || h != (portrait ? 128 : 64) || frames < 1 || frames > MAX_FRAMES) return fail();
    if (file.read((uint8_t*)delays, frames * 2) != (size_t)frames * 2) return fail();
    for (int i = 0; i < frames; i++) delays[i] = delays[i] < 20 ? 20 : delays[i];
    dataStart = 12 + frames * 2;
    if (!pixels) pixels = (uint8_t*)PFMem::alloc(128 * 64);
    if (!pixels) return fail();
    strcpy(name, pick);
    frame = 0;
    shown = -1;
    loops = 0;
    msInFrame = 0;
    return true;
  }

  bool fail() {
    stop();
    return false;
  }

  void stop() {
    if (file) file.close();
    frames = 0;
    name[0] = 0;
  }

  bool playing() const { return frames > 0; }

  void advance(float dt) {
    if (!playing()) return;
    msInFrame += dt * 1000;
    while (msInFrame >= delays[frame]) {
      msInFrame -= delays[frame];
      if (++frame >= frames) {
        frame = 0;
        loops++;
      }
    }
  }

  void draw() {
    if (!playing()) return;
    if (shown != frame) {  // read a frame from flash only when it changes
      file.seek(dataStart + (uint32_t)frame * w * h);
      if (file.read(pixels, w * h) != (size_t)w * h) {
        stop();
        return;
      }
      shown = frame;
    }
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        const uint8_t i = pixels[y * w + x];
        if (i) DashGfx::px(x, y, color(i));
      }
  }
};

inline Player player;

// From the HTTP handlers (via PFLoopSync, so never mid-frame)
inline void removeClip(const char* name) {
  if (strcmp(player.name, name) == 0) player.stop();
  char p[64];
  path(p, sizeof p, name, true);
  FFat.remove(p);
  path(p, sizeof p, name, false);
  FFat.remove(p);
  listDirty = true;
}

}  // namespace DashGifs
