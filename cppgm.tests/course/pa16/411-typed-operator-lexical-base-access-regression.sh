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
  source=$2
  output=$build_dir/$label.lowir
  if ! "$app" --emit-lowir -O0 -o "$output" "$source" \
      >"$output.stdout" 2>"$output.stderr"; then
    echo "$label was rejected" >&2
    sed -n '1,20p' "$output.stderr" >&2
    exit 1
  fi
}

expect_failure()
{
  label=$1
  source=$2
  output=$build_dir/$label.lowir
  if "$app" --emit-lowir -O0 -o "$output" "$source" \
      >"$output.stdout" 2>"$output.stderr"; then
    status=0
  else
    status=$?
  fi
  if [ "$status" -ne 1 ]; then
    echo "$label returned status $status, expected 1" >&2
    sed -n '1,20p' "$output.stderr" >&2
    exit 1
  fi
}

lexical_source=$build_dir/multi-friend-lexical-owner.cpp
printf '%s\n' \
  'struct First;' \
  'struct Second {' \
  '  Second(int input) : secret(input) {}' \
  '  struct Nested { long value; };' \
  '  enum { marker = 2 };' \
  'private:' \
  '  int secret;' \
  '  friend int operator+(const First &, const Second &);' \
  '};' \
  'struct First {' \
  '  struct Nested { int value; };' \
  '  enum { marker = 7 };' \
  '  friend int operator+(const First &, const Second & second) {' \
  '    return sizeof(Nested) == 4 && marker == 7 && second.secret == 5 ? 0 : 1;' \
  '  }' \
  '};' \
  'int main() {' \
  '  First first;' \
  '  Second second(5);' \
  '  return first + second;' \
  '}' >"$lexical_source"
lexical_output=$build_dir/multi-friend-lexical-owner.lowir
expect_success multi-friend-lexical-owner "$lexical_source"
if ! rg -Fq 'call i32 @operatorplus' "$lexical_output" ||
   ! rg -Fq 'const i64 4' "$lexical_output" ||
   ! rg -Fq 'cmp eq i32 7, 7' "$lexical_output" ||
   ! rg -q 'cmp eq i32 %t[0-9]+, 5' "$lexical_output"; then
  echo "multi-friend operator did not retain lexical First lookup and Second access" >&2
  exit 1
fi

public_source=$build_dir/public-base-operator.cpp
printf '%s\n' \
  'struct Base { int value; };' \
  'struct Derived : public Base {};' \
  'int operator+(const Base &, const Base &) { return 7; }' \
  'int main() { Derived value; return value + value == 7 ? 0 : 1; }' \
  >"$public_source"
expect_success public-base-operator "$public_source"

private_source=$build_dir/private-base-operator.cpp
printf '%s\n' \
  'struct Base { int value; };' \
  'struct Derived : private Base {};' \
  'int operator+(const Base &, const Base &) { return 7; }' \
  'int main() { Derived value; return value + value == 7 ? 0 : 1; }' \
  >"$private_source"
expect_failure private-base-operator "$private_source"

protected_source=$build_dir/protected-base-operator.cpp
printf '%s\n' \
  'struct Base { int value; };' \
  'struct Derived : protected Base {};' \
  'int operator+(const Base &, const Base &) { return 7; }' \
  'int main() { Derived value; return value + value == 7 ? 0 : 1; }' \
  >"$protected_source"
expect_failure protected-base-operator "$protected_source"

member_source=$build_dir/member-base-operator.cpp
printf '%s\n' \
  'struct Base { int value; };' \
  'int operator+(const Base &, const Base &) { return 7; }' \
  'struct PrivateDerived : private Base {' \
  '  int member() { return *this + *this == 7 ? 0 : 1; }' \
  '};' \
  'struct ProtectedDerived : protected Base {' \
  '  int member() { return *this + *this == 7 ? 0 : 1; }' \
  '};' \
  'int main() {' \
  '  PrivateDerived private_value;' \
  '  ProtectedDerived protected_value;' \
  '  return private_value.member() + protected_value.member();' \
  '}' >"$member_source"
expect_success member-base-operator "$member_source"

protected_further_source=$build_dir/protected-further-base-operator.cpp
printf '%s\n' \
  'struct Base { int value; };' \
  'int operator+(const Base &, const Base &) { return 7; }' \
  'struct ProtectedMid : protected Base {};' \
  'struct ProtectedFurther : ProtectedMid {' \
  '  int member() { return *this + *this == 7 ? 0 : 1; }' \
  '};' \
  'int main() { ProtectedFurther value; return value.member(); }' \
  >"$protected_further_source"
expect_success protected-further-member "$protected_further_source"

protected_external_source=$build_dir/protected-external-base-operator.cpp
printf '%s\n' \
  'struct Base { int value; };' \
  'int operator+(const Base &, const Base &) { return 7; }' \
  'struct ProtectedMid : protected Base {};' \
  'struct ProtectedFurther : ProtectedMid {};' \
  'int external(ProtectedFurther & value) { return value + value; }' \
  'int main() { ProtectedFurther value; return external(value); }' \
  >"$protected_external_source"
expect_failure protected-external "$protected_external_source"

private_further_source=$build_dir/private-further-base-operator.cpp
printf '%s\n' \
  'struct Base { int value; };' \
  'int operator+(const Base &, const Base &) { return 7; }' \
  'struct PrivateMid : private Base {};' \
  'struct PrivateFurther : PrivateMid {' \
  '  int member() { return *this + *this == 7 ? 0 : 1; }' \
  '};' \
  'int main() { PrivateFurther value; return value.member(); }' \
  >"$private_further_source"
expect_failure private-further-member "$private_further_source"

friend_source=$build_dir/friend-base-operator.cpp
printf '%s\n' \
  'struct Base { int value; };' \
  'int operator+(const Base &, const Base &) { return 7; }' \
  'struct FriendDerived : private Base {' \
  '  friend int friend_case(const FriendDerived &);' \
  '};' \
  'int friend_case(const FriendDerived & value) {' \
  '  return value + value == 7 ? 0 : 1;' \
  '}' \
  'int main() { FriendDerived value; return friend_case(value); }' \
  >"$friend_source"
expect_success friend-base-operator "$friend_source"

echo "411 typed operator lexical/base-access regression: PASS"
