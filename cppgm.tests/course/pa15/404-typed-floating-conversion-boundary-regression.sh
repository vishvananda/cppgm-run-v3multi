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

require_function_line()
{
  function_name=$1
  pattern=$2
  if ! awk -v function_name="$function_name" -v pattern="$pattern" '
    $0 ~ ("^function @" function_name "\\(") { in_function = 1 }
    in_function && index($0, pattern) { found = 1 }
    in_function && /^}/ { exit found ? 0 : 1 }
    END { exit found ? 0 : 1 }
  ' "$output"; then
    echo "missing scoped LowIR line in $function_name: $pattern" >&2
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
require_line 'cmp ne i32 %t'
require_line 'convert trunc u8 i64'
if ! awk '
  /^function @logical_integral_result\(/ { in_function = 1 }
  in_function && /cmp ne i32/ { found = 1 }
  END { exit (in_function && found) ? 0 : 1 }
' "$output"; then
  echo 'integral logical RHS was not compared in its physical i32 type' >&2
  exit 1
fi
require_function_line logical_bool_result 'convert trunc u8 i64'
require_function_line logical_bool_result 'return u8'
require_function_line logical_bool_local_init 'convert trunc u8 i64'
require_function_line logical_bool_local_init 'store u8 %t'
require_function_line logical_bool_assignment 'convert trunc u8 i64'
require_function_line logical_bool_assignment 'store u8 %t'
require_function_line logical_bool_to_int 'convert trunc u8 i64'
require_function_line logical_bool_to_int 'convert zext i32 u8'
require_function_line logical_bool_to_double 'convert trunc u8 i64'
require_function_line logical_bool_to_double 'convert sitofp f64 u8'
require_function_line logical_bool_variadic 'convert trunc u8 i64'
require_function_line logical_bool_variadic 'convert zext i32 u8'
require_function_line logical_bool_variadic 'call i32 @sink('
require_function_line const_bool_reference_result 'convert trunc u8 i64'
require_function_line const_bool_reference_result 'store u8 %t'
require_function_line const_bool_reference_result 'addr $refarg__'
require_function_line const_bool_reference_result 'store ptr %t'
require_function_line const_bool_reference_result 'load u8 %t'
require_function_line rvalue_bool_reference_result 'convert trunc u8 i64'
require_function_line rvalue_bool_reference_result 'store u8 %t'
require_function_line rvalue_bool_reference_result 'addr $refarg__'
require_function_line negative_zero 'cmp ne f80'
require_function_line negative_zero 'branch %t'
require_line 'declare function @sink(%arg0 : ptr) -> i32 [arity=variadic'
require_line 'call i32 @sink('
require_count_at_least 'convert sext i32 i8' 2
require_count_at_least 'convert zext i32 u8' 2
require_count_at_least 'convert sext i32 i16' 1
require_count_at_least 'convert zext i32 u16' 1
require_line 'convert fpext f64 f32'
require_line 'load i32 $integer_value'
require_line 'load f64 $double_value'
require_line 'load ptr $pointer_value'

# ReferenceBinding owns both pointer-reference temporary storage and direct
# function-reference address flow; lower_call must not reconstruct either from
# source category or spelling.
require_line 'function @pointer_reference_target(%value : ptr [pass=reference])'
require_line 'function @function_reference_target(%value : ptr [pass=reference])'
if ! rg -q 'slot \$refarg__[0-9]+ : ptr' "$output"; then
  echo 'missing pointer reference temporary slot' >&2
  exit 1
fi
if ! rg -q 'addr \$refarg__[0-9]+' "$output"; then
  echo 'missing pointer reference temporary address' >&2
  exit 1
fi
require_line 'call i32 @pointer_reference_target('
require_line 'call i32 @function_reference_target('

# These source identifiers deliberately occupy the first helper spellings.
# The generated conditional and reference temporaries must advance past them.
require_line 'slot $refarg__1 : f64'
require_line 'slot $refarg__2 : f64'
require_line 'slot $refarg__3 : f64'
require_line 'slot $refarg__4 : f64'
require_line 'slot $cond__2 : i32'
require_line 'addr $refarg__4'

"$lowir2cy86" -o "$build_dir/floating-boundary.cy86" "$output"
