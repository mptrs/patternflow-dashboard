// ═══════════════════════════════════════════════════════════
// Patternflow Dashboard - the feature
//
// Adds a "Dashboard" entry to the K4 pattern browser: a clock with moon
// phase, weather, forecast and world clocks. This file only wires the
// feature into the core (the descriptor); the screens live in
// preset_dashboard.h.
//
// Lives outside the Patternflow tree and is copied in by build.sh, so it
// never edits a core file.
//
// License: MIT
// ═══════════════════════════════════════════════════════════
#pragma once
#include "../pf_feature.h"
#include "../../src/core_clock.h"
#include "dashboard_config.h"

namespace PFFeatureDashboard {

inline void setup() {
  // Start NTP + local time. The core clock costs nothing until a feature asks.
  PatternflowClock::beginSyncTz(DASH_TZ);
}

inline const PFFeature descriptor = {
    "dashboard",   // name
    "dashboard",   // cap string in /api/status
    setup,
    nullptr,       // onNetwork
    nullptr,       // loop
    nullptr,       // observeFrame
    nullptr,       // fillInput
    nullptr,       // onUserInput
    nullptr,       // claimsPattern
    nullptr,       // takePattern
    nullptr,       // onSleep
    nullptr,       // requestSleep
    nullptr,       // shortName
    nullptr,       // isRuntimeEnabled
    nullptr,       // setRuntimeEnabled
    nullptr,       // appendStatus
    nullptr,       // drawOverlay
};

}  // namespace PFFeatureDashboard
