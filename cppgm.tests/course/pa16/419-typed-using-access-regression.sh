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

expect_success()
{
  label=$1
  source=$2
  output=$build_dir/$label.lowir
  if ! "$app" --emit-lowir -O0 -o "$output" "$source" \
      >"$output.stdout" 2>"$output.stderr"; then
    echo "$label was rejected" >&2
    sed -n '1,20p' "$output.stderr" >&2
    exit 1
  fi
}

expect_failure()
{
  label=$1
  source=$2
  output=$build_dir/$label.lowir
  if "$app" --emit-lowir -O0 -o "$output" "$source" \
      >"$output.stdout" 2>"$output.stderr"; then
    status=0
  else
    status=$?
  fi
  if [ "$status" -ne 1 ]; then
    echo "$label returned status $status, expected 1" >&2
    sed -n '1,20p' "$output.stderr" >&2
    exit 1
  fi
}

public_protected_source=$build_dir/public-protected-reexposure.cpp
printf '%s\n' \
  'class Base {' \
  'public:' \
  '  int public_value;' \
  'protected:' \
  '  int protected_value;' \
  '};' \
  'class Derived : private Base {' \
  'public:' \
  '  using Base::public_value;' \
  '  using Base::protected_value;' \
  '};' \
  'int main() {' \
  '  Derived value;' \
  '  value.public_value = 1;' \
  '  value.protected_value = 2;' \
  '  return value.public_value + value.protected_value == 3 ? 0 : 1;' \
  '}' >"$public_protected_source"
expect_success public-protected-reexposure "$public_protected_source"

private_view_inside_source=$build_dir/private-view-inside.cpp
printf '%s\n' \
  'class Base { public: int value; };' \
  'class Derived : public Base {' \
  'private:' \
  '  using Base::value;' \
  'public:' \
  '  int read() { value = 1; return value; }' \
  '};' \
  'int main() {' \
  '  Derived value;' \
  '  return value.read() == 1 ? 0 : 1;' \
  '}' >"$private_view_inside_source"
expect_success private-view-inside "$private_view_inside_source"

private_view_external_source=$build_dir/private-view-external.cpp
printf '%s\n' \
  'class Base { public: int value; };' \
  'class Derived : public Base {' \
  'private:' \
  '  using Base::value;' \
  '};' \
  'int main() { Derived value; return value.value; }' \
  >"$private_view_external_source"
expect_failure private-view-external "$private_view_external_source"

protected_view_inside_source=$build_dir/protected-view-inside.cpp
printf '%s\n' \
  'class Base {' \
  'protected:' \
  '  int value;' \
  '};' \
  'class Derived : public Base {' \
  'protected:' \
  '  using Base::value;' \
  '};' \
  'class Further : public Derived {' \
  'public:' \
  '  int read(Further & object) {' \
  '    object.value = 4;' \
  '    return object.value;' \
  '  }' \
  '};' \
  'int main() {' \
  '  Further value;' \
  '  return value.read(value) == 4 ? 0 : 1;' \
  '}' >"$protected_view_inside_source"
expect_success protected-view-inside "$protected_view_inside_source"

protected_view_base_source=$build_dir/protected-view-base-object.cpp
printf '%s\n' \
  'class Base {' \
  'protected:' \
  '  int value;' \
  '};' \
  'class Derived : public Base {' \
  'protected:' \
  '  using Base::value;' \
  '};' \
  'class Further : public Derived {' \
  'public:' \
  '  int reject(Derived & object) { return object.value; }' \
  '};' \
  'int main() { Further value; return value.reject(value); }' \
  >"$protected_view_base_source"
expect_failure protected-view-base-object "$protected_view_base_source"

protected_view_actual_base_source=$build_dir/protected-view-actual-base-object.cpp
printf '%s\n' \
  'class Base {' \
  'protected:' \
  '  int value;' \
  '};' \
  'class Derived : public Base {' \
  'protected:' \
  '  using Base::value;' \
  '};' \
  'class Further : public Derived {' \
  'public:' \
  '  int reject(Base & object) { return object.value; }' \
  '};' \
  'int main() { Further value; return value.reject(value); }' \
  >"$protected_view_actual_base_source"
expect_failure protected-view-actual-base "$protected_view_actual_base_source"

protected_view_unrelated_source=$build_dir/protected-view-unrelated.cpp
printf '%s\n' \
  'class Base {' \
  'protected:' \
  '  int value;' \
  '};' \
  'class Derived : public Base {' \
  'protected:' \
  '  using Base::value;' \
  '};' \
  'class Further : public Derived {};' \
  'class Unrelated {' \
  'public:' \
  '  int reject(Further & object) { return object.value; }' \
  '};' \
  'int main() { return 0; }' \
  >"$protected_view_unrelated_source"
expect_failure protected-view-unrelated "$protected_view_unrelated_source"

private_source=$build_dir/private-reexposure.cpp
printf '%s\n' \
  'class Base {' \
  'private:' \
  '  int secret;' \
  '};' \
  'class Derived : public Base {' \
  'public:' \
  '  using Base::secret;' \
  '};' \
  'int main() { return 0; }' >"$private_source"
expect_failure private-reexposure "$private_source"

friend_private_source=$build_dir/friend-private-reexposure.cpp
printf '%s\n' \
  'class Derived;' \
  'class Base {' \
  'private:' \
  '  int secret;' \
  '  friend class Derived;' \
  '};' \
  'class Derived : public Base {' \
  'public:' \
  '  using Base::secret;' \
  '};' \
  'int main() {' \
  '  Derived value;' \
  '  value.secret = 3;' \
  '  return value.secret == 3 ? 0 : 1;' \
  '}' >"$friend_private_source"
expect_success friend-private-reexposure "$friend_private_source"

nontransitive_source=$build_dir/nontransitive-private-reexposure.cpp
printf '%s\n' \
  'class Mid;' \
  'class Base {' \
  'private:' \
  '  int secret;' \
  '  friend class Mid;' \
  '};' \
  'class Mid : public Base {' \
  'public:' \
  '  using Base::secret;' \
  '};' \
  'class Further : public Mid {' \
  'public:' \
  '  using Mid::secret;' \
  '};' \
  'int main() { return 0; }' >"$nontransitive_source"
expect_failure nontransitive-private-reexposure "$nontransitive_source"

friend_protected_source=$build_dir/friend-protected-object.cpp
printf '%s\n' \
  'class Reader;' \
  'class Base {' \
  'protected:' \
  '  int value;' \
  '};' \
  'class Derived : public Base {' \
  '  friend class Reader;' \
  '};' \
  'class Further : public Derived {};' \
  'class Reader {' \
  'public:' \
  '  static int read(Derived & value) { return value.value; }' \
  '  static int read_further(Further & value) { return value.value; }' \
  '};' \
  'int main() {' \
  '  Derived derived;' \
  '  Further further;' \
  '  return Reader::read(derived) + Reader::read_further(further);' \
  '}' >"$friend_protected_source"
expect_success friend-protected-object "$friend_protected_source"

friend_base_object_source=$build_dir/friend-protected-base-object.cpp
printf '%s\n' \
  'class Reader;' \
  'class Base {' \
  'protected:' \
  '  int value;' \
  '};' \
  'class Derived : public Base {' \
  '  friend class Reader;' \
  '};' \
  'class Reader {' \
  'public:' \
  '  static int reject(Base & value) { return value.value; }' \
  '};' \
  'int main() { Base value; return Reader::reject(value); }' \
  >"$friend_base_object_source"
expect_failure friend-protected-base-object "$friend_base_object_source"

public_protected_type_source=$build_dir/public-protected-type.cpp
printf '%s\n' \
  'class Base { protected: typedef int Type; };' \
  'class Derived : public Base {' \
  'public:' \
  '  using Base::Type;' \
  '};' \
  'int main() { Derived::Type value = 0; return value; }' \
  >"$public_protected_type_source"
expect_success public-protected-type "$public_protected_type_source"

private_type_view_source=$build_dir/private-type-view.cpp
printf '%s\n' \
  'class Base { public: typedef int Type; };' \
  'class Derived : public Base {' \
  'private:' \
  '  using Base::Type;' \
  '};' \
  'int main() { Derived::Type value = 0; return value; }' \
  >"$private_type_view_source"
expect_failure private-type-view "$private_type_view_source"

protected_type_view_source=$build_dir/protected-type-view.cpp
printf '%s\n' \
  'class Base { public: typedef int Type; };' \
  'class Derived : public Base {' \
  'protected:' \
  '  using Base::Type;' \
  '};' \
  'int main() { Derived::Type value = 0; return value; }' \
  >"$protected_type_view_source"
expect_failure protected-type-view "$protected_type_view_source"

private_source_type=$build_dir/private-source-type.cpp
printf '%s\n' \
  'class Base { private: typedef int Type; };' \
  'class Derived : public Base {' \
  'public:' \
  '  using Base::Type;' \
  '};' \
  'int main() { return 0; }' >"$private_source_type"
expect_failure private-source-type "$private_source_type"

friend_private_type_source=$build_dir/friend-private-type.cpp
printf '%s\n' \
  'class Derived;' \
  'class Base { private: typedef int Type; friend class Derived; };' \
  'class Derived : public Base {' \
  'public:' \
  '  using Base::Type;' \
  '};' \
  'int main() { Derived::Type value = 0; return value; }' \
  >"$friend_private_type_source"
expect_success friend-private-type "$friend_private_type_source"

namespace_class_using_source=$build_dir/namespace-class-using.cpp
printf '%s\n' \
  'class Base { public: typedef int Type; static int value; };' \
  'int Base::value = 0;' \
  'using Base::Type;' \
  'using Base::value;' \
  'int main() { Type result = value; return result; }' >"$namespace_class_using_source"
expect_success namespace-class-using "$namespace_class_using_source"

namespace_using_source=$build_dir/namespace-using.cpp
printf '%s\n' \
  'namespace Source { typedef int Type; int value = 1; }' \
  'using Source::Type;' \
  'using Source::value;' \
  'int main() { Type result = value; return result == 1 ? 0 : 1; }' \
  >"$namespace_using_source"
expect_success namespace-using "$namespace_using_source"

echo "419 typed using/friend access regression: PASS"
