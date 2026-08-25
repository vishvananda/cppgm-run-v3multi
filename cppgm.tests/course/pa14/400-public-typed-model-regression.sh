#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$test_dir/../../.." && pwd)
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM

${CXX:-g++} -std=c++11 -Wall -Wextra -Werror \
  -I"$repo_root/dev/src" \
  -Dmain=abimangle_driver_main -c \
  "$repo_root/dev/abimangle.cpp" \
  -o "$build_dir/abimangle.o"
${CXX:-g++} -std=c++11 -Wall -Wextra -Werror \
  -I"$repo_root/dev/src" \
  "$test_dir/400-public-typed-model-regression.cpp" \
  "$repo_root/dev/src/abi_mangle.cpp" \
  "$build_dir/abimangle.o" \
  -o "$build_dir/typed-model-regression"
"$build_dir/typed-model-regression"
