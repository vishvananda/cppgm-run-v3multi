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

output=$build_dir/positive.lowir
"$app" --emit-lowir -O0 -o "$output" \
  "$test_dir/403-typed-reinterpret-boundary-regression.source"

zero_copies=$(rg -c -F -- 'copy ptr 0' "$output" || true)
if [ "$zero_copies" -lt 2 ]; then
  echo "typed integer/enum zero did not lower as pointer zero" >&2
  exit 1
fi

"$lowir2cy86" -o "$build_dir/positive.cy86" "$output"

for kind in int enum; do
  bad_output=$build_dir/bad-$kind.lowir
  if "$app" --emit-lowir -O0 -o "$bad_output" \
      "$test_dir/403-typed-reinterpret-boundary-regression-bad-$kind.source" \
      >"$build_dir/bad-$kind.stdout" 2>"$build_dir/bad-$kind.stderr"; then
    echo "nonzero $kind-to-pointer reinterpret unexpectedly compiled" >&2
    exit 1
  fi
	if [ -s "$bad_output" ] && rg -q -e 'copy ptr [1-9]' "$bad_output"; then
		echo "nonzero $kind-to-pointer emitted invalid LowIR" >&2
		exit 1
	fi
done
