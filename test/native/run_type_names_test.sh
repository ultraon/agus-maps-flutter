#!/usr/bin/env bash
# Builds and runs test/native/type_names_test.mm on the host (macOS) against
# the app's bundled localized_types folder.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DATA="${LUMI_COMAPS_DATA:-$ROOT/../../assets/comaps_data}"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

"${CXX:-clang++}" -std=c++20 -O1 -fobjc-arc -x objective-c++ \
  -I"$ROOT/ios/agus_maps_flutter/Sources/agus_maps_flutter_native" \
  "$ROOT/test/native/type_names_test.mm" -framework Foundation \
  -o "$OUT/type_names_test"
"$OUT/type_names_test" "$DATA"
