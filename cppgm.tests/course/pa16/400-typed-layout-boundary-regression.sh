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
  output=$1
  source=$2
  label=$3
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

assert_no_fake_zero_lifetime()
{
  output=$1
  label=$2
  if [ -s "$output" ] && rg -q -e '^  zero [0-9]+' \
      -e '^function @__cppgm_init' "$output"; then
    echo "$label emitted fake zero/no-op lifetime LowIR" >&2
    exit 1
  fi
}

outer_source=$build_dir/nested-outer.cpp
printf '%s\n' \
  'struct Outer {' \
  '  struct Inner { virtual void f(); };' \
  '  Inner *inner;' \
  '  int value;' \
  '};' \
  'int main() { Outer value; return sizeof(value); }' >"$outer_source"
outer_output=$build_dir/nested-outer.lowir
"$app" --emit-lowir -O0 -o "$outer_output" "$outer_source"
if ! rg -Fq -- 'slot $value : obj<16x8>' "$outer_output" ||
   ! rg -Fq -- 'const i64 16' "$outer_output"; then
  echo "outer layout was not emitted as its ordinary pointer/int layout" >&2
  exit 1
fi

inner_source=$build_dir/nested-inner.cpp
printf '%s\n' \
  'struct Outer {' \
  '  struct Inner { virtual void f(); };' \
  '  Inner *inner;' \
  '  int value;' \
  '};' \
  'int main() { return sizeof(Outer::Inner); }' >"$inner_source"
inner_output=$build_dir/nested-inner.lowir
expect_failure "$inner_output" "$inner_source" \
  "nested polymorphic record was flattened"
assert_no_fake_zero_lifetime "$inner_output" \
  "nested polymorphic record"

base_source=$build_dir/derived.cpp
printf '%s\n' \
  'struct Base { int base; };' \
  'struct Derived : Base { int value; };' \
  'int main() { return sizeof(Derived); }' >"$base_source"
base_output=$build_dir/derived.lowir
"$app" --emit-lowir -O0 -o "$base_output" "$base_source"
if ! rg -Fq -- 'const i64 8' "$base_output"; then
  echo "complete direct-base layout was not consumed by sizeof" >&2
  exit 1
fi

aligned_forward_source=$build_dir/aligned-forward.cpp
printf '%s\n' \
  'struct alignas(16) AlignedForward;' \
  'struct AlignedForward { char value; };' \
  'int main() { return 0; }' >"$aligned_forward_source"
expect_failure "$build_dir/aligned-forward.lowir" "$aligned_forward_source" \
  "unaligned definition followed an aligned forward declaration"

mismatched_source=$build_dir/mismatched-alignment.cpp
printf '%s\n' \
  'struct alignas(16) Mismatched;' \
  'struct alignas(32) Mismatched { char value; };' \
  'int main() { return 0; }' >"$mismatched_source"
expect_failure "$build_dir/mismatched-alignment.lowir" "$mismatched_source" \
  "mismatched aligned declarations were accepted"

later_conflict_source=$build_dir/later-alignment-conflict.cpp
printf '%s\n' \
  'struct Defined { char value; };' \
  'struct alignas(16) Defined;' \
  'int main() { return 0; }' >"$later_conflict_source"
expect_failure "$build_dir/later-alignment-conflict.lowir" "$later_conflict_source" \
  "a later aligned declaration conflicted with an existing definition"

alignment_combination_source=$build_dir/alignment-combination.cpp
printf '%s\n' \
  'struct alignas(0) ZeroAlignment { char value; };' \
  'struct alignas(0) alignas(16) CombinedAlignment { char value; };' \
  'int main() { return sizeof(ZeroAlignment) + sizeof(CombinedAlignment); }' \
  >"$alignment_combination_source"
alignment_combination_output=$build_dir/alignment-combination.lowir
"$app" --emit-lowir -O0 -o "$alignment_combination_output" \
  "$alignment_combination_source"
if ! rg -Fq -- 'const i64 1' "$alignment_combination_output" ||
   ! rg -Fq -- 'const i64 16' "$alignment_combination_output"; then
  echo "alignas(0) or same-declaration alignment combination changed layout" >&2
  exit 1
fi

union_source=$build_dir/union-derived.cpp
printf '%s\n' \
  'struct Base { int base; };' \
  'union Invalid : Base { int value; };' \
  'int main() { return sizeof(Invalid); }' >"$union_source"
expect_failure "$build_dir/union-derived.lowir" "$union_source" \
  "union derived record was accepted"

ordinary_source=$build_dir/ordinary-global.cpp
printf '%s\n' \
  'struct Pair {' \
  '  int x;' \
  '  int y;' \
  '  int method() const;' \
  '  static int static_method();' \
  '};' \
  'Pair global_pair;' \
  'int main() { return sizeof(global_pair); }' >"$ordinary_source"
ordinary_output=$build_dir/ordinary-global.lowir
"$app" --emit-lowir -O0 -o "$ordinary_output" "$ordinary_source"
if ! rg -Fq -- 'zero 8' "$ordinary_output" ||
   ! rg -Fq -- 'function @__cppgm_init()' "$ordinary_output"; then
  echo "ordinary two-int namespace object changed" >&2
  exit 1
fi

completed_member_source=$build_dir/completed-member.cpp
printf '%s\n' \
  'struct Child { int x; };' \
  'struct Parent { Child child; int value; };' \
  'Parent global_parent;' \
  'int main() { return sizeof(global_parent); }' >"$completed_member_source"
completed_member_output=$build_dir/completed-member.lowir
"$app" --emit-lowir -O0 -o "$completed_member_output" \
  "$completed_member_source"
if ! rg -Fq -- 'zero 8' "$completed_member_output" ||
   ! rg -Fq -- 'function @__cppgm_init()' "$completed_member_output"; then
  echo "completed member summary was not reused for namespace storage" >&2
  exit 1
fi

dmi_source=$build_dir/default-member-initializer.cpp
printf '%s\n' \
  'struct Dmi { int value = 7; };' \
  'Dmi global_dmi;' \
  'int main() { return 0; }' >"$dmi_source"
dmi_output=$build_dir/default-member-initializer.lowir
expect_failure "$dmi_output" "$dmi_source" \
  "default member initializer was treated as trivial zero storage"
assert_no_fake_zero_lifetime "$dmi_output" "default member initializer"

dtor_source=$build_dir/user-destructor.cpp
printf '%s\n' \
  'struct Dtor { ~Dtor() {} };' \
  'Dtor global_dtor;' \
  'int main() { return 0; }' >"$dtor_source"
dtor_output=$build_dir/user-destructor.lowir
expect_failure "$dtor_output" "$dtor_source" \
  "user-declared destructor was treated as a trivial namespace object"
assert_no_fake_zero_lifetime "$dtor_output" "user-declared destructor"

f80_source=$build_dir/f80-array.cpp
printf '%s\n' \
  'int main() { long double values[2]; return sizeof(values); }' >"$f80_source"
f80_output=$build_dir/f80-array.lowir
"$app" --emit-lowir -O0 -o "$f80_output" "$f80_source"
if ! rg -Fq -- 'obj<32x16>' "$f80_output" ||
   ! rg -Fq -- 'const i64 32' "$f80_output"; then
  echo "long-double array did not retain semantic alignment" >&2
  exit 1
fi

overflow_source=$build_dir/sizeof-overflow.cpp
printf '%s\n' \
  'int main() { return sizeof(char[18446744073709551615ULL]); }' >"$overflow_source"
expect_failure "$build_dir/sizeof-overflow.lowir" "$overflow_source" \
  "out-of-range sizeof reached a signed LowIR operand"
