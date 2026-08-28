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

self_source=$build_dir/self-pointer.cpp
printf '%s\n' \
  'struct Self {' \
  '  int value;' \
  '  Self(Self *other) : value(other == 0 ? 1 : 2) {}' \
  '};' \
  'int main() {' \
  '  Self *other = 0;' \
  '  Self value(other);' \
  '  return value.value == 1 ? 0 : 1;' \
  '}' >"$self_source"
self_output=$build_dir/self-pointer.lowir
"$app" --emit-lowir -O0 -o "$self_output" "$self_source"
if [ "$(rg -c '^function @Self__Self' "$self_output" || true)" -ne 1 ] ||
   [ "$(rg -c 'call void @Self__Self' "$self_output" || true)" -ne 1 ]; then
  echo "self-pointer constructor did not retain one typed constructor call" >&2
  exit 1
fi

protected_source=$build_dir/protected-base.cpp
printf '%s\n' \
  'class Base {' \
  'protected:' \
  '  int value;' \
  '  Base(int value) : value(value) {}' \
  '};' \
  'class Derived : public Base {' \
  'public:' \
  '  Derived(int value) : Base(value) {}' \
  '  int get() { return value; }' \
  '};' \
  'int main() {' \
  '  Derived value(7);' \
  '  return value.get() == 7 ? 0 : 1;' \
  '}' >"$protected_source"
protected_output=$build_dir/protected-base.lowir
"$app" --emit-lowir -O0 -o "$protected_output" "$protected_source"
if [ "$(rg -c '^function @Derived__Derived' "$protected_output" || true)" -ne 1 ] ||
   [ "$(rg -c 'call void @Base__Base' "$protected_output" || true)" -ne 1 ]; then
  echo "protected base constructor action was not lowered" >&2
  exit 1
fi

private_source=$build_dir/private-base.cpp
printf '%s\n' \
  'class Base {' \
  'private:' \
  '  Base(int) {}' \
  '};' \
  'class Derived : public Base {' \
  'public:' \
  '  Derived(int value) : Base(value) {}' \
  '};' \
  'int main() { Derived value(7); return 0; }' >"$private_source"
private_output=$build_dir/private-base.lowir
if "$app" --emit-lowir -O0 -o "$private_output" "$private_source" \
    >"$private_output.stdout" 2>"$private_output.stderr"; then
  status=0
else
  status=$?
fi
if [ "$status" -ne 1 ]; then
  echo "private base constructor returned status $status, expected 1" >&2
  exit 1
fi

aggregate_source=$build_dir/aggregate-constructors.cpp
printf '%s\n' \
  'struct DefaultedAggregate {' \
  '  DefaultedAggregate() = default;' \
  '  int value;' \
  '};' \
  'struct DeletedAggregate {' \
  '  DeletedAggregate() = delete;' \
  '  int value;' \
  '};' \
  'int main() {' \
  '  DefaultedAggregate defaulted = {41};' \
  '  DeletedAggregate deleted = {43};' \
  '  return defaulted.value == 41 && deleted.value == 43 ? 0 : 1;' \
  '}' >"$aggregate_source"
aggregate_output=$build_dir/aggregate-constructors.lowir
"$app" --emit-lowir -O0 -o "$aggregate_output" "$aggregate_source"
if [ "$(rg -c 'store i32 41,' "$aggregate_output" || true)" -ne 1 ] ||
   [ "$(rg -c 'store i32 43,' "$aggregate_output" || true)" -ne 1 ] ||
   rg -q -e '^function @DefaultedAggregate__DefaultedAggregate' \
      -e 'call void @DefaultedAggregate__DefaultedAggregate' \
      -e '^function @DeletedAggregate__DeletedAggregate' \
      -e 'call void @DeletedAggregate__DeletedAggregate' "$aggregate_output"; then
  echo "defaulted/deleted aggregate initialization lost field stores or emitted a constructor" >&2
  exit 1
fi

aggregate_then_default_source=$build_dir/aggregate-then-default.cpp
printf '%s\n' \
  'struct Pair { int first; int second; };' \
  'int main() {' \
  '  Pair rows[1] = {{1, 2}};' \
  '  Pair value = Pair();' \
  '  return rows[0].first == 1 && rows[0].second == 2 &&' \
  '         value.first == 0 && value.second == 0 ? 0 : 1;' \
  '}' >"$aggregate_then_default_source"
aggregate_then_default_output=$build_dir/aggregate-then-default.lowir
if "$app" --emit-lowir -O0 -o "$aggregate_then_default_output" \
    "$aggregate_then_default_source" \
    >"$aggregate_then_default_output.stdout" \
    2>"$aggregate_then_default_output.stderr"; then
  status=0
else
  status=$?
fi
if [ "$status" -ne 0 ] ||
   [ "$(rg -c 'call void @Pair__Pair' "$aggregate_then_default_output" || true)" -ne 1 ] ||
   ! rg -Fq 'call void @Pair__Pair' "$aggregate_then_default_output"; then
  echo "aggregate forwarding helper leaked into later ordinary value construction" >&2
  exit 1
fi
