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

output=$build_dir/enum-boundary.lowir
"$app" --emit-lowir -O0 -o "$output" \
  "$test_dir/402-typed-enum-boundary-regression.source"

if ! rg -Fq -- 'call i32 @api__read(3)' "$output"; then
	echo "default argument was not reused from the declaration context" >&2
	exit 1
fi

if ! rg -Fq -- 'global @nonzero : u8' "$output" ||
   ! rg -Fq -- 'global @nonzero : u8 [binding=strong, object=nonzero] = 1' "$output"; then
	echo "nonzero bool constant did not normalize to one" >&2
	exit 1
fi

if ! rg -Fq -- 'cmp eq u32 4294967295, 4294967295' "$output" ||
   ! rg -Fq -- 'cmp eq u32 2147483648, 2147483648' "$output"; then
	echo "typed conditional or valid shift boundary was not preserved" >&2
	exit 1
fi

if "$app" --emit-lowir -O0 -o "$build_dir/bad.lowir" \
    "$test_dir/402-typed-enum-boundary-regression-bad.source" \
    >"$build_dir/bad.stdout" 2>"$build_dir/bad.stderr"; then
	echo "fixed-underlying enum accepted an out-of-range enumerator" >&2
	exit 1
fi

if "$app" --emit-lowir -O0 -o "$build_dir/bad-shift.lowir" \
    "$test_dir/402-typed-enum-boundary-regression-bad-shift.source" \
    >"$build_dir/bad-shift.stdout" 2>"$build_dir/bad-shift.stderr"; then
	echo "32-bit unsigned shift accepted a count equal to its width" >&2
	exit 1
fi

if "$app" --emit-lowir -O0 -o "$build_dir/bad-bool.lowir" \
    "$test_dir/402-typed-enum-boundary-regression-bad-bool.source" \
    >"$build_dir/bad-bool.stdout" 2>"$build_dir/bad-bool.stderr"; then
	echo "fixed bool enum accepted a value outside its representable range" >&2
	exit 1
fi

if "$app" --emit-lowir -O0 -o "$build_dir/bad-scoped-conditional.lowir" \
    "$test_dir/402-typed-enum-boundary-regression-bad-scoped-conditional.source" \
    >"$build_dir/bad-scoped-conditional.stdout" 2>"$build_dir/bad-scoped-conditional.stderr"; then
	echo "mixed scoped-enum conditional operands were accepted" >&2
	exit 1
fi
