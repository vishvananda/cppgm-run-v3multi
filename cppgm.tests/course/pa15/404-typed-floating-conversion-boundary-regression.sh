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

output=$build_dir/floating-boundary.lowir
"$app" --emit-lowir -O0 -o "$output" \
  "$test_dir/404-typed-floating-conversion-boundary-regression.source"

require_line()
{
  if ! rg -Fq -- "$1" "$output"; then
    echo "missing typed LowIR line: $1" >&2
    exit 1
  fi
}

require_count_at_least()
{
  pattern=$1
  minimum=$2
  count=$(rg -c -F -- "$pattern" "$output" || true)
  if [ "$count" -lt "$minimum" ]; then
    echo "typed LowIR line occurred $count times, expected at least $minimum: $pattern" >&2
    exit 1
  fi
}

require_line 'convert sitofp f32 i32'
require_line 'convert uitofp f32 u32'
require_line 'convert sitofp f64 i64'
require_line 'convert uitofp f80 i64'
require_line 'convert fptosi i32 f32'
require_line 'convert fptoui u32 f32'
require_line 'convert fptosi i64 f64'
require_line 'convert fptoui i64 f80'
require_line 'convert fpext f64 f32'
require_line 'convert fptrunc f32 f80'
require_line 'convert fpext f80 f64'
require_line 'convert fptrunc f64 f80'
require_count_at_least 'convert fpext f64 f32' 2
require_line 'cmp ne f80'
require_line 'unary neg f80 0.0L'
require_line 'cmp ne f80 %t'
require_line 'declare function @sink(%arg0 : ptr) -> i32 [arity=variadic'
require_line 'call i32 @sink('
require_count_at_least 'convert sext i32 i8' 2
require_count_at_least 'convert zext i32 u8' 2
require_count_at_least 'convert sext i32 i16' 1
require_count_at_least 'convert zext i32 u16' 1
require_line 'convert fpext f64 f32'

# These source identifiers deliberately occupy the first helper spellings.
# The generated conditional and reference temporaries must advance past them.
require_line 'slot $refarg__1 : f64'
require_line 'slot $refarg__2 : f64'
require_line 'slot $refarg__3 : f64'
require_line 'slot $refarg__4 : f64'
require_line 'slot $cond__2 : i32'
require_line 'addr $refarg__4'

"$lowir2cy86" -o "$build_dir/floating-boundary.cy86" "$output"
