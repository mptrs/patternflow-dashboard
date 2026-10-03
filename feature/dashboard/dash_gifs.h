// Patternflow Dashboard - GIF clips between the dashboard screens.
//
// The browser does all the GIF work (decode, scale, convert) on the settings
// page and uploads two clips per GIF: one for portrait (64x128) and one for
// landscape (128x64). The panel reads the next clip into PSRAM in the
// background and plays it from there.
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

// ---------------------------------------------------------------- loading
// A clip is read into PSRAM by a task on core 0, in small chunks, while the
// screen before it is showing; it then plays from RAM. Reading frames from
// flash during playback would hold the flash (and with it, briefly, the other
// core and Wi-Fi) many times a second.
struct Clip {
  uint8_t* pixels = nullptr;  // frames * w * h, in PSRAM, allocated once at the largest size
  int w = 0, h = 0, frames = 0;
  bool portrait = true;
  char name[NAME_LEN + 1] = "";
  uint16_t delays[MAX_FRAMES];
};

constexpr size_t CLIP_BYTES = (size_t)MAX_FRAMES * 128 * 64;
inline Clip clips[2];
inline int playing_ = 0;                 // clips[playing_] is the one on screen
inline volatile int loadState = 0;       // 0 idle, 1 loading, 2 ready, 3 failed
inline Clip& loaded() { return clips[1 - playing_]; }

inline void loadTask(void*) {
  Clip& c = loaded();
  bool ok = false;
  char p[64];
  path(p, sizeof p, c.name, c.portrait);
  File f = FFat.open(p, FILE_READ);
  uint8_t head[12];
  if (f && f.read(head, 12) == 12 && memcmp(head, "DGF1", 4) == 0) {
    c.w = head[4] | head[5] << 8;
    c.h = head[6] | head[7] << 8;
    c.frames = head[8] | head[9] << 8;
    const bool shape = c.w == (c.portrait ? 64 : 128) && c.h == (c.portrait ? 128 : 64);
    if (shape && c.frames >= 1 && c.frames <= MAX_FRAMES &&
        f.read((uint8_t*)c.delays, c.frames * 2) == (size_t)c.frames * 2) {
      const size_t total = (size_t)c.frames * c.w * c.h;
      size_t done = 0;
      while (done < total) {
        const size_t n = total - done < 8192 ? total - done : 8192;
        if (f.read(c.pixels + done, n) != n) break;
        done += n;
        vTaskDelay(pdMS_TO_TICKS(2));  // let Wi-Fi and the web server in between chunks
      }
      ok = done == total;
      for (int i = 0; i < c.frames; i++) c.delays[i] = c.delays[i] < 20 ? 20 : c.delays[i];
    }
  }
  if (f) f.close();
  loadState = ok ? 2 : 3;
  vTaskDelete(nullptr);
}

// Have the next clip ready for this orientation. Cheap to call every frame.
inline void prepare(bool portrait) {
  if (loadState == 1 || !any()) return;
  Clip& c = loaded();
  if (loadState == 2 && c.portrait == portrait) return;  // already waiting
  if (!c.pixels) c.pixels = (uint8_t*)PFMem::alloc(CLIP_BYTES);
  if (!c.pixels) return;
  strcpy(c.name, names[nextClip % count]);
  c.portrait = portrait;
  loadState = 1;
  if (xTaskCreatePinnedToCore(loadTask, "dash_gif", 4096, nullptr, 1, nullptr, 0) != pdPASS) loadState = 3;
}

// After an upload or delete: a clip read before it may be stale.
inline void forget(const char* name) {
  if (loadState == 2 && strcmp(loaded().name, name) == 0) loadState = 0;
}

// ---------------------------------------------------------------- player
struct Player {
  Clip* clip = nullptr;
  char name[NAME_LEN + 1] = "";
  int frame = 0, loops = 0;
  bool wrapped = false;  // a loop ended during the last advance()
  float msInFrame = 0;

  // Plays the clip prepare() has loaded, if it is for this orientation.
  bool start(bool portrait) {
    stop();
    if (loadState == 3) {  // that one could not be read: try the next next time
      loadState = 0;
      nextClip++;
    }
    if (loadState != 2 || loaded().portrait != portrait) {
      prepare(portrait);
      return false;
    }
    playing_ = 1 - playing_;
    loadState = 0;
    nextClip++;
    clip = &clips[playing_];
    strcpy(name, clip->name);
    frame = 0;
    loops = 0;
    msInFrame = 0;
    return true;
  }

  void stop() {
    clip = nullptr;
    name[0] = 0;
  }

  bool playing() const { return clip != nullptr; }

  void advance(float dt) {
    wrapped = false;
    if (!playing()) return;
    msInFrame += dt * 1000;
    while (msInFrame >= clip->delays[frame]) {
      msInFrame -= clip->delays[frame];
      if (++frame >= clip->frames) {
        frame = 0;
        loops++;
        wrapped = true;
      }
    }
  }

  void draw() {
    if (!playing()) return;
    const int w = clip->w, h = clip->h;
    const uint8_t* px = clip->pixels + (size_t)frame * w * h;
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        const uint8_t i = px[y * w + x];
        if (i) DashGfx::px(x, y, color(i));
      }
  }
};

inline Player player;

// From the HTTP handlers (via PFLoopSync, so never mid-frame)
inline void removeClip(const char* name) {
  if (strcmp(player.name, name) == 0) player.stop();
  forget(name);
  char p[64];
  path(p, sizeof p, name, true);
  FFat.remove(p);
  path(p, sizeof p, name, false);
  FFat.remove(p);
  listDirty = true;
}

// Renames both files of a clip; nullptr when done, else what went wrong.
inline const char* renameClip(const char* from, const char* to) {
  if (strcmp(from, to) == 0) return nullptr;
  char a[64], b[64];
  path(b, sizeof b, to, true);
  if (FFat.exists(b)) return "that name is taken";
  path(a, sizeof a, from, true);
  if (!FFat.exists(a)) return "no such GIF";
  if (loadState == 1 && strcmp(loaded().name, from) == 0) return "it is being loaded: try again in a second";
  forget(from);
  if (!FFat.rename(a, b)) return "could not rename";
  path(a, sizeof a, from, false);
  path(b, sizeof b, to, false);
  FFat.rename(a, b);
  if (strcmp(player.name, from) == 0) strcpy(player.name, to);
  listDirty = true;
  return nullptr;
}

}  // namespace DashGifs
