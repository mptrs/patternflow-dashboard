// ═══════════════════════════════════════════════════════════
// Patternflow Dashboard - the "Dashboard" pattern
//
// Shows up in the K4 pattern browser like any other pattern and rotates
// through: clock + moon, weather now, the next hours, the next days and
// world clocks, with an uploaded GIF after every screen. Every screen has a
// portrait (64x128, Patternflow's usual mounting) and a landscape (128x64) layout.
//
// K1 turn: previous / next screen      K1 click: automatic rotation on / off
// K2 turn: orientation                 K4 click: back to the previous pattern
//
// License: MIT
// ═══════════════════════════════════════════════════════════
#pragma once
#include <stdio.h>
#include <time.h>
#include "../../src/core_canvas.h"
#include "../../src/core_clock.h"
#include "../../src/core_mem.h"
#include "dashboard_config.h"
#include "dash_gfx.h"
#include "dash_clocks.h"
#include "dash_gifs.h"
#include "dash_icons.h"
#include "dash_state.h"
#include "dash_tz.h"
#include "dash_weather.h"

namespace Dashboard {

const char* NAME = "Dashboard";
const char* KNOB_LABELS[4] = {"Screen", "Rotate", "-", "Back"};
constexpr bool ABSOLUTE_READY = false;

using namespace DashGfx;

enum Screen { CLOCK, WEATHER, HOURLY, FORECAST, WORLD, GIF, SCREEN_COUNT };
// The rotation: a GIF after every screen (GIF slots are skipped when there are none).
static const int SEQUENCE[] = {CLOCK, GIF, WEATHER, GIF, HOURLY, GIF, FORECAST, GIF, WORLD, GIF};
constexpr int SLOTS = sizeof(SEQUENCE) / sizeof(SEQUENCE[0]);
constexpr float GIF_SECONDS = 10;      // at least one full loop and at least this long
constexpr float GIF_MAX_SECONDS = 300; // only a safety net: a long GIF always plays to its end
static const char* const DAY_NAMES[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
static const char* const MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

static int slot = 0;            // position in SEQUENCE
static int screen = CLOCK;      // SEQUENCE[slot]
static int gifOrientation = -1; // orientation the playing clip was opened for
static float onScreen = 0;      // seconds on the current screen
static float manualHold = 0;    // seconds left before rotating after a manual pick
static bool autoRotate = true;

static DashIcons::Slot bigIcon, dayIcons[4], hourIcons[10];
static void* alloc(size_t n) { return PFMem::alloc(n); }

static int screenSeconds(int s) { return s == CLOCK ? DashConfig::SECONDS_CLOCK : DashConfig::SECONDS_OTHER; }
static bool portrait() { return DashGfx::H > DashGfx::W; }
static bool portraitFor(int orientation) { return orientation & 1; }

static void message(const char* a, const char* b = nullptr, const char* c = nullptr) {
  const char* lines[] = {a, b, c};
  int n = 0;
  for (auto l : lines) n += l ? 1 : 0;
  int y = H / 2 - (n * 10) / 2;
  for (auto l : lines)
    if (l) {
      text(SMALL, l, W / 2, y, GREY, 1, 'c');
      y += 10;
    }
}

static void drawDegrees(const char* number, int cx, int y, int k) {
  // the pixel fonts have no degree sign: a small "o" set high does the job
  const int w = textWidth(LARGE, number, k);
  text(LARGE, number, cx - 2, y, WHITE, k, 'c');
  text(SMALL, "o", cx - 2 + w / 2 + 1, y, WHITE);
}

static bool weatherReady() {
  if (!DashWeather::hasLocation()) {
    message("SET LOCATION", "patternflow", ".local/dashboard");
    return false;
  }
  if (!DashWeather::current.valid) {
    message("LOADING", "WEATHER");
    return false;
  }
  return true;
}

// ---------------------------------------------------------------- screens
static void drawClock() {
  struct tm t;
  if (!PatternflowClock::localTime(&t)) {
    message("WAITING", "FOR TIME");
    return;
  }
  char hm[8], date[16], phaseText[16], rise[16], set[16];
  snprintf(hm, sizeof hm, "%02d:%02d", t.tm_hour, t.tm_min);
  snprintf(date, sizeof date, "%s %d %s", DAY_NAMES[(t.tm_wday + 6) % 7], t.tm_mday, MONTH_NAMES[t.tm_mon]);
  const float phase = moonPhase((int64_t)time(nullptr));
  const int lit = moonLit(phase);
  if (lit >= 98) snprintf(phaseText, sizeof phaseText, "FULL MOON");
  else if (lit <= 2) snprintf(phaseText, sizeof phaseText, "NEW MOON");
  else snprintf(phaseText, sizeof phaseText, "%s %d%%", phase < 0.5f ? "WAXING" : "WANING", lit);
  const auto& w = DashWeather::current;
  snprintf(rise, sizeof rise, "RISE %s", w.sunrise);
  snprintf(set, sizeof set, "SET  %s", w.sunset);

  if (portrait()) {
    moon(32, 25, 23, phase);
    text(LARGE, hm, 32, 54, WHITE, 2, 'c');
    text(SMALL, date, 32, 76, GREY, 1, 'c');
    text(SMALL, phaseText, 32, 86, MOONSHADE, 1, 'c');
    if (w.valid) {
      dotsH(98, 2, 62);
      text(SMALL, rise, 32, 104, YELLOW, 1, 'c');
      text(SMALL, set, 32, 114, ORANGE, 1, 'c');
    }
  } else {
    moon(32, 32, 29, phase);
    text(LARGE, hm, 96, 3, WHITE, 2, 'c');
    text(SMALL, date, 96, 24, GREY, 1, 'c');
    text(SMALL, phaseText, 96, 34, MOONSHADE, 1, 'c');
    if (w.valid) {
      text(SMALL, rise, 96, 46, YELLOW, 1, 'c');
      text(SMALL, set, 96, 55, ORANGE, 1, 'c');
    }
  }
}

static void drawWeather() {
  if (!weatherReady()) return;
  const auto& w = DashWeather::current;
  char t[8], feels[16], lo[8], hi[8], rain[16], wind[16];
  snprintf(t, sizeof t, "%d", (int)lroundf(w.temp));
  snprintf(feels, sizeof feels, "FEELS %d", (int)lroundf(w.feels));
  snprintf(lo, sizeof lo, "%d", (int)lroundf(w.dMin[0]));
  snprintf(hi, sizeof hi, "%d", (int)lroundf(w.dMax[0]));
  snprintf(rain, sizeof rain, "RAIN %d%%", w.dRain[0]);
  snprintf(wind, sizeof wind, portrait() ? "WIND %d" : "WIND %d KMH", (int)lroundf(w.wind));
  blit(bigIcon.get(DashIcons::fromWmo(w.code, w.isDay), 64, alloc), 64, 0, 0);
  const int cx = portrait() ? 32 : 96, y0 = portrait() ? 64 : 2;
  drawDegrees(t, cx, y0, 3);
  text(SMALL, feels, cx, y0 + 28, GREY, 1, 'c');
  text(SMALL, lo, cx - 14, y0 + 37, LIGHTBLUE, 1, 'c');
  text(SMALL, "/", cx, y0 + 37, GREY, 1, 'c');
  text(SMALL, hi, cx + 14, y0 + 37, ORANGE, 1, 'c');
  text(SMALL, rain, cx, y0 + 46, LIGHTBLUE, 1, 'c');
  text(SMALL, wind, cx, y0 + 55, GREY, 1, 'c');
}

// A block of the hourly and daily screens: icon, label, a big number in
// colour, and a small line underneath. Portrait stacks four blocks of
// 29 rows; landscape puts four columns of 32 side by side.
static void block(int i, const uint8_t* icon, const char* label, const char* big, RGB bigColor, const char* small,
                  RGB smallColor) {
  if (portrait()) {
    const int y = 11 + i * 29;
    if (i) dotsH(y - 1, 2, 62);
    blit(icon, 24, 2, y + 2);
    text(SMALL, label, 46, y + 2, GREY, 1, 'c');
    text(LARGE, big, 46, y + 10, bigColor, 1, 'c');
    text(SMALL, small, 46, y + 21, smallColor, 1, 'c');
  } else {
    const int x = i * 32;
    if (i) dotsV(x, 2, 62);
    text(SMALL, label, x + 16, 1, GREY, 1, 'c');
    blit(icon, 24, x + 4, 10);
    text(LARGE, big, x + 16, 37, bigColor, 1, 'c');
    text(SMALL, small, x + 16, 49, smallColor, 1, 'c');
  }
}

static void header(const char* title) {
  if (portrait()) text(SMALL, title, 32, 1, GREY, 1, 'c');
}

static RGB tempColor(float t) { return t >= 25 ? ORANGE : t >= 15 ? YELLOW : t >= 5 ? WHITE : LIGHTBLUE; }

static void drawHourly() {
  if (!weatherReady()) return;
  const auto& w = DashWeather::current;
  header("NEXT HOURS");
  char label[8], temp[8], rain[8];
  for (int i = 0; i < 4; i++) {
    const int h = i * 3;  // now, +3, +6, +9 hours
    if (h >= w.hours) break;
    if (i == 0) snprintf(label, sizeof label, "NOW");
    else snprintf(label, sizeof label, "%02d:00", w.hHour[h]);
    snprintf(temp, sizeof temp, "%d", (int)lroundf(w.hTemp[h]));
    snprintf(rain, sizeof rain, "%d%%", w.hRain[h]);
    block(i, hourIcons[i].get(DashIcons::fromWmo(w.hCode[h], w.hDay[h]), 24, alloc), label, temp,
          tempColor(w.hTemp[h]), rain, w.hRain[h] >= 10 ? LIGHTBLUE : DIM);
  }
}

static void drawForecast() {
  if (!weatherReady()) return;
  const auto& w = DashWeather::current;
  header("NEXT DAYS");
  char hi[8], lo[8];
  for (int i = 0; i < 4 && i + 1 < w.days; i++) {
    const int d = i + 1;  // from tomorrow
    snprintf(hi, sizeof hi, "%d", (int)lroundf(w.dMax[d]));
    snprintf(lo, sizeof lo, "%d", (int)lroundf(w.dMin[d]));
    block(i, dayIcons[i].get(DashIcons::fromWmo(w.dCode[d], true), 24, alloc),
          DAY_NAMES[DashTz::weekday(w.dYear[d], w.dMonth[d], w.dDay[d])], hi, tempColor(w.dMax[d]), lo, LIGHTBLUE);
  }
}

static void drawWorld() {
  const int64_t now = (int64_t)time(nullptr);
  struct tm home;
  if (!PatternflowClock::localTime(&home) || now < 1700000000) {
    message("WAITING", "FOR TIME");
    return;
  }
  const int64_t homeDay = DashTz::daysFromCivil(home.tm_year + 1900, home.tm_mon + 1, home.tm_mday);
  char hm[8];
  const int n = DashClocks::count;
  for (int i = 0; i < n; i++) {
    const char* name = DashClocks::names[i];
    const time_t local = (time_t)(now + DashTz::utcOffset(now, DashClocks::zones[i]));
    struct tm t;
    gmtime_r(&local, &t);
    snprintf(hm, sizeof hm, "%02d:%02d", t.tm_hour, t.tm_min);
    const int dayDiff = (int)(DashTz::daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday) - homeDay);
    const bool day = t.tm_hour >= 7 && t.tm_hour < 19;  // day or night over there
    if (portrait()) {
      // name across the top, a clock face on the left, the time on the right
      const int y = i * 32;
      if (i) dotsH(y, 2, 62);
      text(SMALL, name, 32, y + 2, GREY, 1, 'c');
      dial(12, y + 20, 10, t.tm_hour, t.tm_min, day);
      text(LARGE, hm, 44, y + 13, WHITE, 1, 'c');
      if (dayDiff) text(SMALL, dayDiff > 0 ? "+1 DAY" : "-1 DAY", 44, y + 24, CYAN, 1, 'c');
    } else {
      const int y = i * 16;
      if (i) dotsH(y, 1, 127);
      dial(8, y + 8, 6, t.tm_hour, t.tm_min, day);
      text(LARGE, name, 19, y + 4, GREY);
      if (dayDiff) text(SMALL, dayDiff > 0 ? "+1" : "-1", 86, y + 5, CYAN);
      text(LARGE, hm, 127, y + 4, WHITE, 1, 'r');
    }
  }
}

// ---------------------------------------------------------------- rotation
// Moves `step` slots on, skipping GIF slots when there is nothing to play.
static void moveSlot(int step) {
  for (int tries = 0; tries < SLOTS; tries++) {
    slot = ((slot + step) % SLOTS + SLOTS) % SLOTS;
    screen = SEQUENCE[slot];
    if (screen == WORLD && DashClocks::count == 0) continue;  // no clocks set: skip
    if (screen != GIF) break;
    gifOrientation = DashState::orientation;
    if (DashGifs::player.start(portraitFor(gifOrientation))) break;
  }
  if (screen != GIF) DashGifs::player.stop();
  onScreen = 0;
}

// ---------------------------------------------------------------- pattern API
void setup() {
  onScreen = 0;
  manualHold = 0;
}

void update(float dt, const InputFrame& input) {
  if (input.knobDeltas[0]) {
    moveSlot(input.knobDeltas[0] > 0 ? 1 : -1);
    manualHold = DashConfig::SECONDS_MANUAL_HOLD;
  }
  if (input.btnPressed[0]) autoRotate = !autoRotate;
  if (input.knobDeltas[1]) {
    DashState::orientation = ((DashState::orientation + (input.knobDeltas[1] > 0 ? 1 : -1)) % 4 + 4) % 4;
    DashState::saveOrientation();
  }
  if (input.btnPressed[3]) DashState::wantBack = true;

  // Read the next GIF into memory while another screen is showing
  if (screen != GIF) DashGifs::prepare(portraitFor(DashState::orientation));
  if (screen == GIF) {
    if (gifOrientation != DashState::orientation) {  // turned while playing: reopen in the new shape
      gifOrientation = DashState::orientation;
      if (!DashGifs::player.start(portraitFor(gifOrientation))) moveSlot(1);
    }
    DashGifs::player.advance(dt);
  }
  if (manualHold > 0) {
    manualHold -= dt;
    return;
  }
  if (!autoRotate) return;
  onScreen += dt;
  // Ends with a loop, never in the middle of one
  const bool gifDone = screen == GIF && ((DashGifs::player.wrapped && onScreen >= GIF_SECONDS) ||
                                         onScreen >= GIF_MAX_SECONDS || !DashGifs::player.playing());
  if (screen == GIF ? gifDone : onScreen >= screenSeconds(screen)) moveSlot(1);
}

void draw() {
  DashGfx::begin(DashState::orientation);
  switch (screen) {
    case CLOCK: drawClock(); break;
    case WEATHER: drawWeather(); break;
    case HOURLY: drawHourly(); break;
    case FORECAST: drawForecast(); break;
    case WORLD: drawWorld(); break;
    case GIF: DashGifs::player.draw(); break;
  }
  PFCanvas::present();
}

}  // namespace Dashboard
