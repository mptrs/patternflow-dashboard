#!/bin/bash
# Build the Patternflow Dashboard firmware.
#
#   ./build.sh                          build against the version in PATTERNFLOW_VERSION
#   PF_VERSION=v3.11.0 ./build.sh       build against another Patternflow version
#   ./build.sh flash [patternflow.local]   build, then install it on a panel over Wi-Fi
#
# Fetches Patternflow, copies our feature and edition files into it and builds
# the same way Patternflow's own firmware/bundles/build.sh does for one
# composition (that script needs bash 4, which macOS does not ship).
# Patternflow itself is never edited: the checkout is reset before every build.
#
# Output: dist/patternflow-dashboard-<edition version>-pf<patternflow version>.bin
#         dist/<pattern>.pfm for every pattern in patterns/
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
PF_VERSION="${PF_VERSION:-$(tr -d '[:space:]' < "$ROOT/PATTERNFLOW_VERSION")}"
PF="$ROOT/.build/patternflow"
SKETCH="$PF/firmware/patternflow"
FEATURES="$SKETCH/features"
BUILD_DIR="$ROOT/.build/pio"

# 1. Patternflow at the pinned version, clean
if [ ! -d "$PF/.git" ]; then
  git clone --quiet https://github.com/engmung/Patternflow.git "$PF"
fi
git -C "$PF" fetch --quiet --tags origin
git -C "$PF" checkout --quiet --force "$PF_VERSION"
git -C "$PF" clean -fdq   # remove whatever a previous build copied in
echo "patternflow: $PF_VERSION"

# 2. Our feature directory and the edition's two files
mkdir -p "$FEATURES/dashboard"
cp -R "$ROOT/feature/dashboard/." "$FEATURES/dashboard/"
cp "$ROOT"/edition/features_local.h "$ROOT"/edition/overrides.h "$FEATURES/"

# 3. The vendored libraries Patternflow expects in lib/ (same list as its build.sh)
for pair in \
  "HUB75 https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA.git" \
  "Adafruit_GFX https://github.com/adafruit/Adafruit-GFX-Library.git" \
  "Adafruit_BusIO https://github.com/adafruit/Adafruit_BusIO.git" \
  "WebSockets https://github.com/Links2004/arduinoWebSockets.git"; do
  read -r name url <<< "$pair"
  [ -d "$SKETCH/lib/$name" ] || git clone -q --depth 1 "$url" "$SKETCH/lib/$name"
done

# 4. Build with PlatformIO (from our own virtualenv when there is one)
PY=python3
[ -x "$ROOT/.venv/bin/python" ] && PY="$ROOT/.venv/bin/python"

# The settings page, stamped with Patternflow's console chrome and gzipped by its own tool
"$PY" "$ROOT/tools/console_page.py" "$SKETCH" "$ROOT/feature/dashboard/dashboard.html" "$FEATURES/dashboard/dashboard_index.h"
( cd "$SKETCH" && PLATFORMIO_BUILD_DIR="$BUILD_DIR" "$PY" -m platformio run -e firmware )
BIN="$BUILD_DIR/firmware/firmware.bin"

# Never publish an image with someone's Wi-Fi password baked in.
if [ -f "$SKETCH/patternflow_secrets.h" ]; then
  echo "NOTE: built WITH patternflow_secrets.h - your Wi-Fi password is in this image. Do not publish it."
elif ! grep -qa YOUR_WIFI_SSID "$BIN"; then
  echo "WARNING: placeholder SSID missing from the image - check before publishing." >&2
  exit 1
fi

EDITION_VERSION="$(sed -n 's/.*PF_VARIANT_VERSION *"\(.*\)".*/\1/p' "$ROOT/edition/overrides.h")"
mkdir -p "$ROOT/dist"
OUT="$ROOT/dist/patternflow-dashboard-$EDITION_VERSION-pf$PF_VERSION.bin"
cp "$BIN" "$OUT"
echo ""
echo "firmware: $OUT ($(wc -c < "$OUT" | tr -d ' ') bytes)"

# 5. Patterns: every patterns/*/preset_*.h becomes a .pfm module in dist/
rm -rf "$ROOT/.build/modules"
for header in "$ROOT"/patterns/*/preset_*.h; do
  [ -e "$header" ] || continue
  "$PY" "$PF/firmware/toolchain/port_preset.py" "$header" --out-dir "$ROOT/.build/modules" > /dev/null
done
if [ -d "$ROOT/.build/modules" ]; then
  "$PY" "$PF/firmware/toolchain/build_module.py" "$ROOT"/.build/modules/* --out "$ROOT/.build/pfm" | tail -1
  cp "$ROOT"/.build/pfm/*.pfm "$ROOT/dist/"
  for f in "$ROOT"/.build/pfm/*.pfm; do echo "pattern:  dist/$(basename "$f")"; done
fi

# Optional: install over Wi-Fi, the same way the panel's /update page does
if [ "${1:-}" = "flash" ]; then
  DEV="${2:-patternflow.local}"
  echo "installing on $DEV ..."
  # Patternflow only takes an update once it is armed on the device: hold K2 for NETWORK, turn K4 to UPDATE
  curl -sS --max-time 240 -T "$OUT" "http://$DEV/update?size=$(wc -c < "$OUT" | tr -d ' ')" || {
    echo "install failed: is $DEV reachable? Try the panel's IP address instead"; exit 1; }
  echo ""
fi
