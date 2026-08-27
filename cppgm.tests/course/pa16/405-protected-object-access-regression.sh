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
  'class Base {' \
  'protected:' \
  '  int protected_method() { return 4; }' \
  '};' \
  'class Derived : public Base {' \
  'public:' \
  '  int qualified() { return Base::protected_method(); }' \
  '};' \
  'int main() {' \
  '  Derived value;' \
  '  return value.qualified() == 4 ? 0 : 1;' \
  '}' >"$positive_source"

positive_output=$build_dir/protected-this.lowir
"$app" --emit-lowir -O0 -o "$positive_output" "$positive_source"
positive_body=$build_dir/Derived__qualified.body
sed -n '/^function @Derived__qualified/,/^}/p' "$positive_output" >"$positive_body"
if ! rg -Fq 'function @Derived__qualified(%this : ptr)' "$positive_output" ||
   [ "$(rg -c 'projection=base_subobject' "$positive_body" || true)" -ne 1 ] ||
   ! rg -Fq 'call i32 @Base__protected_method' "$positive_body"; then
  echo "qualified protected base call did not retain its typed base projection" >&2
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
  '  int protected_method() { return 4; }' \
  '};' \
  'class Derived : public Base {' \
  'public:' \
  '  int call(Base &other) { return other.protected_method(); }' \
  '};' \
  'int main() { return 0; }' >"$method_source"
expect_failure "$method_source"
