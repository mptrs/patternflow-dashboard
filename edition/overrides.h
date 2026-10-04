// ═══════════════════════════════════════════════════════════
// Patternflow Dashboard edition - what it calls itself, and what it sets
//
// Nothing else is changed: same partition table, same /update, same Wi-Fi
// and patterns when you switch to or from this edition.
//
// License: MIT
// ═══════════════════════════════════════════════════════════
#pragma once

#define PF_VARIANT          "dashboard"
#define PF_VARIANT_VERSION  "v0.5.11"

// The dashboard and the GIFs show up in the K4 pattern browser like any other pattern.
#define PF_FEATURE_PRESET_INCLUDE "dashboard/presets.h"
#define PF_FEATURE_PRESETS PATTERN_ENTRY(Dashboard), PATTERN_ENTRY(Gifs),
