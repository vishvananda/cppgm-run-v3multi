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

run_valid()
{
  name=$1
  shift
  source=$build_dir/$name.cpp
  output=$build_dir/$name.lowir
  printf '%s\n' "$@" >"$source"
  "$app" --emit-lowir -O0 -o "$output" "$source"
}

expect_failure()
{
  name=$1
  shift
  source=$build_dir/$name.cpp
  output=$build_dir/$name.lowir
  printf '%s\n' "$@" >"$source"
  if "$app" --emit-lowir -O0 -o "$output" "$source" \
      >"$output.stdout" 2>"$output.stderr"; then
    echo "expected PA16 rejection: $name" >&2
    return 1
  fi
}

run_valid nonvoid-array-cast \
  'int main() {' \
  '  char storage[4];' \
  '  char* value = (char*)storage;' \
  '  return value == 0 ? 0 : 0;' \
  '}'

run_valid placement-void-pointer \
  'void* operator new(unsigned long, void* p) { return p; }' \
  'struct Value { int member; };' \
  'int main() {' \
  '  char storage[4];' \
  '  Value* value = ::new((void*)storage) Value{1};' \
  '  return value == 0 ? 0 : 0;' \
  '}'

expect_failure allocation-nonvoid-result \
  'int* operator new(unsigned long, void*);' \
  'struct Value { int member; };' \
  'int main() {' \
  '  char storage[4];' \
  '  Value* value = ::new((void*)storage) Value{1};' \
  '  return value == 0;' \
  '}'

expect_failure allocation-nonsizet-first-parameter \
  'void* operator new(int, void*);' \
  'struct Value { int member; };' \
  'int main() {' \
  '  char storage[4];' \
  '  Value* value = ::new((void*)storage) Value{1};' \
  '  return value == 0;' \
  '}'

expect_failure array-new \
  'void* operator new(unsigned long, void*);' \
  'struct Value { int member; };' \
  'int main() {' \
  '  char storage[4];' \
  '  ::new((void*)storage) Value[1];' \
  '  return 0;' \
  '}'

echo "423 typed placement-new boundary regression: PASS"
