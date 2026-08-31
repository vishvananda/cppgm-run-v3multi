#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$test_dir/../../.." && pwd)
app=${CPPGM_PA16_APP:-$repo_root/dev/cppgm++}
lowir2cy86=${CPPGM_PA16_LOWIR2CY86:-$repo_root/dev/lowir2cy86}
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM

if [ ! -x "$app" ] || [ ! -x "$lowir2cy86" ]; then
  echo "PA16 typed equality-carrier tools are not executable" >&2
  exit 1
fi

source=$build_dir/valid.cpp
valid_lowir=$build_dir/valid.lowir
printf '%s\n' \
  'enum UnsignedE : unsigned int { EOne = 1 };' \
  'struct Bits { unsigned int value : 1; };' \
  'struct EnumBits { UnsignedE value : 1; };' \
  'int main() {' \
  '  Bits bits = {1};' \
  '  EnumBits enums = {EOne};' \
  '  return bits.value == 1 && bits.value != 0 && enums.value == EOne ? 0 : 1;' \
  '}' \
  >"$source"

"$app" --emit-lowir -O0 -o "$valid_lowir" "$source"
if ! rg -q 'cmp eq u32 %t[0-9]+, 1' "$valid_lowir" ||
   ! rg -q 'cmp ne u32 %t[0-9]+, 0' "$valid_lowir"; then
  echo "canonical bit-field equality carriers were not emitted" >&2
  exit 1
fi
"$lowir2cy86" -o "$build_dir/valid.cy86" "$valid_lowir"

reject_mismatch() {
  name=$1
  predicate=$2
  type=$3
  value_type=$4
  lowir=$build_dir/$name.lowir
  printf '%s\n' \
    'function @main() -> i32 {' \
    '  block ^entry:' \
    "    %a = const $value_type 1" \
    "    %r = cmp $predicate $type %a, 1" \
    '    return i32 0' \
    '}' \
    >"$lowir"
  if "$lowir2cy86" -o "$build_dir/$name.cy86" "$lowir" \
      >"$build_dir/$name.stdout" 2>"$build_dir/$name.stderr"; then
    echo "malformed equality carrier was accepted: $name" >&2
    exit 1
  fi
}

reject_mismatch narrow-u16 eq u16 i16
reject_mismatch reverse-i32 eq i32 u32
reject_mismatch relational-u32 lt u32 i32

right_lowir=$build_dir/right-mismatch.lowir
printf '%s\n' \
  'function @main() -> i32 {' \
  '  block ^entry:' \
  '    %right = const i16 1' \
  '    %result = cmp eq u32 1, %right' \
  '    return i32 0' \
  '}' \
  >"$right_lowir"
if "$lowir2cy86" -o "$build_dir/right-mismatch.cy86" "$right_lowir" \
    >"$build_dir/right-mismatch.stdout" 2>"$build_dir/right-mismatch.stderr"; then
  echo "right comparison operand mismatch was accepted" >&2
  exit 1
fi

echo "433 typed equality-carrier validation regression: PASS"
