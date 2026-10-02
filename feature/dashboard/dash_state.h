// Patternflow Dashboard - state shared between the feature and the Dashboard pattern.
#pragma once
#include <Preferences.h>

namespace DashState {

inline int orientation = 1;      // 0 landscape, 1 portrait (Patternflow's usual mounting), 2/3 upside down
inline bool wantBack = false;    // K4 click: go back to the pattern before the dashboard
inline int previousPattern = -1; // the last pattern that was not the dashboard

inline void load() {
  Preferences prefs;
  if (!prefs.begin("dashboard", true)) return;
  orientation = prefs.getUChar("orient", 1) & 3;
  prefs.end();
}

inline void saveOrientation() {
  Preferences prefs;
  if (!prefs.begin("dashboard", false)) return;
  prefs.putUChar("orient", (uint8_t)orientation);
  prefs.end();
}

}  // namespace DashState
