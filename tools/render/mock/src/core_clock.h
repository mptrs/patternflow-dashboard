#pragma once
#include <time.h>
namespace PatternflowClock {
inline time_t fixedNow = 0;  // the renderer sets "now"; local time is UTC+2 (CEST)
inline bool synced() { return true; }
inline bool localTime(struct tm* out) { time_t t = fixedNow + 7200; gmtime_r(&t, out); return true; }
inline int minutesOfDay() { struct tm t; localTime(&t); return t.tm_hour * 60 + t.tm_min; }
inline void beginSyncTz(const char*) {}
}
