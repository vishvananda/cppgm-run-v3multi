#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$test_dir/../../.." && pwd)
app=${CPPGM_PA16_APP:-$repo_root/dev/cppgm++}
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM

if [ ! -x "$app" ]; then
  echo "PA16 driver is not executable: $app" >&2
  exit 1
fi

source=$build_dir/cast-boundary.cpp
output=$build_dir/cast-boundary.lowir
printf '%s\n' \
  'int left_value() { return 1; }' \
  'long right_value() { return 2; }' \
  'int main() { return static_cast<short>(left_value()) + right_value(); }' \
  >"$source"

"$app" --emit-lowir -O0 -o "$output" "$source"

awk '
  /^function @main\(/ { inside = 1; next }
  inside && /^}/ { exit }
  inside && /call i32 @left_value\(\)/ { left = NR }
  inside && /convert trunc i16 i32/ { cast = NR }
  inside && /call i64 @right_value\(\)/ { right = NR }
  inside && /convert sext i64 i16/ { promotion = NR }
  inside && /binary add i64/ { add = NR }
  END {
    if (!left || !cast || !right || !promotion || !add ||
        !(left < cast && cast < right && right < promotion && promotion < add))
      exit 1
  }
' "$output"

echo "432 typed binary cast-boundary regression: PASS"
