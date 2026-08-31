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
append_line 'namespace shadow_source {'
append_line 'int operator ""_fallback(const char* text, size_t size) {'
append_line '  return size == 5 && text[0] == '\''s'\'' && text[4] == '\''e'\'' ? 0 : 23;'
append_line '}'
append_line '}'
append_line 'namespace shadow_scope {'
append_line 'int operator ""_blocker(const char*, size_t) { return 29; }'
append_line 'using namespace shadow_source;'
append_line 'int shadow_invoke() { return "scale"_fallback; }'
append_line '}'
append_line 'int invoke() {'
append_line '  using namespace cooked;'
append_line '  return "hello"_pick + "world"_other + "scale"_slot63 +'
append_line '    shadow_scope::shadow_invoke();'
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

signature_source=$build_dir/signature-filter-udl.cpp
signature_lowir=$build_dir/signature-filter-udl.lowir
signature_cy86_source=$build_dir/signature-filter-udl.cy86
signature_program=$build_dir/signature-filter-udl.program
printf '%s\n' \
  'typedef unsigned long size_t;' \
  'namespace signature_filter {' \
  'int operator ""_shape(unsigned long long) { return 31; }' \
  'int operator ""_shape(const char* text, size_t size) {' \
  '  return size == 5 && text[0] == '\''s'\'' && text[4] == '\''e'\'' ? 0 : 37;' \
  '}' \
  'int shape_invoke() { return "shape"_shape; }' \
  '}' \
  'int main() { return signature_filter::shape_invoke(); }' >"$signature_source"
"$app" --emit-lowir -O0 -o "$signature_lowir" "$signature_source"
if ! rg -Fq 'li6_shape' "$signature_lowir"; then
  echo "typed UDL signature-filter ABI symbol was not emitted" >&2
  exit 1
fi
"$lowir2cy86" -o "$signature_cy86_source" "$signature_lowir"
"$cy86" -o "$signature_program" "$signature_cy86_source"
"$signature_program"

invalid_signature_source=$build_dir/invalid-signature-udl.cpp
invalid_signature_lowir=$build_dir/invalid-signature-udl.lowir
printf '%s\n' \
  'typedef unsigned long size_t;' \
  'int operator ""_bad(const char*, int) { return 0; }' \
  'int main() { return 0; }' >"$invalid_signature_source"
if "$app" --emit-lowir -O0 -o "$invalid_signature_lowir" \
    "$invalid_signature_source" >"$invalid_signature_lowir.stdout" \
    2>"$invalid_signature_lowir.stderr"; then
  invalid_signature_status=0
else
  invalid_signature_status=$?
fi
if [ "$invalid_signature_status" -ne 1 ]; then
  echo "invalid cooked literal-operator signature returned status $invalid_signature_status, expected 1" >&2
  exit 1
fi

invalid_arity_source=$build_dir/invalid-arity-udl.cpp
invalid_arity_lowir=$build_dir/invalid-arity-udl.lowir
printf '%s\n' \
  'typedef unsigned long size_t;' \
  'int operator ""_wrong(const char*, size_t, int) { return 0; }' \
  'int main() { return 0; }' >"$invalid_arity_source"
if "$app" --emit-lowir -O0 -o "$invalid_arity_lowir" \
    "$invalid_arity_source" >"$invalid_arity_lowir.stdout" \
    2>"$invalid_arity_lowir.stderr"; then
  invalid_arity_status=0
else
  invalid_arity_status=$?
fi
if [ "$invalid_arity_status" -ne 1 ]; then
  echo "invalid literal-operator arity returned status $invalid_arity_status, expected 1" >&2
  exit 1
fi

collision_source=$build_dir/operatorliteral-collision-udl.cpp
collision_lowir=$build_dir/operatorliteral-collision-udl.lowir
collision_cy86_source=$build_dir/operatorliteral-collision-udl.cy86
collision_program=$build_dir/operatorliteral-collision-udl.program
printf '%s\n' \
  'typedef unsigned long size_t;' \
  'namespace collision {' \
  'int operator ""_collision(const char*, size_t) { return 0; }' \
  'int operatorliteral = 0;' \
  'int invoke() { return operatorliteral + "value"_collision; }' \
  '}' \
  'int main() { return collision::invoke(); }' >"$collision_source"
"$app" --emit-lowir -O0 -o "$collision_lowir" "$collision_source"
"$lowir2cy86" -o "$collision_cy86_source" "$collision_lowir"
"$cy86" -o "$collision_program" "$collision_cy86_source"
"$collision_program"

negative_source=$build_dir/invalid-linkage-udl.cpp
negative_lowir=$build_dir/invalid-linkage-udl.lowir
printf '%s\n' \
  'typedef unsigned long size_t;' \
  'extern "C" int operator ""_link(const char*, size_t) { return 0; }' \
  'int main() { return 0; }' >"$negative_source"
if "$app" --emit-lowir -O0 -o "$negative_lowir" "$negative_source" \
    >"$negative_lowir.stdout" 2>"$negative_lowir.stderr"; then
  negative_status=0
else
  negative_status=$?
fi
if [ "$negative_status" -ne 1 ]; then
  echo "C-linkage literal operator returned status $negative_status, expected 1" >&2
  exit 1
fi

scope_class_source=$build_dir/class-scope-udl.cpp
scope_class_lowir=$build_dir/class-scope-udl.lowir
printf '%s\n' \
  'typedef unsigned long size_t;' \
  'struct invalid_scope {' \
  '  int operator ""_member(const char*, size_t) { return 0; }' \
  '};' \
  'int main() { return 0; }' >"$scope_class_source"
if "$app" --emit-lowir -O0 -o "$scope_class_lowir" "$scope_class_source" \
    >"$scope_class_lowir.stdout" 2>"$scope_class_lowir.stderr"; then
  scope_class_status=0
else
  scope_class_status=$?
fi
if [ "$scope_class_status" -ne 1 ]; then
  echo "class-scope literal operator returned status $scope_class_status, expected 1" >&2
  exit 1
fi

scope_function_source=$build_dir/function-scope-udl.cpp
scope_function_lowir=$build_dir/function-scope-udl.lowir
printf '%s\n' \
  'typedef unsigned long size_t;' \
  'int invalid_scope() {' \
  '  int operator ""_local(const char*, size_t);' \
  '  return 0;' \
  '}' \
  'int main() { return invalid_scope(); }' >"$scope_function_source"
if "$app" --emit-lowir -O0 -o "$scope_function_lowir" "$scope_function_source" \
    >"$scope_function_lowir.stdout" 2>"$scope_function_lowir.stderr"; then
  scope_function_status=0
else
  scope_function_status=$?
fi
if [ "$scope_function_status" -ne 1 ]; then
  echo "function-scope literal operator returned status $scope_function_status, expected 1" >&2
  exit 1
fi

echo "427 typed user-defined-literal boundary regression: PASS"
