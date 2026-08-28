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

namespace_source=$build_dir/namespace-qualified-type-and-call.cpp
printf '%s\n' \
  'namespace N { typedef long T; int f(int value) { return value + 1; } }' \
  'int main() { int value = 7; return (N::T)(value) == 7 && (N::f)(0) == 1 ? 0 : 1; }' \
  >"$namespace_source"
namespace_output=$build_dir/namespace-qualified-type-and-call.lowir
"$app" --emit-lowir -O0 -o "$namespace_output" "$namespace_source"
if ! rg -Fq 'function @N__f(%value : i32) -> i32' "$namespace_output" ||
   ! rg -q 'call i32 @N__f\(0\)' "$namespace_output" ||
   ! rg -Fq 'convert sext i64 i32' "$namespace_output"; then
  echo "namespace-qualified type conversion or function call lost typed ownership" >&2
  exit 1
fi

class_source=$build_dir/class-qualified-same-spelling-cast.cpp
printf '%s\n' \
  'struct Owner { typedef unsigned int mask; };' \
  'int main() { unsigned int mask = 7; return (Owner::mask)mask == 7 ? 0 : 1; }' \
  >"$class_source"
class_output=$build_dir/class-qualified-same-spelling-cast.lowir
"$app" --emit-lowir -O0 -o "$class_output" "$class_source"
if ! rg -q 'function @main' "$class_output"; then
  echo "class-qualified same-spelling cast did not lower" >&2
  exit 1
fi

echo "417 qualified parenthesized type/call regression: PASS"
