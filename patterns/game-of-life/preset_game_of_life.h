#pragma once

// ===== Patternflow pattern =====
// Title:   Game of Life
// Author:  Michiel Peters
// Date:    2026-10-02
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================
//
// Conway's Game of Life on a wrap-around world. Cells fade in when they are
// born, shift color as they age and leave a fading trail when they die. A
// world runs until it is truly finished (the whole grid repeats), then it
// dissolves and a new one grows.
//
// Knob 1: speed (generations per second) · click: pause / resume
// Knob 2: density of a new world          · click: new world (next seed style)
// Knob 3: color theme                     · click: next rule (Life, HighLife, Day & Night)
// Knob 4: trail length                    · click: sprinkle live cells

#include <Arduino.h>
#include "config.h"
#include "../src/core_display.h"
#include "../src/core_encoders.h"
#include "../src/core_canvas.h"
#include "../src/core_mem.h"
#include "../src/core_params.h"

namespace GameOfLife {

const char* NAME = "Game of Life";
const char* const KNOB_LABELS[4] = {"Speed", "Density", "Theme", "Trail"};
constexpr bool ABSOLUTE_READY = true;

constexpr int W = PANEL_RES_W;
constexpr int H = PANEL_RES_H;
constexpr int N = W * H;
// Remember this many generations to recognise repetition. A glider needs
// 4 * 128 = 512 generations for one lap of this wrap-around world.
constexpr int HISTORY = 1024;
// The world must show only earlier states this many generations in a row
// before it counts as finished (an accidental hash match resets the count).
constexpr int STALE_GENS = 40;
constexpr int MAX_AGE = 60;          // color keeps shifting up to this age
constexpr float FADE_SECONDS = 1.2f; // dissolve / grow between worlds

// Rules as bit masks over the number of live neighbors (bit n = n neighbors).
struct Rule { uint16_t born, survive; };
constexpr Rule RULES[] = {
    {1 << 3, (1 << 2) | (1 << 3)},                                                         // Life      B3/S23
    {(1 << 3) | (1 << 6), (1 << 2) | (1 << 3)},                                           // HighLife  B36/S23
    {(1 << 3) | (1 << 6) | (1 << 7) | (1 << 8), (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 8)},  // Day & Night
};
constexpr int RULE_COUNT = sizeof(RULES) / sizeof(RULES[0]);

constexpr int THEMES = 4;
// Per theme: 5 color stops from young to old, then the trail color.
static const uint8_t PALETTE[THEMES][6][3] = {
    {{255, 255, 255}, {96, 224, 255}, {32, 140, 255}, {40, 60, 230}, {120, 40, 200}, {40, 60, 120}},   // ocean
    {{255, 255, 170}, {255, 210, 0}, {255, 130, 0}, {230, 50, 0}, {140, 10, 30}, {120, 40, 0}},        // fire
    {{220, 255, 220}, {80, 255, 80}, {20, 200, 60}, {0, 140, 110}, {0, 80, 90}, {20, 90, 20}},         // matrix
    {{255, 255, 255}, {255, 70, 200}, {190, 70, 255}, {80, 90, 255}, {0, 190, 220}, {90, 30, 110}},    // neon
};

// Knob parameters
static float speed = 8.0f;     // generations per second
static float density = 0.33f;  // chance a cell starts alive
static int theme = 0;
static float trail = 0.75f;    // how much of the trail survives each generation
static int rule = 0;
static int seedStyle = 0;      // 0 random, 1 mirrored, 2 methuselahs

// World state (allocated in setup)
static uint8_t* cur = nullptr;
static uint8_t* nxt = nullptr;
static uint8_t* age = nullptr;    // 0 = dead, 1..MAX_AGE = age of a live cell
static uint8_t* ghost = nullptr;  // trail brightness of dead cells, 0..255
static uint32_t* history = nullptr;
static uint8_t lut[MAX_AGE + 1][3];  // age -> color for the current theme
static int lutTheme = -1;
static int histIndex = 0;
static int stale = 0;
static float pending = 0.0f;   // generations owed (fractional): also the fade-in phase
static float fade = 1.0f;      // whole-world brightness during a world change
static int fadeDir = 0;        // -1 dissolving, +1 growing, 0 steady
static bool paused = false;

static void buildLut() {
  const uint8_t(*p)[3] = PALETTE[theme];
  for (int a = 1; a <= MAX_AGE; a++) {
    // ages 1..MAX_AGE spread over the 5 stops, faster at the young end
    float t = sqrtf((float)(a - 1) / (MAX_AGE - 1)) * 4.0f;
    int i = (int)t;
    if (i > 3) i = 3;
    float f = t - i;
    for (int c = 0; c < 3; c++) lut[a][c] = (uint8_t)(p[i][c] + (p[i + 1][c] - p[i][c]) * f);
  }
  lutTheme = theme;
}

static uint32_t fingerprint() {
  uint32_t h = 2166136261u;  // FNV-1a
  for (int i = 0; i < N; i++) h = (h ^ cur[i]) * 16777619u;
  return h;
}

static void setCell(int x, int y) {
  const int i = ((y + H) % H) * W + (x + W) % W;
  cur[i] = 1;
  age[i] = 1;
}

static void seedWorld() {
  for (int i = 0; i < N; i++) {
    cur[i] = 0;
    age[i] = 0;
    ghost[i] = 0;
  }
  const long limit = (long)(density * 1000.0f);
  if (seedStyle == 0) {  // random soup
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++)
        if (random(1000) < limit) setCell(x, y);
  } else if (seedStyle == 1) {  // mirrored soup in the middle: grows like a kaleidoscope
    const int hw = W / 4, hh = H / 4;
    for (int y = 0; y < hh; y++)
      for (int x = 0; x < hw; x++)
        if (random(1000) < limit) {
          setCell(W / 2 - 1 - x, H / 2 - 1 - y);
          setCell(W / 2 + x, H / 2 - 1 - y);
          setCell(W / 2 - 1 - x, H / 2 + y);
          setCell(W / 2 + x, H / 2 + y);
        }
  } else {  // a few methuselahs: tiny seeds that grow for hundreds of generations
    static const int8_t R_PENTOMINO[][2] = {{1, 0}, {2, 0}, {0, 1}, {1, 1}, {1, 2}};
    static const int8_t ACORN[][2] = {{1, 0}, {3, 1}, {0, 2}, {1, 2}, {4, 2}, {5, 2}, {6, 2}};
    const int count = 1 + random(3);
    for (int k = 0; k < count; k++) {
      const int ox = random(W), oy = random(H);
      if (random(2)) {
        for (auto& c : R_PENTOMINO) setCell(ox + c[0], oy + c[1]);
      } else {
        for (auto& c : ACORN) setCell(ox + c[0], oy + c[1]);
      }
    }
  }
  for (int i = 0; i < HISTORY; i++) history[i] = 0;
  histIndex = 0;
  stale = 0;
  pending = 0.0f;
}

// Start a world change: dissolve the current world, then grow the next one.
static void changeWorld() {
  if (fadeDir == 0) fadeDir = -1;
}

static void sprinkle() {
  const int cx = random(W), cy = random(H);
  for (int dy = -4; dy <= 4; dy++)
    for (int dx = -4; dx <= 4; dx++)
      if (random(2)) {
        const int i = ((cy + dy + H) % H) * W + (cx + dx + W) % W;
        if (!cur[i]) {
          cur[i] = 1;
          age[i] = 1;
        }
      }
  stale = 0;
}

static void step() {
  const Rule r = RULES[rule];
  for (int y = 0; y < H; y++) {
    const uint8_t* up = cur + ((y + H - 1) % H) * W;
    const uint8_t* row = cur + y * W;
    const uint8_t* dn = cur + ((y + 1) % H) * W;
    // Column sums over 3 rows; walk them with a sliding window.
    int left = up[W - 1] + row[W - 1] + dn[W - 1];
    int mid = up[0] + row[0] + dn[0];
    for (int x = 0; x < W; x++) {
      const int xr = (x + 1) % W;
      const int right = up[xr] + row[xr] + dn[xr];
      const int i = y * W + x;
      const int n = left + mid + right - row[x];  // live neighbors
      if (cur[i]) {
        if (r.survive >> n & 1) {
          nxt[i] = 1;
          if (age[i] < MAX_AGE) age[i]++;
        } else {  // dies, leaves a trail
          nxt[i] = 0;
          age[i] = 0;
          ghost[i] = 255;
        }
      } else if (r.born >> n & 1) {
        nxt[i] = 1;
        age[i] = 1;
        ghost[i] = 0;
      } else {
        nxt[i] = 0;
      }
      left = mid;
      mid = right;
    }
  }
  uint8_t* t = cur;
  cur = nxt;
  nxt = t;

  // Finished? Once the whole world equals an earlier state, it repeats forever.
  const uint32_t h = fingerprint();
  bool seen = false;
  for (int i = 0; i < HISTORY; i++)
    if (history[i] == h) {
      seen = true;
      break;
    }
  if (seen) {
    stale++;
  } else {
    stale = 0;
    history[histIndex] = h;
    histIndex = (histIndex + 1) % HISTORY;
  }
  if (stale > STALE_GENS) changeWorld();
}

void setup() {
  randomSeed(esp_random());
  if (!cur) {
    cur = (uint8_t*)PFMem::alloc(N);
    nxt = (uint8_t*)PFMem::alloc(N);
    age = (uint8_t*)PFMem::alloc(N);
    ghost = (uint8_t*)PFMem::alloc(N);
    history = (uint32_t*)PFMem::alloc(HISTORY * sizeof(uint32_t));
  }
  if (!cur || !nxt || !age || !ghost || !history) return;
  seedWorld();
  fade = 0.0f;
  fadeDir = 1;
}

void update(float dt, const InputFrame& input) {
  if (!cur || !nxt || !age || !ghost || !history) return;  // allocation failed

  PFParams::apply(input, 0, &speed, 1.0f, 60.0f, 1.0f);
  PFParams::apply(input, 1, &density, 0.05f, 0.6f, 0.01f);
  PFParams::applyInt(input, 2, &theme, 0, THEMES - 1, 1, true);
  PFParams::apply(input, 3, &trail, 0.0f, 0.95f, 0.05f);

  if (input.btnPressed[0]) paused = !paused;
  if (input.btnPressed[1]) {
    seedStyle = (seedStyle + 1) % 3;
    changeWorld();
  }
  if (input.btnPressed[2]) {
    rule = (rule + 1) % RULE_COUNT;
    stale = 0;
  }
  if (input.btnPressed[3]) sprinkle();

  // World change: dissolve, reseed, grow
  if (fadeDir != 0) {
    fade += fadeDir * dt / FADE_SECONDS;
    if (fade <= 0.0f) {
      fade = 0.0f;
      seedWorld();
      fadeDir = 1;
    } else if (fade >= 1.0f) {
      fade = 1.0f;
      fadeDir = 0;
    }
  }

  // Trails fade per frame, so they look smooth at every speed
  if (!paused) {
    const float keep = powf(trail, dt * speed);
    const int k = (int)(keep * 256.0f);
    for (int i = 0; i < N; i++)
      if (ghost[i] && !cur[i]) ghost[i] = (uint8_t)((ghost[i] * k) >> 8);
  }

  if (paused || fadeDir < 0) return;  // no new generations while dissolving
  pending += dt * speed;
  int steps = 0;
  while (pending >= 1.0f && steps < 4) {  // never more than 4 per frame
    step();
    pending -= 1.0f;
    steps++;
  }
  if (pending > 4.0f) pending = 0.0f;  // can't keep up: drop the backlog
}

void draw() {
  if (!cur) return;
  if (lutTheme != theme) buildLut();
  const uint8_t* tc = PALETTE[theme][5];
  // Newborn cells fade in during their first generation (slow speeds only).
  const float born = (speed < 12.0f && !paused) ? fminf(1.0f, pending * 2.0f) : 1.0f;
  const int bornK = (int)(born * fade * 256.0f);
  const int fadeK = (int)(fade * 256.0f);
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      const int i = y * W + x;
      if (cur[i]) {
        const uint8_t* c = lut[age[i]];
        const int k = age[i] == 1 ? bornK : fadeK;
        PFCanvas::setPixel(x, y, (c[0] * k) >> 8, (c[1] * k) >> 8, (c[2] * k) >> 8);
      } else if (ghost[i]) {
        const int g = (ghost[i] * fadeK) >> 8;
        PFCanvas::setPixel(x, y, (tc[0] * g) >> 8, (tc[1] * g) >> 8, (tc[2] * g) >> 8);
      } else {
        PFCanvas::setPixel(x, y, 0, 0, 0);
      }
    }
  }
  PFCanvas::present();
}

}  // namespace GameOfLife
