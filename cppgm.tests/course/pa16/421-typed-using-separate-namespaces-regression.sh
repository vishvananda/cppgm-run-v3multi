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
    echo "$(basename "$source") returned status $status, expected EXIT_FAILURE (1)" >&2
    exit 1
  fi
  printf '%s status=%s (expected EXIT_FAILURE)\n' "$(basename "$source")" "$status"
}

type_source=$build_dir/type-with-ordinary-value.cpp
printf '%s\n' \
  'namespace source { struct f {}; }' \
  'namespace destination { int f = 7; using source::f; }' \
  'int main() { struct destination::f object; return destination::f == 7 ? 0 : 1; }' \
  >"$type_source"
type_output=$build_dir/type-with-ordinary-value.lowir
"$app" --emit-lowir -O0 -o "$type_output" "$type_source"
if ! rg -q '^function @main' "$type_output"; then
  echo "type import did not coexist with the destination ordinary value" >&2
  exit 1
fi
printf '%s status=0\n' 'class-tag-to-ordinary-value'

function_source=$build_dir/function-with-tag.cpp
printf '%s\n' \
  'namespace source { int f() { return 4; } }' \
  'namespace destination { struct f {}; using source::f; }' \
  'int main() { return destination::f() - 4; }' \
  >"$function_source"
function_output=$build_dir/function-with-tag.lowir
"$app" --emit-lowir -O0 -o "$function_output" "$function_source"
if ! rg -q 'call i32 @source__f\(' "$function_output"; then
  echo "function import did not coexist with the destination tag" >&2
  exit 1
fi
printf '%s status=0\n' 'ordinary-function-to-class-tag'

enum_type_source=$build_dir/enum-with-ordinary-function.cpp
printf '%s\n' \
  'namespace source { enum f { zero = 0 }; }' \
  'namespace destination { int f() { return 9; } using source::f; }' \
  'int main() { return destination::f() == 9 ? 0 : 1; }' \
  >"$enum_type_source"
enum_type_output=$build_dir/enum-with-ordinary-function.lowir
"$app" --emit-lowir -O0 -o "$enum_type_output" "$enum_type_source"
if ! rg -q 'call i32 @destination__f\(' "$enum_type_output"; then
  echo "enum tag import did not coexist with the destination function" >&2
  exit 1
fi
printf '%s status=0\n' 'enum-tag-to-ordinary-function'

enum_value_source=$build_dir/ordinary-value-with-enum.cpp
printf '%s\n' \
  'namespace source { int f = 11; }' \
  'namespace destination { enum f { zero = 0 }; using source::f; }' \
  'int main() { return destination::f == 11 ? 0 : 1; }' \
  >"$enum_value_source"
enum_value_output=$build_dir/ordinary-value-with-enum.lowir
"$app" --emit-lowir -O0 -o "$enum_value_output" "$enum_value_source"
if ! rg -q '^function @main' "$enum_value_output"; then
  echo "ordinary value import did not coexist with the destination enum tag" >&2
  exit 1
fi
printf '%s status=0\n' 'ordinary-value-to-enum-tag'

typedef_value_source=$build_dir/source-typedef-with-value.cpp
printf '%s\n' \
  'namespace source { typedef int f; }' \
  'namespace destination { int f = 7; using source::f; }' \
  'int main() { return destination::f == 7 ? 0 : 1; }' \
  >"$typedef_value_source"
expect_failure "$typedef_value_source"

alias_function_source=$build_dir/source-alias-with-function.cpp
printf '%s\n' \
  'namespace source { using f = int; }' \
  'namespace destination { int f() { return 7; } using source::f; }' \
  'int main() { return destination::f() == 7 ? 0 : 1; }' \
  >"$alias_function_source"
expect_failure "$alias_function_source"

function_typedef_source=$build_dir/source-function-with-typedef.cpp
printf '%s\n' \
  'namespace source { int f() { return 13; } }' \
  'namespace destination { typedef int f; using source::f; }' \
  'int main() { return destination::f() == 13 ? 0 : 1; }' \
  >"$function_typedef_source"
expect_failure "$function_typedef_source"

value_alias_source=$build_dir/source-value-with-alias.cpp
printf '%s\n' \
  'namespace source { int f = 17; }' \
  'namespace destination { using f = int; using source::f; }' \
  'int main() { return destination::f == 17 ? 0 : 1; }' \
  >"$value_alias_source"
expect_failure "$value_alias_source"

echo "421 typed using separate-namespaces regression: PASS"
