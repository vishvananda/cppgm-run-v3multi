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

return_source=$build_dir/return-before-later.cpp
printf '%s\n' \
  'struct Guard {' \
  '  Guard() {}' \
  '  ~Guard() {}' \
  '};' \
  'int main() {' \
  '  { Guard first; return 0; Guard later; }' \
  '}' >"$return_source"
return_output=$build_dir/return-before-later.lowir
"$app" --emit-lowir -O0 -o "$return_output" "$return_source"
return_main=$(sed -n '/^function @main/,/^}/p' "$return_output")
return_dtors=$(printf '%s\n' "$return_main" |
  rg -c 'call void @Guard___Guard' || true)
if [ "$return_dtors" -ne 1 ]; then
  echo "return before later declaration emitted $return_dtors Guard destructors, expected 1" >&2
  exit 1
fi

loop_source=$build_dir/loop-exits.cpp
printf '%s\n' \
  'struct Guard {' \
  '  Guard() {}' \
  '  ~Guard() {}' \
  '};' \
  'int main() {' \
  '  int value = 1;' \
  '  while (value) {' \
  '    Guard guard;' \
  '    if (value == 1) {' \
  '      value = 0;' \
  '      continue;' \
  '    } else {' \
  '      value = 0;' \
  '      break;' \
  '    }' \
  '  }' \
  '  return 0;' \
  '}' >"$loop_source"
loop_output=$build_dir/loop-exits.lowir
"$app" --emit-lowir -O0 -o "$loop_output" "$loop_source"
loop_main=$(sed -n '/^function @main/,/^}/p' "$loop_output")
loop_dtors=$(printf '%s\n' "$loop_main" |
  rg -c 'call void @Guard___Guard' || true)
if [ "$loop_dtors" -ne 2 ]; then
  echo "loop break/continue emitted $loop_dtors Guard destructors, expected 2" >&2
  exit 1
fi
if ! printf '%s\n' "$loop_main" | rg -U -q \
    'call void @Guard___Guard\([^\n]+\)\n    jump \^while_cond_'; then
  echo "continue path did not destroy its active Guard before the loop condition" >&2
  exit 1
fi
if ! printf '%s\n' "$loop_main" | rg -U -q \
    'call void @Guard___Guard\([^\n]+\)\n    jump \^while_end_'; then
  echo "break path did not destroy its active Guard before the loop exit" >&2
  exit 1
fi

branch_source=$build_dir/branch-exits.cpp
printf '%s\n' \
  'struct Guard {' \
  '  Guard() {}' \
  '  ~Guard() {}' \
  '};' \
  'int main(int value) {' \
  '  Guard outer;' \
  '  if (value) {' \
  '    return 1;' \
  '  } else {' \
  '    value = 0;' \
  '  }' \
  '  return value;' \
  '}' >"$branch_source"
branch_output=$build_dir/branch-exits.lowir
"$app" --emit-lowir -O0 -o "$branch_output" "$branch_source"
branch_main=$(sed -n '/^function @main/,/^}/p' "$branch_output")
branch_returns=$(printf '%s\n' "$branch_main" | rg -c 'return i32' || true)
branch_dtors=$(printf '%s\n' "$branch_main" |
  rg -c 'call void @Guard___Guard' || true)
if [ "$branch_returns" -ne 2 ] || [ "$branch_dtors" -ne 2 ]; then
  echo "branch paths emitted $branch_dtors Guard destructors for $branch_returns returns, expected 2 and 2" >&2
  exit 1
fi
branch_cleanup_returns=$(printf '%s\n' "$branch_main" |
  rg -U -c 'call void @Guard___Guard\([^\n]+\)\n    return i32' || true)
if [ "$branch_cleanup_returns" -ne 2 ]; then
  echo "not every branch return has exactly one preceding Guard destructor" >&2
  exit 1
fi

nested_source=$build_dir/nested-array.cpp
printf '%s\n' \
  'struct Guard {' \
  '  Guard() {}' \
  '  ~Guard() {}' \
  '};' \
  'int main() {' \
  '  Guard nested[2][3];' \
  '  return 0;' \
  '}' >"$nested_source"
nested_output=$build_dir/nested-array.lowir
"$app" --emit-lowir -O0 -o "$nested_output" "$nested_source"
nested_main=$(sed -n '/^function @main/,/^}/p' "$nested_output")
nested_cleanup=$(printf '%s\n' "$nested_main" | awk '
  /^  block / {
    if (has_return) printf "%s", block_text
    block_text = $0 "\n"
    has_return = 0
    next
  }
  {
    block_text = block_text $0 "\n"
    if ($0 ~ /return i32/) has_return = 1
  }
  END {
    if (has_return) printf "%s", block_text
  }
')
nested_dtors=$(printf '%s\n' "$nested_cleanup" |
  rg -c 'call void @Guard___Guard' || true)
if [ "$nested_dtors" -ne 6 ]; then
  echo "nested array emitted $nested_dtors Guard destructors, expected 6" >&2
  exit 1
fi
if ! printf '%s\n' "$nested_cleanup" | rg -q 'binary mul i64 1, 3'; then
  echo "nested array did not emit the checked outer byte stride" >&2
  exit 1
fi
outer_strides=$(printf '%s\n' "$nested_cleanup" |
  rg 'binary mul i64 [01], 3' |
  sed -E 's/.*i64 ([01]), 3.*/\1/' | tr '\n' ' ')
if [ "$outer_strides" != '1 0 ' ]; then
  echo "nested array outer cleanup stride order was '$outer_strides', expected '1 0 '" >&2
  exit 1
fi
inner_indices=$(printf '%s\n' "$nested_cleanup" |
  rg 'index i8 .*\, [0-9]+$' |
  sed -E 's/.*,[[:space:]]*([0-9]+)$/\1/' | tr '\n' ' ')
if [ "$inner_indices" != '2 1 0 2 1 0 ' ]; then
  echo "nested array inner cleanup order was '$inner_indices', expected '2 1 0 2 1 0 '" >&2
  exit 1
fi

stress_delta_16=0
stress_delta_32=0
for stress_e in 8 16 32; do
  stress_source=$build_dir/flat-array-$stress_e.cpp
  printf '%s\n' \
    'struct Guard {' \
    '  Guard() {}' \
    '  ~Guard() {}' \
    '};' \
    'int main() {' \
    "  Guard values[$stress_e];" \
    '  return 0;' \
    '}' >"$stress_source"
  stress_output=$build_dir/flat-array-$stress_e.lowir
  "$app" --emit-lowir -O0 -o "$stress_output" "$stress_source"
  stress_main=$(sed -n '/^function @main/,/^}/p' "$stress_output")
  stress_shape=$(printf '%s\n' "$stress_main" | awk '
    /^  block \^array_ctor_cleanup_[0-9]+:/ { in_cleanup=1; nodes++; next }
    /^  block / { in_cleanup=0 }
    in_cleanup && /call void @Guard___Guard/ { calls++ }
    END { printf "%d %d", nodes + 0, calls + 0 }
  ')
  stress_nodes=${stress_shape%% *}
  stress_calls=${stress_shape##* }
  stress_lines=$(printf '%s\n' "$stress_main" | wc -l)
  expected_cleanup=$((stress_e - 1))
  if [ "$stress_nodes" -ne "$expected_cleanup" ] ||
     [ "$stress_calls" -ne "$expected_cleanup" ]; then
    echo "flat E=$stress_e emitted $stress_nodes cleanup nodes/$stress_calls cleanup calls, expected $expected_cleanup" >&2
    exit 1
  fi
  if [ "$stress_e" -eq 16 ]; then
    stress_delta_16=$((stress_lines - stress_lines_8))
  elif [ "$stress_e" -eq 32 ]; then
    stress_delta_32=$((stress_lines - stress_lines_16))
  fi
  eval "stress_lines_$stress_e=$stress_lines"
  printf 'PA16 structural flat E=%s cleanup_calls=%s main_lines=%s\n' \
    "$stress_e" "$stress_calls" "$stress_lines"
done
if [ "$stress_delta_16" -le 0 ] || [ "$stress_delta_32" -le 0 ] ||
   [ "$stress_delta_32" -gt $((stress_delta_16 * 2)) ]; then
  echo "flat-array LowIR line growth is not bounded linearly: deltas $stress_delta_16/$stress_delta_32" >&2
  exit 1
fi
