// Patternflow Dashboard - night mode: the panel sleeps from NIGHT_START to NIGHT_END.
//
// Uses Patternflow's own sleep (panel off, board idle, still on Wi-Fi) via the
// requestSleep hook, so it works whatever pattern is running. Any knob or
// button wakes the panel as usual; woken during the night, it goes back to
// sleep after NIGHT_WAKE_MINUTES without input.
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "../../src/core_clock.h"

namespace DashNight {

constexpr uint32_t NIGHT_WAKE_MINUTES = 10;

inline bool enabled = true;
inline int startMin = 22 * 60;  // minutes after midnight
inline int endMin = 8 * 60;

inline bool sleeping = false;        // as reported by the core (onSleep)
inline int pending = -1;             // -1 nothing, 0 wake, 1 sleep
inline int wasNight = -1;            // -1 unknown (first check after boot)
inline uint32_t lastInputMs = 0;
inline uint32_t nextCheckMs = 0;

inline void load() {
  Preferences prefs;
  if (!prefs.begin("dashboard", true)) return;
  enabled = prefs.getBool("nightOn", true);
  startMin = prefs.getUShort("nightStart", 22 * 60) % 1440;
  endMin = prefs.getUShort("nightEnd", 8 * 60) % 1440;
  prefs.end();
}

inline void save(bool on, int start, int end) {
  enabled = on;
  startMin = start % 1440;
  endMin = end % 1440;
  wasNight = -1;  // re-evaluate now, as if just booted
  Preferences prefs;
  if (!prefs.begin("dashboard", false)) return;
  prefs.putBool("nightOn", enabled);
  prefs.putUShort("nightStart", (uint16_t)startMin);
  prefs.putUShort("nightEnd", (uint16_t)endMin);
  prefs.end();
}

inline bool inWindow(int minute) {
  if (startMin == endMin) return false;
  if (startMin < endMin) return minute >= startMin && minute < endMin;
  return minute >= startMin || minute < endMin;  // across midnight, e.g. 22:00-08:00
}

// "HH:MM" <-> minutes
inline int parseClock(const char* s) {
  if (!s || !s[0]) return -1;
  const int h = atoi(s);
  const char* colon = strchr(s, ':');
  const int m = colon ? atoi(colon + 1) : 0;
  if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

inline void userInput() { lastInputMs = millis(); }

inline void onSleep(bool s) {
  if (sleeping && !s) lastInputMs = millis();  // just woke up: the idle timer starts now
  sleeping = s;
}

// Once a second from the feature loop (also while the panel sleeps).
inline void tick() {
  const uint32_t now = millis();
  if ((int32_t)(now - nextCheckMs) < 0) return;
  nextCheckMs = now + 1000;
  if (!enabled || !PatternflowClock::synced()) {
    if (wasNight == 1 && sleeping) pending = 0;  // night mode switched off at night: wake up
    wasNight = -1;
    return;
  }
  const bool night = inWindow(PatternflowClock::minutesOfDay());
  if (night && wasNight != 1) {
    if (!sleeping) pending = 1;             // start of the night (or booted at night)
  } else if (!night && wasNight == 1) {
    if (sleeping) pending = 0;              // morning
  } else if (night && !sleeping && now - lastInputMs > NIGHT_WAKE_MINUTES * 60000UL) {
    pending = 1;                            // woken at night and left alone
  }
  wasNight = night ? 1 : 0;
}

inline bool request(bool* sleep) {
  if (pending < 0) return false;
  *sleep = pending == 1;
  Serial.printf("[DASH] night mode: %s\n", pending ? "sleep" : "wake");
  pending = -1;
  return true;
}

}  // namespace DashNight
