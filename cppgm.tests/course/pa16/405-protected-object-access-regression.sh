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

positive_source=$build_dir/protected-this.cpp
printf '%s\n' \
  'struct Further;' \
  'class Base {' \
  'protected:' \
  '  int protected_field;' \
  '  int protected_method() const { return 4; }' \
  'public:' \
  '  int same_owner(Base &object) { return object.protected_field; }' \
  '};' \
  'class Derived : public Base {' \
  'public:' \
  '  int implicit_field() { return protected_field; }' \
  '  int qualified_method() { return Base::protected_method(); }' \
  '  int dot(Derived &object) { return object.protected_field; }' \
  '  int arrow(Derived *object) { return object->protected_method(); }' \
  '  int const_reference(const Derived &object) {' \
  '    return object.protected_field + object.protected_method();' \
  '  }' \
  '  int further_dot(Further &object);' \
  '  int further_arrow(Further *object);' \
  '};' \
  'struct Further : Derived {};' \
  'int Derived::further_dot(Further &object) {' \
  '  return object.protected_field;' \
  '}' \
  'int Derived::further_arrow(Further *object) {' \
  '  return object->protected_method();' \
  '}' \
  'int main() {' \
  '  Base base;' \
  '  Derived value;' \
  '  Further further;' \
  '  return value.implicit_field() + value.qualified_method() +' \
  '    value.dot(value) + value.arrow(&value) +' \
  '    value.const_reference(value) + value.further_dot(further) +' \
  '    value.further_arrow(&further) + base.same_owner(base);' \
  '}' >"$positive_source"

positive_output=$build_dir/protected-this.lowir
"$app" --emit-lowir -O0 -o "$positive_output" "$positive_source"
check_positive_body()
{
  symbol=$1
  expected_bases=$2
  expected_fields=$3
  expected_call=${4:-}
  body=$build_dir/$symbol.body
  sed -n "/^function @${symbol}(/,/^}/p" "$positive_output" >"$body"
  if ! rg -Fq "function @${symbol}(" "$positive_output"; then
    echo "$symbol was not emitted" >&2
    exit 1
  fi
  bases=$(rg -c 'projection=base_subobject' "$body" || true)
  fields=$(rg -c 'projection=field' "$body" || true)
  [ -n "$bases" ] || bases=0
  [ -n "$fields" ] || fields=0
  if [ "$bases" -ne "$expected_bases" ] ||
     [ "$fields" -ne "$expected_fields" ]; then
    echo "$symbol emitted $bases base projections and $fields field projections" >&2
    exit 1
  fi
  if [ -n "$expected_call" ] &&
     ! rg -Fq "call i32 @${expected_call}" "$body"; then
    echo "$symbol did not retain the typed protected method call" >&2
    exit 1
  fi
}

check_positive_body Base__same_owner 0 1
check_positive_body Derived__implicit_field 1 1
check_positive_body Derived__qualified_method 1 0 Base__protected_method
check_positive_body Derived__dot 1 1
check_positive_body Derived__arrow 1 0 Base__protected_method
check_positive_body Derived__const_reference 2 1 Base__protected_method
check_positive_body Derived__further_dot 2 1
check_positive_body Derived__further_arrow 2 0 Base__protected_method

nested_positive_source=$build_dir/nested-protected-derived.cpp
printf '%s\n' \
  'class Base {' \
  'protected:' \
  '  int protected_field;' \
  '  int protected_method() const { return 4; }' \
  '};' \
  'class Derived : public Base {' \
  'public:' \
  '  struct Nested {' \
  '    int read(Derived &object);' \
  '  };' \
  '};' \
  'int Derived::Nested::read(Derived &object) {' \
  '  return object.protected_field + object.protected_method();' \
  '}' \
  'int main() { return 0; }' >"$nested_positive_source"
"$app" --emit-lowir -O0 -o "$build_dir/nested-protected-derived.lowir" \
  "$nested_positive_source"

expect_failure()
{
  source=$1
  expected_diagnostic=${2:-}
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
  if [ -n "$expected_diagnostic" ] &&
     ! rg -Fq "$expected_diagnostic" "$output.stderr"; then
    echo "$(basename "$source") did not emit $expected_diagnostic" >&2
    exit 1
  fi
}

field_source=$build_dir/protected-field-through-base.cpp
printf '%s\n' \
  'class Base {' \
  'protected:' \
  '  int protected_field;' \
  '};' \
  'class Derived : public Base {' \
  'public:' \
  '  int read(Base &other) { return other.protected_field; }' \
  '};' \
  'int main() { return 0; }' >"$field_source"
expect_failure "$field_source"

method_source=$build_dir/protected-method-through-base.cpp
printf '%s\n' \
  'class Base {' \
  'protected:' \
  '  int protected_method() const { return 4; }' \
  '};' \
  'class Derived : public Base {' \
  'public:' \
  '  int call(const Base *other) { return other->protected_method(); }' \
  '};' \
  'int main() { return 0; }' >"$method_source"
expect_failure "$method_source"

nested_base_source=$build_dir/nested-protected-base.cpp
printf '%s\n' \
  'class Base {' \
  'protected:' \
  '  int protected_field;' \
  '  int protected_method() const { return 4; }' \
  '};' \
  'class Derived : public Base {' \
  'public:' \
  '  struct Nested {' \
  '    int reject(Base &object);' \
  '  };' \
  '};' \
  'int Derived::Nested::reject(Base &object) {' \
  '  return object.protected_field + object.protected_method();' \
  '}' \
  'int main() { return 0; }' >"$nested_base_source"
expect_failure "$nested_base_source" 'ERROR: PA12 record member is inaccessible'

static_source=$build_dir/protected-static-object-spelling.cpp
printf '%s\n' \
  'class Base {' \
  'protected:' \
  '  static const int protected_static = 7;' \
  '};' \
  'class Derived : public Base {' \
  'public:' \
  '  int read(Base &object) { return object.protected_static; }' \
  '};' \
  'int main() {' \
  '  Base base;' \
  '  Derived value;' \
  '  return value.read(base);' \
  '}' >"$static_source"
static_output=$build_dir/protected-static-object-spelling.lowir
if "$app" --emit-lowir -O0 -o "$static_output" "$static_source" \
    >"$static_output.stdout" 2>"$static_output.stderr"; then
  static_status=0
else
  static_status=$?
fi
if [ "$static_status" -ne 0 ] ||
   ! rg -Fq 'return i32 7' "$static_output" ||
   rg -Fq 'projection=field' "$static_output" ||
   rg -Fq 'ERROR:' "$static_output.stderr"; then
  echo "protected static object spelling did not use the static storage boundary" >&2
  sed -n '1,80p' "$static_output.stderr" >&2
  exit 1
fi
