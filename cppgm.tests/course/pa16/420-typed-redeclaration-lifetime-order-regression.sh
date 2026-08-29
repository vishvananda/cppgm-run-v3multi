#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$test_dir/../../.." && pwd)
app=${CPPGM_PA16_APP:-$repo_root/dev/cppgm++}
lowir2cy86=${CPPGM_PA16_LOWIR2CY86:-$repo_root/dev/lowir2cy86}
cy86=${CPPGM_PA16_CY86:-$repo_root/dev/cy86}
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM

if [ ! -x "$app" ]; then
  echo "PA16 driver is not executable: $app" >&2
  exit 1
fi
if [ ! -x "$lowir2cy86" ]; then
  echo "PA13 LowIR runner is not executable: $lowir2cy86" >&2
  exit 1
fi
if [ ! -x "$cy86" ]; then
  echo "PA9 CY86 runner is not executable: $cy86" >&2
  exit 1
fi

count_matches()
{
  count=$(rg -c -- "$1" "$2" || true)
  if [ -z "$count" ]; then
    count=0
  fi
  printf '%s\n' "$count"
}

first_line()
{
  line=$(rg -n -m1 -- "$1" "$2" | cut -d: -f1 || true)
  printf '%s\n' "$line"
}

run_case()
{
  name=$1
  shift
  source=$build_dir/$name.cpp
  lowir=$build_dir/$name.lowir
  cy86_source=$build_dir/$name.cy86
  program=$build_dir/$name.program
  printf '%s\n' "$@" >"$source"
  "$app" --emit-lowir -O0 -o "$lowir" "$source"
  "$lowir2cy86" -o "$cy86_source" "$lowir"
  "$cy86" -o "$program" "$cy86_source"
  "$program"
  printf '%s\n' "$lowir"
}

check_ordinary_case()
{
  name=$1
  first=$2
  second=$3
  lowir=$4
  init_body=$build_dir/$name.init
  fini_body=$build_dir/$name.fini
  sed -n '/^function @__cppgm_init/,/^}/p' "$lowir" >"$init_body"
  sed -n '/^function @__cppgm_fini/,/^}/p' "$lowir" >"$fini_body"
  if [ "$(count_matches '^global @first ' "$lowir")" -ne 1 ] ||
     [ "$(count_matches '^global @second ' "$lowir")" -ne 1 ] ||
     [ "$(count_matches '^function @__cppgm_init\(\)' "$lowir")" -ne 1 ] ||
     [ "$(count_matches '^function @__cppgm_fini\(\)' "$lowir")" -ne 1 ] ||
     [ "$(count_matches "call void @${first}__${first}\\(" "$init_body")" -ne 1 ] ||
     [ "$(count_matches "call void @${second}__${second}\\(" "$init_body")" -ne 1 ] ||
     [ "$(count_matches "call void @${first}___${first}\\(" "$fini_body")" -ne 1 ] ||
     [ "$(count_matches "call void @${second}___${second}\\(" "$fini_body")" -ne 1 ]; then
    echo "$name emitted a duplicate or missing typed lifetime action" >&2
    exit 1
  fi
  first_init=$(first_line "call void @${first}__${first}\\(" "$init_body")
  second_init=$(first_line "call void @${second}__${second}\\(" "$init_body")
  first_fini=$(first_line "call void @${first}___${first}\\(" "$fini_body")
  second_fini=$(first_line "call void @${second}___${second}\\(" "$fini_body")
  if [ -z "$first_init" ] || [ -z "$second_init" ] ||
     [ -z "$first_fini" ] || [ -z "$second_fini" ] ||
     [ "$first_init" -ge "$second_init" ] ||
     [ "$second_fini" -ge "$first_fini" ]; then
    echo "$name lost source-order initialization or reverse-order destruction" >&2
    exit 1
  fi
}

one_lowir=$(run_case definition-then-extern \
  'int init_events = 0;' \
  'int fini_events = 0;' \
  'struct First {' \
  '  First() { init_events = init_events * 10 + 1; }' \
  '  ~First() { fini_events = fini_events * 10 + 1; }' \
  '};' \
  'struct Second {' \
  '  Second() { init_events = init_events * 10 + 2; }' \
  '  ~Second() { fini_events = fini_events * 10 + 2; }' \
  '};' \
  'First first;' \
  'Second second;' \
  'extern First first;' \
  'int main() { return init_events == 12 ? 0 : 1; }')
check_ordinary_case definition-then-extern First Second "$one_lowir"

two_lowir=$(run_case extern-then-definition \
  'int init_events = 0;' \
  'int fini_events = 0;' \
  'struct First {' \
  '  First() { init_events = init_events * 10 + 1; }' \
  '  ~First() { fini_events = fini_events * 10 + 1; }' \
  '};' \
  'struct Second {' \
  '  Second() { init_events = init_events * 10 + 2; }' \
  '  ~Second() { fini_events = fini_events * 10 + 2; }' \
  '};' \
  'extern First first;' \
  'Second second;' \
  'First first;' \
  'int main() { return init_events == 21 ? 0 : 1; }')
check_ordinary_case extern-then-definition Second First "$two_lowir"

static_source=$build_dir/static-member-definition.cpp
printf '%s\n' \
  'int init_events = 0;' \
  'int fini_events = 0;' \
  'struct Cell {' \
  '  Cell() { init_events = 7; }' \
  '  ~Cell() { fini_events = 7; }' \
  '  static Cell object;' \
  '};' \
  'Cell Cell::object;' \
  'extern Cell Cell::object;' \
  'int main() { return init_events == 7 ? 0 : 1; }' \
  >"$static_source"
static_lowir=$build_dir/static-member-definition.lowir
static_cy86=$build_dir/static-member-definition.cy86
static_program=$build_dir/static-member-definition.program
"$app" --emit-lowir -O0 -o "$static_lowir" "$static_source"
if [ "$(count_matches '^global @Cell__object ' "$static_lowir")" -ne 1 ] ||
   [ "$(count_matches '^function @__cppgm_init\(\)' "$static_lowir")" -ne 1 ] ||
   [ "$(count_matches '^function @__cppgm_fini\(\)' "$static_lowir")" -ne 1 ] ||
   [ "$(count_matches 'call void @Cell__Cell\(' "$static_lowir")" -ne 1 ] ||
   [ "$(count_matches 'call void @Cell___Cell\(' "$static_lowir")" -ne 1 ]; then
  echo "static member definition/redeclaration emitted duplicate or missing lifetime" >&2
  exit 1
fi
"$lowir2cy86" -o "$static_cy86" "$static_lowir"
"$cy86" -o "$static_program" "$static_cy86"
"$static_program"

tls_source=$build_dir/tls-definition.cpp
printf '%s\n' \
  'struct Thread {' \
  '  int value;' \
  '  Thread() { value = 7; }' \
  '  ~Thread() { value = 0; }' \
  '};' \
  'thread_local Thread tls;' \
  'extern thread_local Thread tls;' \
  'int main() { return 0; }' \
  >"$tls_source"
tls_lowir=$build_dir/tls-definition.lowir
tls_cy86=$build_dir/tls-definition.cy86
tls_program=$build_dir/tls-definition.program
"$app" --emit-lowir -O0 -o "$tls_lowir" "$tls_source"
if [ "$(count_matches '^global @tls ' "$tls_lowir")" -ne 1 ] ||
   [ "$(count_matches '^global @__cppgm_tls_guard__tls ' "$tls_lowir")" -ne 1 ] ||
   [ "$(count_matches '^function @__cppgm_tls_init__tls\(\)' "$tls_lowir")" -ne 1 ] ||
   [ "$(count_matches 'call void @Thread__Thread\(' "$tls_lowir")" -ne 1 ]; then
  echo "TLS definition/redeclaration emitted duplicate or missing guarded initialization" >&2
  exit 1
fi
"$lowir2cy86" -o "$tls_cy86" "$tls_lowir"
"$cy86" -o "$tls_program" "$tls_cy86"
"$tls_program"
