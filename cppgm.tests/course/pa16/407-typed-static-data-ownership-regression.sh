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

redecl_source=$build_dir/redecl-after-definition.cpp
printf '%s\n' \
  'struct Reopened {' \
  '  static int value;' \
  '};' \
  'int Reopened::value = 3;' \
  'extern int Reopened::value;' \
  'int main() { return Reopened::value - 3; }' \
  >"$redecl_source"
redecl_output=$build_dir/redecl-after-definition.lowir
"$app" --emit-lowir -O0 -o "$redecl_output" "$redecl_source"
if [ "$(rg -c '^global @Reopened__value .* = 3$' "$redecl_output" || true)" -ne 1 ] ||
   rg -Fq 'global @Reopened__value : i32 [binding=strong, object=_ZN8Reopened5valueE] = zero' \
     "$redecl_output"; then
  echo "a later bodyless redeclaration erased the static definition initializer" >&2
  exit 1
fi

inherited_use_source=$build_dir/inherited-use.cpp
printf '%s\n' \
  'struct Base { static int value; };' \
  'int Base::value = 5;' \
  'struct Derived : Base { static int read() { return value; } };' \
  'int main() { return Derived::value + Derived::read() - 10; }' \
  >"$inherited_use_source"
inherited_use_output=$build_dir/inherited-use.lowir
"$app" --emit-lowir -O0 -o "$inherited_use_output" "$inherited_use_source"
if [ "$(rg -c '^global @Base__value .* = 5$' "$inherited_use_output" || true)" -ne 1 ] ||
   rg -Fq '@Derived__value' "$inherited_use_output"; then
  echo "inherited static use did not retain the declaring owner" >&2
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

inherited_source=$build_dir/inherited-definition.cpp
printf '%s\n' \
  'struct Base { static int value; };' \
  'struct Derived : Base {};' \
  'int Derived::value = 1;' \
  'int main() { return 0; }' \
  >"$inherited_source"
expect_failure "$inherited_source"

imported_source=$build_dir/imported-definition.cpp
printf '%s\n' \
  'struct Base { static int value; };' \
  'struct Derived : Base { using Base::value; };' \
  'int Derived::value = 1;' \
  'int main() { return 0; }' \
  >"$imported_source"
expect_failure "$imported_source"

nonstatic_member_source=$build_dir/nonstatic-member-from-static.cpp
printf '%s\n' \
  'struct A { const int x = 1; static int f() { return x; } };' \
  'int main() { return A::f(); }' \
  >"$nonstatic_member_source"
expect_failure "$nonstatic_member_source"

qualified_nonstatic_source=$build_dir/qualified-nonstatic-member.cpp
printf '%s\n' \
  'struct A { const int x = 1; };' \
  'int main() { return A::x; }' \
  >"$qualified_nonstatic_source"
expect_failure "$qualified_nonstatic_source"

outer_fallback_source=$build_dir/outer-name-fallback.cpp
printf '%s\n' \
  'int x = 41;' \
  'struct A { const int x = 1; static int f() { return x; } };' \
  'int main() { return A::f(); }' \
  >"$outer_fallback_source"
expect_failure "$outer_fallback_source"

lexical_shadow_source=$build_dir/lexical-shadow.cpp
printf '%s\n' \
  'int x = 41;' \
  'struct A { const int x = 1; static int f() { int x = 7; return x; } };' \
  'int main() { return A::f() - 7; }' \
  >"$lexical_shadow_source"
lexical_shadow_output=$build_dir/lexical-shadow.lowir
"$app" --emit-lowir -O0 -o "$lexical_shadow_output" "$lexical_shadow_source"
if ! rg -Fq 'slot $x : i32' "$lexical_shadow_output" ||
   ! rg -Fq 'store i32 7, $x' "$lexical_shadow_output"; then
  echo "a local lexical shadow was not preserved over the class member" >&2
  exit 1
fi
