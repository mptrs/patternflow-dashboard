// ═══════════════════════════════════════════════════════════
// Patternflow Dashboard - the "GIFs" pattern
//
// Only the GIFs uploaded on http://patternflow.local/dashboard, one after
// another, without the dashboard screens in between. Shows up in the K4
// pattern browser next to the Dashboard and shares its player, so the next
// GIF is read into PSRAM while the current one plays.
//
// K1 turn: previous / next GIF          K1 click: hold this GIF (loop it) on / off
// K2 turn: orientation                  K2 click: show the GIF's name
// K3 turn: speed                        K3 click: normal speed
// K4 turn: how long each GIF plays      K4 click: back to the previous pattern
//
// License: MIT
// ═══════════════════════════════════════════════════════════
#pragma once
#include <ctype.h>
#include <Preferences.h>
#include "../../src/core_canvas.h"
#include "dash_gfx.h"
#include "dash_gifs.h"
#include "dash_state.h"

namespace Gifs {

const char* NAME = "GIFs";
const char* KNOB_LABELS[4] = {"GIF", "Rotate", "Speed", "Length"};
constexpr bool ABSOLUTE_READY = false;

using namespace DashGfx;

static const float SPEEDS[] = {0.25f, 0.5f, 0.75f, 1, 1.5f, 2, 3, 4};
static const char* const SPEED_NAMES[] = {"1/4X", "1/2X", "3/4X", "1X", "1.5X", "2X", "3X", "4X"};
constexpr int SPEED_COUNT = 8, SPEED_NORMAL = 3;
// Seconds each GIF plays at least (0: one loop); it always ends with a loop, never in the middle of one.
static const int LENGTHS[] = {0, 10, 30, 60, 300};
static const char* const LENGTH_NAMES[] = {"1 LOOP", "10 S", "30 S", "1 MIN", "5 MIN"};
constexpr int LENGTH_COUNT = 5;
constexpr float LABEL_SECONDS = 2;

static int want = 0;  // index in DashGifs::names of the GIF to show
// ... and its name, so an upload or delete elsewhere in the list does not change it
static char current[DashGifs::NAME_LEN + 1] = "";
static int step = 1;  // direction of the last move: a GIF that cannot be read is skipped that way
static bool hold = false;
static int speed = SPEED_NORMAL, length = 1;
static float onScreen = 0;            // seconds the current GIF has played
static const char* label = nullptr;   // what the label shows; nullptr: the name of the GIF
static float labelLeft = 0;
static bool settingsRead = false;

static bool portrait() { return DashState::orientation & 1; }

static void load() {
  Preferences prefs;
  if (!prefs.begin("dashboard", true)) return;
  hold = prefs.getBool("gifHold", false);
  speed = prefs.getUChar("gifSpeed", SPEED_NORMAL);
  length = prefs.getUChar("gifLength", 1);
  prefs.end();
  if (speed >= SPEED_COUNT) speed = SPEED_NORMAL;
  if (length >= LENGTH_COUNT) length = 1;
}

static void save() {
  Preferences prefs;
  if (!prefs.begin("dashboard", false)) return;
  prefs.putBool("gifHold", hold);
  prefs.putUChar("gifSpeed", (uint8_t)speed);
  prefs.putUChar("gifLength", (uint8_t)length);
  prefs.end();
}

static void say(const char* s) {
  label = s;
  labelLeft = LABEL_SECONDS;
}

static void move(int by) {
  step = by > 0 ? 1 : -1;
  want += by;
  current[0] = 0;
  onScreen = 0;
}

static int indexOf(const char* name) {
  for (int i = 0; i < DashGifs::count; i++)
    if (strcmp(DashGifs::names[i], name) == 0) return i;
  return -1;
}

// Finds the GIF to show again after the list changed. Deleted: the one now in its place.
static void follow() {
  const int n = DashGifs::count;
  int i = current[0] ? indexOf(current) : -1;
  if (i < 0 && current[0] && DashGifs::player.playing()) i = indexOf(DashGifs::player.name);  // renamed
  want = i >= 0 ? i : (want % n + n) % n;
  strcpy(current, DashGifs::names[want]);
}

static int clamp(int v, int n) { return v < 0 ? 0 : v >= n ? n - 1 : v; }

// The GIF the player has on screen is the one we want, in this orientation.
static bool showing(const char* name) {
  DashGifs::Player& p = DashGifs::player;
  return p.playing() && p.clip->portrait == portrait() && strcmp(p.name, name) == 0;
}

// ---------------------------------------------------------------- drawing
// A line of text on a black band at the bottom: bold when it fits, else thin, else cut short.
static void drawLabel(const char* s) {
  char up[DashGifs::NAME_LEN + 1];
  size_t n = 0;
  for (; s[n] && n < DashGifs::NAME_LEN; n++) up[n] = (char)toupper((unsigned char)s[n]);
  up[n] = 0;
  const GFXfont* f = SMALL;
  if (textWidth(f, up) > W - 4) f = THIN;
  while (n > 1 && textWidth(f, up) > W - 4) up[--n] = 0;
  const int h = f == SMALL ? 7 : 6;
  fillRect(0, H - h - 4, W, h + 4, BLACK);
  text(f, up, W / 2, H - h - 2, WHITE, 1, 'c');
}

static void drawEmpty() {
  const bool p = portrait();
  text(SMALL, "NO GIFS", W / 2, p ? 48 : 20, WHITE, 1, 'c');
  text(THIN, "UPLOAD ON", W / 2, p ? 62 : 33, GREY, 1, 'c');
  text(THIN, "/DASHBOARD", W / 2, p ? 70 : 41, GREY, 1, 'c');
}

// ---------------------------------------------------------------- pattern API
void setup() {
  if (!settingsRead) {
    load();
    settingsRead = true;
  }
  // Coming from the dashboard while it plays a GIF: carry on with that one
  if (DashGifs::player.playing() && DashGifs::any() && indexOf(DashGifs::player.name) >= 0)
    strcpy(current, DashGifs::player.name);
  onScreen = 0;
  labelLeft = 0;
}

void update(float dt, const InputFrame& input) {
  if (input.knobDeltas[0]) {
    move(input.knobDeltas[0]);
    say(nullptr);
  }
  if (input.btnPressed[0]) {
    hold = !hold;
    save();
    say(hold ? "HOLD" : "PLAY");
  }
  if (input.knobDeltas[1]) {
    DashState::orientation = ((DashState::orientation + (input.knobDeltas[1] > 0 ? 1 : -1)) % 4 + 4) % 4;
    DashState::saveOrientation();
  }
  if (input.btnPressed[1]) say(nullptr);
  if (input.knobDeltas[2] || input.btnPressed[2]) {
    speed = input.btnPressed[2] ? SPEED_NORMAL : clamp(speed + (input.knobDeltas[2] > 0 ? 1 : -1), SPEED_COUNT);
    save();
    say(SPEED_NAMES[speed]);
  }
  if (input.knobDeltas[3]) {
    length = clamp(length + (input.knobDeltas[3] > 0 ? 1 : -1), LENGTH_COUNT);
    save();
    say(LENGTH_NAMES[length]);
  }
  if (input.btnPressed[3]) DashState::wantBack = true;
  if (labelLeft > 0) labelLeft -= dt;

  if (!DashGifs::any()) {
    DashGifs::player.stop();
    return;
  }
  const int n = DashGifs::count;
  follow();
  DashGifs::Player& player = DashGifs::player;

  if (showing(DashGifs::names[want])) {
    player.advance(dt * SPEEDS[speed]);
    onScreen += dt;
    if (!hold && n > 1 && player.wrapped && onScreen >= LENGTHS[length]) {
      move(1);
    } else {
      DashGifs::prepareClip(want + 1, portrait());  // have the next one ready
    }
    return;
  }
  // Switching to another GIF (or turned): the old one plays on until the new one is read
  if (DashGifs::loadState == 3) {  // a read failed: when it was this one, skip it
    const bool thisOne = strcmp(DashGifs::loaded().name, DashGifs::names[want]) == 0;
    DashGifs::loadState = 0;
    if (thisOne) {
      move(step);
      return;
    }
  }
  DashGifs::prepareClip(want, portrait());
  if (DashGifs::ready(want, portrait())) {
    player.start(portrait());
    onScreen = 0;
  } else if (player.playing()) {
    player.advance(dt * SPEEDS[speed]);
  }
}

void draw() {
  DashGfx::begin(DashState::orientation);
  DashGifs::Player& player = DashGifs::player;
  if (!DashGifs::any()) {
    drawEmpty();
  } else {
    const bool shown = player.playing() && player.clip->portrait == portrait();
    if (shown) player.draw();
    // The label, or the name of the GIF being read while there is nothing to show
    if (labelLeft > 0 || !shown) drawLabel(labelLeft > 0 && label ? label : DashGifs::names[(want % DashGifs::count + DashGifs::count) % DashGifs::count]);
  }
  PFCanvas::present();
}

}  // namespace Gifs
