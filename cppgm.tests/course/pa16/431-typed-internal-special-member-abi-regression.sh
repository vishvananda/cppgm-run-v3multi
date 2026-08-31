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

source=$build_dir/internal-special-member-pair.cpp
printf '%s\n' \
  'namespace {' \
  '  struct Dmi { int value = 7; };' \
  '  Dmi dmi;' \
  '  struct WithDtor { ~WithDtor() {} };' \
  '  WithDtor with_dtor;' \
  '  struct OutDtor { ~OutDtor(); };' \
  '  OutDtor out_dtor;' \
  '  OutDtor::~OutDtor() {}' \
  '}' \
  'int main() { return dmi.value; }' >"$source"

output=$build_dir/internal-special-member-pair.lowir
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
    echo "expected one LowIR entry matching: $pattern" >&2
    exit 1
  fi
}

expect_one 'function @_GLOBAL__N_1__Dmi__Dmi(%this : ptr)'
expect_one 'function @_GLOBAL__N_1__Dmi__Dmi__base_entry(%this : ptr)'
expect_one 'function @_GLOBAL__N_1__Dmi__Dmi(%this : ptr) -> void [unwind=no, binding=internal,'
expect_one 'function @_GLOBAL__N_1__Dmi__Dmi__base_entry(%this : ptr) -> void [unwind=no, binding=internal,'
expect_one 'object=_ZN12_GLOBAL__N_13DmiC1Ev'
expect_one 'object=_ZN12_GLOBAL__N_13DmiC2Ev'
if rg -Fq -- 'alias object _ZN12_GLOBAL__N_13DmiC2Ev' "$output"; then
  echo "internal Dmi C2 entry was duplicated by a complete-entry alias" >&2
  exit 1
fi

expect_one 'function @_GLOBAL__N_1__WithDtor__WithDtor(%this : ptr)'
expect_one 'function @_GLOBAL__N_1__WithDtor__WithDtor__base_entry(%this : ptr)'
expect_one 'function @_GLOBAL__N_1__WithDtor__WithDtor(%this : ptr) -> void [unwind=no, binding=internal,'
expect_one 'function @_GLOBAL__N_1__WithDtor__WithDtor__base_entry(%this : ptr) -> void [unwind=no, binding=internal,'
expect_one 'object=_ZN12_GLOBAL__N_18WithDtorC1Ev'
expect_one 'object=_ZN12_GLOBAL__N_18WithDtorC2Ev'
if rg -Fq -- 'alias object _ZN12_GLOBAL__N_18WithDtorC2Ev' "$output"; then
  echo "internal WithDtor C2 entry was duplicated by a complete-entry alias" >&2
  exit 1
fi
expect_one 'alias object _ZN12_GLOBAL__N_18WithDtorD2Ev'
expect_one 'function @_GLOBAL__N_1__WithDtor___WithDtor(%this : ptr) -> void [binding=internal, object=_ZN12_GLOBAL__N_18WithDtorD1Ev]'
if rg -Fq -- 'function @_GLOBAL__N_1__WithDtor___WithDtor__base_entry' "$output"; then
  echo "in-class WithDtor unexpectedly published a destructor base entry" >&2
  exit 1
fi

expect_one 'function @_GLOBAL__N_1__OutDtor___OutDtor(%this : ptr)'
expect_one 'function @_GLOBAL__N_1__OutDtor___OutDtor__base_entry(%this : ptr)'
expect_one 'function @_GLOBAL__N_1__OutDtor___OutDtor(%this : ptr) -> void [binding=strong, object=_ZN12_GLOBAL__N_17OutDtorD1Ev]'
expect_one 'function @_GLOBAL__N_1__OutDtor___OutDtor__base_entry(%this : ptr) -> void [binding=strong, object=_ZN12_GLOBAL__N_17OutDtorD2Ev]'
expect_one 'object=_ZN12_GLOBAL__N_17OutDtorD1Ev'
expect_one 'object=_ZN12_GLOBAL__N_17OutDtorD2Ev'
expect_one 'function @_GLOBAL__N_1__OutDtor__OutDtor(%this : ptr) -> void [unwind=no, binding=internal, object=_ZN12_GLOBAL__N_17OutDtorC1Ev]'
expect_one 'function @_GLOBAL__N_1__OutDtor__OutDtor__base_entry(%this : ptr) -> void [unwind=no, binding=internal, object=_ZN12_GLOBAL__N_17OutDtorC2Ev]'
expect_one 'object=_ZN12_GLOBAL__N_17OutDtorC1Ev'
expect_one 'object=_ZN12_GLOBAL__N_17OutDtorC2Ev'
if rg -Fq -- 'alias object _ZN12_GLOBAL__N_17OutDtorD2Ev' "$output"; then
  echo "internal OutDtor D2 entry was duplicated by a complete-entry alias" >&2
  exit 1
fi
if rg -Fq -- 'alias object _ZN12_GLOBAL__N_17OutDtorC2Ev' "$output"; then
  echo "internal OutDtor C2 entry was duplicated by a complete-entry alias" >&2
  exit 1
fi
expect_one '    call void @_GLOBAL__N_1__OutDtor___OutDtor('

if rg -Fq -- '_GLOBAL__N_2' "$output"; then
  echo "internal special-member probe allocated a second unnamed namespace component" >&2
  exit 1
fi

echo "431 typed internal special-member ABI regression: PASS"
