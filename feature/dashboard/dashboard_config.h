// Patternflow Dashboard - defaults fixed at build time.
// Your location is set on the panel itself: http://patternflow.local/dashboard
#pragma once

// Home timezone as a POSIX TZ string. Default: Central European Time with
// EU daylight saving (Amsterdam, Berlin, Paris, ...).
#ifndef DASH_TZ
#define DASH_TZ "CET-1CEST,M3.5.0,M10.5.0/3"
#endif

namespace DashConfig {

// Seconds each screen stays up while the dashboard rotates by itself.
constexpr int SECONDS_CLOCK = 20;
constexpr int SECONDS_OTHER = 10;
// After turning K1 to pick a screen, stay there this long before rotating on.
constexpr int SECONDS_MANUAL_HOLD = 60;
// Weather refresh interval, and the retry delay after a failed fetch.
constexpr uint32_t WEATHER_EVERY_MS = 15UL * 60UL * 1000UL;
constexpr uint32_t WEATHER_RETRY_MS = 60UL * 1000UL;

// World clocks: name (max ~8 letters fits portrait), standard UTC offset in
// hours, daylight saving rule (see dash_tz.h).
struct WorldClock { const char* name; float utcOffset; char rule; };
constexpr WorldClock WORLD_CLOCKS[] = {
    {"NEW YORK", -5, 'U'},
    {"LONDON", 0, 'E'},
    {"TOKYO", 9, 0},
    {"SYDNEY", 10, 'A'},
};
constexpr int WORLD_CLOCK_COUNT = sizeof(WORLD_CLOCKS) / sizeof(WORLD_CLOCKS[0]);

}  // namespace DashConfig
