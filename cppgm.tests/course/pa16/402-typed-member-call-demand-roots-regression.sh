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

tag_method_source=$build_dir/member-tag-method-same-spelling.cpp
printf '%s\n' \
  'int f() { return 99; }' \
  'struct Base {' \
  '  struct f {};' \
  '  int f() { return 7; }' \
  '};' \
  'struct Derived : Base {' \
  '  int call() { return f(); }' \
  '};' \
  'int main() { Derived value; return value.call(); }' \
  >"$tag_method_source"
tag_method_output=$build_dir/member-tag-method-same-spelling.lowir
"$app" --emit-lowir -O0 -o "$tag_method_output" "$tag_method_source"
if ! rg -Fq 'function @Derived__call(%this : ptr)' "$tag_method_output" ||
   ! rg -q -e '^    %t[0-9]+ = index i8 \[projection=base_subobject\] %t[0-9]+, 0$' \
    "$tag_method_output" ||
   ! rg -q -e '^    %t[0-9]+ = call i32 @Base__f\(%t[0-9]+\)$' \
    "$tag_method_output"; then
  echo "same-scope base tag did not leave the ordinary method as the owner" >&2
  exit 1
fi

owned_type_body()
{
  output=$1
  body=$build_dir/$(basename "$output").Derived__call
  sed -n '/^function @Derived__call/,/^}/p' "$output" >"$body"
  if ! rg -Fq 'function @Derived__call(%this : ptr)' "$output" ||
     ! rg -Fq 'return i32 0' "$body" ||
     rg -q 'call .*@f' "$body"; then
    echo "owned type-only member lookup reopened an outer call" >&2
    exit 1
  fi
}

direct_type_source=$build_dir/member-owned-direct-type.cpp
printf '%s\n' \
  'int f() { return 99; }' \
  'struct Derived {' \
  '  typedef int f;' \
  '  int call() { return f(); }' \
  '};' \
  'int main() { Derived value; return value.call(); }' \
  >"$direct_type_source"
direct_type_output=$build_dir/member-owned-direct-type.lowir
"$app" --emit-lowir -O0 -o "$direct_type_output" "$direct_type_source"
owned_type_body "$direct_type_output"

base_type_source=$build_dir/member-owned-base-type.cpp
printf '%s\n' \
  'int f() { return 99; }' \
  'struct Base {' \
  '  typedef int f;' \
  '};' \
  'struct Derived : Base {' \
  '  int call() { return f(); }' \
  '};' \
  'int main() { Derived value; return value.call(); }' \
  >"$base_type_source"
base_type_output=$build_dir/member-owned-base-type.lowir
"$app" --emit-lowir -O0 -o "$base_type_output" "$base_type_source"
owned_type_body "$base_type_output"

inherited_field_source=$build_dir/member-inherited-field-blocked.cpp
printf '%s\n' \
  'int f() { return 99; }' \
  'struct Base { int f; };' \
  'struct Derived : Base {' \
  '  int call() { return f(); }' \
  '};' \
  'int main() { Derived value; return value.call(); }' \
  >"$inherited_field_source"
inherited_field_output=$build_dir/member-inherited-field-blocked.lowir
if "$app" --emit-lowir -O0 -o "$inherited_field_output" \
    "$inherited_field_source"; then
  inherited_field_status=0
else
  inherited_field_status=$?
fi
if [ "$inherited_field_status" -ne 1 ]; then
  echo "inherited non-callable member reopened outer f" >&2
  exit 1
fi
if [ -s "$inherited_field_output" ] &&
   rg -q 'call .*@f' "$inherited_field_output"; then
  echo "blocked inherited member emitted an outer f call" >&2
  exit 1
fi

# A default argument is represented by one typed fact reused by both calls.
# Keep the address demand rooted through repeated transparent casts so the
# PA15 index accepts semantic DAG sharing and visits each cast fact once.
shared_source=$build_dir/static-shared-fact-demand.cpp
printf '%s\n' \
  'struct SharedStatic {' \
  '  static const int value = 7;' \
  '};' \
  'int read(const int *value = reinterpret_cast<const int *>(reinterpret_cast<const void *>(&SharedStatic::value))) {' \
  '  return *value;' \
  '}' \
  'int main() { return read() == 7 && read() == 7 ? 0 : 1; }' \
  >"$shared_source"
shared_output=$build_dir/static-shared-fact-demand.lowir
if ! "$app" --emit-lowir -O0 -o "$shared_output" "$shared_source" \
    >"$shared_output.stdout" 2>"$shared_output.stderr"; then
  cat "$shared_output.stderr" >&2
  exit 1
fi
if [ "$(rg -c '^declare global @SharedStatic__value' "$shared_output" || true)" -ne 1 ] ||
   rg -Fq 'multiple parents' "$shared_output.stderr" ||
   rg -Fq 'projection=field' "$shared_output"; then
  echo "shared default static address demand did not use one typed global" >&2
  sed -n '1,80p' "$shared_output.stderr" >&2
  exit 1
fi
