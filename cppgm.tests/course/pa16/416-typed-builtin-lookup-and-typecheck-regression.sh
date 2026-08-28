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

expect_failure()
{
  source=$1
  output=$build_dir/$(basename "$source").lowir
  if "$app" --emit-lowir -O0 -o "$output" "$source" \
      >"$output.stdout" 2>"$output.stderr"; then
    status=0
  else
    status=$?
  fi
  if [ "$status" -ne 1 ]; then
    echo "$(basename "$source") returned status $status, expected 1" >&2
    exit 1
  fi
}

shadow_source=$build_dir/ordinary-builtin-spelling-shadow.cpp
printf '%s\n' \
  'int __builtin_strlen(int value) { return value + 1; }' \
  'int __builtin_memset(int value) { return value + 2; }' \
  'int main() { return __builtin_strlen(4) == 5 && __builtin_memset(4) == 6 ? 0 : 1; }' \
  >"$shadow_source"
shadow_output=$build_dir/ordinary-builtin-spelling-shadow.lowir
"$app" --emit-lowir -O0 -o "$shadow_output" "$shadow_source"
if ! rg -Fq 'function @__builtin_strlen(%value : i32) -> i32' "$shadow_output" ||
   ! rg -Fq 'function @__builtin_memset(%value : i32) -> i32' "$shadow_output" ||
   ! rg -q 'call i32 @__builtin_strlen\(' "$shadow_output" ||
   ! rg -q 'call i32 @__builtin_memset\(' "$shadow_output" ||
   rg -Fq 'cppgm_builtin_' "$shadow_output"; then
  echo "ordinary declarations did not own exact/reserved builtin spellings" >&2
  exit 1
fi

local_shadow_source=$build_dir/local-builtin-spelling-shadow.cpp
printf '%s\n' \
  'int main() { int __builtin_strlen = 0; return __builtin_strlen("x"); }' \
  >"$local_shadow_source"
expect_failure "$local_shadow_source"

decltype_source=$build_dir/decltype-builtin-bad-argument.cpp
printf '%s\n' \
  'decltype(__builtin_strlen(7)) value = 0;' \
  'int main() { return 0; }' \
  >"$decltype_source"
expect_failure "$decltype_source"

valid_decltype_source=$build_dir/decltype-builtin-valid-argument.cpp
printf '%s\n' \
  'decltype(__builtin_strlen("x")) value = 0;' \
  'int main() { return 0; }' \
  >"$valid_decltype_source"
valid_decltype_output=$build_dir/decltype-builtin-valid-argument.lowir
"$app" --emit-lowir -O0 -o "$valid_decltype_output" "$valid_decltype_source"
if rg -q 'cppgm_builtin_|call .*@__builtin_strlen|declare function @__builtin_strlen|function @__builtin_strlen' \
    "$valid_decltype_output"; then
  echo "valid unevaluated builtin call acquired LowIR demand" >&2
  exit 1
fi

echo "416 typed builtin lookup/type-check regression: PASS"
