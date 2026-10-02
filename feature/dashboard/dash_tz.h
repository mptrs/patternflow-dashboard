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

}  // namespace DashTz
