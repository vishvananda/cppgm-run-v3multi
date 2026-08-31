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

previous_lines=
previous_delta=
delta_8_to_16=
delta_16_to_32=

for element_count in 8 16 32; do
  source=$build_dir/member-array-$element_count.cpp
  lowir=$build_dir/member-array-$element_count.lowir
  printf '%s\n' \
    'struct Element {' \
    '  Element() {}' \
    '  ~Element() {}' \
    '};' \
    'struct Holder {' \
    "  Element elements[$element_count];" \
    '};' \
    'int main() {' \
    '  Holder holder;' \
    '  return 0;' \
    '}' >"$source"

  "$app" --emit-lowir -O0 -o "$lowir" "$source"
  holder_function=$(sed -n '/^function @Holder__Holder/,/^}/p' "$lowir")
  cleanup_nodes=$(printf '%s\n' "$holder_function" |
    rg -c '^  block \^array_ctor_cleanup_[0-9]+:' || true)
  cleanup_calls=$(printf '%s\n' "$holder_function" |
    rg -c 'call void @Element___Element' || true)
  malformed_nodes=$(printf '%s\n' "$holder_function" | awk '
    /^  block \^array_ctor_cleanup_[0-9]+:/ {
      if (in_cleanup && calls != 1) malformed = 1
      in_cleanup = 1
      calls = 0
      next
    }
    /^  block / {
      if (in_cleanup && calls != 1) malformed = 1
      in_cleanup = 0
    }
    in_cleanup && /call void @Element___Element/ { calls++ }
    END {
      if (in_cleanup && calls != 1) malformed = 1
      print malformed + 0
    }')
  holder_lines=$(printf '%s\n' "$holder_function" | wc -l | tr -d ' ')
  expected=$((element_count - 1))
  if [ "$cleanup_nodes" -ne "$expected" ] ||
     [ "$cleanup_calls" -ne "$expected" ] ||
     [ "$malformed_nodes" -ne 0 ]; then
    echo "E=$element_count expected $expected one-call cleanup nodes, got $cleanup_nodes nodes/$cleanup_calls calls (malformed=$malformed_nodes)" >&2
    exit 1
  fi

  if [ -n "$previous_lines" ]; then
    delta=$((holder_lines - previous_lines))
    if [ "$delta" -le 0 ]; then
      echo "E=$element_count did not increase Holder constructor LowIR lines" >&2
      exit 1
    fi
    if [ -n "$previous_delta" ] && [ "$delta" -ne $((previous_delta * 2)) ]; then
      echo "member/action array LowIR growth was not linear: prior delta $previous_delta, current delta $delta" >&2
      exit 1
    fi
    if [ "$element_count" -eq 16 ]; then
      delta_8_to_16=$delta
    else
      delta_16_to_32=$delta
    fi
    previous_delta=$delta
  fi
  previous_lines=$holder_lines
  printf 'PA16 typed action/member-array E=%s cleanup_nodes=%s cleanup_calls=%s Holder_lines=%s\n' \
    "$element_count" "$cleanup_nodes" "$cleanup_calls" "$holder_lines"
done

printf 'PA16 typed action/member-array linear deltas: 8->16=%s 16->32=%s\n' \
  "$delta_8_to_16" "$delta_16_to_32"
