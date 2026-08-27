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

source=$build_dir/inherited-member-field.cpp
printf '%s\n' \
  'struct A { int a; int get(int value) { return value + a; } };' \
  'struct B : A { int b; };' \
  'struct C : B {' \
  '  int c;' \
  '  int implicit() { return a; }' \
  '  int qualified() { return A::a; }' \
  '};' \
  'int read_dot(C &value) { return value.a; }' \
  'int read_arrow(C *value) { return value->a; }' \
  'int call_dot(C &value) { return value.get(7); }' \
  'int call_arrow(C *value) { return value->get(7); }' \
  'int main() {' \
  '  C value;' \
  '  return read_dot(value) + read_arrow(&value) + value.implicit() + value.qualified() + call_dot(value) + call_arrow(&value);' \
  '}' >"$source"

output=$build_dir/inherited-member-field.lowir
"$app" --emit-lowir -O0 -o "$output" "$source"

check_body()
{
  symbol=$1
  label=$2
  body=$build_dir/$symbol.body
  sed -n "/^function @${symbol}(/,/^}/p" "$output" >"$body"
  if ! rg -Fq "function @${symbol}(" "$output"; then
    echo "$label function was not emitted" >&2
    exit 1
  fi
  bases=$(rg -c 'projection=base_subobject' "$body" || true)
  fields=$(rg -c 'projection=field' "$body" || true)
  if [ "$bases" -ne 2 ] || [ "$fields" -ne 1 ]; then
    echo "$label emitted $bases base projections and $fields field projections" >&2
    exit 1
  fi
  second_base=$(rg -n 'projection=base_subobject' "$body" | tail -1 | cut -d: -f1)
  field=$(rg -n 'projection=field' "$body" | head -1 | cut -d: -f1)
  if [ "$second_base" -ge "$field" ]; then
    echo "$label did not order base projections before the owner field" >&2
    exit 1
  fi
}

check_body C__implicit implicit-this access
check_body C__qualified qualified-base access
check_body read_dot dot access
check_body read_arrow arrow access

check_call_body()
{
  symbol=$1
  label=$2
  body=$build_dir/$symbol.body
  sed -n "/^function @${symbol}(/,/^}/p" "$output" >"$body"
  if ! rg -Fq "function @${symbol}(" "$output"; then
    echo "$label function was not emitted" >&2
    exit 1
  fi
  bases=$(rg -c 'projection=base_subobject' "$body" || true)
  if [ "$bases" -ne 2 ] || ! rg -Fq 'call i32 @A__get' "$body"; then
    echo "$label did not retain the typed inherited call" >&2
    exit 1
  fi
  second_base=$(rg -n 'projection=base_subobject' "$body" | tail -1 | cut -d: -f1)
  call=$(rg -n 'call i32 @A__get' "$body" | head -1 | cut -d: -f1)
  if [ "$second_base" -ge "$call" ]; then
    echo "$label did not order base projections before the call" >&2
    exit 1
  fi
}

check_call_body call_dot dot inherited call
check_call_body call_arrow arrow inherited call
