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

anonymous_source=$build_dir/anonymous-member.cpp
printf '%s\n' \
  'struct Outer {' \
  '  union {' \
  '    int first;' \
  '    int second;' \
  '  };' \
  '  int tail;' \
  '};' \
  'int main() {' \
  '  Outer value;' \
  '  value.first = 7;' \
  '  return 0;' \
  '}' >"$anonymous_source"
expect_failure "$anonymous_source"

cv_source=$build_dir/reference-return-cv.cpp
printf '%s\n' \
  'struct Cell { int value; };' \
  'const Cell &get_cell();' \
  'int main() {' \
  '  get_cell().value = 1;' \
  '  return 0;' \
  '}' >"$cv_source"
expect_failure "$cv_source"
