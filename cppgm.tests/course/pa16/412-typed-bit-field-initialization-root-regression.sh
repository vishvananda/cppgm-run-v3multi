#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$test_dir/../../.." && pwd)
app=${CPPGM_PA16_APP:-$repo_root/dev/cppgm++}
lowir2cy86=${CPPGM_PA16_LOWIR2CY86:-$repo_root/dev/lowir2cy86}
cy86=${CPPGM_PA16_CY86:-$repo_root/dev/cy86}
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM

if [ ! -x "$app" ]; then
  echo "PA16 driver is not executable: $app" >&2
  exit 1
fi
if [ ! -x "$lowir2cy86" ]; then
  echo "PA13 LowIR runner is not executable: $lowir2cy86" >&2
  exit 1
fi
if [ ! -x "$cy86" ]; then
  echo "PA9 CY86 runner is not executable: $cy86" >&2
  exit 1
fi

source=$build_dir/bit-field-roots.cpp
printf '%s\n' \
  'struct Bits {' \
  '  signed int first : 4;' \
  '  signed int second : 4;' \
  '};' \
  'struct Holder {' \
  '  Bits inner;' \
  '  Bits array[2];' \
  '};' \
  'int main() {' \
  '  Bits first = {1, 2};' \
  '  Bits second = {3, 4};' \
  '  Holder outer = {{5, 6}, {{7, 1}, {2, 3}}};' \
  '  Holder another = {{4, 5}, {{6, 7}, {1, 2}}};' \
  '  if (first.first != 1) return 1;' \
  '  if (first.second != 2) return 2;' \
  '  if (second.first != 3) return 3;' \
  '  if (second.second != 4) return 4;' \
  '  if (outer.inner.first != 5) return 5;' \
  '  if (outer.inner.second != 6) return 6;' \
  '  if (outer.array[0].first != 7) return 7;' \
  '  if (outer.array[0].second != 1) return 8;' \
  '  if (outer.array[1].first != 2) return 9;' \
  '  if (outer.array[1].second != 3) return 10;' \
  '  if (another.inner.first != 4) return 11;' \
  '  if (another.inner.second != 5) return 12;' \
  '  if (another.array[0].first != 6) return 13;' \
  '  if (another.array[0].second != 7) return 14;' \
  '  if (another.array[1].first != 1) return 15;' \
  '  if (another.array[1].second != 2) return 16;' \
  '  return 0;' \
  '}' \
  >"$source"

lowir=$build_dir/bit-field-roots.lowir
cy86_source=$build_dir/bit-field-roots.cy86
program=$build_dir/bit-field-roots.program
"$app" --emit-lowir -O0 -o "$lowir" "$source"
"$lowir2cy86" -o "$cy86_source" "$lowir"
"$cy86" -o "$program" "$cy86_source"
"$program"
