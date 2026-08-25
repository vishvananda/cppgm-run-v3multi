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

output=$build_dir/raw.lowir
"$app" --emit-lowir -O0 -o "$output" \
  "$test_dir/400-raw-lowir-linkage-regression.source"

require_line()
{
  if ! rg -Fq -- "$1" "$output"; then
    echo "missing raw LowIR line: $1" >&2
    exit 1
  fi
}

require_line 'function @a__f() -> i32 [linkage=c, binding=internal, object=_ZN1a1fEv] {'
require_line 'function @b__f() -> i32 [linkage=c, binding=internal, object=_ZN1b1fEv] {'

if rg -Fq -- '[binding=strong, object=_ZN1a1fEv]' "$output" ||
   rg -Fq -- '[binding=strong, object=_ZN1b1fEv]' "$output"; then
  echo "raw LowIR retained the old strong/no-linkage metadata" >&2
  exit 1
fi
