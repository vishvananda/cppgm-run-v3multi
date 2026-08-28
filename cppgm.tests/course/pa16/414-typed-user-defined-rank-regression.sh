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

source=$build_dir/user-defined-rank.cpp
printf '%s\n' \
  'struct FromPointer {' \
  '  int tag;' \
  '  FromPointer(int *value) : tag(value != 0) {}' \
  '};' \
  'struct FromInteger {' \
  '  int tag;' \
  '  FromInteger(int value) : tag(value) {}' \
  '};' \
  'struct X {};' \
  'int operator+(const X &, const FromPointer &) { return 2; }' \
  'int operator+(const X &, bool) { return 1; }' \
  'int operator-(const X &, const FromInteger &) { return 4; }' \
  'int operator-(const X &, long double) { return 3; }' \
  'int main() {' \
  '  X value;' \
  '  int *pointer = 0;' \
  '  return value + pointer == 1 && value - 3 == 3 ? 0 : 1;' \
  '}' >"$source"

output=$build_dir/user-defined-rank.lowir
if ! "$app" --emit-lowir -O0 -o "$output" "$source" \
    >"$output.stdout" 2>"$output.stderr"; then
  echo "user-defined-vs-exact overload was rejected" >&2
  sed -n '1,20p' "$output.stderr" >&2
  exit 1
fi

if ! rg -q 'call i32 @operatorplus__2\(' "$output"; then
  echo "pointer-to-bool did not beat the converting-constructor overload" >&2
  exit 1
fi

if ! rg -q 'call i32 @operatorminus__2\(' "$output"; then
  echo "larger standard rank did not beat the converting-constructor overload" >&2
  exit 1
fi

echo "414 typed user-defined standard-rank regression: PASS"
