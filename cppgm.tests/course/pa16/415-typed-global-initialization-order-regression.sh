#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$test_dir/../../.." && pwd)
app=${CPPGM_PA16_APP:-$repo_root/dev/cppgm++}
lowir2cy86=${CPPGM_PA16_LOWIR2CY86:-$repo_root/dev/lowir2cy86}
cy86=${CPPGM_PA16_CY86:-$repo_root/dev/cy86}
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM

source=$build_dir/interleaved-global-initializers.cpp
printf '%s\n' \
  'int order = 0;' \
  'int first() { order = order * 10 + 1; return 7; }' \
  'struct Pair { int first; int second; };' \
  'Pair values[1] = {{first(), 2}};' \
  'int second() { order = order * 10 + 2; return order; }' \
  'int observed = second();' \
  'int main() { return order == 12 && observed == 12 && values[0].first == 7 ? 0 : 1; }' \
  >"$source"

lowir=$build_dir/interleaved-global-initializers.lowir
"$app" --emit-lowir -O0 -o "$lowir" "$source"
init_body=$build_dir/init.body
sed -n '/^function @__cppgm_init/,/^}/p' "$lowir" >"$init_body"
first_line=$(rg -n -m1 'call i32 @first\(' "$init_body" | cut -d: -f1 || true)
second_line=$(rg -n -m1 'call i32 @second\(' "$init_body" | cut -d: -f1 || true)
if [ -z "$first_line" ] || [ -z "$second_line" ] ||
   [ "$first_line" -ge "$second_line" ]; then
  echo "deferred global initialization was regrouped by implementation kind" >&2
  exit 1
fi

cy86_source=$build_dir/interleaved-global-initializers.cy86
program=$build_dir/interleaved-global-initializers.program
"$lowir2cy86" -o "$cy86_source" "$lowir"
"$cy86" -o "$program" "$cy86_source"
"$program"
