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
      text(textWidth(SMALL, l) <= W ? SMALL : THIN, l, W / 2, y, GREY, 1, 'c');
      y += 10;
    }
}

static RGB tempColor(float t) { return t >= 25 ? ORANGE : t >= 15 ? YELLOW : t >= 5 ? WHITE : LIGHTBLUE; }

static void drawDegrees(const char* number, int cx, int y, int k) {
  // '`' is the bold font's degree sign
  const int w = textWidth(LARGE, number, k);
  text(LARGE, number, cx - 2, y, WHITE, k, 'c');
  text(SMALL, "`", cx - 2 + w / 2 + 2, y, WHITE);
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
// Every screen keeps at least 3 LEDs from each edge. Text in SMALL is 7 rows
// high; LARGE is 8 rows, drawn 2x (16) for the clock and 3x (24) for the
// temperature.

static void header(const char* title) {
  if (portrait()) text(SMALL, title, 32, 3, GREY, 1, 'c');
}

static void drawClock() {
  struct tm t;
  if (!PatternflowClock::localTime(&t)) {
    message("WAITING", "FOR TIME");
    return;
  }
  char hm[8], date[16], phaseText[16];
  snprintf(hm, sizeof hm, "%02d:%02d", t.tm_hour, t.tm_min);
  snprintf(date, sizeof date, "%s %d %s", DAY_NAMES[(t.tm_wday + 6) % 7], t.tm_mday, MONTH_NAMES[t.tm_mon]);
  const float phase = moonPhase((int64_t)time(nullptr));
  const int lit = moonLit(phase);
  if (lit >= 98) snprintf(phaseText, sizeof phaseText, "FULL MOON");
  else if (lit <= 2) snprintf(phaseText, sizeof phaseText, "NEW MOON");
  else snprintf(phaseText, sizeof phaseText, "%s %d%%", phase < 0.5f ? "WAX" : "WANE", lit);
  const auto& w = DashWeather::current;

  if (portrait()) {
    moon(32, 26, 22, phase);                       // 4..48
    text(LARGE, hm, 32, 54, WHITE, 2, 'c');        // 54..69
    text(SMALL, date, 32, 76, GREY, 1, 'c');       // 76..82
    text(SMALL, phaseText, 32, 86, MOONSHADE, 1, 'c');
    if (w.valid) {
      dotsH(99, 3, 61);
      text(SMALL, "RISE", 3, 106, GREY);
      text(SMALL, w.sunrise, 61, 106, YELLOW, 1, 'r');
      text(SMALL, "SET", 3, 116, GREY);
      text(SMALL, w.sunset, 61, 116, ORANGE, 1, 'r');
    }
  } else {
    moon(31, 32, 27, phase);                       // 4..58, 5..59
    text(LARGE, hm, 96, 5, WHITE, 2, 'c');         // 5..20
    text(SMALL, date, 96, 26, GREY, 1, 'c');
    text(SMALL, phaseText, 96, 36, MOONSHADE, 1, 'c');
    if (w.valid) {  // sunrise in yellow, sunset in orange
      text(SMALL, w.sunrise, 94, 50, YELLOW, 1, 'r');
      text(SMALL, w.sunset, 98, 50, ORANGE);
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
  snprintf(wind, sizeof wind, "WIND %d", (int)lroundf(w.wind));
  const bool p = portrait();
  const int size = p ? 52 : 56;
  blit(bigIcon.get(DashIcons::fromWmo(w.code, w.isDay), size, alloc), size, p ? 6 : 4, p ? 3 : 4);
  const int cx = p ? 32 : 94, y0 = p ? 58 : 4;
  drawDegrees(t, cx, y0, 3);                                          // 24 rows
  text(SMALL, feels, cx, y0 + 28, GREY, 1, 'c');
  text(SMALL, lo, cx - 6, y0 + 38, LIGHTBLUE, 1, 'r');               // today's low - high
  fillRect(cx - 3, y0 + 41, 6, 1, GREY);
  text(SMALL, hi, cx + 6, y0 + 38, tempColor(w.dMax[0]));
  text(SMALL, rain, cx, y0 + 48, w.dRain[0] >= 10 ? LIGHTBLUE : GREY, 1, 'c');
  if (p) text(SMALL, wind, cx, y0 + 58, GREY, 1, 'c');
}

// The next 12 hours as a chart: an icon every 4 hours, right above its point
// on the temperature line, and the chance of rain as bars underneath.
static void drawHourly() {
  if (!weatherReady()) return;
  const auto& w = DashWeather::current;
  const int n = w.hours < 13 ? w.hours : 13;
  if (n < 2) return;
  const bool p = portrait();
  // x of hour i: portrait 11..53 (3.5 per hour), landscape 16..112 (8); both centred
  auto xOf = [&](int i) { return p ? 11 + i * 7 / 2 : 16 + i * 8; };
  const int iconSize = p ? 13 : 14;
  const int iconY = p ? 13 : 3, labelY = p ? 29 : 19;
  const int top = p ? 52 : 39, bottom = p ? 90 : 49;     // the temperature line
  const int rainTop = p ? 106 : 55, rainBottom = p ? 124 : 60;
  header("NEXT 12H");
  float lo = 1e9f, hi = -1e9f;
  for (int i = 0; i < n; i++) lo = fminf(lo, w.hTemp[i]), hi = fmaxf(hi, w.hTemp[i]);
  if (hi - lo < 4) { const float mid = (hi + lo) / 2; lo = mid - 2; hi = mid + 2; }
  auto yOf = [&](float t) { return (int)lroundf(bottom - (t - lo) * (bottom - top) / (hi - lo)); };
  char buf[8];
  for (int k = 0; k * 4 < n; k++) {
    const int i = k * 4, cx = xOf(i);
    blit(hourIcons[k].get(DashIcons::fromWmo(w.hCode[i], w.hDay[i]), iconSize, alloc), iconSize, cx - iconSize / 2, iconY);
    snprintf(buf, sizeof buf, "%02d", w.hHour[i]);
    text(SMALL, buf, cx, labelY, i ? GREY : WHITE, 1, 'c');
    dotsV(cx, labelY + 10, yOf(w.hTemp[i]) - 2, 2);  // a guide down to its point
  }
  int wettest = 0;
  for (int i = 0; i < n; i++) {
    const int hgt = w.hRain[i] * (rainBottom - rainTop) / 100;
    if (hgt > 0) fillRect(xOf(i) - 1, rainBottom - hgt, p ? 2 : 3, hgt, BLUE);
    if (w.hRain[i] > wettest) wettest = w.hRain[i];
  }
  if (p && wettest < 10) text(SMALL, "DRY", 32, rainTop + 6, GREY, 1, 'c');
  for (int i = 0; i + 1 < n; i++) {
    const int xa = xOf(i), xb = xOf(i + 1), ya = yOf(w.hTemp[i]), yb = yOf(w.hTemp[i + 1]);
    for (int x = xa; x <= xb; x++) {
      const float f = (float)(x - xa) / (xb - xa);
      const int y = (int)lroundf(ya + (yb - ya) * f);
      const RGB c = tempColor(w.hTemp[i] + (w.hTemp[i + 1] - w.hTemp[i]) * f);
      px(x, y, c);
      px(x, y + 1, c);
    }
  }
  // the warmest and the coldest hour, labelled beside the line
  int iHi = 0, iLo = 0;
  for (int i = 0; i < n; i++) {
    if (w.hTemp[i] > w.hTemp[iHi]) iHi = i;
    if (w.hTemp[i] < w.hTemp[iLo]) iLo = i;
  }
  auto label = [&](int i, bool above) {
    snprintf(buf, sizeof buf, "%d", (int)lroundf(w.hTemp[i]));
    const int tw = textWidth(SMALL, buf);
    int x = xOf(i) - tw / 2;
    if (x < 3) x = 3;
    if (x + tw > W - 3) x = W - 3 - tw;
    text(SMALL, buf, x, yOf(w.hTemp[i]) + (above ? -9 : 4), tempColor(w.hTemp[i]));
  };
  label(iHi, true);
  if (iLo != iHi) label(iLo, false);
}

// The next four days, each with a bar from its low to its high on a scale
// shared by all four, so warmer and colder days stand out at a glance.
static void drawForecast() {
  if (!weatherReady()) return;
  const auto& w = DashWeather::current;
  header("NEXT DAYS");
  const int days = w.days - 1 < 4 ? w.days - 1 : 4;
  float lo = 1e9f, hi = -1e9f;
  for (int i = 1; i <= days; i++) lo = fminf(lo, w.dMin[i]), hi = fmaxf(hi, w.dMax[i]);
  if (hi - lo < 1) hi = lo + 1;
  char a[8], b[8];
  for (int i = 0; i < days; i++) {
    const int d = i + 1;
    const char* name = DAY_NAMES[DashTz::weekday(w.dYear[d], w.dMonth[d], w.dDay[d])];
    snprintf(a, sizeof a, "%d", (int)lroundf(w.dMin[d]));
    snprintf(b, sizeof b, "%d", (int)lroundf(w.dMax[d]));
    int barX0, barX1, barY;
    if (portrait()) {
      const int y = 18 + i * 28;  // rows of 28: name and icon, then the bar
      if (i) dotsH(y - 5, 3, 61);
      text(SMALL, name, 3, y, WHITE);
      blit(dayIcons[i].get(DashIcons::fromWmo(w.dCode[d], true), 16, alloc), 16, 45, y - 3);
      text(SMALL, a, 3, y + 14, LIGHTBLUE);
      text(SMALL, b, 61, y + 14, tempColor(w.dMax[d]), 1, 'r');
      barX0 = 18, barX1 = 46, barY = y + 16;
    } else {
      const int y = 7 + i * 14;  // rows of 14, no lines: the bars line up by themselves
      text(SMALL, name, 6, y, WHITE);
      blit(dayIcons[i].get(DashIcons::fromWmo(w.dCode[d], true), 14, alloc), 14, 28, y - 4);
      text(SMALL, a, 58, y, LIGHTBLUE, 1, 'r');
      text(SMALL, b, 122, y, tempColor(w.dMax[d]), 1, 'r');
      barX0 = 62, barX1 = 106, barY = y + 2;
    }
    // the shared scale, dim; this day's range on it, coloured from cold to warm
    for (int x = barX0; x <= barX1; x++) px(x, barY, DIM), px(x, barY + 1, DIM);
    const int xa = barX0 + (int)lroundf((w.dMin[d] - lo) * (barX1 - barX0) / (hi - lo));
    const int xb = barX0 + (int)lroundf((w.dMax[d] - lo) * (barX1 - barX0) / (hi - lo));
    for (int x = xa; x <= xb; x++) {
      const RGB c = tempColor(lo + (x - barX0) * (hi - lo) / (barX1 - barX0));
      px(x, barY - 1, c), px(x, barY, c), px(x, barY + 1, c), px(x, barY + 2, c);
    }
  }
}

// ---------------------------------------------------------------- world clocks
// Per city its name, the time there, and a band of 24 hours showing day and
// night over there with a marker at now.

// Day 07-19, an hour of dusk either side: a rough picture of light over there
// (the panel knows each city's time zone, not where it is).
static RGB lightAt(float hour) {
  if ((hour >= 6 && hour < 7) || (hour >= 19 && hour < 20)) return RGB{150, 70, 40};
  return hour >= 7 && hour < 19 ? RGB{120, 95, 20} : RGB{20, 30, 90};
}

static void drawWorld() {
  const int64_t now = (int64_t)time(nullptr);
  struct tm home;
  if (!PatternflowClock::localTime(&home) || now < 1700000000) {
    message("WAITING", "FOR TIME");
    return;
  }
  const int64_t homeDay = DashTz::daysFromCivil(home.tm_year + 1900, home.tm_mon + 1, home.tm_mday);
  const bool p = portrait();
  const int n = DashClocks::count;
  // rows: portrait 31 apart, landscape 14; centred top to bottom
  const int pitch = p ? 31 : 14, rowH = p ? 26 : 11;
  const int y0 = (H - ((n - 1) * pitch + rowH)) / 2;
  const int left = p ? 3 : 6, right = W - 1 - left;  // right: the last LED used
  char hm[8];
  for (int i = 0; i < n; i++) {
    const time_t local = (time_t)(now + DashTz::utcOffset(now, DashClocks::zones[i]));
    struct tm t;
    gmtime_r(&local, &t);
    snprintf(hm, sizeof hm, "%02d:%02d", t.tm_hour, t.tm_min);
    const int dayDiff = (int)(DashTz::daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday) - homeDay);
    const int y = y0 + i * pitch;
    if (p && i) dotsH(y - 3, left, right + 1);
    text(SMALL, DashClocks::names[i], left, y, GREY);
    const int timeY = p ? y + 10 : y;
    text(SMALL, hm, right + 1, timeY, WHITE, 1, 'r');
    if (dayDiff) text(SMALL, dayDiff > 0 ? "+1" : "-1", p ? left : right - 30, timeY, CYAN, 1, p ? 'l' : 'r');
    const int by = p ? y + 22 : y + 9;
    const int w = right - left + 1;
    for (int x = 0; x < w; x++) {
      const RGB c = lightAt(x * 24.0f / w);
      px(left + x, by, c);
      px(left + x, by + 1, c);
    }
    const int mx = left + (int)((t.tm_hour + t.tm_min / 60.0f) * w / 24);
    for (int k = p ? -1 : 0; k <= (p ? 2 : 1); k++) px(mx, by + k, WHITE);
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
