// Patternflow Dashboard - timezones with daylight saving time, no internet needed.
// Ported from the Matrix Portal version (tested against the tz database
// for 2024-2030, 400k timestamps, no differences).
//
// A zone is a standard UTC offset in hours plus a DST rule:
//   'E' - EU: last Sunday of March to last Sunday of October, 01:00 UTC
//   'U' - US: second Sunday of March to first Sunday of November, 02:00 local
//   'A' - south-east Australia: first Sunday of October to first Sunday of April
//   0   - no daylight saving time
#pragma once
#include <stdint.h>

namespace DashTz {

inline int weekday(int y, int m, int d) {  // 0 = Monday
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d + 6) % 7;
}

inline int64_t daysFromCivil(int y, int m, int d) {  // days since 1970-01-01
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}

inline int64_t wall(int y, int m, int d, int hour) { return daysFromCivil(y, m, d) * 86400 + hour * 3600; }

inline int sunday(int y, int m, int n) {  // n-th Sunday, or the last one for n = -1
  static const int dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const int first = 1 + (6 - weekday(y, m, 1)) % 7;
  if (n > 0) return first + 7 * (n - 1);
  return first + 7 * ((dim[m - 1] - first) / 7);
}

// Offset from UTC in seconds at the given unix time (UTC).
inline int32_t utcOffset(int64_t unix, float stdHours, char rule) {
  const int32_t std = (int32_t)(stdHours * 3600);
  if (!rule) return std;
  // Around New Year this year can be off by a day; the DST answer is the
  // same in both years then, so that does not matter.
  const int y = 1970 + (int)((unix / 86400) * 400 / 146097);
  bool dst = false;
  if (rule == 'E') {
    dst = wall(y, 3, sunday(y, 3, -1), 1) <= unix && unix < wall(y, 10, sunday(y, 10, -1), 1);
  } else if (rule == 'U') {
    dst = wall(y, 3, sunday(y, 3, 2), 2) - std <= unix && unix < wall(y, 11, sunday(y, 11, 1), 2) - std - 3600;
  } else if (rule == 'A') {
    const int64_t end = wall(y, 4, sunday(y, 4, 1), 3) - std - 3600;
    const int64_t start = wall(y, 10, sunday(y, 10, 1), 2) - std;
    dst = !(end <= unix && unix < start);
  }
  return dst ? std + 3600 : std;
}

// ---------------------------------------------------------------- any zone
// A zone as the browser derives it from the time zone database (see the
// settings page): standard offset and DST shift in minutes, and the two
// switch moments as "month, week (5 = last), weekday (0 = Sunday), minutes",
// in local time before the switch - the same idea as a POSIX TZ "M" rule.
// Minutes may lie outside 0..1440 ("Saturday 24:00", "Sunday minus two days").
struct Zone {
  int16_t stdMin = 0;  // standard offset from UTC, minutes east
  int16_t dstMin = 0;  // extra minutes during DST; 0 = no DST
  uint8_t sm = 0, sw = 0, sd = 0;
  int16_t st = 0;      // DST starts: month, week, weekday, local standard time
  uint8_t em = 0, ew = 0, ed = 0;
  int16_t et = 0;      // DST ends: month, week, weekday, local daylight time
};

inline int daysInMonth(int y, int m) {
  static const int dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 29 : dim[m - 1];
}

// Day of the month of the n-th (5 = last) weekday (0 = Sunday).
inline int nthWeekday(int y, int m, int week, int wday) {
  const int firstWday = (weekday(y, m, 1) + 1) % 7;  // weekday() is Monday-based
  int day = 1 + (wday - firstWday + 7) % 7 + 7 * (week - 1);
  while (day > daysInMonth(y, m)) day -= 7;
  return day;
}

inline int32_t utcOffset(int64_t unix, const Zone& z) {
  const int32_t std = z.stdMin * 60;
  if (!z.dstMin || !z.sm || !z.em) return std;
  const int32_t dst = std + z.dstMin * 60;
  const int y = 1970 + (int)(((unix + std) / 86400) * 400 / 146097);
  const int64_t start = daysFromCivil(y, z.sm, nthWeekday(y, z.sm, z.sw, z.sd)) * 86400 + (int64_t)z.st * 60 - std;
  const int64_t end = daysFromCivil(y, z.em, nthWeekday(y, z.em, z.ew, z.ed)) * 86400 + (int64_t)z.et * 60 - dst;
  const bool inDst = start < end ? (start <= unix && unix < end)    // northern hemisphere
                                 : !(end <= unix && unix < start); // southern: DST over New Year
  return inDst ? dst : std;
}

}  // namespace DashTz
