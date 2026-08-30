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

source=$build_dir/pack-wide-bitfield.cpp
printf '%s\n' \
  '#pragma pack(push, 1)' \
  'struct PackedWide {' \
  '  char prefix;' \
  '  int value : 33;' \
  '  char suffix;' \
  '};' \
  '#pragma pack(pop)' \
  'struct NaturalWide {' \
  '  char prefix;' \
  '  int value : 33;' \
  '  char suffix;' \
  '};' \
  'int main() {' \
  '  if (sizeof(PackedWide) != 10) return 1;' \
  '  if (sizeof(NaturalWide) != 16) return 2;' \
  '  return 0;' \
  '}' >"$source"

lowir=$build_dir/pack-wide-bitfield.lowir
cy86_source=$build_dir/pack-wide-bitfield.cy86
program=$build_dir/pack-wide-bitfield.program
"$app" --emit-lowir -O0 -o "$lowir" "$source"
"$lowir2cy86" -o "$cy86_source" "$lowir"
"$cy86" -o "$program" "$cy86_source"
"$program"

echo "422 typed pack wide-bitfield layout regression: PASS"
