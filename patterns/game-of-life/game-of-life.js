// ===== Patternflow pattern =====
// Title:   Game of Life
// Author:  Michiel Peters
// SPDX-License-Identifier: CC-BY-SA-4.0
// ===============================
// JavaScript twin of preset_game_of_life.h, for the Patternflow Live Editor /
// Pattern Lab (https://patternflow.work/pattern). Paste this whole file there.
//
// @knobs Speed=1..60, Density=0.05..0.6, Theme=0..3, Trail=0..0.95
// Knob 1: speed (generations per second) · click: pause / resume
// Knob 2: density of a new world          · click: new world
// Knob 3: color theme                     · click: next theme
// Knob 4: trail length                    · click: sprinkle live cells

const W = 128, H = 64, N = W * H;
const HISTORY = 1024, STALE_GENS = 40;
const PALETTE = [
  [[255,255,255],[96,224,255],[32,160,255],[16,96,224],[8,48,176],[4,28,128],[40,60,120]],     // ocean
  [[255,255,160],[255,208,0],[255,144,0],[255,80,0],[208,32,0],[144,16,0],[120,40,0]],         // fire
  [[208,255,208],[64,255,64],[16,208,16],[8,160,8],[4,112,4],[2,72,2],[20,90,20]],             // matrix
  [[255,255,255],[255,64,192],[192,64,255],[112,64,255],[64,96,255],[32,128,192],[90,30,110]], // neon
];

function fingerprint(p) {
  let h = 2166136261;
  for (let i = 0; i < N; i++) h = Math.imul(h ^ p.cur[i], 16777619) >>> 0;
  return h;
}

function newWorld(p) {
  for (let i = 0; i < N; i++) {
    const v = Math.random() < p.density ? 1 : 0;
    p.cur[i] = v; p.age[i] = v; p.ghost[i] = 0;
  }
  p.history.fill(0); p.histIndex = 0; p.stale = 0;
}

function sprinkle(p) {
  const cx = Math.floor(Math.random() * W), cy = Math.floor(Math.random() * H);
  for (let dy = -4; dy <= 4; dy++) for (let dx = -4; dx <= 4; dx++) {
    if (Math.random() < 0.5) {
      const i = ((cy + dy + H) % H) * W + (cx + dx + W) % W;
      if (!p.cur[i]) { p.cur[i] = 1; p.age[i] = 1; }
    }
  }
  p.stale = 0;
}

function step(p) {
  const { cur, nxt, age, ghost } = p;
  for (let y = 0; y < H; y++) {
    const up = ((y + H - 1) % H) * W, row = y * W, dn = ((y + 1) % H) * W;
    let left = cur[up + W - 1] + cur[row + W - 1] + cur[dn + W - 1];
    let mid = cur[up] + cur[row] + cur[dn];
    for (let x = 0; x < W; x++) {
      const xr = (x + 1) % W;
      const right = cur[up + xr] + cur[row + xr] + cur[dn + xr];
      const s = left + mid + right, i = row + x;
      if (cur[i]) {
        if (s === 3 || s === 4) { nxt[i] = 1; if (age[i] < 6) age[i]++; }
        else { nxt[i] = 0; age[i] = 0; ghost[i] = 255; }
      } else if (s === 3) { nxt[i] = 1; age[i] = 1; ghost[i] = 0; }
      else { nxt[i] = 0; ghost[i] = Math.floor(ghost[i] * p.trail); }
      left = mid; mid = right;
    }
  }
  p.cur = nxt; p.nxt = cur;
  const h = fingerprint(p);
  if (p.history.includes(h)) p.stale++;
  else { p.stale = 0; p.history[p.histIndex] = h; p.histIndex = (p.histIndex + 1) % HISTORY; }
  if (p.stale > STALE_GENS) newWorld(p);
}

export function setup(params) {
  params.speed = 8; params.density = 0.33; params.theme = 0; params.trail = 0.75;
  params.cur = new Uint8Array(N); params.nxt = new Uint8Array(N);
  params.age = new Uint8Array(N); params.ghost = new Uint8Array(N);
  params.history = new Uint32Array(HISTORY); params.histIndex = 0; params.stale = 0;
  params.pending = 0; params.paused = false;
  newWorld(params);
}

export function update(dt, input, params) {
  if (input && input.knobValues) {
    params.speed = input.knobValues[0];
    params.density = input.knobValues[1];
    params.theme = Math.round(input.knobValues[2]) % PALETTE.length;
    params.trail = input.knobValues[3];
  }
  if (input && input.btnPressed) {
    if (input.btnPressed[0]) params.paused = !params.paused;
    if (input.btnPressed[1]) newWorld(params);
    if (input.btnPressed[2]) params.theme = (params.theme + 1) % PALETTE.length;
    if (input.btnPressed[3]) sprinkle(params);
  }
  if (params.paused) return;
  params.pending += dt * params.speed;
  let steps = 0;
  while (params.pending >= 1 && steps < 4) { step(params); params.pending -= 1; steps++; }
  if (params.pending > 4) params.pending = 0;
}

export function draw(display, params, time) {
  const pal = PALETTE[params.theme];
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
    const i = y * W + x;
    if (params.cur[i]) { const c = pal[params.age[i] - 1]; display.setPixel(x, y, c[0], c[1], c[2]); }
    else if (params.ghost[i]) { const c = pal[6], g = params.ghost[i] / 255; display.setPixel(x, y, c[0] * g, c[1] * g, c[2] * g); }
    else display.setPixel(x, y, 0, 0, 0);
  }
}
