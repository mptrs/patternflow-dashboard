// ═══════════════════════════════════════════════════════════
// Patternflow Dashboard - the feature
//
// Wires the dashboard into the Patternflow core without editing it:
//   - setup:       load settings, start the core clock (NTP + timezone)
//   - onNetwork:   the settings page /dashboard, and a first weather fetch
//   - loop:        weather refresh (fetched on core 0, never blocks a frame)
//                  and remembering which pattern ran before the dashboard
//   - takePattern: K4 click on the dashboard (or the GIFs) goes back to that pattern
//   - night mode:  requestSleep / onSleep / observeFrame (see dash_night.h)
//   - rotation:    follows the optional accelerometer (dash_accel.h); composeFrame
//                  turns Patternflow's own patterns 180 degrees when upside down
// The screens themselves are the "Dashboard" pattern in preset_dashboard.h, and
// the GIFs on their own the "GIFs" pattern in preset_gifs.h.
//
// License: MIT
// ═══════════════════════════════════════════════════════════
#pragma once
#include <string.h>
#include "../pf_feature.h"
#include "../../src/core_clock.h"
#include "../../src/core_mem.h"
#include "dashboard_config.h"
#include "dash_accel.h"
#include "dash_clocks.h"
#include "dash_http.h"
#include "dash_night.h"
#include "dash_state.h"
#include "dash_weather.h"

namespace PFFeatureDashboard {

inline void setup() {
  DashWeather::loadSettings();
  DashWeather::requestFetch(30000);  // not before Patternflow has settled (see onNetwork)
  DashState::load();
  DashNight::load();
  DashClocks::load();
  DashAccel::begin();
  PatternflowClock::beginSyncTz(DASH_TZ);
  Serial.printf("[DASH] ready, location %s\n", DashWeather::hasLocation() ? DashWeather::place : "not set");
}

inline void onNetwork() {
  static bool routes = false;
  if (!routes) {
    DashHttp::registerRoutes();
    routes = true;
  }
  // Not straight away: right after connecting, Patternflow itself is busy
  // (time sync, mDNS, pattern loading) and the board's memory is at its tightest.
  DashWeather::requestFetch(30000);
}

inline void loop(const PFFeatureFrame& frame) {
  DashWeather::tick();
  DashNight::tick();
  DashState::dashboardShowing = frame.patternName && (strcmp(frame.patternName, "Dashboard") == 0 ||
                                                      strcmp(frame.patternName, "GIFs") == 0);
  if (frame.patternName && !DashState::dashboardShowing && frame.patternIndex >= 0) {
    DashState::previousPattern = frame.patternIndex;
  }
  // Follow the accelerometer when the panel is turned (K2 still overrides until the next turn)
  static uint32_t seen = 0;
  if (DashAccel::present && DashAccel::autoRotate && DashAccel::stableSerial != seen) {
    seen = DashAccel::stableSerial;
    if (DashAccel::stable >= 0) DashState::orientation = DashAccel::stable;
  }
}

// Patternflow's own patterns are drawn for one fixed way up. When the panel
// hangs upside down, turn the whole frame 180 degrees (the dashboard and the GIFs turn themselves).
inline const uint8_t* composeFrame(const uint8_t* frame, int w, int h) {
  if (!DashAccel::flipPatterns || DashState::dashboardShowing || DashState::orientation < 2) return nullptr;
  static uint8_t* flipped = nullptr;
  if (!flipped) flipped = (uint8_t*)PFMem::alloc((size_t)w * h * 3);
  if (!flipped) return nullptr;
  const int n = w * h;
  for (int i = 0; i < n; i++) memcpy(flipped + (n - 1 - i) * 3, frame + i * 3, 3);
  return flipped;
}

inline bool takePattern(int* idx) {
  if (!DashState::wantBack) return false;
  DashState::wantBack = false;
  if (DashState::previousPattern < 0) return false;
  *idx = DashState::previousPattern;
  return true;
}

inline void observeFrame(const InputFrame& input, const PFFeatureFrame&) {
  for (int i = 0; i < 4; i++)
    if (input.knobDeltas[i] || input.btnPressed[i]) DashNight::userInput();
}

inline void onSleep(bool sleeping) { DashNight::onSleep(sleeping); }

inline bool requestSleep(bool* sleeping) { return DashNight::request(sleeping); }

inline const PFFeature descriptor = {
    "dashboard",   // name
    "dashboard",   // cap string in /api/status
    setup,
    onNetwork,
    loop,
    observeFrame,
    nullptr,       // fillInput
    nullptr,       // onUserInput
    nullptr,       // claimsPattern
    takePattern,
    onSleep,
    requestSleep,
    nullptr,       // shortName
    nullptr,       // isRuntimeEnabled
    nullptr,       // setRuntimeEnabled
    nullptr,       // appendStatus
    nullptr,       // drawOverlay
    "/dashboard",  // navPath - the console header link
    "Dashboard",   // navLabel
    "Location for the weather, night mode, rotation, and the GIFs for the dashboard and the GIFs pattern.",
    composeFrame,
};

}  // namespace PFFeatureDashboard
