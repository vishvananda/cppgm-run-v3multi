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

source=$build_dir/typed-user-defined-literal.cpp
lowir=$build_dir/typed-user-defined-literal.lowir
cy86_source=$build_dir/typed-user-defined-literal.cy86
program=$build_dir/typed-user-defined-literal.program

append_line()
{
  printf '%s\n' "$1" >>"$source"
}

: >"$source"
append_line 'typedef unsigned long size_t;'
append_line 'namespace cooked {'
append_line 'int operator ""_pick(const char* text, size_t size);'
append_line 'int operator ""_other(const char* text, size_t size);'
append_line 'int operator ""_pick(const char* text, size_t size) {'
append_line '  return size == 5 && text[0] == '\''h'\'' && text[4] == '\''o'\'' ? 0 : 11;'
append_line '}'
append_line 'int operator ""_other(const char* text, size_t size) {'
append_line '  return size == 5 && text[0] == '\''w'\'' && text[4] == '\''d'\'' ? 0 : 13;'
append_line '}'

i=0
while [ "$i" -lt 64 ]; do
  suffix=$(printf '_slot%02d' "$i")
  append_line "int operator \"\"$suffix(const char* text, size_t size);"
  i=$((i + 1))
done

i=0
while [ "$i" -lt 64 ]; do
  suffix=$(printf '_slot%02d' "$i")
  if [ "$i" -eq 63 ]; then
    append_line "int operator \"\"$suffix(const char* text, size_t size) {"
    append_line "  return size == 5 && text[0] == 's' && text[4] == 'e' ? 0 : 17;"
  else
    append_line "int operator \"\"$suffix(const char*, size_t) { return 19; }"
  fi
  if [ "$i" -eq 63 ]; then
    append_line '}'
  fi
  i=$((i + 1))
done

append_line '}'
append_line 'int invoke() {'
append_line '  using namespace cooked;'
append_line '  return "hello"_pick + "world"_other + "scale"_slot63;'
append_line '}'
append_line 'int main() { return invoke(); }'

"$app" --emit-lowir -O0 -o "$lowir" "$source"
if ! rg -Fq 'li5_pick' "$lowir" ||
   ! rg -Fq 'li6_other' "$lowir" ||
   ! rg -Fq 'li7_slot63' "$lowir"; then
  echo "typed UDL ABI symbols did not retain all selected suffixes" >&2
  exit 1
fi
scale_symbols=$(rg -o 'li7_slot[0-9][0-9]' "$lowir" | sort -u | wc -l)
if [ "$scale_symbols" -ne 64 ]; then
  echo "typed UDL scale emitted $scale_symbols unique suffix symbols, expected 64" >&2
  exit 1
fi
"$lowir2cy86" -o "$cy86_source" "$lowir"
"$cy86" -o "$program" "$cy86_source"
"$program"

negative_source=$build_dir/invalid-linkage-udl.cpp
negative_lowir=$build_dir/invalid-linkage-udl.lowir
printf '%s\n' \
  'extern "C"_bad int value;' \
  'int main() { return value; }' >"$negative_source"
if "$app" --emit-lowir -O0 -o "$negative_lowir" "$negative_source" \
    >"$negative_lowir.stdout" 2>"$negative_lowir.stderr"; then
  negative_status=0
else
  negative_status=$?
fi
if [ "$negative_status" -ne 1 ]; then
  echo "extern UDL linkage token returned status $negative_status, expected 1" >&2
  exit 1
fi

echo "427 typed user-defined-literal boundary regression: PASS"
