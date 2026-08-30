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

expect_run()
{
  name=$1
  shift
  source=$build_dir/$name.cpp
  lowir=$build_dir/$name.lowir
  cy86_source=$build_dir/$name.cy86
  program=$build_dir/$name.program
  printf '%s\n' "$@" >"$source"
  "$app" --emit-lowir -O0 -o "$lowir" "$source"
  "$lowir2cy86" -o "$cy86_source" "$lowir"
  "$cy86" -o "$program" "$cy86_source"
  "$program"
}

expect_failure()
{
  name=$1
  shift
  source=$build_dir/$name.cpp
  lowir=$build_dir/$name.lowir
  printf '%s\n' "$@" >"$source"
  if "$app" --emit-lowir -O0 -o "$lowir" "$source" \
      >"$lowir.stdout" 2>"$lowir.stderr"; then
    status=0
  else
    status=$?
  fi
  if [ "$status" -ne 1 ]; then
    echo "$name returned status $status, expected EXIT_FAILURE (1)" >&2
    exit 1
  fi
}

expect_run inline-type-enclosing-function \
  'namespace api {' \
  'inline namespace v1 {' \
  'struct tag {};' \
  '}' \
  'bool inspect(const v1::tag& value) {' \
  '  return true;' \
  '}' \
  '}' \
  'bool invoke() {' \
  '  api::tag value;' \
  '  return inspect(value);' \
  '}' \
  'int main() {' \
  '  return invoke() ? 0 : 1;' \
  '}'

expect_run enclosing-type-inline-function \
  'namespace api {' \
  'struct tag {};' \
  'bool inspect(int value) {' \
  '  return false;' \
  '}' \
  'inline namespace v1 {' \
  'bool inspect(const tag& value) {' \
  '  return true;' \
  '}' \
  '}' \
  '}' \
  'bool invoke() {' \
  '  api::tag value;' \
  '  return inspect(value);' \
  '}' \
  'int main() {' \
  '  return invoke() ? 0 : 1;' \
  '}'

expect_run pointer-associated-class \
  'namespace adl_pointer {' \
  'struct tag {};' \
  'int inspect(tag* value) {' \
  '  return value != 0 ? 17 : 0;' \
  '}' \
  '}' \
  'bool invoke() {' \
  '  adl_pointer::tag value;' \
  '  return inspect(&value) == 17;' \
  '}' \
  'int main() {' \
  '  return invoke() ? 0 : 1;' \
  '}'

expect_run array-associated-element \
  'namespace adl_array {' \
  'struct tag {};' \
  'int inspect(const tag* value) {' \
  '  return value != 0 ? 23 : 0;' \
  '}' \
  '}' \
  'bool invoke() {' \
  '  adl_array::tag values[1];' \
  '  return inspect(values) == 23;' \
  '}' \
  'int main() {' \
  '  return invoke() ? 0 : 1;' \
  '}'

expect_run function-pointer-associated-parameter \
  'namespace adl_function {' \
  'struct tag {};' \
  'int callback(const tag& value) {' \
  '  return 31;' \
  '}' \
  'int inspect(int (*value)(const tag&)) {' \
  '  return value != 0 ? 31 : 0;' \
  '}' \
  '}' \
  'bool invoke() {' \
  '  return inspect(&adl_function::callback) == 31;' \
  '}' \
  'int main() {' \
  '  return invoke() ? 0 : 1;' \
  '}'

expect_failure ordinary-parent-not-associated \
  'namespace outer {' \
  'namespace inner {' \
  'struct tag {};' \
  '}' \
  'bool inspect(const inner::tag& value) {' \
  '  return true;' \
  '}' \
  '}' \
  'bool invoke() {' \
  '  outer::inner::tag value;' \
  '  return inspect(value);' \
  '}' \
  'int main() {' \
  '  return invoke() ? 0 : 1;' \
  '}'

expect_failure using-directive-not-associated \
  'namespace api {' \
  'struct tag {};' \
  'namespace imported {' \
  'bool inspect(const tag& value) {' \
  '  return true;' \
  '}' \
  '}' \
  'using namespace imported;' \
  '}' \
  'bool invoke() {' \
  '  api::tag value;' \
  '  return inspect(value);' \
  '}' \
  'int main() {' \
  '  return invoke() ? 0 : 1;' \
  '}'

echo "426 typed ADL inline-namespace regression: PASS"
