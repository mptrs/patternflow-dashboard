// Patternflow Dashboard - the world clocks (up to 4), set on the settings page.
// Stored in NVS as text, one clock per line: "NAME|std,dst,sm.sw.sd/st,em.ew.ed/et|Zone/Label"
// (see DashTz::Zone). The browser derives the rule from its time zone database; the
// label (e.g. Asia/Tokyo) is only for the settings page.
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <stdio.h>
#include <string.h>
#include "dash_tz.h"

namespace DashClocks {

constexpr int MAX = 4;
constexpr int NAME_LEN = 10;
constexpr int LABEL_LEN = 40;

inline int count = 0;
inline char names[MAX][NAME_LEN + 1];
inline char labels[MAX][LABEL_LEN + 1];
inline DashTz::Zone zones[MAX];

constexpr const char* DEFAULTS =
    "NEW YORK|-300,60,3.2.0/120,11.1.0/120|America/New_York\n"
    "LONDON|0,60,3.5.0/60,10.5.0/120|Europe/London\n"
    "TOKYO|540,0|Asia/Tokyo\n"
    "SYDNEY|600,60,10.1.0/120,4.1.0/180|Australia/Sydney\n";

// Parses one "NAME|spec" line. Returns false (and leaves nothing behind) if it is malformed.
inline bool parseLine(const char* line, char* name, char* label, DashTz::Zone& z) {
  const char* bar = strchr(line, '|');
  if (!bar || bar == line || bar - line > NAME_LEN) return false;
  memcpy(name, line, bar - line);
  name[bar - line] = 0;
  for (char* c = name; *c; c++)
    if (*c < 0x20 || *c > 0x7e || *c == '"' || *c == '\\') return false;
  label[0] = 0;
  const char* bar2 = strchr(bar + 1, '|');
  if (bar2) {
    const size_t n = strlen(bar2 + 1);
    if (n > LABEL_LEN) return false;
    for (size_t i = 0; i < n; i++) {
      const char c = bar2[1 + i];
      if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '/' || c == '_' || c == '-' || c == '+'))
        return false;
    }
    memcpy(label, bar2 + 1, n + 1);
  }
  int std = 0, dst = 0, sm = 0, sw = 0, sd = 0, st = 0, em = 0, ew = 0, ed = 0, et = 0;
  const int n = sscanf(bar + 1, "%d,%d,%d.%d.%d/%d,%d.%d.%d/%d", &std, &dst, &sm, &sw, &sd, &st, &em, &ew, &ed, &et);
  if (n != 2 && n != 10) return false;
  if (std < -720 || std > 840 || dst < 0 || dst > 120) return false;
  if (n == 10 && (sm < 1 || sm > 12 || em < 1 || em > 12 || sw < 1 || sw > 5 || ew < 1 || ew > 5 || sd > 6 || ed > 6 ||
                  st < -4320 || st > 4320 || et < -4320 || et > 4320))
    return false;
  z = DashTz::Zone();
  z.stdMin = (int16_t)std;
  if (n == 10 && dst) {
    z.dstMin = (int16_t)dst;
    z.sm = sm, z.sw = sw, z.sd = sd, z.st = (int16_t)st;
    z.em = em, z.ew = ew, z.ed = ed, z.et = (int16_t)et;
  }
  return true;
}

// Replaces all clocks with the lines in `text`. Returns how many were valid.
inline int parseAll(const char* text) {
  int n = 0;
  char line[128];
  while (text && *text && n < MAX) {
    const char* nl = strchr(text, '\n');
    const size_t len = nl ? (size_t)(nl - text) : strlen(text);
    if (len > 0 && len < sizeof line) {
      memcpy(line, text, len);
      line[len] = 0;
      if (parseLine(line, names[n], labels[n], zones[n])) n++;
    }
    text = nl ? nl + 1 : text + len;
  }
  count = n;
  return n;
}

// The current clocks as text (the same format), for the page and for NVS.
inline String format() {
  String out;
  char line[128];
  for (int i = 0; i < count; i++) {
    const auto& z = zones[i];
    if (z.dstMin)
      snprintf(line, sizeof line, "%s|%d,%d,%d.%d.%d/%d,%d.%d.%d/%d|%s\n", names[i], z.stdMin, z.dstMin, z.sm, z.sw,
               z.sd, z.st, z.em, z.ew, z.ed, z.et, labels[i]);
    else
      snprintf(line, sizeof line, "%s|%d,0|%s\n", names[i], z.stdMin, labels[i]);
    out += line;
  }
  return out;
}

inline void load() {
  Preferences prefs;
  String text;
  if (prefs.begin("dashboard", true)) {
    if (prefs.isKey("clocks")) text = prefs.getString("clocks", "");
    prefs.end();
  }
  if (!text.length()) parseAll(DEFAULTS);  // never configured: the defaults
  else parseAll(text.c_str());            // (an empty list is a valid choice)
}

inline void save() {
  Preferences prefs;
  if (!prefs.begin("dashboard", false)) return;
  prefs.putString("clocks", format());
  prefs.end();
}

}  // namespace DashClocks
