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

write_source()
{
  mode=$1
  source=$2
  params=
  arguments=
  i=0
  while [ "$i" -lt 64 ]; do
    if [ -n "$params" ]; then
      params="$params, "
      arguments="$arguments, "
    fi
    params="${params}int p${i}"
    case "$mode" in
      pure) argument=$i ;;
      side) argument=$([ "$i" -eq 0 ] && echo 'side()' || echo "$i") ;;
      body) argument=$i ;;
      volatile) argument=$([ "$i" -eq 0 ] && echo 'source' || echo "$i") ;;
      *) echo "unknown source mode: $mode" >&2; exit 1 ;;
    esac
    arguments="${arguments}${argument}"
    i=$((i + 1))
  done
  {
    if [ "$mode" = side ] || [ "$mode" = body ]; then
      printf '%s\n' 'int side() { return 7; }'
    fi
    printf '%s\n' 'struct value {'
    if [ "$mode" = body ]; then
      printf '  value(%s) { side(); }\n' "$params"
    else
      printf '  value(%s) {}\n' "$params"
    fi
    printf '%s\n' '};' 'struct holder {' '  value first;' '  int second;' '};'
    printf '%s\n' 'int main() {'
    if [ "$mode" = volatile ]; then
      printf '%s\n' '  volatile int source = 7;'
    fi
    printf '  holder x{value{%s}, 0};\n' "$arguments"
    printf '%s\n' '  return x.second;' '}'
  } >"$source"
}

main_body()
{
  awk '
    /^function @main[(]/ { inside = 1; next }
    inside && /^}/ { exit }
    inside { print }
  ' "$1"
}

pure_source=$build_dir/pure.cpp
pure_output=$build_dir/pure.lowir
write_source pure "$pure_source"
"$app" --emit-lowir -O0 -o "$pure_output" "$pure_source"
pure_main=$(main_body "$pure_output")
pure_constructor_calls=$(printf '%s\n' "$pure_main" |
  rg -c 'call void @value__value' || echo 0)
if [ "$(rg -c '^function @value__value[(]' "$pure_output" || true)" -ne 1 ] ||
   [ "$pure_constructor_calls" -ne 0 ] ||
   ! printf '%s\n' "$pure_main" | rg -q 'index i8 \[projection=field\].*, 0'; then
  echo '64-argument empty aggregate constructor did not retain projection while omitting its call' >&2
  exit 1
fi

side_source=$build_dir/side.cpp
side_output=$build_dir/side.lowir
write_source side "$side_source"
"$app" --emit-lowir -O0 -o "$side_output" "$side_source"
side_main=$(main_body "$side_output")
side_calls=$(printf '%s\n' "$side_main" | rg -c 'call i32 @side[(]' || echo 0)
side_constructor_calls=$(printf '%s\n' "$side_main" |
  rg -c 'call void @value__value' || echo 0)
if [ "$side_calls" -ne 1 ] || [ "$side_constructor_calls" -ne 1 ]; then
  echo 'side-effecting aggregate constructor argument was incorrectly elided' >&2
  exit 1
fi

body_source=$build_dir/body.cpp
body_output=$build_dir/body.lowir
write_source body "$body_source"
"$app" --emit-lowir -O0 -o "$body_output" "$body_source"
body_main=$(main_body "$body_output")
body_side_calls=$(rg -c 'call i32 @side[(]' "$body_output" || echo 0)
body_constructor_calls=$(printf '%s\n' "$body_main" |
  rg -c 'call void @value__value' || echo 0)
if [ "$body_side_calls" -ne 1 ] || [ "$body_constructor_calls" -ne 1 ]; then
  echo 'effectful aggregate constructor body was incorrectly elided' >&2
  exit 1
fi

volatile_source=$build_dir/volatile.cpp
volatile_output=$build_dir/volatile.lowir
write_source volatile "$volatile_source"
"$app" --emit-lowir -O0 -o "$volatile_output" "$volatile_source"
volatile_main=$(main_body "$volatile_output")
volatile_constructor_calls=$(printf '%s\n' "$volatile_main" |
  rg -c 'call void @value__value' || echo 0)
if [ "$volatile_constructor_calls" -ne 1 ] ||
   [ "$(printf '%s\n' "$volatile_main" | rg -c 'load .*i32' || true)" -lt 1 ]; then
  echo 'volatile aggregate constructor argument did not retain its read and constructor call' >&2
  exit 1
fi

echo '429 nested braced aggregate-member constructor regression: PASS'
