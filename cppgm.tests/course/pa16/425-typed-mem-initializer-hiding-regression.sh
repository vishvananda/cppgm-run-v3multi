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

expect_success()
{
  label=$1
  shift
  source=$build_dir/$label.cpp
  output=$build_dir/$label.lowir
  printf '%s\n' "$@" >"$source"
  "$app" --emit-lowir -O0 -o "$output" "$source"
  body=$build_dir/$label.body
  sed -n '/^function @Derived__Derived(/,/^}/p' "$output" >"$body"
  if ! rg -q 'store i32 7' "$body"; then
    echo "$label did not initialize the colliding member" >&2
    exit 1
  fi
  if rg -q 'call void @Base__Base__base_entry\([^\n]*, 7\)' "$body"; then
    echo "$label misclassified the member initializer as a base initializer" >&2
    exit 1
  fi
}

expect_failure()
{
  label=$1
  shift
  source=$build_dir/$label.cpp
  output=$build_dir/$label.lowir
  printf '%s\n' "$@" >"$source"
  if "$app" --emit-lowir -O0 -o "$output" "$source" \
      >"$output.stdout" 2>"$output.stderr"; then
    status=0
  else
    status=$?
  fi
  if [ "$status" -ne 1 ]; then
    echo "$label returned status $status, expected 1" >&2
    exit 1
  fi
}

expect_success direct-base-name \
  'struct Base {' \
  '  int value;' \
  '  Base() : value(3) {}' \
  '  Base(int value) : value(value) {}' \
  '};' \
  'struct Derived : Base {' \
  '  int Base;' \
  '  Derived() : Base(7) {}' \
  '};' \
  'int main() { Derived value; return value.Base == 7 ? 0 : 1; }'

expect_success direct-alias-name \
  'struct Base {' \
  '  int value;' \
  '  Base() : value(3) {}' \
  '};' \
  'typedef Base Alias;' \
  'struct Derived : Alias {' \
  '  int Alias;' \
  '  Derived() : Alias(7) {}' \
  '};' \
  'int main() { Derived value; return value.Alias == 7 ? 0 : 1; }'

expect_failure inherited-member-hides-alias \
  'struct Base {' \
  '  int Alias;' \
  '  Base() : Alias(3) {}' \
  '};' \
  'typedef Base Alias;' \
  'struct Derived : Alias {' \
  '  Derived() : Alias() {}' \
  '};' \
  'int main() { Derived value; return 0; }'

expect_failure duplicate-base-alias \
  'struct Base { Base() {} };' \
  'typedef Base Alias;' \
  'struct Derived : Alias {' \
  '  Derived() : Alias(), Base() {}' \
  '};' \
  'int main() { Derived value; return 0; }'

expect_failure array-alias-not-base \
  'struct Base { Base() {} };' \
  'typedef Base Alias;' \
  'typedef Alias AliasArray[1];' \
  'struct Derived : Alias {' \
  '  Derived() : AliasArray() {}' \
  '};' \
  'int main() { Derived value; return 0; }'

expect_failure nested-type-hides-base \
  'struct Base { Base() {} };' \
  'struct Derived : Base {' \
  '  typedef int Base;' \
  '  Derived() : Base() {}' \
  '};' \
  'int main() { Derived value; return 0; }'
