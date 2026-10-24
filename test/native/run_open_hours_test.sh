#!/usr/bin/env bash
# Builds and runs test/native/open_hours_test.cpp on the host with the CoMaps
# opening hours sources from thirdparty/comaps (the build tool clones them).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
COMAPS="$ROOT/thirdparty/comaps"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

INCLUDES=(-I"$ROOT/src" -I"$COMAPS" -I"$COMAPS/3party")
for dir in "$COMAPS"/3party/boost/libs/*/include "$COMAPS"/3party/boost/libs/*/*/include; do
  INCLUDES+=(-I"$dir")
done

"${CXX:-clang++}" -std=c++20 -O1 -w "${INCLUDES[@]}" \
  "$ROOT/test/native/open_hours_test.cpp" "$COMAPS"/3party/opening_hours/*.cpp \
  -o "$OUT/open_hours_test"
TZ=Europe/Warsaw "$OUT/open_hours_test"
