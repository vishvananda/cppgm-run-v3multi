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

source=$build_dir/member-demand-roots.cpp
printf '%s\n' \
  'struct Box {' \
  '  int used() { return this->helper(); }' \
  '  int unused() { return this->hidden(); }' \
  '  int helper() { return 3; }' \
  '  int hidden() { return 9; }' \
  '};' \
  'int main() {' \
  '  Box box;' \
  '  return box.used() == 3 ? 0 : 1;' \
  '}' >"$source"

output=$build_dir/member-demand-roots.lowir
"$app" --emit-lowir -O0 -o "$output" "$source"

if ! rg -Fq 'function @Box__used(%this : ptr)' "$output" ||
   ! rg -Fq 'function @Box__helper' "$output" ||
   ! rg -q -e '^    %t[0-9]+ = call i32 @Box__helper\(%t[0-9]+\)$' "$output"; then
  echo "reachable member-call demand was not followed transitively" >&2
  exit 1
fi
if [ "$(rg -c '^function @Box__helper' "$output")" -ne 1 ]; then
  echo "reachable member helper was not emitted exactly once" >&2
  exit 1
fi
if rg -Fq 'function @Box__unused' "$output" ||
   rg -Fq 'function @Box__hidden' "$output"; then
  echo "unreachable member bodies leaked into LowIR" >&2
  exit 1
fi

cv_source=$build_dir/member-cv-rank.cpp
printf '%s\n' \
  'struct Cell {' \
  '  int get() const { return 20; }' \
  '  int get() { return 7; }' \
  '};' \
  'int read_mutable(Cell & cell) { return cell.get(); }' \
  'int read_const(const Cell & cell) { return cell.get(); }' \
  'int main() {' \
  '  Cell cell;' \
  '  return read_mutable(cell) + read_const(cell) == 27 ? 0 : 1;' \
  '}' >"$cv_source"

cv_output=$build_dir/member-cv-rank.lowir
"$app" --emit-lowir -O0 -o "$cv_output" "$cv_source"
mutable_symbol=$(sed -n 's/^function @\([^ (]*\).*object=_ZN4Cell3getEv.*/\1/p' "$cv_output")
const_symbol=$(sed -n 's/^function @\([^ (]*\).*object=_ZNK4Cell3getEv.*/\1/p' "$cv_output")
if [ -z "$mutable_symbol" ] || [ -z "$const_symbol" ] ||
   [ "$mutable_symbol" = "$const_symbol" ] ||
   ! rg -Fq "call i32 @${mutable_symbol}(" "$cv_output" ||
   ! rg -Fq "call i32 @${const_symbol}(" "$cv_output"; then
  echo "implicit-object cv ranking did not select both typed overloads" >&2
  exit 1
fi

subset_source=$build_dir/member-cv-subset.cpp
printf '%s\n' \
  'struct Qualified {' \
  '  int get() const volatile { return 20; }' \
  '  int get() const { return 10; }' \
  '};' \
  'int read_qualified(Qualified & value) { return value.get(); }' \
  'int main() { Qualified value; return read_qualified(value); }' >"$subset_source"
subset_output=$build_dir/member-cv-subset.lowir
"$app" --emit-lowir -O0 -o "$subset_output" "$subset_source"
subset_symbol=$(sed -n 's/^function @\([^ (]*\).*object=_ZNK9Qualified3getEv.*/\1/p' \
  "$subset_output")
if [ -z "$subset_symbol" ] ||
   ! rg -Fq "call i32 @${subset_symbol}(" "$subset_output" ||
   rg -Fq 'object=_ZNVK9Qualified3getEv' "$subset_output"; then
  echo "implicit-object qualification subset did not select const" >&2
  exit 1
fi

incomparable_source=$build_dir/member-cv-incomparable.cpp
printf '%s\n' \
  'struct Incomparable {' \
  '  int get() const { return 1; }' \
  '  int get() volatile { return 2; }' \
  '};' \
  'int main() { Incomparable value; return value.get(); }' \
  >"$incomparable_source"
expect_failure "$incomparable_source"

declaration_source=$build_dir/member-declaration-only.cpp
printf '%s\n' \
  'struct ExternalMember {' \
  '  int called(int value) const;' \
  '  int unused(int value) const volatile;' \
  '};' \
  'int main() { ExternalMember value; return value.called(7); }' \
  >"$declaration_source"
declaration_output=$build_dir/member-declaration-only.lowir
"$app" --emit-lowir -O0 -o "$declaration_output" "$declaration_source"
if ! rg -Fq \
    'declare function @ExternalMember__called(%this : ptr, %arg0 : i32) -> i32' \
    "$declaration_output" ||
   ! rg -Fq 'object=_ZNK14ExternalMember6calledEi' "$declaration_output" ||
   ! rg -q -e '^    %t[0-9]+ = call i32 @ExternalMember__called\(%t[0-9]+, 7\)$' \
    "$declaration_output" ||
   rg -Fq 'ExternalMember__unused' "$declaration_output"; then
  echo "declaration-only member demand did not retain its typed boundary" >&2
  exit 1
fi

variadic_source=$build_dir/member-variadic-ellipsis.cpp
printf '%s\n' \
  'struct Variadic {' \
  '  int choose(int value, ...) { return value + 1; }' \
  '};' \
  'int main() {' \
  '  Variadic value;' \
  '  return value.choose(1, 2);' \
  '}' >"$variadic_source"
variadic_output=$build_dir/member-variadic-ellipsis.lowir
"$app" --emit-lowir -O0 -o "$variadic_output" "$variadic_source"
if ! rg -q -e '^    %t[0-9]+ = call i32 @Variadic__choose\(%t[0-9]+, 1, 2\)$' \
    "$variadic_output"; then
  echo "member ellipsis argument did not retain the typed hidden object call" >&2
  exit 1
fi

ambiguity_source=$build_dir/member-variadic-ambiguity.cpp
printf '%s\n' \
  'struct Ambiguous {' \
  '  int choose(int value) { return value; }' \
  '  int choose(int value, ...) { return value + 1; }' \
  '};' \
  'int main() {' \
  '  Ambiguous value;' \
  '  return value.choose(1);' \
  '}' >"$ambiguity_source"
expect_failure "$ambiguity_source"
