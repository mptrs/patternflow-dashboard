#pragma once

// ===== Patternflow pattern =====
// Title:   Game of Life
// Author:  Michiel Peters
// Date:    2026-10-02
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================
//
// Conway's Game of Life on a wrap-around world. Cells are colored by age and
// leave a fading trail when they die. A world runs until it is truly finished
// (the whole grid repeats), then a new one starts.
//
// Knob 1: speed (generations per second) · click: pause / resume
// Knob 2: density of a new world          · click: new world
// Knob 3: color theme                     · click: next theme
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
constexpr int THEMES = 4;

// Colors per theme: age 1 (just born) .. age 6 (old), then the trail color.
static const uint8_t PALETTE[THEMES][7][3] = {
    {{255, 255, 255}, {96, 224, 255}, {32, 160, 255}, {16, 96, 224}, {8, 48, 176}, {4, 28, 128}, {40, 60, 120}},   // ocean
    {{255, 255, 160}, {255, 208, 0}, {255, 144, 0}, {255, 80, 0}, {208, 32, 0}, {144, 16, 0}, {120, 40, 0}},      // fire
    {{208, 255, 208}, {64, 255, 64}, {16, 208, 16}, {8, 160, 8}, {4, 112, 4}, {2, 72, 2}, {20, 90, 20}},          // matrix
    {{255, 255, 255}, {255, 64, 192}, {192, 64, 255}, {112, 64, 255}, {64, 96, 255}, {32, 128, 192}, {90, 30, 110}}, // neon
};

// Knob parameters
static float speed = 8.0f;     // generations per second
static float density = 0.33f;  // chance a cell starts alive
static int theme = 0;
static float trail = 0.75f;    // how much of the trail survives each generation

// World state (allocated in setup)
static uint8_t* cur = nullptr;
static uint8_t* nxt = nullptr;
static uint8_t* age = nullptr;    // 0 = dead, 1..6 = age of a live cell
static uint8_t* ghost = nullptr;  // trail brightness of dead cells, 0..255
static uint32_t* history = nullptr;
static int histIndex = 0;
static int stale = 0;
static float pending = 0.0f;      // generations owed (fractional)
static bool paused = false;

static uint32_t fingerprint() {
  uint32_t h = 2166136261u;  // FNV-1a
  for (int i = 0; i < N; i++) h = (h ^ cur[i]) * 16777619u;
  return h;
}

static void newWorld() {
  const long limit = (long)(density * 1000.0f);
  for (int i = 0; i < N; i++) {
    uint8_t v = random(1000) < limit ? 1 : 0;
    cur[i] = v;
    age[i] = v;
    ghost[i] = 0;
  }
  for (int i = 0; i < HISTORY; i++) history[i] = 0;
  histIndex = 0;
  stale = 0;
}

static void sprinkle() {
  const int cx = random(W), cy = random(H);
  for (int dy = -4; dy <= 4; dy++) {
    for (int dx = -4; dx <= 4; dx++) {
      if (random(2)) {
        const int i = ((cy + dy + H) % H) * W + (cx + dx + W) % W;
        if (!cur[i]) {
          cur[i] = 1;
          age[i] = 1;
        }
      }
    }
  }
  stale = 0;
}

static void step() {
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
      const int s = left + mid + right;  // 3x3 block including the cell itself
      const int i = y * W + x;
      if (cur[i]) {
        if (s == 3 || s == 4) {  // 2 or 3 neighbors: stays alive
          nxt[i] = 1;
          if (age[i] < 6) age[i]++;
        } else {                 // dies, leaves a trail
          nxt[i] = 0;
          age[i] = 0;
          ghost[i] = 255;
        }
      } else if (s == 3) {       // exactly 3 neighbors: born
        nxt[i] = 1;
        age[i] = 1;
        ghost[i] = 0;
      } else {
        nxt[i] = 0;
        ghost[i] = (uint8_t)(ghost[i] * trail);
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
  for (int i = 0; i < HISTORY; i++) {
    if (history[i] == h) {
      seen = true;
      break;
    }
  }
  if (seen) {
    stale++;
  } else {
    stale = 0;
    history[histIndex] = h;
    histIndex = (histIndex + 1) % HISTORY;
  }
  if (stale > STALE_GENS) newWorld();
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
  newWorld();
}

void update(float dt, const InputFrame& input) {
  if (!cur || !nxt || !age || !ghost || !history) return;  // allocation failed

  PFParams::apply(input, 0, &speed, 1.0f, 60.0f, 1.0f);
  PFParams::apply(input, 1, &density, 0.05f, 0.6f, 0.01f);
  PFParams::applyInt(input, 2, &theme, 0, THEMES - 1, 1, true);
  PFParams::apply(input, 3, &trail, 0.0f, 0.95f, 0.05f);

  if (input.btnPressed[0]) paused = !paused;
  if (input.btnPressed[1]) newWorld();
  if (input.btnPressed[2]) theme = (theme + 1) % THEMES;
  if (input.btnPressed[3]) sprinkle();

  if (paused) return;
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
  const uint8_t(*pal)[3] = PALETTE[theme];
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      const int i = y * W + x;
      if (cur[i]) {
        const uint8_t* c = pal[age[i] - 1];
        PFCanvas::setPixel(x, y, c[0], c[1], c[2]);
      } else if (ghost[i]) {
        const uint8_t* c = pal[6];
        const int g = ghost[i];
        PFCanvas::setPixel(x, y, c[0] * g / 255, c[1] * g / 255, c[2] * g / 255);
      } else {
        PFCanvas::setPixel(x, y, 0, 0, 0);
      }
    }
  }
  PFCanvas::present();
}

}  // namespace GameOfLife
