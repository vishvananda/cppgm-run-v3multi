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

positive_semantics=$build_dir/positive.semantics
"$app" --emit-semantics -o "$positive_semantics" \
    "$test_dir/406-typed-label-positive.source"
if [ "$(rg -F -c -- 'labeled-statement target' "$positive_semantics")" -ne 2 ] ||
   [ "$(rg -F -c -- 'goto-statement target' "$positive_semantics")" -ne 2 ]; then
  echo "typed label/goto facts were not published for both positive paths" >&2
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
