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
  destination=$1
  body=$2
  printf '%s\n' \
    'struct Choice {' \
    '  int tag;' \
    '  explicit Choice(int) : tag(1) {}' \
    '  Choice(long) : tag(2) {}' \
    '};' \
    "$body" \
    >"$destination"
}

copy_source=$build_dir/copy.cpp
write_source "$copy_source" \
  'int main() { Choice copy = 7; return copy.tag == 2 ? 0 : 1; }'
copy_output=$build_dir/copy.lowir
"$app" --emit-lowir -O0 -o "$copy_output" "$copy_source"
if [ "$(rg -c '^function @Choice__Choice' "$copy_output" || true)" -ne 1 ] ||
   ! rg -q '^function @Choice__Choice\([^)]* : i64\)' "$copy_output" ||
   ! rg -q 'call void @Choice__Choice\([^\n]*\)' "$copy_output" ||
   rg -q '^function @Choice__Choice\([^)]* : i32\)' "$copy_output"; then
  echo "copy-initialization did not exclude the explicit exact-match constructor" >&2
  exit 1
fi

list_source=$build_dir/copy-list.cpp
write_source "$list_source" \
  'int main() { Choice list = {7}; return 0; }'
list_output=$build_dir/copy-list.lowir
if "$app" --emit-lowir -O0 -o "$list_output" "$list_source" \
    >"$list_output.stdout" 2>"$list_output.stderr"; then
  status=0
else
  status=$?
fi
if [ "$status" -ne 1 ]; then
  echo "copy-list initialization returned status $status, expected 1" >&2
  exit 1
fi

late_source=$build_dir/late-default.cpp
printf '%s\n' \
  'struct Late {' \
  '  int value;' \
  '  int make() { Late value; return value.value; }' \
  '  Late(int value = 7) : value(value) {}' \
  '};' \
  'int main() { Late value; return value.make() == 7 ? 0 : 1; }' \
  >"$late_source"
late_output=$build_dir/late-default.lowir
"$app" --emit-lowir -O0 -o "$late_output" "$late_source"
if ! rg -q 'call void @Late__Late\([^\n]*, 7\)' "$late_output"; then
  echo "in-class method did not see the later constructor default argument" >&2
  exit 1
fi
