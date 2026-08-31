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

# Static and non-static declarations are separate lookup categories.  Keep
# both sides of this using-view alive: the object call must select the direct
# non-static member while the qualified call must select the imported static
# member.
append_line 'struct category_tag {};'
append_line 'struct category_base {'
append_line '  static int choose_category(category_tag*) { return 71; }'
append_line '};'
append_line 'struct category_derived : category_base {'
append_line '  int choose_category(category_tag*) { return 72; }'
append_line '  using category_base::choose_category;'
append_line '};'

# cv qualifiers are part of the typed signature key.  The const base overload
# must remain usable through a const derived view even when the derived class
# declares the same parameter list without const.
append_line 'struct qualifier_base {'
append_line '  int choose_qual(tag00*) const { return 81; }'
append_line '};'
append_line 'struct qualifier_derived : qualifier_base {'
append_line '  int choose_qual(tag00*) { return 83; }'
append_line '  using qualifier_base::choose_qual;'
append_line '};'

append_line 'struct Box { int x; };'
append_line 'struct Holder {'
append_line '  Box& ref;'
append_line '  explicit Holder(Box& box) : ref(box) {}'
append_line '};'
append_line 'struct PairHolder {'
append_line '  Box& first;'
append_line '  Box& second;'
append_line '  explicit PairHolder(Box& first_box, Box& second_box) : first(first_box), second(second_box) {}'
append_line '};'

append_line 'int assigned = 0;'
append_line 'struct Iter {'
append_line '  Iter& operator=(int value) { assigned = value; return *this; }'
append_line '  Iter& operator*() { return *this; }'
append_line '};'

append_line 'int main() {'
append_line '  Box box;'
append_line '  box.x = 7;'
append_line '  Box second_box;'
append_line '  second_box.x = 8;'
append_line '  Holder holder = Holder(box);'
append_line '  Holder wrapped_holder = (Holder(box));'
append_line '  PairHolder pair = PairHolder(box, second_box);'
append_line '  holder.ref.x = 9;'
append_line '  wrapped_holder.ref.x = 10;'
append_line '  pair.second.x = 12;'
append_line '  Iter it;'
append_line '  Iter* assignment_result = &(*it = 17);'
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
append_line '  category_tag category_argument;'
append_line '  category_derived category_object;'
append_line '  int category_member = category_object.choose_category(&category_argument);'
append_line '  int category_static = category_derived::choose_category(&category_argument);'
append_line '  qualifier_derived qualifier_object;'
append_line '  const qualifier_derived& const_qualifier = qualifier_object;'
append_line '  tag00 qualifier_cv_tag_object;'
append_line '  int cv_direct = qualifier_object.choose_qual(&qualifier_cv_tag_object);'
append_line '  int cv_imported = const_qualifier.choose_qual(&qualifier_cv_tag_object);'
append_line '  return derived::choose(direct_tag) == 63 &&'
append_line '    derived::choose(imported_tag) == 62 &&'
append_line '    box.x == 10 && holder.ref.x == 10 && wrapped_holder.ref.x == 10 &&'
append_line '    pair.first.x == 10 && pair.second.x == 12 && assigned == 17 &&'
append_line '    assignment_result == &it && scalar == 2 && *pointer == 4 &&'
append_line '    category_member == 72 && category_static == 71 &&'
append_line '    cv_direct == 83 && cv_imported == 81 ? 0 : 1;'
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
holder_constructor_calls=$(rg -F -c 'call void @Holder__Holder' "$lowir" || true)
pair_constructor_calls=$(rg -F -c 'call void @PairHolder__PairHolder' "$lowir" || true)
if [ "$holder_constructor_calls" -ne 2 ] ||
   [ "$pair_constructor_calls" -ne 1 ]; then
  echo "functional construction emitted $holder_constructor_calls Holder and $pair_constructor_calls PairHolder constructor calls, expected 2/1" >&2
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
