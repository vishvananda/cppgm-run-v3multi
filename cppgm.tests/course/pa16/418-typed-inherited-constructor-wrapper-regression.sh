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

dmi_source=$build_dir/inherited-dmi.cpp
printf '%s\n' \
  'struct Base {' \
  '  int value;' \
  '  Base(int value) : value(value) {}' \
  '};' \
  'struct Member {' \
  '  int value;' \
  '  Member() : value(11) {}' \
  '};' \
  'struct Derived : Base {' \
  '  Member child;' \
  '  int marker = 9;' \
  '  using Base::Base;' \
  '};' \
  'int main() { Derived value(4); return value.marker == 9 && value.child.value == 11 ? 0 : 1; }' \
  >"$dmi_source"
dmi_output=$build_dir/inherited-dmi.lowir
"$app" --emit-lowir -O0 -o "$dmi_output" "$dmi_source"
dmi_body=$build_dir/inherited-dmi.body
sed -n '/^function @Derived__Derived(/,/^}/p' "$dmi_output" >"$dmi_body"
if ! rg -Fq 'store i32 9' "$dmi_body"; then
  echo "inherited constructor did not apply the derived default member initializer" >&2
  exit 1
fi
if ! rg -q 'call void @Member__Member\(' "$dmi_body"; then
  echo "inherited constructor did not demand the derived member default constructor" >&2
  exit 1
fi

defaults_source=$build_dir/inherited-defaults.cpp
printf '%s\n' \
  'struct Base {' \
  '  int value;' \
  '  Base(int value, int extra = 3) : value(value + extra) {}' \
  '};' \
  'struct Derived : Base { using Base::Base; };' \
  'int main() { Derived value(4); return value.value == 7 ? 0 : 1; }' \
  >"$defaults_source"
defaults_output=$build_dir/inherited-defaults.lowir
"$app" --emit-lowir -O0 -o "$defaults_output" "$defaults_source"
if ! rg -q '^function @Derived__Derived\(%this : ptr, %value : i32\) -> void' \
    "$defaults_output"; then
  echo "notional trailing-default wrapper did not publish its shortened signature" >&2
  exit 1
fi
defaults_body=$build_dir/inherited-defaults.body
sed -n '/^function @Derived__Derived(/,/^}/p' "$defaults_output" >"$defaults_body"
if ! rg -q 'call void @Base__Base__base_entry\([^\n]*, 3\)' \
    "$defaults_body"; then
  echo "notional wrapper did not forward the omitted typed default to the base entry" >&2
  exit 1
fi

transitive_source=$build_dir/hard-only-transitive.cpp
printf '%s\n' \
  'struct LibraryBase {' \
  '  LibraryBase(const char &token, int extra = 5) {}' \
  '};' \
  'struct Soft : LibraryBase { using LibraryBase::LibraryBase; };' \
  'struct Hard : Soft { using Soft::Soft; };' \
  'int main() { const char token = 1; Hard value(token); return 0; }' \
  >"$transitive_source"
transitive_output=$build_dir/hard-only-transitive.lowir
"$app" --emit-lowir -O0 -o "$transitive_output" "$transitive_source"
if ! rg -q '^function @Hard__Hard\(%this : ptr, %token : ptr \[pass=reference\]\) -> void' \
    "$transitive_output"; then
  echo "hard-only construction did not discover the typed transitive notional wrapper" >&2
  exit 1
fi
hard_body=$build_dir/hard-only-transitive.hard.body
sed -n '/^function @Hard__Hard(/,/^}/p' "$transitive_output" >"$hard_body"
if ! rg -q 'call void @Soft__Soft__base_entry\(' "$hard_body"; then
  echo "hard-only construction did not call the immediate base entry" >&2
  exit 1
fi
soft_body=$build_dir/hard-only-transitive.soft.body
sed -n '/^function @Soft__Soft__base_entry(/,/^}/p' \
  "$transitive_output" >"$soft_body"
if ! rg -q 'call void @LibraryBase__LibraryBase__base_entry\([^\n]*, 5\)' \
    "$soft_body"; then
  echo "transitive wrapper did not forward the omitted typed default" >&2
  exit 1
fi

n3485_source=$build_dir/inheriting-default-example.cpp
printf '%s\n' \
  'struct B1 {' \
  '  B1(int value) {}' \
  '};' \
  'struct D1 : B1 { using B1::B1; };' \
  'struct B2 {' \
  '  int value;' \
  '  B2(int first = 13, int second = 42) : value(first + second) {}' \
  '};' \
  'struct D2 : B2 { using B2::B2; };' \
  'int main() { D1 one(7); D2 two; return two.value == 55 ? 0 : 1; }' \
  >"$n3485_source"
n3485_output=$build_dir/inheriting-default-example.lowir
"$app" --emit-lowir -O0 -o "$n3485_output" "$n3485_source"
if ! rg -q '^function @D1__D1\(%this : ptr, %value : i32\) -> void' \
    "$n3485_output"; then
  echo "N3485 D1 did not publish its one-parameter notional inherited constructor" >&2
  exit 1
fi
if ! rg -q '^function @D2__D2\(%this : ptr\) -> void' "$n3485_output" || \
    rg -q '^function @D2__D2\(%this : ptr,' "$n3485_output"; then
  echo "N3485 D2 did not keep its implicit default distinct from inherited wrappers" >&2
  exit 1
fi
d2_body=$build_dir/inheriting-default-example.d2
sed -n '/^function @D2__D2(%this : ptr) -> void/,/^}/p' \
  "$n3485_output" >"$d2_body"
if ! rg -q 'call void @B2__B2__base_entry\([^\n]*, 13, 42\)' "$d2_body"; then
  echo "N3485 D2 implicit default did not forward typed base defaults" >&2
  exit 1
fi

explicit_source=$build_dir/inherited-explicit-copy.cpp
printf '%s\n' \
  'struct Base {' \
  '  explicit Base(int) {}' \
  '};' \
  'struct Derived : Base { using Base::Base; };' \
  'int main() { Derived value = 4; return 0; }' \
  >"$explicit_source"
explicit_output=$build_dir/inherited-explicit-copy.lowir
if "$app" --emit-lowir -O0 -o "$explicit_output" "$explicit_source" \
    >"$explicit_output.stdout" 2>"$explicit_output.stderr"; then
  explicit_status=0
else
  explicit_status=$?
fi
if [ "$explicit_status" -ne 1 ]; then
  echo "inherited explicit constructor copy-initialization returned status "\
    "$explicit_status, expected 1" >&2
  exit 1
fi
