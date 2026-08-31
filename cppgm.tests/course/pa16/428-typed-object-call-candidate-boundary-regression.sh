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

source=$build_dir/typed-object-call.cpp
types=$build_dir/typed-object-call.types
lowir=$build_dir/typed-object-call.lowir
cy86_source=$build_dir/typed-object-call.cy86
program=$build_dir/typed-object-call.program

append_line()
{
  printf '%s\n' "$1" >>"$source"
}

: >"$source"
i=0
while [ "$i" -lt 64 ]; do
  tag=$(printf 'tag%02d' "$i")
  append_line "struct $tag {};"
  i=$((i + 1))
done

append_line 'struct base {'
i=0
while [ "$i" -lt 64 ]; do
  tag=$(printf 'tag%02d' "$i")
  if [ "$i" -eq 63 ]; then
    value=163
  else
    value=$i
  fi
  append_line "  static long choose($tag*) { return $value; }"
  i=$((i + 1))
done
append_line '};'
append_line 'struct derived : base {'
# This declaration is intentionally late relative to the large base overload
# set.  The using-import follows it so PA11 can publish the differing-return
# declarations and PA12 can suppress only base::choose(tag63*).
append_line '  static char choose(tag63*) { return 63; }'
append_line '  using base::choose;'
append_line '};'

append_line 'struct Box { int x; };'
append_line 'struct Holder {'
append_line '  Box& ref;'
append_line '  Holder(Box& box) : ref(box) {}'
append_line '};'

append_line 'int assigned = 0;'
append_line 'struct Iter {'
append_line '  Iter& operator=(int value) { assigned = value; return *this; }'
append_line '  Iter& operator*() { return *this; }'
append_line '};'

append_line 'int main() {'
append_line '  Box box;'
append_line '  box.x = 7;'
append_line '  Holder holder = Holder(box);'
append_line '  holder.ref.x = 9;'
append_line '  Iter it;'
append_line '  *it = 17;'
append_line '  int scalar = 1;'
append_line '  scalar = 2;'
append_line '  tag63 direct_tag_object;'
append_line '  tag62 imported_tag_object;'
append_line '  tag63* direct_tag = &direct_tag_object;'
append_line '  tag62* imported_tag = &imported_tag_object;'
append_line '  int first = 3;'
append_line '  int second = 4;'
append_line '  int* pointer = &first;'
append_line '  pointer = &second;'
append_line '  return derived::choose(direct_tag) == 63 &&'
append_line '    derived::choose(imported_tag) == 62 &&'
append_line '    box.x == 9 && holder.ref.x == 9 && assigned == 17 &&'
append_line '    scalar == 2 && *pointer == 4 ? 0 : 1;'
append_line '}'

"$app" --emit-types -o "$types" "$source"
base_candidate_count=$(awk '
  /^    scope class base$/ { inside=1; next }
  inside && /^    scope / { exit }
  inside && /^      function choose/ { count++ }
  END { print count + 0 }
' "$types")
derived_candidate_count=$(awk '
  /^    scope class derived$/ { inside=1; next }
  inside && /^    scope / { exit }
  inside && /^      function choose/ { count++ }
  END { print count + 0 }
' "$types")
if [ "$base_candidate_count" -ne 64 ] ||
   [ "$derived_candidate_count" -ne 65 ]; then
  echo "typed object-call type publication exposed $base_candidate_count base and $derived_candidate_count derived candidates, expected 64/65" >&2
  exit 1
fi

"$app" --emit-lowir -O0 -o "$lowir" "$source"
if ! rg -Fq 'function @derived__choose' "$lowir" ||
   ! rg -Fq 'function @base__choose' "$lowir"; then
  echo "typed object-call stress source did not emit both derived and base calls" >&2
  exit 1
fi
"$lowir2cy86" -o "$cy86_source" "$lowir"
"$cy86" -o "$program" "$cy86_source"
"$program"

rejected_source=$build_dir/overloaded-assignment-rejection.cpp
rejected_lowir=$build_dir/overloaded-assignment-rejection.lowir
printf '%s\n' \
  'struct Reject {' \
  '  Reject& operator=(int value) { return *this; }' \
  '};' \
  'int main() {' \
  '  Reject value;' \
  '  int* pointer = 0;' \
  '  value = pointer;' \
  '  return 0;' \
  '}' >"$rejected_source"
if "$app" --emit-lowir -O0 -o "$rejected_lowir" "$rejected_source" \
    >"$rejected_lowir.stdout" 2>"$rejected_lowir.stderr"; then
  rejected_status=0
else
  rejected_status=$?
fi
if [ "$rejected_status" -ne 1 ]; then
  echo "non-convertible overloaded assignment returned status $rejected_status, expected 1" >&2
  exit 1
fi

echo "428 typed object-call candidate boundary regression: PASS"
