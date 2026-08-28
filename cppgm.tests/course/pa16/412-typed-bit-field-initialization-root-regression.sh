#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$test_dir/../../.." && pwd)
app=${CPPGM_PA16_APP:-$repo_root/dev/cppgm++}
lowir2cy86=${CPPGM_PA16_LOWIR2CY86:-$repo_root/dev/lowir2cy86}
cy86=${CPPGM_PA16_CY86:-$repo_root/dev/cy86}
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM

if [ ! -x "$app" ]; then
  echo "PA16 driver is not executable: $app" >&2
  exit 1
fi
if [ ! -x "$lowir2cy86" ]; then
  echo "PA13 LowIR runner is not executable: $lowir2cy86" >&2
  exit 1
fi
if [ ! -x "$cy86" ]; then
  echo "PA9 CY86 runner is not executable: $cy86" >&2
  exit 1
fi

run_valid() {
  name=$1
  source=$2
  lowir=$build_dir/$name.lowir
  cy86_source=$build_dir/$name.cy86
  program=$build_dir/$name.program
  "$app" --emit-lowir -O0 -o "$lowir" "$source"
  "$lowir2cy86" -o "$cy86_source" "$lowir"
  "$cy86" -o "$program" "$cy86_source"
  "$program"
}

expect_rejected() {
  name=$1
  source=$2
  if "$app" --emit-lowir -O0 -o "$build_dir/$name.lowir" "$source" \
      >"$build_dir/$name.stdout" 2>"$build_dir/$name.stderr"; then
    echo "expected PA16 rejection: $name" >&2
    return 1
  fi
}

source=$build_dir/bit-field-roots.cpp
printf '%s\n' \
  'struct Bits {' \
  '  signed int first : 4;' \
  '  signed int second : 4;' \
  '};' \
  'struct BoolBits {' \
  '  bool value : 1;' \
  '};' \
  'struct Holder {' \
  '  Bits inner;' \
  '  Bits array[2];' \
  '};' \
  'int main() {' \
  '  Bits first = {1, 2};' \
  '  Bits second = {3, 4};' \
  '  BoolBits boolean = {1};' \
  '  Holder outer = {{5, 6}, {{7, 1}, {2, 3}}};' \
  '  Holder another = {{4, 5}, {{6, 7}, {1, 2}}};' \
  '  if (first.first != 1) return 1;' \
  '  if (first.second != 2) return 2;' \
  '  if (second.first != 3) return 3;' \
  '  if (second.second != 4) return 4;' \
  '  if (boolean.value != 1) return 5;' \
  '  if (outer.inner.first != 5) return 17;' \
  '  if (outer.inner.second != 6) return 6;' \
  '  if (outer.array[0].first != 7) return 7;' \
  '  if (outer.array[0].second != 1) return 8;' \
  '  if (outer.array[1].first != 2) return 9;' \
  '  if (outer.array[1].second != 3) return 10;' \
  '  if (another.inner.first != 4) return 11;' \
  '  if (another.inner.second != 5) return 12;' \
  '  if (another.array[0].first != 6) return 13;' \
  '  if (another.array[0].second != 7) return 14;' \
  '  if (another.array[1].first != 1) return 15;' \
  '  if (another.array[1].second != 2) return 16;' \
  '  return 0;' \
  '}' \
  >"$source"

run_valid bit-field-roots "$source"

controls=$build_dir/bit-field-controls.cpp
printf '%s\n' \
  'enum UnsignedE : unsigned int { EU = 1 };' \
  'struct LongBits { long value : 1; };' \
  'struct LongLongBits { long long value : 1; };' \
  'struct IntBits { unsigned int narrow : 1; unsigned int full : 32; };' \
  'struct EnumBits { UnsignedE value : 1; };' \
  'struct WideBits { unsigned int wide : 65; unsigned int after : 1; };' \
  'struct WideChar { unsigned char wide : 9; unsigned int after : 1; };' \
  'struct WideSeparated { unsigned int : 65; unsigned int after : 1; };' \
  'union WideUnion { unsigned int value : 65; unsigned char byte; };' \
  'struct MixedBits { unsigned char ordinary; unsigned int first : 1; int ordinary2; unsigned int next : 1; };' \
  'int choose(int) { return 1; }' \
  'int choose(unsigned int) { return 2; }' \
  'int choose(long) { return 3; }' \
  'int choose(long long) { return 4; }' \
  'int choose(UnsignedE) { return 5; }' \
  'int main() {' \
  '  LongBits l = {1};' \
  '  LongLongBits ll = {1};' \
  '  IntBits i = {1, 3};' \
  '  EnumBits e = {EU};' \
  '  WideBits w = {1, 1};' \
  '  WideChar c = {1, 1};' \
  '  WideSeparated s = {1};' \
  '  MixedBits mixed;' \
  '  mixed.ordinary = 7;' \
  '  mixed.first = 1;' \
  '  mixed.ordinary2 = 9;' \
  '  mixed.next = 1;' \
  '  WideUnion u;' \
  '  u.value = 3;' \
  '  if (choose(l.value) != 3) return 1;' \
  '  if (choose(ll.value) != 4) return 2;' \
  '  if (choose(i.narrow) != 2) return 3;' \
  '  if (choose(i.full) != 2) return 4;' \
  '  if (choose(e.value) != 5) return 5;' \
  '  if (w.wide != 1 || w.after != 1) return 6;' \
  '  if (c.wide != 1 || c.after != 1) return 7;' \
  '  if (s.after != 1) return 8;' \
  '  if (mixed.ordinary != 7 || mixed.first != 1 || mixed.ordinary2 != 9 || mixed.next != 1) return 9;' \
  '  if (u.value != 3 || sizeof(WideUnion) != 12) return 10;' \
  '  return 0;' \
  '}' >"$controls"
run_valid bit-field-controls "$controls"

references=$build_dir/bit-field-references.cpp
printf '%s\n' \
  'int read_ref(const unsigned int &value) { return value ? 0 : 1; }' \
  'struct RefBits {' \
  '  unsigned int value : 1;' \
  '  int implicit() { const unsigned int &ref = value; return read_ref(ref); }' \
  '};' \
  'int main() {' \
  '  RefBits bits = {1};' \
  '  const unsigned int &direct = bits.value;' \
  '  if (read_ref(direct) != 0) return 1;' \
  '  const unsigned int &casted = static_cast<const unsigned int &>(bits.value);' \
  '  if (read_ref(casted) != 0) return 2;' \
  '  return bits.implicit();' \
  '}' >"$references"
run_valid bit-field-references "$references"

address_overload=$build_dir/bit-field-address-overload.cpp
printf '%s\n' \
  'enum E { EOne = 1 };' \
  'E operator&(E value) { return value; }' \
  'struct AddressBits { E value : 2; };' \
  'int main() {' \
  '  AddressBits bits = {EOne};' \
  '  return (&bits.value) == EOne ? 0 : 1;' \
  '}' >"$address_overload"
run_valid bit-field-address-overload "$address_overload"

invalid_ref_direct=$build_dir/bit-field-invalid-ref-direct.cpp
printf '%s\n' \
  'struct Bits { unsigned int value : 1; };' \
  'int main() { Bits bits = {1}; unsigned int &bad = bits.value; return bad; }' \
  >"$invalid_ref_direct"
expect_rejected bit-field-invalid-ref-direct "$invalid_ref_direct"

invalid_ref_this=$build_dir/bit-field-invalid-ref-this.cpp
printf '%s\n' \
  'struct Bits { unsigned int value : 1; void run() { unsigned int &bad = value; (void)bad; } };' \
  'int main() { Bits bits = {1}; bits.run(); return 0; }' \
  >"$invalid_ref_this"
expect_rejected bit-field-invalid-ref-this "$invalid_ref_this"

invalid_bool_direct=$build_dir/bit-field-invalid-bool-direct.cpp
printf '%s\n' \
  'struct Bits { bool value : 1; };' \
  'int main() { Bits bits = {1}; --bits.value; return 0; }' \
  >"$invalid_bool_direct"
expect_rejected bit-field-invalid-bool-direct "$invalid_bool_direct"

invalid_bool_this=$build_dir/bit-field-invalid-bool-this.cpp
printf '%s\n' \
  'struct Bits { bool value : 1; void run() { --value; } };' \
  'int main() { Bits bits = {1}; bits.run(); return 0; }' \
  >"$invalid_bool_this"
expect_rejected bit-field-invalid-bool-this "$invalid_bool_this"

invalid_sizeof_direct=$build_dir/bit-field-invalid-sizeof-direct.cpp
printf '%s\n' \
  'struct Bits { unsigned int value : 1; };' \
  'int main() { Bits bits = {1}; return sizeof(bits.value); }' \
  >"$invalid_sizeof_direct"
expect_rejected bit-field-invalid-sizeof-direct "$invalid_sizeof_direct"

invalid_sizeof_paren=$build_dir/bit-field-invalid-sizeof-paren.cpp
printf '%s\n' \
  'struct Bits { unsigned int value : 1; };' \
  'int main() { Bits bits = {1}; return sizeof((bits.value)); }' \
  >"$invalid_sizeof_paren"
expect_rejected bit-field-invalid-sizeof-paren "$invalid_sizeof_paren"

invalid_sizeof_this=$build_dir/bit-field-invalid-sizeof-this.cpp
printf '%s\n' \
  'struct Bits { unsigned int value : 1; int run() { return sizeof(value); } };' \
  'int main() { Bits bits = {1}; return bits.run(); }' \
  >"$invalid_sizeof_this"
expect_rejected bit-field-invalid-sizeof-this "$invalid_sizeof_this"

invalid_declarator_pointer=$build_dir/bit-field-invalid-declarator-pointer.cpp
printf '%s\n' \
  'struct Bad { unsigned int *value : 1; };' \
  'int main() { return 0; }' \
  >"$invalid_declarator_pointer"
expect_rejected bit-field-invalid-declarator-pointer "$invalid_declarator_pointer"

invalid_declarator_array=$build_dir/bit-field-invalid-declarator-array.cpp
printf '%s\n' \
  'struct Bad { unsigned int value[1] : 1; };' \
  'int main() { return 0; }' \
  >"$invalid_declarator_array"
expect_rejected bit-field-invalid-declarator-array "$invalid_declarator_array"

invalid_declarator_paren=$build_dir/bit-field-invalid-declarator-paren.cpp
printf '%s\n' \
  'struct Bad { unsigned int (value) : 1; };' \
  'int main() { return 0; }' \
  >"$invalid_declarator_paren"
expect_rejected bit-field-invalid-declarator-paren "$invalid_declarator_paren"

invalid_overflow=$build_dir/bit-field-invalid-overflow.cpp
printf '%s\n' \
  'struct Overflow {' \
  '  unsigned char value : 18446744073709551615ULL;' \
  '  char tail[18446744073709551615ULL];' \
  '};' \
  'int main() { return sizeof(Overflow); }' \
  >"$invalid_overflow"
expect_rejected bit-field-invalid-overflow "$invalid_overflow"
