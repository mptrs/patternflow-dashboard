// ═══════════════════════════════════════════════════════════
// Patternflow Dashboard - the feature
//
// Wires the dashboard into the Patternflow core without editing it:
//   - setup:       load settings, start the core clock (NTP + timezone)
//   - onNetwork:   the settings page /dashboard, and a first weather fetch
//   - loop:        weather refresh (fetched on core 0, never blocks a frame)
//                  and remembering which pattern ran before the dashboard
//   - takePattern: K4 click on the dashboard goes back to that pattern
//   - night mode:  requestSleep / onSleep / observeFrame (see dash_night.h)
// The screens themselves are the "Dashboard" pattern in preset_dashboard.h.
//
// License: MIT
// ═══════════════════════════════════════════════════════════
#pragma once
#include <string.h>
#include "../pf_feature.h"
#include "../../src/core_clock.h"
#include "dashboard_config.h"
#include "dash_http.h"
#include "dash_night.h"
#include "dash_state.h"
#include "dash_weather.h"

namespace PFFeatureDashboard {

inline void setup() {
  DashWeather::loadSettings();
  DashState::load();
  DashNight::load();
  PatternflowClock::beginSyncTz(DASH_TZ);
  Serial.printf("[DASH] ready, location %s\n", DashWeather::hasLocation() ? DashWeather::place : "not set");
}

inline void onNetwork() {
  static bool routes = false;
  if (!routes) {
    DashHttp::registerRoutes();
    routes = true;
  }
  DashWeather::requestFetch();
}

inline void loop(const PFFeatureFrame& frame) {
  DashWeather::tick();
  DashNight::tick();
  if (frame.patternName && strcmp(frame.patternName, "Dashboard") != 0 && frame.patternIndex >= 0) {
    DashState::previousPattern = frame.patternIndex;
  }
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
    "Location for the weather, night mode, and the GIFs between the dashboard screens.",
};

}  // namespace PFFeatureDashboard
