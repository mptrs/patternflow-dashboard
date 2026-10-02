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
#define PF_VARIANT_VERSION  "v0.1.0"

// The dashboard shows up in the K4 pattern browser like any other pattern.
#define PF_FEATURE_PRESET_INCLUDE "dashboard/preset_dashboard.h"
#define PF_FEATURE_PRESETS PATTERN_ENTRY(Dashboard),
