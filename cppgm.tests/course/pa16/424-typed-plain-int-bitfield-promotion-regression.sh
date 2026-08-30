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

source=$build_dir/plain-int-bitfield.cpp
printf '%s\n' \
  'struct Bits {' \
  '  int low : 3;' \
  '  int neighbor : 3;' \
  '  int full : 32;' \
  '  signed int signed_value : 3;' \
  '};' \
  'int main() {' \
  '  Bits bits = {-1, 5, -1, -1};' \
  '  if (bits.low != 7) return 1;' \
  '  if (bits.neighbor != 5) return 2;' \
  '  if (bits.full < 0) return 3;' \
  '  if (bits.signed_value != -1) return 4;' \
  '  bits.low = 6;' \
  '  ++bits.low;' \
  '  if (bits.low != 7 || bits.neighbor != 5) return 5;' \
  '  bits.full = 0;' \
  '  --bits.full;' \
  '  if (bits.full != 4294967295U) return 6;' \
  '  return 0;' \
  '}' >"$source"

lowir=$build_dir/plain-int-bitfield.lowir
cy86_source=$build_dir/plain-int-bitfield.cy86
program=$build_dir/plain-int-bitfield.program
"$app" --emit-lowir -O0 -o "$lowir" "$source"
if ! rg -q 'binary sub u32' "$lowir"; then
  echo "plain-int full-width bit-field did not use unsigned update" >&2
  exit 1
fi
if ! rg -q 'cmp ult u32' "$lowir"; then
  echo "plain-int full-width bit-field did not use unsigned comparison" >&2
  exit 1
fi
"$lowir2cy86" -o "$cy86_source" "$lowir"
"$cy86" -o "$program" "$cy86_source"
"$program"

echo "424 typed plain-int bit-field promotion regression: PASS"
