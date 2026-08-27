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

state_free=$build_dir/state-free-chain.cpp
printf '%s\n' \
  'struct Base { int base() { return 1; } };' \
  'struct Middle : Base { int middle() { return base(); } };' \
  'struct Derived : Middle { int run() { return middle(); } };' \
  'int main() { Derived value; return value.run() == 1 ? 0 : 1; }' \
  >"$state_free"

state_free_output=$build_dir/state-free-chain.lowir
"$app" --emit-lowir -O0 -o "$state_free_output" "$state_free"
if rg -q -e '@Base__Base|@Middle__Middle|@Derived__Derived' \
    "$state_free_output"; then
  echo "state-free implicit construction emitted a synthetic constructor" >&2
  exit 1
fi

middle_body=$build_dir/middle.body
derived_body=$build_dir/derived.body
sed -n '/^function @Middle__middle/,/^}/p' "$state_free_output" >"$middle_body"
sed -n '/^function @Derived__run/,/^}/p' "$state_free_output" >"$derived_body"
if [ "$(rg -c 'projection=base_subobject' "$middle_body" || true)" -ne 1 ] ||
   [ "$(rg -c 'projection=base_subobject' "$derived_body" || true)" -ne 1 ] ||
   ! rg -Fq 'call i32 @Base__base' "$middle_body" ||
   ! rg -Fq 'call i32 @Middle__middle' "$derived_body"; then
  echo "state-free multi-level base demand did not retain ordered projections" >&2
  exit 1
fi

runtime_required=$build_dir/runtime-required-base.cpp
printf '%s\n' \
  'struct Base { int state = 7; };' \
  'struct Derived : Base {}; ' \
  'int main() { Derived value; return 0; }' \
  >"$runtime_required"

runtime_output=$build_dir/runtime-required-base.lowir
if "$app" --emit-lowir -O0 -o "$runtime_output" "$runtime_required" \
    >"$runtime_output.stdout" 2>"$runtime_output.stderr"; then
  status=0
else
  status=$?
fi
if [ "$status" -ne 1 ]; then
  echo "runtime-requiring implicit construction returned status $status" >&2
  exit 1
fi

member_required=$build_dir/runtime-required-member.cpp
printf '%s\n' \
  'struct State { int state = 7; };' \
  'struct Holder { State state; };' \
  'int main() { Holder value; return 0; }' \
  >"$member_required"

member_output=$build_dir/runtime-required-member.lowir
if "$app" --emit-lowir -O0 -o "$member_output" "$member_required" \
    >"$member_output.stdout" 2>"$member_output.stderr"; then
  status=0
else
  status=$?
fi
if [ "$status" -ne 1 ]; then
  echo "runtime-requiring member construction returned status $status" >&2
  exit 1
fi
