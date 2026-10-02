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
// Knob 2: density of a new world          · click: new world (next seed style)
// Knob 3: color theme                     · click: next rule (Life, HighLife, Day & Night)
// Knob 4: trail length                    · click: sprinkle live cells

const W = 128, H = 64, N = W * H;
const HISTORY = 1024, STALE_GENS = 40, MAX_AGE = 60, FADE_SECONDS = 1.2, SHOW_END_SECONDS = 8;
const bits = (...ns) => ns.reduce((m, n) => m | (1 << n), 0);
const RULES = [
  { born: bits(3), survive: bits(2, 3) },                       // Life      B3/S23
  { born: bits(3, 6), survive: bits(2, 3) },                    // HighLife  B36/S23
  { born: bits(3, 6, 7, 8), survive: bits(3, 4, 6, 7, 8) },     // Day & Night
];
// Per theme: 5 color stops from young to old, then the trail color.
const PALETTE = [
  [[255,255,255],[96,224,255],[32,140,255],[40,60,230],[120,40,200],[40,60,120]],  // ocean
  [[255,255,170],[255,210,0],[255,130,0],[230,50,0],[140,10,30],[120,40,0]],       // fire
  [[220,255,220],[80,255,80],[20,200,60],[0,140,110],[0,80,90],[20,90,20]],        // matrix
  [[255,255,255],[255,70,200],[190,70,255],[80,90,255],[0,190,220],[90,30,110]],   // neon
];
const R_PENTOMINO = [[1,0],[2,0],[0,1],[1,1],[1,2]];
const ACORN = [[1,0],[3,1],[0,2],[1,2],[4,2],[5,2],[6,2]];

function buildLut(p) {
  const pal = PALETTE[p.theme];
  p.lut = [[0, 0, 0]];
  for (let a = 1; a <= MAX_AGE; a++) {
    const t = Math.sqrt((a - 1) / (MAX_AGE - 1)) * 4;
    const i = Math.min(3, Math.floor(t)), f = t - i;
    p.lut.push([0, 1, 2].map(c => pal[i][c] + (pal[i + 1][c] - pal[i][c]) * f));
  }
  p.lutTheme = p.theme;
}

function fingerprint(p) {
  let h = 2166136261;
  for (let i = 0; i < N; i++) h = Math.imul(h ^ p.cur[i], 16777619) >>> 0;
  return h;
}

function setCell(p, x, y) {
  const i = ((y + H) % H) * W + (x + W) % W;
  p.cur[i] = 1; p.age[i] = 1;
}

function seedWorld(p) {
  p.cur.fill(0); p.age.fill(0); p.ghost.fill(0);
  if (p.seedStyle === 0) {                    // random soup
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) if (Math.random() < p.density) setCell(p, x, y);
  } else if (p.seedStyle === 1) {             // mirrored soup over the whole world
    for (let y = 0; y < H / 2; y++) for (let x = 0; x < W / 2; x++) if (Math.random() < p.density) {
      setCell(p, W / 2 - 1 - x, H / 2 - 1 - y); setCell(p, W / 2 + x, H / 2 - 1 - y);
      setCell(p, W / 2 - 1 - x, H / 2 + y);     setCell(p, W / 2 + x, H / 2 + y);
    }
  } else if (p.rule !== 0) {                  // methuselahs only work under Life: dense blobs instead
    const r = p.rule === 2 ? 16 : 8, fill = p.rule === 2 ? 0.5 : p.density + 0.15;
    const count = 2 + Math.floor(Math.random() * 3);
    for (let k = 0; k < count; k++) {
      const ox = Math.floor(Math.random() * W), oy = Math.floor(Math.random() * H);
      for (let dy = -r; dy <= r; dy++) for (let dx = -r; dx <= r; dx++)
        if (dx * dx + dy * dy <= r * r && Math.random() < fill) setCell(p, ox + dx, oy + dy);
    }
  } else {                                    // a few methuselahs
    const count = 1 + Math.floor(Math.random() * 3);
    for (let k = 0; k < count; k++) {
      const ox = Math.floor(Math.random() * W), oy = Math.floor(Math.random() * H);
      for (const [x, y] of Math.random() < 0.5 ? R_PENTOMINO : ACORN) setCell(p, ox + x, oy + y);
    }
  }
  p.history.fill(0); p.histIndex = 0; p.stale = 0; p.pending = 0; p.finishedFor = -1;
}

function changeWorld(p) { if (p.fadeDir === 0) p.fadeDir = -1; }

function sprinkle(p) {
  const cx = Math.floor(Math.random() * W), cy = Math.floor(Math.random() * H);
  for (let dy = -4; dy <= 4; dy++) for (let dx = -4; dx <= 4; dx++) {
    if (Math.random() < 0.5) {
      const i = ((cy + dy + H) % H) * W + (cx + dx + W) % W;
      if (!p.cur[i]) { p.cur[i] = 1; p.age[i] = 1; }
    }
  }
  p.stale = 0; p.finishedFor = -1;
}

function step(p) {
  const { cur, nxt, age, ghost } = p, r = RULES[p.rule];
  for (let y = 0; y < H; y++) {
    const up = ((y + H - 1) % H) * W, row = y * W, dn = ((y + 1) % H) * W;
    let left = cur[up + W - 1] + cur[row + W - 1] + cur[dn + W - 1];
    let mid = cur[up] + cur[row] + cur[dn];
    for (let x = 0; x < W; x++) {
      const xr = (x + 1) % W;
      const right = cur[up + xr] + cur[row + xr] + cur[dn + xr];
      const i = row + x, n = left + mid + right - cur[i];
      if (cur[i]) {
        if (r.survive >> n & 1) { nxt[i] = 1; if (age[i] < MAX_AGE) age[i]++; }
        else { nxt[i] = 0; age[i] = 0; ghost[i] = 255; }
      } else if (r.born >> n & 1) { nxt[i] = 1; age[i] = 1; ghost[i] = 0; }
      else nxt[i] = 0;
      left = mid; mid = right;
    }
  }
  p.cur = nxt; p.nxt = cur;
  const h = fingerprint(p);
  if (p.history.includes(h)) p.stale++;
  else { p.stale = 0; p.history[p.histIndex] = h; p.histIndex = (p.histIndex + 1) % HISTORY; }
  if (p.stale > STALE_GENS && p.finishedFor < 0) p.finishedFor = 0;  // finished: show it a while
}

export function setup(params) {
  Object.assign(params, { speed: 4, density: 0.33, theme: 0, trail: 0.75, rule: 0, seedStyle: 0,
    histIndex: 0, stale: 0, pending: 0, paused: false, fade: 0, fadeDir: 1, lutTheme: -1 });
  params.cur = new Uint8Array(N); params.nxt = new Uint8Array(N);
  params.age = new Uint8Array(N); params.ghost = new Uint8Array(N);
  params.history = new Uint32Array(HISTORY);
  seedWorld(params);
}

export function update(dt, input, params) {
  const p = params;
  if (input && input.knobValues) {
    p.speed = input.knobValues[0];
    p.density = input.knobValues[1];
    p.theme = Math.round(input.knobValues[2]) % PALETTE.length;
    p.trail = input.knobValues[3];
  }
  if (input && input.btnPressed) {
    if (input.btnPressed[0]) p.paused = !p.paused;
    if (input.btnPressed[1]) { p.seedStyle = (p.seedStyle + 1) % 3; changeWorld(p); }
    if (input.btnPressed[2]) { p.rule = (p.rule + 1) % RULES.length; p.stale = 0; p.finishedFor = -1; }
    if (input.btnPressed[3]) sprinkle(p);
  }
  if (p.finishedFor >= 0 && !p.paused) {
    p.finishedFor += dt;
    if (p.finishedFor >= SHOW_END_SECONDS) changeWorld(p);
  }
  if (p.fadeDir !== 0) {
    p.fade += p.fadeDir * dt / FADE_SECONDS;
    if (p.fade <= 0) { p.fade = 0; seedWorld(p); p.fadeDir = 1; }
    else if (p.fade >= 1) { p.fade = 1; p.fadeDir = 0; }
  }
  if (!p.paused) {
    const keep = Math.pow(p.trail, dt * p.speed);
    for (let i = 0; i < N; i++) if (p.ghost[i] && !p.cur[i]) p.ghost[i] = Math.floor(p.ghost[i] * keep);
  }
  if (p.paused || p.fadeDir < 0) return;
  p.pending += dt * p.speed;
  let steps = 0;
  while (p.pending >= 1 && steps < 4) { step(p); p.pending -= 1; steps++; }
  if (p.pending > 4) p.pending = 0;
}

export function draw(display, params, time) {
  const p = params;
  if (p.lutTheme !== p.theme) buildLut(p);
  const tc = PALETTE[p.theme][5];
  const born = (p.speed < 12 && !p.paused) ? Math.min(1, p.pending * 2) : 1;
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
    const i = y * W + x;
    if (p.cur[i]) {
      const c = p.lut[p.age[i]], k = (p.age[i] === 1 ? born : 1) * p.fade;
      display.setPixel(x, y, c[0] * k, c[1] * k, c[2] * k);
    } else if (p.ghost[i]) {
      const g = p.ghost[i] / 255 * p.fade;
      display.setPixel(x, y, tc[0] * g, tc[1] * g, tc[2] * g);
    } else display.setPixel(x, y, 0, 0, 0);
  }
}
