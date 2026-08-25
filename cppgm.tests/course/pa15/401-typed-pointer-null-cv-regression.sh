#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$test_dir/../../.." && pwd)
app=${CPPGM_PA15_APP:-$repo_root/dev/cppgm++}
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM

if [ ! -x "$app" ]; then
  echo "PA15 driver is not executable: $app" >&2
  exit 1
fi

output=$build_dir/typed-null-cv.lowir
"$app" --emit-lowir -O0 -o "$output" \
  "$test_dir/401-typed-pointer-null-cv-regression.source"

require_line()
{
  if ! rg -Fq -- "$1" "$output"; then
    echo "missing typed-null LowIR line: $1" >&2
    exit 1
  fi
}

require_line 'load ptr $top'
require_line 'copy ptr nullptr'
if ! rg -q 'cmp ne ptr [^,]+, 0' "$output"; then
  echo "missing typed nullptr-to-bool pointer comparison" >&2
  exit 1
fi

if "$app" --emit-lowir -O0 -o "$build_dir/drop.lowir" \
    "$test_dir/401-typed-pointer-null-cv-drop.source" \
    >"$build_dir/drop.stdout" 2>"$build_dir/drop.stderr"; then
  echo "pointee qualification drop unexpectedly compiled" >&2
  exit 1
fi
