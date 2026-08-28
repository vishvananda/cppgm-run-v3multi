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

run_valid() {
  name=$1
  source=$build_dir/$name.cpp
  shift
  printf '%s\n' "$@" >"$source"
  "$app" --emit-semantics -o "$build_dir/$name.semantics" "$source"
}

expect_rejected() {
  name=$1
  source=$build_dir/$name.cpp
  shift
  printf '%s\n' "$@" >"$source"
  if "$app" --emit-semantics -o "$build_dir/$name.semantics" "$source" \
      >"$build_dir/$name.stdout" 2>"$build_dir/$name.stderr"; then
    echo "expected PA16 rejection: $name" >&2
    return 1
  fi
}

run_valid qualified-member \
  'struct Box {' \
  '  int value;' \
  '  auto get(int delta) const noexcept -> int;' \
  '};' \
  'auto Box::get(int delta) const noexcept -> int {' \
  '  return value + delta;' \
  '}' \
  'int main() {' \
  '  Box box;' \
  '  box.value = 4;' \
  '  return box.get(3) == 7 ? 0 : 1;' \
  '}'

run_valid static-auto-function \
  'static auto static_value() -> int { return 7; }'

run_valid nested-function-pointer \
  'auto (*callback)() -> int;' \
  'int main() { return callback == 0 ? 0 : 1; }'

expect_rejected mixed-auto-base \
  'auto int f() -> int { return 0; }'
expect_rejected mixed-auto-named-base \
  'struct S {};' \
  'auto S f() -> int { return 0; }'
expect_rejected duplicate-auto \
  'auto auto f() -> int { return 0; }'
expect_rejected cv-qualified-auto \
  'const auto f() -> int { return 0; }'
expect_rejected typedef-auto \
  'typedef auto f() -> int { return 0; }'
expect_rejected auto-in-type-id \
  'auto f() -> auto { return 0; }'
expect_rejected missing-auto-placeholder \
  'int f() -> int { return 0; }'
expect_rejected trailing-on-object \
  'int object -> int;'
expect_rejected suffix-after-trailing-return \
  'auto f() -> int noexcept { return 0; }'
expect_rejected invalid-qualifier-order \
  'auto f() noexcept const -> int { return 0; }'
expect_rejected unsupported-ref-qualifier \
  'auto f() & -> int { return 0; }'
expect_rejected auto-parameter \
  'int f(auto value) -> int { return value; }'
expect_rejected auto-without-trailing-return \
  'auto f() { return 0; }'
expect_rejected auto-simple-declaration \
  'auto value;'
