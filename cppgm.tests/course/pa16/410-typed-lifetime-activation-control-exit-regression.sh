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

if_source=$build_dir/unbraced-if.cpp
printf '%s\n' \
  'struct Guard {' \
  '  Guard() {}' \
  '  ~Guard() {}' \
  '};' \
  'int main(int value) {' \
  '  if (value) Guard guard;' \
  '  return 0;' \
  '}' >"$if_source"
if_output=$build_dir/unbraced-if.lowir
"$app" --emit-lowir -O0 -o "$if_output" "$if_source"
if_main=$(sed -n '/^function @main/,/^}/p' "$if_output")
if_dtor_line=$(printf '%s\n' "$if_main" |
  rg -n 'call void @Guard___Guard' | head -n1 | cut -d: -f1 || true)
if_else_line=$(printf '%s\n' "$if_main" |
  rg -n '^  block \^if_else_' | head -n1 | cut -d: -f1 || true)
if [ -z "$if_dtor_line" ] || [ -z "$if_else_line" ] ||
   [ "$if_dtor_line" -ge "$if_else_line" ]; then
  echo "unbraced if body did not destroy Guard in its statement scope" >&2
  exit 1
fi

while_source=$build_dir/unbraced-while.cpp
printf '%s\n' \
  'struct Guard {' \
  '  Guard() {}' \
  '  ~Guard() {}' \
  '};' \
  'int main(int value) {' \
  '  while (value) Guard guard;' \
  '  return 0;' \
  '}' >"$while_source"
while_output=$build_dir/unbraced-while.lowir
"$app" --emit-lowir -O0 -o "$while_output" "$while_source"
while_main=$(sed -n '/^function @main/,/^}/p' "$while_output")
if ! printf '%s\n' "$while_main" | rg -U -q \
    'call void @Guard___Guard\([^\n]+\)\n    jump \^while_cond_'; then
  echo "unbraced while body did not destroy Guard before the next condition" >&2
  exit 1
fi

for_body_source=$build_dir/unbraced-for.cpp
printf '%s\n' \
  'struct Guard {' \
  '  Guard() {}' \
  '  ~Guard() {}' \
  '};' \
  'int main() {' \
  '  for (int value = 0; value < 1; ++value) Guard guard;' \
  '  return 0;' \
  '}' >"$for_body_source"
for_body_output=$build_dir/unbraced-for.lowir
"$app" --emit-lowir -O0 -o "$for_body_output" "$for_body_source"
for_body_main=$(sed -n '/^function @main/,/^}/p' "$for_body_output")
if ! printf '%s\n' "$for_body_main" | rg -U -q \
    'call void @Guard___Guard\([^\n]+\)\n    jump \^for_iter_'; then
  echo "unbraced for body did not destroy Guard before iteration" >&2
  exit 1
fi

for_init_source=$build_dir/for-init-lifetime.cpp
printf '%s\n' \
  'struct Guard {' \
  '  Guard() {}' \
  '  ~Guard() {}' \
  '};' \
  'int main() {' \
  '  for (Guard guard; 0; ) {}' \
  '  return 0;' \
  '}' >"$for_init_source"
for_init_output=$build_dir/for-init-lifetime.lowir
"$app" --emit-lowir -O0 -o "$for_init_output" "$for_init_source"
for_init_main=$(sed -n '/^function @main/,/^}/p' "$for_init_output")
for_init_dtors=$(printf '%s\n' "$for_init_main" |
  rg -c 'call void @Guard___Guard' || true)
if [ "$for_init_dtors" -ne 1 ] || ! printf '%s\n' "$for_init_main" |
  rg -U -q 'call void @Guard___Guard\([^\n]+\)\n    jump \^for_end_'; then
  echo "for-init Guard was not destroyed on the normal loop exit" >&2
  exit 1
fi

dtor_return_source=$build_dir/destructor-return.cpp
printf '%s\n' \
  'struct Base { ~Base() {} };' \
  'struct Derived: Base { ~Derived() { return; } };' \
  'int main() {' \
  '  Derived value;' \
  '  return 0;' \
  '}' >"$dtor_return_source"
dtor_return_output=$build_dir/destructor-return.lowir
"$app" --emit-lowir -O0 -o "$dtor_return_output" "$dtor_return_source"
dtor_return_function=$(sed -n '/^function @Derived___Derived/,/^}/p' \
  "$dtor_return_output")
if ! printf '%s\n' "$dtor_return_function" | rg -U -q \
    'call void @Base___Base\([^\n]+\)\n    return void'; then
  echo "destructor early return did not preserve base destruction" >&2
  exit 1
fi

loop_state_source=$build_dir/loop-state-join.cpp
printf '%s\n' \
  'struct Guard {' \
  '  Guard() {}' \
  '  ~Guard() {}' \
  '};' \
  'int main() {' \
  '  Guard outer;' \
  '  while (0) {' \
  '    return 1;' \
  '  }' \
  '  return 0;' \
  '}' >"$loop_state_source"
loop_state_output=$build_dir/loop-state-join.lowir
"$app" --emit-lowir -O0 -o "$loop_state_output" "$loop_state_source"
loop_state_main=$(sed -n '/^function @main/,/^}/p' "$loop_state_output")
loop_state_dtors=$(printf '%s\n' "$loop_state_main" |
  rg -c 'call void @Guard___Guard' || true)
loop_state_returns=$(printf '%s\n' "$loop_state_main" |
  rg -U -c 'call void @Guard___Guard\([^\n]+\)\n    return i32' || true)
if [ "$loop_state_dtors" -ne 2 ] || [ "$loop_state_returns" -ne 2 ]; then
  echo "loop join lost the outer Guard lifetime ($loop_state_dtors destructors/$loop_state_returns returns)" >&2
  exit 1
fi

switch_source=$build_dir/switch-state-join.cpp
printf '%s\n' \
  'struct Guard {' \
  '  Guard() {}' \
  '  ~Guard() {}' \
  '};' \
  'int main(int value) {' \
  '  Guard outer;' \
  '  switch (value) {' \
  '  case 1:' \
  '    return 1;' \
  '  case 2:' \
  '    break;' \
  '  default:' \
  '    break;' \
  '  }' \
  '  return 0;' \
  '}' >"$switch_source"
switch_output=$build_dir/switch-state-join.lowir
"$app" --emit-lowir -O0 -o "$switch_output" "$switch_source"
switch_main=$(sed -n '/^function @main/,/^}/p' "$switch_output")
switch_dtors=$(printf '%s\n' "$switch_main" |
  rg -c 'call void @Guard___Guard' || true)
switch_cleanup_returns=$(printf '%s\n' "$switch_main" |
  rg -U -c 'call void @Guard___Guard\([^\n]+\)\n    return i32' || true)
if [ "$switch_dtors" -ne 2 ] || [ "$switch_cleanup_returns" -ne 2 ]; then
  echo "switch arm state did not preserve outer Guard cleanup ($switch_dtors destructors/$switch_cleanup_returns returns)" >&2
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
