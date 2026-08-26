#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$test_dir/../../.." && pwd)
lowir2cy86=${CPPGM_PA13_LOWIR2CY86:-$repo_root/dev/lowir2cy86}
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM

if [ ! -x "$lowir2cy86" ]; then
  echo "LowIR validator is not executable: $lowir2cy86" >&2
  exit 1
fi

positive=$build_dir/positive.lowir
cat >"$positive" <<'EOF'
global @zero_i32 : i32 = zero
global @zero_ptr : ptr = zero
global @anchor : i64 = zero

function @main() -> i64 [role=entry] {
  block ^entry:
    %signed = const i8 1
    %retagged = copy u8 %signed
    %wide = convert zext i64 u8 %retagged
    %left = addr @anchor
    %right = addr @anchor
    %distance = binary sub ptr %left, %right
    %result = binary add i64 %distance, %wide
    return i64 %result
}
EOF

unequal_width=$build_dir/unequal-width-copy.lowir
cat >"$unequal_width" <<'EOF'
function @main() -> i64 [role=entry] {
  block ^entry:
    %signed = const i8 1
    %bad = copy u16 %signed
    return i64 0
}
EOF

bad_zero=$build_dir/void-zero.lowir
cat >"$bad_zero" <<'EOF'
global @bad : void = zero

function @main() -> i64 [role=entry] {
  block ^entry:
    return i64 0
}
EOF

pointer_add=$build_dir/pointer-add.lowir
cat >"$pointer_add" <<'EOF'
global @anchor : i64 = zero

function @main() -> i64 [role=entry] {
  block ^entry:
    %left = addr @anchor
    %right = addr @anchor
    %bad = binary add ptr %left, %right
    return i64 0
}
EOF

run_case()
{
  name=$1
  expected=$2
  input=$3
  output=$build_dir/$name.cy86
  if "$lowir2cy86" -o "$output" "$input"; then
    status=0
  else
    status=$?
  fi

  if [ "$expected" = success ] && [ "$status" -ne 0 ]; then
    echo "$name: expected exit 0, got $status" >&2
    exit 1
  fi
  if [ "$expected" = failure ] && [ "$status" -eq 0 ]; then
    echo "$name: expected nonzero exit" >&2
    exit 1
  fi
  echo "$name: exit $status"
}

run_case positive success "$positive"
run_case unequal-width-copy failure "$unequal_width"
run_case void-zero-global failure "$bad_zero"
run_case pointer-add failure "$pointer_add"
