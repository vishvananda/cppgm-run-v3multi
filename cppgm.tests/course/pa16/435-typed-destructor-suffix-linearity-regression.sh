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
  source=$build_dir/destructor-suffix-$element_count.cpp
  lowir=$build_dir/destructor-suffix-$element_count.lowir
  printf '%s\n' \
    'struct Leaf {' \
    '  ~Leaf() noexcept(false) {}' \
    '};' \
    'struct Holder {' \
    "  Leaf leaves[$element_count];" \
    '  ~Holder() {}' \
    '};' \
    'int main() {' \
    '  Holder holder;' \
    '  return 0;' \
    '}' >"$source"

  "$app" --emit-lowir -O0 -o "$lowir" "$source"
  holder_function=$(sed -n '/^function @Holder___Holder/,/^}/p' "$lowir")
  if [ -z "$holder_function" ]; then
    echo "E=$element_count did not emit Holder destructor" >&2
    exit 1
  fi

  suffix_nodes=$(printf '%s\n' "$holder_function" |
    rg -c '^  block \^destructor_suffix_cleanup_[0-9]+:' || true)
  suffix_handlers=$(printf '%s\n' "$holder_function" |
    rg -c '^    eh_cleanup \^destructor_suffix_cleanup_[0-9]+$' || true)
  suffix_terminal=$(printf '%s\n' "$holder_function" |
    rg -c '^  block \^destructor_suffix_terminal_[0-9]+:' || true)
  expected=$((element_count - 1))
  if [ "$suffix_nodes" -ne "$expected" ] ||
     [ "$suffix_handlers" -ne "$expected" ] ||
     [ "$suffix_terminal" -ne 1 ]; then
    echo "E=$element_count expected $expected shared suffix nodes/handlers and one terminal, got $suffix_nodes/$suffix_handlers/$suffix_terminal" >&2
    exit 1
  fi

  suffix_edges=$(printf '%s\n' "$holder_function" | awk '
    /^  block \^destructor_suffix_cleanup_[0-9]+:/ {
      if (in_cleanup) print name "|" element "|" target "|" calls "|" jumps "|" eh_ops;
      name = $2;
      sub(/^\^/, "", name);
      sub(/:$/, "", name);
      element = "";
      target = "";
      calls = 0;
      jumps = 0;
      eh_ops = 0;
      in_cleanup = 1;
      next;
    }
    /^  block / {
      if (in_cleanup) print name "|" element "|" target "|" calls "|" jumps "|" eh_ops;
      in_cleanup = 0;
    }
    in_cleanup && /projection=array_element/ {
      element = $NF;
    }
    in_cleanup && /call void @Leaf___Leaf/ { calls++; }
    in_cleanup && /    jump \^/ {
      target = $NF;
      sub(/^\^/, "", target);
      jumps++;
    }
    in_cleanup && /    eh_/ { eh_ops++; }
    END {
      if (in_cleanup) print name "|" element "|" target "|" calls "|" jumps "|" eh_ops;
    }')
  malformed=$(printf '%s\n' "$suffix_edges" | awk -F'|' '
    NF != 6 || $2 == "" || $3 == "" || $4 != 1 || $5 != 1 || $6 != 0 { bad = 1 }
    END { print bad + 0 }')
  if [ "$malformed" -ne 0 ]; then
    echo "E=$element_count has malformed shared suffix nodes: $suffix_edges" >&2
    exit 1
  fi
  suffix_calls=$(printf '%s\n' "$suffix_edges" |
    awk -F'|' '{ total += $4 } END { print total + 0 }')
  if [ "$suffix_calls" -ne "$expected" ]; then
    echo "E=$element_count emitted $suffix_calls suffix calls, expected $expected" >&2
    exit 1
  fi

  normal_order=$(printf '%s\n' "$holder_function" | awk '
    /^  block \^(entry|destructor_suffix_next_[0-9]+):/ {
      if (in_normal) print element "|" calls;
      in_normal = 1;
      element = "";
      calls = 0;
      next;
    }
    /^  block / {
      if (in_normal) print element "|" calls;
      in_normal = 0;
    }
    in_normal && /projection=array_element/ { element = $NF; }
    in_normal && /call void @Leaf___Leaf/ { calls++; }
    END { if (in_normal) print element "|" calls; }')
  normal_count=$(printf '%s\n' "$normal_order" | awk 'NF { count++ } END { print count + 0 }')
  if [ "$normal_count" -ne "$element_count" ]; then
    echo "E=$element_count emitted $normal_count normal destructor blocks, expected $element_count" >&2
    exit 1
  fi
  normal_index=$((element_count - 1))
  while IFS='|' read -r actual_index calls; do
    if [ "$actual_index" -ne "$normal_index" ] || [ "$calls" -ne 1 ]; then
      echo "E=$element_count normal destruction order/call shape was '$normal_order'" >&2
      exit 1
    fi
    normal_index=$((normal_index - 1))
  done <<EOF
$normal_order
EOF
  if [ "$normal_index" -ne -1 ]; then
    echo "E=$element_count normal destruction order did not reach element 0" >&2
    exit 1
  fi

  head=$(printf '%s\n' "$holder_function" |
    sed -n 's/^    eh_cleanup \^\(destructor_suffix_cleanup_[0-9][0-9]*\)$/\1/p' |
    head -n1)
  terminal=$(printf '%s\n' "$holder_function" |
    sed -n 's/^  block \^\(destructor_suffix_terminal_[0-9][0-9]*\):$/\1/p')
  if [ -z "$head" ] || [ -z "$terminal" ]; then
    echo "E=$element_count has no shared suffix head or terminal" >&2
    exit 1
  fi
  expected_index=$((element_count - 2))
  node=$head
  while [ "$expected_index" -ge 0 ]; do
    edge=$(printf '%s\n' "$suffix_edges" |
      awk -F'|' -v wanted="$node" '$1 == wanted { print; found = 1; exit } END { if (!found) exit 1 }') || {
        echo "E=$element_count suffix chain lost node $node" >&2
        exit 1
      }
    actual_index=$(printf '%s\n' "$edge" | cut -d'|' -f2)
    next_node=$(printf '%s\n' "$edge" | cut -d'|' -f3)
    if [ "$actual_index" -ne "$expected_index" ]; then
      echo "E=$element_count suffix order was $actual_index at $node, expected $expected_index" >&2
      exit 1
    fi
    if [ "$expected_index" -eq 0 ]; then
      if [ "$next_node" != "$terminal" ]; then
        echo "E=$element_count final suffix node targets $next_node, expected $terminal" >&2
        exit 1
      fi
    else
      node=$next_node
    fi
    expected_index=$((expected_index - 1))
  done

  terminal_body=$(printf '%s\n' "$holder_function" | awk '
    /^  block \^destructor_suffix_terminal_[0-9]+:/ { in_terminal = 1; next }
    /^  block / { if (in_terminal) exit }
    in_terminal { print }')
  terminal_end=$(printf '%s\n' "$terminal_body" | rg -c '^    eh_end$' || true)
  terminal_resume=$(printf '%s\n' "$terminal_body" | rg -c '^    resume$' || true)
  terminal_other=$(printf '%s\n' "$terminal_body" |
    awk 'NF && $0 !~ /^    (eh_end|resume)$/ { bad = 1 } END { print bad + 0 }')
  if [ "$terminal_end" -ne 1 ] || [ "$terminal_resume" -ne 1 ] ||
     [ "$terminal_other" -ne 0 ]; then
    echo "E=$element_count shared suffix terminal is malformed" >&2
    exit 1
  fi

  holder_lines=$(printf '%s\n' "$holder_function" | wc -l | tr -d ' ')
  if [ -n "$previous_lines" ]; then
    delta=$((holder_lines - previous_lines))
    if [ "$delta" -le 0 ]; then
      echo "E=$element_count did not increase Holder destructor LowIR lines" >&2
      exit 1
    fi
    if [ -n "$previous_delta" ] && [ "$delta" -ne $((previous_delta * 2)) ]; then
      echo "destructor suffix LowIR growth was not linear: prior delta $previous_delta, current delta $delta" >&2
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
  printf 'PA16 typed destructor-suffix E=%s suffix_nodes=%s suffix_calls=%s terminal=1 Holder_lines=%s\n' \
    "$element_count" "$suffix_nodes" "$suffix_calls" "$holder_lines"
done

printf 'PA16 typed destructor-suffix linear deltas: 8->16=%s 16->32=%s\n' \
  "$delta_8_to_16" "$delta_16_to_32"
