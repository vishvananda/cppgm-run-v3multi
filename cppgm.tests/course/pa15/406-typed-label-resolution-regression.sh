#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$test_dir/../../.." && pwd)
app=${CPPGM_PA15_APP:-$repo_root/dev/cppgm++}
lowir2cy86=${CPPGM_PA15_LOWIR2CY86:-$repo_root/dev/lowir2cy86}
cy86=${CPPGM_PA15_CY86:-$repo_root/dev/cy86}
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
if [ ! -x "$cy86" ]; then
  echo "CY86 compiler is not executable: $cy86" >&2
  exit 1
fi

positive_semantics=$build_dir/positive.semantics
"$app" --emit-semantics -o "$positive_semantics" \
    "$test_dir/406-typed-label-positive.source"
if [ "$(rg -F -c -- 'labeled-statement' "$positive_semantics")" -ne 22 ] ||
   [ "$(rg -F -c -- 'goto-statement' "$positive_semantics")" -ne 22 ]; then
  echo "typed label/goto facts were not published for all positive paths" >&2
  exit 1
fi

positive_lowir=$build_dir/positive.lowir
"$app" --emit-lowir -O0 -o "$positive_lowir" \
    "$test_dir/406-typed-label-positive.source"
"$lowir2cy86" -o "$build_dir/positive.cy86" "$positive_lowir"

function_counts()
{
  awk -v name="$1" '
    $0 ~ "^function @" name "[(]" { active = 1; next }
    active && /^}/ { print blocks + 0, edges + 0; exit }
    active && /^  block [^ ]*goto_/ { ++blocks }
    active && /jump [^ ]*goto_/ { ++edges }
  ' "$positive_lowir"
}

if [ "$(function_counts forward_nested)" != "1 1" ]; then
  echo "forward nested label did not form one target block and one edge" >&2
  exit 1
fi
if awk '
    $0 ~ "^function @forward_nested[(]" { active = 1; next }
    active && /^}/ { exit found ? 0 : 1 }
    active && ($0 ~ /store i32 99,/ || $0 ~ /store i32 88,/) { found = 1 }
    END { if (active) exit found ? 0 : 1 }
  ' "$positive_lowir"; then
  echo "unreachable forward-goto siblings were emitted" >&2
  exit 1
fi
if [ "$(function_counts backward_label)" != "1 2" ]; then
  echo "backward label did not converge normal and goto edges" >&2
  exit 1
fi
if [ "$(function_counts chained_goto)" != "2 2" ]; then
  echo "chained goto recovery did not materialize both target blocks and edges" >&2
  exit 1
fi
if [ "$(function_counts chained_fallthrough)" != "2 3" ]; then
  echo "chained fallthrough did not preserve the enclosing compound continuation" >&2
  exit 1
fi
if ! awk '
    $0 ~ "^function @chained_fallthrough[(]" { active = 1; next }
    active && /^}/ { active = 0; next }
    active && /store i32 1,/ { saw_store = 1 }
    active && /binary add i32/ && /, 2$/ { saw_add = 1 }
    active && /jump [^ ]*goto_/ { saw_jump = 1 }
    active && /branch / { saw_branch = 1 }
    active && /return i32 / { saw_return = 1 }
    END { exit (saw_store && saw_add && saw_jump && saw_branch &&
      saw_return) ? 0 : 1 }
  ' "$positive_lowir"; then
  echo "chained fallthrough omitted the intervening assignment path" >&2
  exit 1
fi
if [ "$(function_counts deferred_branch)" != "2 2" ]; then
  echo "branch-created deferred target did not materialize both target blocks and edges" >&2
  exit 1
fi
if ! awk '
    $0 ~ "^function @deferred_branch[(]" { active = 1; next }
    active && /^}/ { active = 0; next }
    active && /branch / { saw_branch = 1 }
    active && /return i32 1/ { saw_one = 1 }
    active && /return i32 2/ { saw_two = 1 }
    END { exit (saw_branch && saw_one && saw_two) ? 0 : 1 }
  ' "$positive_lowir"; then
  echo "deferred branch lost one of its live return paths" >&2
  exit 1
fi
if [ "$(function_counts nested_fallthrough)" != "2 3" ]; then
  echo "nested compound recovery did not preserve both continuation levels" >&2
  exit 1
fi
if ! awk '
    $0 ~ "^function @nested_fallthrough[(]" { active = 1; next }
    active && /^}/ { active = 0; next }
    active && /store i32 1,/ { saw_store = 1 }
    active && /binary add i32/ && /, 2$/ { saw_add = 1 }
    active && /jump [^ ]*goto_/ { saw_jump = 1 }
    active && /branch / { saw_branch = 1 }
    active && /return i32 / { saw_return = 1 }
    END { exit (saw_store && saw_add && saw_jump && saw_branch &&
      saw_return) ? 0 : 1 }
  ' "$positive_lowir"; then
  echo "nested compound recovery omitted inner fallthrough or outer label" >&2
  exit 1
fi
if [ "$(function_counts loop_entry)" != "1 2" ]; then
  echo "loop entry recovery changed its target edges unexpectedly" >&2
  exit 1
fi
if ! awk '
    $0 ~ "^function @loop_entry[(]" { active = 1; next }
    active && /^}/ { active = 0; next }
    active && /branch / { saw_branch = 1 }
    active && /binary add i32/ && /, 1$/ { saw_add = 1 }
    active && /jump [^ ]*while_cond_/ { saw_backedge = 1 }
    active && /return i32 / { saw_return = 1 }
    END { exit (saw_branch && saw_add && saw_backedge && saw_return) ? 0 : 1 }
  ' "$positive_lowir"; then
  echo "loop entry recovery omitted condition or body backedge" >&2
  exit 1
fi
if [ "$(function_counts nested_control)" != "1 2" ]; then
  echo "nested loop recovery did not retain the label entry edges" >&2
  exit 1
fi
if ! awk '
    $0 ~ "^function @nested_control[(]" { active = 1; next }
    active && /^}/ { active = 0; next }
    active && /^  block / {
      current = $2
      sub(/:$/, "", current)
      next
    }
    active && current ~ /^\^while_end_/ && /jump \^while_end_/ {
      target = $2
      if (current != target) saw_outer_break = 1
    }
    END { exit saw_outer_break ? 0 : 1 }
  ' "$positive_lowir"; then
  echo "nested loop recovery retained the inner break target" >&2
  exit 1
fi
if ! awk '
    $0 ~ "^function @nested_control[(]" { active = 1; next }
    active && /^}/ { active = 0; next }
    active && /branch / { ++conditions }
    active && /binary add i32/ && /, 1$/ { saw_add = 1 }
    END { exit (conditions >= 2 && saw_add) ? 0 : 1 }
  ' "$positive_lowir"; then
  echo "nested loop recovery omitted one of its typed loop conditions" >&2
  exit 1
fi
if [ "$(function_counts switch_loop_context)" != "1 2" ]; then
  echo "switch/loop recovery did not retain the label entry edges" >&2
  exit 1
fi
if ! awk '
    $0 ~ "^function @switch_loop_context[(]" { active = 1; next }
    active && /^}/ { active = 0; next }
    active && /^  block / {
      current = $2
      sub(/:$/, "", current)
      next
    }
    active && current ~ /^\^goto_/ && /jump \^switch_end_/ { saw_switch_break = 1 }
    active && current ~ /^\^if_then_/ && /jump \^while_cond_/ { saw_continue = 1 }
    active && current ~ /^\^if_end_/ && /jump \^while_end_/ { saw_outer_break = 1 }
    END { exit (saw_switch_break && saw_continue && saw_outer_break) ? 0 : 1 }
  ' "$positive_lowir"; then
  echo "switch/loop recovery did not replace inner break with outer control targets" >&2
  exit 1
fi
if [ "$(function_counts switch_ordinary_deferred)" != "2 3" ]; then
  echo "deferred ordinary switch label did not preserve case fallthrough" >&2
  exit 1
fi
if ! awk '
    $0 ~ "^function @switch_ordinary_deferred[(]" { active = 1; next }
    active && /^}/ { active = 0; next }
    active && /store i32 1,/ { saw_store = 1 }
    active && /binary add i32/ && /, 2$/ { saw_add = 1 }
    active && /binary add i32/ && /, 3$/ { saw_default_add = 1 }
    active && /jump [^ ]*switch_end_/ { saw_break = 1 }
    active && /jump [^ ]*switch_case_/ { saw_case_fallthrough = 1 }
    active && /return i32 / { saw_return = 1 }
    END { exit (saw_store && saw_add && saw_default_add && saw_break &&
      saw_case_fallthrough && saw_return) ? 0 : 1 }
  ' "$positive_lowir"; then
  echo "deferred ordinary switch label omitted sibling case/break continuation" >&2
  exit 1
fi
if [ "$(function_counts switch_deferred)" != "3 5" ]; then
  echo "deferred switch recovery did not preserve ordinary label targets" >&2
  exit 1
fi
if ! awk '
    $0 ~ "^function @switch_deferred[(]" { active = 1; next }
    active && /^}/ { active = 0; next }
    active && /switch / { saw_switch = 1 }
    active && /binary add i32/ && /, 2$/ { saw_add = 1 }
    active && /binary add i32/ && /, 3$/ { saw_default_add = 1 }
    active && /jump [^ ]*switch_end_/ { saw_break = 1 }
    active && /jump [^ ]*switch_resume_/ { saw_resume = 1 }
    END { exit (saw_switch && saw_add && saw_default_add && saw_break && saw_resume) ? 0 : 1 }
  ' "$positive_lowir"; then
  echo "deferred ordinary switch-label entry omitted case continuation" >&2
  exit 1
fi
if [ "$(function_counts shared_recovery_tail)" != "4 6" ]; then
  echo "shared recovery tail did not retain all typed label targets and edges" >&2
  exit 1
fi
if [ "$(awk '
    $0 ~ "^function @shared_recovery_tail[(]" { active = 1; next }
    active && /^}/ { active = 0; next }
    active && /^  block [^ ]*label_cont_/ { ++count }
    END { print count + 0 }
  ' "$positive_lowir")" -ne 1 ]; then
  echo "overlapping recovery tail was not canonicalized to one continuation block" >&2
  exit 1
fi
if ! awk '
    $0 ~ "^function @shared_recovery_tail[(]" { active = 1; next }
    active && /^}/ { active = 0; next }
    active && /binary add i32/ && /, 1$/ { saw_one = 1 }
    active && /binary add i32/ && /, 2$/ { saw_two = 1 }
    active && /jump [^ ]*goto_/ { ++goto_edges }
    active && /return i32 / { saw_return = 1 }
    END { exit (saw_one && saw_two && goto_edges >= 3 && saw_return) ? 0 : 1 }
  ' "$positive_lowir"; then
  echo "shared recovery tail lost a fallthrough or canonical join edge" >&2
  exit 1
fi

"$cy86" -o "$build_dir/positive.exe" "$build_dir/positive.cy86"
timeout 5s "$build_dir/positive.exe"

if "$app" --emit-semantics -o "$build_dir/duplicate.semantics" \
    "$test_dir/406-typed-label-resolution-regression.source" \
    >"$build_dir/duplicate.stdout" 2>"$build_dir/duplicate.stderr"; then
  echo "duplicate labels were accepted by PA12" >&2
  exit 1
fi

if "$app" --emit-semantics -o "$build_dir/unresolved.semantics" \
    "$test_dir/406-typed-label-unresolved-regression.source" \
    >"$build_dir/unresolved.stdout" 2>"$build_dir/unresolved.stderr"; then
  echo "unresolved goto label was accepted by PA12" >&2
  exit 1
fi
