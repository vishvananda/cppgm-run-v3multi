#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$test_dir/../../.." && pwd)
app=${CPPGM_PA15_APP:-$repo_root/dev/cppgm++}
lowir2cy86=${CPPGM_PA15_LOWIR2CY86:-$repo_root/dev/lowir2cy86}
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM

if [ ! -x "$app" ]; then
  echo "PA15 driver is not executable: $app" >&2
  exit 1
fi
if [ ! -x "$lowir2cy86" ]; then
  echo "LowIR validator is not executable: $lowir2cy86" >&2
  exit 1
fi

output=$build_dir/typed-discarded-return.lowir
"$app" --emit-lowir -O0 -o "$output" \
  "$test_dir/405-typed-discarded-return-regression.source"

require_line()
{
  if ! rg -Fq -- "$1" "$output"; then
    echo "missing typed LowIR line: $1" >&2
    exit 1
  fi
}

require_line 'function @empty_bool() -> u8'
require_line 'function @empty_integral() -> i64'
require_line 'function @empty_float() -> f32'
require_line 'function @empty_double() -> f64'
require_line 'function @empty_long_double() -> f80'
require_line 'function @empty_pointer() -> ptr'
require_line 'return u8 0'
require_line 'return i64 0'
require_line 'return f32 0.0F'
require_line 'return f64 0.0'
require_line 'return f80 0.0L'
require_line 'return ptr 0'
require_line 'function @void_to_void(%value : ptr [pass=reference]) -> void'
require_line 'declare function @runtime_gate() -> u8'
require_line 'declare function @kept_address() -> u8'
require_line 'call u8 @runtime_gate()'
require_line 'addr @kept_address'

if rg -q '^declare function @unavailable[(]' "$output"; then
  echo 'unreachable literal logical call demanded a declaration' >&2
  exit 1
fi
if rg -q '^declare function @omitted_address[(]' "$output"; then
  echo 'unreachable literal logical address demanded a declaration' >&2
  exit 1
fi

if [ "$(awk '
  /^function @for_init_discards_lvalue[(]/ { in_function = 1; next }
  in_function && /^  block [\^]for_cond/ { exit }
  in_function && /load ptr/ { found += 1 }
  END { print found + 0 }
' "$output")" -ne 1 ]; then
  echo 'discarded for-init lvalue performed unexpected storage loads' >&2
  exit 1
fi

if [ "$(awk '
  /^function @for_init_discards_lvalue[(]/ { in_function = 1; next }
  in_function && /^  block [\^]for_cond/ { exit }
  in_function && /load i32/ { found = 1 }
  END { print found + 0 }
' "$output")" -ne 0 ]; then
  echo 'nonvolatile reference discard materialized an unused value' >&2
  exit 1
fi

if [ "$(awk '
  /^function @void_to_void[(]/ { in_function = 1; next }
  in_function && /^}/ { exit }
  in_function && /load i32/ { found = 1 }
  END { print found + 0 }
' "$output")" -ne 0 ]; then
  echo 'void-to-void return loaded a discarded scalar value' >&2
  exit 1
fi

if [ "$(awk '
  /^function @volatile_object_discard[(]/ { in_function = 1; next }
  in_function && /^}/ { exit }
  in_function && /load i32/ { found += 1 }
  END { print found + 0 }
' "$output")" -ne 1 ]; then
  echo 'volatile scalar discard did not perform exactly one typed load' >&2
  exit 1
fi

if [ "$(awk '
  /^function @volatile_reference_discard[(]/ { in_function = 1; next }
  in_function && /^  block \^for_cond/ { exit }
  in_function && /load i32/ { found += 1 }
  END { print found + 0 }
' "$output")" -ne 2 ]; then
  echo 'volatile reference discard did not load direct and comma RHS values' >&2
  exit 1
fi

"$lowir2cy86" -o "$build_dir/typed-discarded-return.cy86" "$output"
