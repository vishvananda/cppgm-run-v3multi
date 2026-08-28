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

state_free=$build_dir/state-free-chain.cpp
printf '%s\n' \
  'struct Base { int base() { return 1; } };' \
  'struct Middle : Base { int middle() { return base(); } };' \
  'struct Derived : Middle { int run() { return middle(); } };' \
  'struct Unused { int state = 9; };' \
  'int main() { Derived value; return value.run() == 1 ? 0 : 1; }' \
  >"$state_free"

state_free_output=$build_dir/state-free-chain.lowir
"$app" --emit-lowir -O0 -o "$state_free_output" "$state_free"
if rg -q -e '@Base__Base|@Middle__Middle|@Derived__Derived' \
    "$state_free_output"; then
  echo "state-free implicit construction emitted a synthetic constructor" >&2
  exit 1
fi
if rg -q -e 'function @Unused__Unused|@Unused__Unused' \
    "$state_free_output"; then
  echo "unused DMI class emitted a synthetic constructor" >&2
  exit 1
fi

empty_default=$build_dir/empty-default.cpp
printf '%s\n' \
  'struct Empty { };' \
  'int main() { Empty value; return 0; }' \
  >"$empty_default"

empty_default_output=$build_dir/empty-default.lowir
if "$app" --emit-lowir -O0 -o "$empty_default_output" "$empty_default" \
    >"$empty_default_output.stdout" 2>"$empty_default_output.stderr"; then
  status=0
else
  status=$?
fi
if [ "$status" -ne 0 ] ||
   rg -q -e 'function @Empty__Empty|call void @Empty__Empty' \
     "$empty_default_output"; then
  echo "default-initialized empty local emitted a useless constructor helper" >&2
  exit 1
fi

empty_value=$build_dir/empty-value-subobject.cpp
printf '%s\n' \
  'struct Empty { };' \
  'struct Holder { Empty value{}; };' \
  'int main() { Holder value{}; return 0; }' \
  >"$empty_value"

empty_value_output=$build_dir/empty-value-subobject.lowir
if "$app" --emit-lowir -O0 -o "$empty_value_output" "$empty_value" \
    >"$empty_value_output.stdout" 2>"$empty_value_output.stderr"; then
  status=0
else
  status=$?
fi
if [ "$status" -ne 0 ] ||
   rg -q -e 'function @Empty__Empty|call void @Empty__Empty' \
     "$empty_value_output" ||
   ! rg -q 'store [^ ]+ 0,' "$empty_value_output"; then
  echo "value-initialized empty subobject lost zeroing or emitted a helper" >&2
  exit 1
fi

empty_aggregate=$build_dir/empty-aggregate-dmi.cpp
printf '%s\n' \
  'struct Empty { };' \
  'struct Aggregate { Empty value{}; int initialized = 5; };' \
  'int main() { Aggregate value{}; return value.initialized; }' \
  >"$empty_aggregate"

empty_aggregate_output=$build_dir/empty-aggregate-dmi.lowir
if "$app" --emit-lowir -O0 -o "$empty_aggregate_output" "$empty_aggregate" \
    >"$empty_aggregate_output.stdout" 2>"$empty_aggregate_output.stderr"; then
  status=0
else
  status=$?
fi
empty_aggregate_body=$build_dir/empty-aggregate-dmi.main
empty_main_body=$build_dir/empty-aggregate-dmi.main-function
sed -n '/^function @Aggregate__Aggregate/,/^}/p' "$empty_aggregate_output" >"$empty_aggregate_body"
sed -n '/^function @main/,/^}/p' "$empty_aggregate_output" >"$empty_main_body"
empty_zero_line=$(rg -n -m1 'store [^ ]+ 0,' "$empty_aggregate_body" | cut -d: -f1 || true)
empty_dmi_line=$(rg -n -m1 'store [^ ]+ 5,' "$empty_aggregate_body" | cut -d: -f1 || true)
if [ "$status" -ne 0 ] ||
   rg -q -e 'function @Empty__Empty|call void @Empty__Empty' \
      "$empty_aggregate_output" ||
   [ "$(rg -c '^function @main' "$empty_aggregate_output" || true)" -ne 1 ] ||
   [ "$(rg -c 'call void @Aggregate__Aggregate' "$empty_main_body" || true)" -ne 1 ] ||
   [ "$(rg -c '^function @Aggregate__Aggregate' "$empty_aggregate_output" || true)" -ne 1 ] ||
   [ -z "$empty_zero_line" ] || [ -z "$empty_dmi_line" ] ||
   [ "$empty_zero_line" -ge "$empty_dmi_line" ]; then
  echo "non-aggregate DMI constructor was not uniquely reached with zero/DMI ordering" >&2
  exit 1
fi

middle_body=$build_dir/middle.body
derived_body=$build_dir/derived.body
sed -n '/^function @Middle__middle/,/^}/p' "$state_free_output" >"$middle_body"
sed -n '/^function @Derived__run/,/^}/p' "$state_free_output" >"$derived_body"
if [ "$(rg -c 'projection=base_subobject' "$middle_body" || true)" -ne 1 ] ||
   [ "$(rg -c 'projection=base_subobject' "$derived_body" || true)" -ne 1 ] ||
   ! rg -Fq 'call i32 @Base__base' "$middle_body" ||
   ! rg -Fq 'call i32 @Middle__middle' "$derived_body"; then
  echo "state-free multi-level base demand did not retain ordered projections" >&2
  exit 1
fi

runtime_required=$build_dir/runtime-required-base.cpp
printf '%s\n' \
  'struct Base { int state = 7; };' \
  'struct Derived : Base {}; ' \
  'int main() { Derived value; return 0; }' \
  >"$runtime_required"

runtime_output=$build_dir/runtime-required-base.lowir
if "$app" --emit-lowir -O0 -o "$runtime_output" "$runtime_required" \
    >"$runtime_output.stdout" 2>"$runtime_output.stderr"; then
  status=0
else
  status=$?
fi
if [ "$status" -ne 0 ] ||
   ! rg -Fq 'function @Derived__Derived' "$runtime_output" ||
   [ "$(rg -c '^function @Derived__Derived' "$runtime_output" || true)" -ne 1 ] ||
   [ "$(rg -c 'call void @Base__Base' "$runtime_output" || true)" -ne 1 ] ||
   ! rg -Fq 'call void @Base__Base' "$runtime_output" ||
   ! rg -Fq 'projection=base_subobject' "$runtime_output" ||
   [ "$(rg -c '^function @Base__Base' "$runtime_output" || true)" -ne 1 ] ||
   ! rg -Fq 'store i32 7' "$runtime_output"; then
  echo "runtime-requiring implicit construction returned status $status" >&2
  exit 1
fi

member_required=$build_dir/runtime-required-member.cpp
printf '%s\n' \
  'struct State { int state = 7; };' \
  'struct Holder { State state; };' \
  'int main() { Holder value; return 0; }' \
  >"$member_required"

member_output=$build_dir/runtime-required-member.lowir
if "$app" --emit-lowir -O0 -o "$member_output" "$member_required" \
    >"$member_output.stdout" 2>"$member_output.stderr"; then
  status=0
else
  status=$?
fi
if [ "$status" -ne 0 ] ||
   ! rg -Fq 'function @Holder__Holder' "$member_output" ||
   [ "$(rg -c '^function @Holder__Holder' "$member_output" || true)" -ne 1 ] ||
   [ "$(rg -c 'call void @State__State' "$member_output" || true)" -ne 1 ] ||
   ! rg -Fq 'call void @State__State' "$member_output" ||
   ! rg -Fq 'projection=field' "$member_output" ||
   [ "$(rg -c '^function @State__State' "$member_output" || true)" -ne 1 ] ||
   ! rg -Fq 'store i32 7' "$member_output"; then
  echo "runtime-requiring member construction returned status $status" >&2
  exit 1
fi

value_initialized=$build_dir/value-initialized-member.cpp
printf '%s\n' \
  'struct State { int untouched; int initialized = 5; };' \
  'struct Holder { State state; Holder() : state() {} };' \
  'int main() { Holder value; return 0; }' \
  >"$value_initialized"

value_output=$build_dir/value-initialized-member.lowir
if "$app" --emit-lowir -O0 -o "$value_output" "$value_initialized" \
    >"$value_output.stdout" 2>"$value_output.stderr"; then
  status=0
else
  status=$?
fi
value_holder_body=$build_dir/value-initialized-member.holder
value_state_body=$build_dir/value-initialized-member.state
sed -n '/^function @Holder__Holder/,/^}/p' "$value_output" >"$value_holder_body"
sed -n '/^function @State__State/,/^}/p' "$value_output" >"$value_state_body"
zero_line=$(rg -n -m1 'store [^ ]+ 0,' "$value_holder_body" | cut -d: -f1 || true)
state_call_line=$(rg -n -m1 'call void @State__State' "$value_holder_body" | cut -d: -f1 || true)
dmi_line=$(rg -n -m1 'store [^ ]+ 5,' "$value_state_body" | cut -d: -f1 || true)
if [ "$status" -ne 0 ] ||
   ! rg -Fq 'function @Holder__Holder' "$value_output" ||
   [ "$(rg -c '^function @Holder__Holder' "$value_output" || true)" -ne 1 ] ||
   [ "$(rg -c 'call void @State__State' "$value_output" || true)" -ne 1 ] ||
   [ -z "$zero_line" ] || [ -z "$state_call_line" ] ||
   [ -z "$dmi_line" ] || [ "$zero_line" -ge "$state_call_line" ] ||
   [ "$(rg -c 'store [^ ]+ 5,' "$value_state_body" || true)" -ne 1 ]; then
  echo "class-subobject value initialization did not zero before the constructor call" >&2
  exit 1
fi

user_provided=$build_dir/user-provided-value.cpp
printf '%s\n' \
  'struct User { int untouched; User() {} };' \
  'struct Holder { User value{}; };' \
  'int main() { Holder value; return 0; }' \
  >"$user_provided"

user_output=$build_dir/user-provided-value.lowir
if "$app" --emit-lowir -O0 -o "$user_output" "$user_provided" \
    >"$user_output.stdout" 2>"$user_output.stderr"; then
  status=0
else
  status=$?
fi
user_holder_body=$build_dir/user-provided-value.holder
sed -n '/^function @Holder__Holder/,/^}/p' "$user_output" >"$user_holder_body"
if [ "$status" -ne 0 ] ||
   [ "$(rg -c '^function @Holder__Holder' "$user_output" || true)" -ne 1 ] ||
   [ "$(rg -c 'call void @User__User' "$user_holder_body" || true)" -ne 1 ] ||
   rg -q 'store [^ ]+ 0,' "$user_holder_body"; then
  echo "user-provided value initialization was incorrectly zeroed" >&2
  exit 1
fi

aggregate_value=$build_dir/aggregate-value.cpp
printf '%s\n' \
  'struct AggregateValue { int untouched; int initialized = 5; };' \
  'struct AggregateHolder { AggregateValue member; int tail = 7; };' \
  'int main() { AggregateHolder value{}; return value.member.untouched + value.member.initialized + value.tail; }' \
  >"$aggregate_value"

aggregate_output=$build_dir/aggregate-value.lowir
if "$app" --emit-lowir -O0 -o "$aggregate_output" "$aggregate_value" \
    >"$aggregate_output.stdout" 2>"$aggregate_output.stderr"; then
  status=0
else
  status=$?
fi
aggregate_body=$build_dir/aggregate-value.main
aggregate_holder_body=$build_dir/aggregate-value.holder
aggregate_member_body=$build_dir/aggregate-value.member
sed -n '/^function @main/,/^}/p' "$aggregate_output" >"$aggregate_body"
sed -n '/^function @AggregateHolder__AggregateHolder/,/^}/p' \
  "$aggregate_output" >"$aggregate_holder_body"
sed -n '/^function @AggregateValue__AggregateValue/,/^}/p' \
  "$aggregate_output" >"$aggregate_member_body"
aggregate_zero_line=$(rg -n -m1 'store [^ ]+ 0,' "$aggregate_body" | cut -d: -f1 || true)
aggregate_member_dmi_line=$(rg -n -m1 'store [^ ]+ 5,' \
  "$aggregate_member_body" | cut -d: -f1 || true)
aggregate_member_call_line=$(rg -n -m1 'call void @AggregateValue__AggregateValue' \
  "$aggregate_holder_body" | cut -d: -f1 || true)
aggregate_tail_line=$(rg -n -m1 'store [^ ]+ 7,' "$aggregate_holder_body" | \
  cut -d: -f1 || true)
aggregate_holder_call_line=$(rg -n -m1 'call void @AggregateHolder__AggregateHolder' \
  "$aggregate_body" | cut -d: -f1 || true)
if [ "$status" -ne 0 ] || [ -z "$aggregate_zero_line" ] ||
   [ -z "$aggregate_member_dmi_line" ] ||
   [ -z "$aggregate_member_call_line" ] || [ -z "$aggregate_tail_line" ] ||
   [ -z "$aggregate_holder_call_line" ] ||
   [ "$(rg -c '^function @main' "$aggregate_output" || true)" -ne 1 ] ||
   [ "$(rg -c '^function @AggregateHolder__AggregateHolder' "$aggregate_output" || true)" -ne 1 ] ||
   [ "$(rg -c '^function @AggregateValue__AggregateValue' "$aggregate_output" || true)" -ne 1 ] ||
   [ "$(rg -c 'call void @AggregateHolder__AggregateHolder' "$aggregate_body" || true)" -ne 1 ] ||
   [ "$(rg -c 'call void @AggregateValue__AggregateValue' "$aggregate_holder_body" || true)" -ne 1 ] ||
   [ "$aggregate_zero_line" -ge "$aggregate_holder_call_line" ] ||
   [ "$aggregate_member_call_line" -ge "$aggregate_tail_line" ]; then
  echo "aggregate empty-brace initialization was not uniquely reached with DMI ordering" >&2
  exit 1
fi
