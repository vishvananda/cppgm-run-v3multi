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

source=$build_dir/unnamed-namespace-per-parent.cpp
printf '%s\n' \
  'namespace alpha {' \
  '  namespace {' \
  '    struct X { friend void f(const X &) {} };' \
  '    X x;' \
  '  }' \
  '}' \
  'namespace alpha {' \
  '  namespace {' \
  '    extern X x;' \
  '  }' \
  '}' \
  'namespace beta {' \
  '  namespace {' \
  '    struct X { friend void f(const X &) {} };' \
  '    X x;' \
  '  }' \
  '}' \
  'int main() { return 0; }' \
  >"$source"

output=$build_dir/unnamed-namespace-per-parent.lowir
"$app" --emit-lowir -O0 -o "$output" "$source"

count_fixed()
{
  count=$(rg -F -c -- "$1" "$2" || true)
  if [ -z "$count" ]; then
    count=0
  fi
  printf '%s\n' "$count"
}

expect_one()
{
  pattern=$1
  if [ "$(count_fixed "$pattern" "$output")" -ne 1 ]; then
    echo "expected one LowIR definition matching: $pattern" >&2
    exit 1
  fi
}

if rg -Fq -- '_GLOBAL__N_2' "$output"; then
  echo "unnamed namespace ABI identity advanced beyond the fixed per-parent component" >&2
  exit 1
fi

for parent in alpha beta; do
  expect_one "global @${parent}___GLOBAL__N_1__x [binding=internal,"
  expect_one "function @${parent}___GLOBAL__N_1__f(%__param0 : ptr [pass=reference]) -> void [binding=internal,"
  expect_one "function @${parent}___GLOBAL__N_1__X__X(%this : ptr) -> void [unwind=no, binding=internal,"
  expect_one "function @${parent}___GLOBAL__N_1__X__X__base_entry(%this : ptr) -> void [unwind=no, binding=internal,"
  if ! rg -Fq -- "object=_ZN${#parent}${parent}12_GLOBAL__N_1" "$output"; then
    echo "$parent ABI names did not retain its typed unnamed namespace component" >&2
    exit 1
  fi
done

components=$(rg -o '_GLOBAL__N_[0-9]+__' "$output" | sort -u || true)
if [ "$components" != "_GLOBAL__N_1__" ]; then
  echo "unexpected unnamed namespace ABI components: $components" >&2
  exit 1
fi

echo "430 typed unnamed-namespace per-parent regression: PASS"
