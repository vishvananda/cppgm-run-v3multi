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

expect_ambiguous_failure()
{
  source=$1
  output=$build_dir/$(basename "$source").lowir
  if "$app" --emit-lowir -O0 -o "$output" "$source" \
      >"$output.stdout" 2>"$output.stderr"; then
    status=0
  else
    status=$?
  fi
  if [ "$status" -ne 1 ] ||
     ! rg -Fq 'PA12 ambiguous member call' "$output.stderr"; then
    echo "$(basename "$source") was not rejected as an ambiguous member call" >&2
    exit 1
  fi
}

qualified_source=$build_dir/qualified-parenthesized-static.cpp
printf '%s\n' \
  'struct Qualified {' \
  '  static int f(int value) { return value + 1; }' \
  '  static int call(int value) { return (f)(value); }' \
  '};' \
  'int main() { return (Qualified::f)(1) == 2 && Qualified::call(2) == 3 ? 0 : 1; }' \
  >"$qualified_source"
qualified_output=$build_dir/qualified-parenthesized-static.lowir
"$app" --emit-lowir -O0 -o "$qualified_output" "$qualified_source"
if ! rg -Fq 'function @Qualified__f(%value : i32) -> i32' "$qualified_output" ||
   rg -Fq 'function @Qualified__f(%this' "$qualified_output" ||
   ! rg -q -e '^    %t[0-9]+ = call i32 @Qualified__f\(1\)$' \
     "$qualified_output" ||
   ! rg -Fq 'function @Qualified__call(%value : i32) -> i32' "$qualified_output" ||
   ! rg -q -e 'call i32 @Qualified__f\([^)]*\)' \
     "$qualified_output"; then
  echo "parenthesized qualified static call lost its raw typed boundary" >&2
  exit 1
fi

nonstatic_source=$build_dir/qualified-nonstatic-rejected.cpp
printf '%s\n' \
  'int f(int value) { return value + 10; }' \
  'struct Qualified {' \
  '  int f(int value) { return value; }' \
  '};' \
  'int main() { return Qualified::f(1); }' \
  >"$nonstatic_source"
expect_failure "$nonstatic_source"

unrelated_source=$build_dir/qualified-unrelated-nonstatic-rejected.cpp
printf '%s\n' \
  'struct Other {' \
  '  int f(int value) { return value; }' \
  '};' \
  'struct Caller {' \
  '  int call() { return Other::f(1); }' \
  '};' \
  'int main() { Caller value; return value.call(); }' \
  >"$unrelated_source"
expect_failure "$unrelated_source"

mixed_source=$build_dir/mixed-static-nonstatic-overloads.cpp
printf '%s\n' \
  'struct Mix {' \
  '  int f(long value) { return value + 10; }' \
  '  static int f(int value) { return value + 20; }' \
  '  int qualified_nonstatic() { return Mix::f(1L); }' \
  '  int qualified_static() { return Mix::f(1); }' \
  '  int unqualified_nonstatic() { return f(1L); }' \
  '  int unqualified_static() { return f(1); }' \
  '};' \
  'int main() {' \
  '  Mix value;' \
  '  return value.qualified_nonstatic() == 11 &&' \
  '    value.qualified_static() == 21 &&' \
  '    value.unqualified_nonstatic() == 11 &&' \
  '    value.unqualified_static() == 21 ? 0 : 1;' \
  '}' \
  >"$mixed_source"
mixed_output=$build_dir/mixed-static-nonstatic-overloads.lowir
"$app" --emit-lowir -O0 -o "$mixed_output" "$mixed_source"
mixed_nonstatic_symbol=$(sed -n \
  's/^function @\([^ (]*\).*object=_ZN3Mix1fEl.*/\1/p' \
  "$mixed_output")
mixed_static_symbol=$(sed -n \
  's/^function @\([^ (]*\).*object=_ZN3Mix1fEi.*/\1/p' \
  "$mixed_output")
mixed_qualified_nonstatic=$build_dir/mixed-qualified-nonstatic.body
mixed_qualified_static=$build_dir/mixed-qualified-static.body
mixed_unqualified_nonstatic=$build_dir/mixed-unqualified-nonstatic.body
mixed_unqualified_static=$build_dir/mixed-unqualified-static.body
sed -n '/^function @Mix__qualified_nonstatic/,/^}/p' "$mixed_output" \
  >"$mixed_qualified_nonstatic"
sed -n '/^function @Mix__qualified_static/,/^}/p' "$mixed_output" \
  >"$mixed_qualified_static"
sed -n '/^function @Mix__unqualified_nonstatic/,/^}/p' "$mixed_output" \
  >"$mixed_unqualified_nonstatic"
sed -n '/^function @Mix__unqualified_static/,/^}/p' "$mixed_output" \
  >"$mixed_unqualified_static"
if [ -z "$mixed_nonstatic_symbol" ] || [ -z "$mixed_static_symbol" ] ||
   ! rg -Fq "function @${mixed_nonstatic_symbol}(%this : ptr, %value : i64) -> i32" \
     "$mixed_output" ||
   ! rg -Fq "function @${mixed_static_symbol}(%value : i32) -> i32" \
     "$mixed_output" ||
   ! rg -Fq "call i32 @${mixed_nonstatic_symbol}(" "$mixed_qualified_nonstatic" ||
   rg -Fq "call i32 @${mixed_static_symbol}(" "$mixed_qualified_nonstatic" ||
   ! rg -Fq "call i32 @${mixed_static_symbol}(" "$mixed_qualified_static" ||
   rg -Fq "call i32 @${mixed_nonstatic_symbol}(" "$mixed_qualified_static" ||
   ! rg -Fq "call i32 @${mixed_nonstatic_symbol}(" "$mixed_unqualified_nonstatic" ||
   rg -Fq "call i32 @${mixed_static_symbol}(" "$mixed_unqualified_nonstatic" ||
   ! rg -Fq "call i32 @${mixed_static_symbol}(" "$mixed_unqualified_static" ||
   rg -Fq "call i32 @${mixed_nonstatic_symbol}(" "$mixed_unqualified_static"; then
  echo "mixed static/non-static overload ownership was not ranked as one set" >&2
  exit 1
fi

ambiguous_qualified_source=$build_dir/mixed-static-nonstatic-ambiguous-qualified.cpp
printf '%s\n' \
  'struct AmbiguousQualified {' \
  '  static int f(long value) { return value + 10; }' \
  '  int f(unsigned long value) const { return value + 20; }' \
  '  int call() { return AmbiguousQualified::f(1); }' \
  '};' \
  'int main() { AmbiguousQualified value; return value.call(); }' \
  >"$ambiguous_qualified_source"
expect_ambiguous_failure "$ambiguous_qualified_source"

ambiguous_unqualified_source=$build_dir/mixed-static-nonstatic-ambiguous-unqualified.cpp
printf '%s\n' \
  'struct AmbiguousUnqualified {' \
  '  static int f(long value) { return value + 10; }' \
  '  int f(unsigned long value) const { return value + 20; }' \
  '  int call() { return f(1); }' \
  '};' \
  'int main() { AmbiguousUnqualified value; return value.call(); }' \
  >"$ambiguous_unqualified_source"
expect_ambiguous_failure "$ambiguous_unqualified_source"

inherited_source=$build_dir/inherited-static-body.cpp
printf '%s\n' \
  'int inherited(int value) { return value + 99; }' \
  'class Base {' \
  'protected:' \
  '  static int inherited(int value) { return value + 1; }' \
  '};' \
  'class Derived : public Base {' \
  'public:' \
  '  static int call(int value) { return inherited(value); }' \
  '};' \
  'int main() { return Derived::call(1) == 2 ? 0 : 1; }' \
  >"$inherited_source"
inherited_output=$build_dir/inherited-static-body.lowir
"$app" --emit-lowir -O0 -o "$inherited_output" "$inherited_source"
inherited_body=$build_dir/inherited-static-body.body
sed -n '/^function @Derived__call/,/^}/p' "$inherited_output" >"$inherited_body"
if ! rg -Fq 'function @Derived__call(%value : i32)' "$inherited_output" ||
   rg -Fq 'function @Derived__call(%this' "$inherited_output" ||
   ! rg -Fq 'function @Base__inherited(%value : i32)' "$inherited_output" ||
   ! rg -Fq 'call i32 @Base__inherited' "$inherited_body" ||
   rg -q 'projection=' "$inherited_body"; then
  echo "static-body inherited lookup did not retain the base owner/raw ABI" >&2
  exit 1
fi

private_source=$build_dir/inherited-private-static-body.cpp
printf '%s\n' \
  'class Base {' \
  'private:' \
  '  static int hidden(int value) { return value; }' \
  '};' \
  'class Derived : public Base {' \
  'public:' \
  '  static int call(int value) { return hidden(value); }' \
  '};' \
  'int main() { return Derived::call(1); }' \
  >"$private_source"
expect_failure "$private_source"

declaration_source=$build_dir/inherited-static-declaration-default.cpp
printf '%s\n' \
  'struct ExternalBase {' \
  '  static int called(int value);' \
  '};' \
  'struct ExternalDerived : ExternalBase {}; ' \
  'int main() { return ExternalDerived::called(7) == 7 ? 0 : 1; }' \
  >"$declaration_source"
declaration_output=$build_dir/inherited-static-declaration-default.lowir
if ! "$app" --emit-lowir -O0 -o "$declaration_output" "$declaration_source" \
    >"$declaration_output.stdout" 2>"$declaration_output.stderr"; then
  cat "$declaration_output.stderr" >&2
  exit 1
fi
if ! rg -Fq \
    'declare function @ExternalBase__called(%arg0 : i32) -> i32' \
    "$declaration_output" ||
   rg -q '^function @ExternalBase__called' "$declaration_output" ||
   ! rg -q -e '^    %t[0-9]+ = call i32 @ExternalBase__called\(7\)$' \
    "$declaration_output"; then
  echo "inherited declaration-only static call lost its declaration boundary" >&2
  exit 1
fi

redeclaration_source=$build_dir/static-redeclaration-identity.cpp
printf '%s\n' \
  'struct Declared {' \
  '  static int f(int value);' \
  '  static int f(int value) { return value + 1; }' \
  '};' \
  'int main() { return Declared::f(1) == 2 ? 0 : 1; }' \
  >"$redeclaration_source"
redeclaration_output=$build_dir/static-redeclaration-identity.lowir
"$app" --emit-lowir -O0 -o "$redeclaration_output" "$redeclaration_source"
if [ "$(rg -c '^function @Declared__f' "$redeclaration_output" || true)" -ne 1 ] ||
   ! rg -q -e '^    %t[0-9]+ = call i32 @Declared__f\(1\)$' \
    "$redeclaration_output"; then
  echo "static redeclaration did not retain one canonical emitted function" >&2
  exit 1
fi

recursive_source=$build_dir/static-recursive-demand.cpp
printf '%s\n' \
  'struct Recursive {' \
  '  static int loop(int value) {' \
  '    return value == 0 ? 0 : loop(value - 1);' \
  '  }' \
  '};' \
  'int main() { return Recursive::loop(2); }' \
  >"$recursive_source"
recursive_output=$build_dir/static-recursive-demand.lowir
"$app" --emit-lowir -O0 -o "$recursive_output" "$recursive_source"
recursive_body=$build_dir/static-recursive-demand.body
sed -n '/^function @Recursive__loop/,/^}/p' "$recursive_output" >"$recursive_body"
if [ "$(rg -c '^function @Recursive__loop' "$recursive_output" || true)" -ne 1 ] ||
   ! rg -Fq 'call i32 @Recursive__loop' "$recursive_body" ||
   rg -Fq 'function @Recursive__loop(%this' "$recursive_output"; then
  echo "recursive static demand was not deduplicated at the raw ABI" >&2
  exit 1
fi
