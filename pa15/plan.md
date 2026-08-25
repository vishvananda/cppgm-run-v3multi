# PA15 Typed Address/Value Boundary Checkpoint

## 1. Stage Design

PA15 consumes PA12's typed `SemanticFactId`, `BindingId`, `ScopeId`, and
`TypeId` facts and produces the typed LowIR address/value boundary. PA12 owns
semantic identity, categories, conversions, linkage, and typed constant
initializer facts; PA15 does not parse semantic dumps, LowIR text, tests, or
reference output. The boundary is implemented in `pa15_lowering.cpp` and
`pa15_lowering_flow.cpp`, with their shared typed declaration in
`pa15_lowering.h`.

`LoweredValue` carries both semantic and physical LowIR type. One reusable
`lower_address` path produces storage identity, while `apply_conversions`
materializes a value only when required. This preserves lvalue/xvalue address
identity through assignment, comma, and conditional expressions. Global
binding identities are indexed before initializer lowering; declaration-only
extern objects remain `GlobalDeclaration` records. LowIR `IK_INDEX` and its
projection metadata are serialized by `lowir_model.cpp`; the driver accepts a
valid LowIR unit without a `main` definition because a checked-in PA15 unit
requires that form.

## 2. Failure Map

The clean turn-start baseline was **21/109 passing, 88 failing, all 109
covered**. Its authoritative log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The exact baseline failure inventory was:

```text
100-array-cv-rvalue-reference-overload
100-c-linkage-reference-declaration-metadata
100-condition-declaration-variable-rvalue
100-const-integral-lvalue-overload-category
100-enum-default-argument-constant-fold
100-extern-unknown-bound-array-reference
100-function-pointer-ref-call
100-global-function-pointer-argument-call
100-global-variable
100-scoped-enum-braced-assignment
100-scoped-enum-previous-enumerator-bitwise-or
100-sizeof-local-value-shadows-type-name
100-string-hex-escape-code-unit
100-subscript-sizeof
100-unary-logical-conditional
100-unary-plus-array-decay
100-unnamed-parameter-storage
100-using-directive-imported-value-function-body
200-address-of-local-const-integral-uses-storage
200-comma-expression-lvalue-address
200-comma-expression-xvalue-reference-return
200-compound-assignment-evaluates-lhs-once
200-conditional-array-decay-subscript
200-const-cast-pointer-const-drop
200-const-cast-reference-array-subscript
200-const-cast-reference-similar-pointer
200-const-ref-converted-float-argument
200-enum-class-scalar-lowering
200-extern-c-internal-header-const
200-extern-function-pointer-indirect-call
200-floating-compound-assign-integral-rhs
200-floating-condition-declaration-negative-zero
200-floating-logical-branch
200-floating-return-integral-conversion
200-for-init-assignment-expression
200-for-iteration-discards-void-comma-rhs
200-function-reference-static-cast-call
200-functional-reference-typedef-cast
200-generated-slot-name-collision
200-global-address-reinterpret-cast-initializer
200-global-array-bitwise-or-enum-init
200-global-array-conditional-cast-initializer
200-global-array-decay-compare
200-global-array-element-address-initializer
200-global-array-one-past-end-pointer
200-global-array-scalar-cast-init
200-global-array-static-const-byte-init
200-global-object-address-initializer
200-global-pointer-array-null-fill
200-global-pointer-array-nullptr-init
200-global-pointer-array-subscript-load
200-goto-case-block-entry-label
200-goto-case-block-label-after-statement
200-included-namespace-global-definition
200-inferred-local-array-bound
200-integral-multiply-compound-assignment
200-literal-logical-short-circuit-omits-unreachable-call
200-local-direct-init-array-subscript
200-local-function-type-typedef-reference
200-local-int-slot-width
200-local-lvalue-reference-alias-init
200-lvalue-conditional-address
200-lvalue-conditional-reference-return
200-namespace-default-argument-declaration-lookup
200-nested-conditional-array-decay
200-partial-local-array-zero-initialization
200-pointer-compound-assignment-scale
200-pointer-deref-byte-load
200-pointer-operator-array-decay
200-postfix-incdec-evaluates-lhs-once
200-prefix-incdec-lvalue-address
200-prefix-pointer-decrement-reference-argument
200-qualified-namespace-overload-definition-symbol
200-reference-parameter-temp-name-collision
200-reinterpret-enum-to-pointer
200-reinterpret-reference-conditional-materialization
200-return-void-call-expression
200-scalar-assignment-address-lvalue
200-scalar-reference-static-cast-return
200-scoped-enum-global-constant-init
200-scoped-enum-underlying-type
200-scoped-enum-unsigned-high-bit
200-signed-enum-compare-lowering
200-switch-case-nested-inside-if
200-unscoped-enum-promotion-overload
200-variadic-float-argument-promotes-to-double
200-wide-unscoped-enum-promotion
300-return-empty-braces-scalar
```

The authoritative final PA15 log is
`/tmp/pa15-perf-evidence.jpwpRj/full-pa15-final.log`:
**68/109 passed, 41 failed, all 109 covered** (exit 2). The exact current
failure inventory is:

```text
100-const-integral-lvalue-overload-category
100-enum-default-argument-constant-fold
100-function-pointer-ref-call
100-scoped-enum-braced-assignment
100-scoped-enum-previous-enumerator-bitwise-or
100-string-hex-escape-code-unit
100-unnamed-parameter-storage
200-comma-expression-xvalue-reference-return
200-const-cast-pointer-const-drop
200-const-cast-reference-array-subscript
200-const-ref-converted-float-argument
200-enum-class-scalar-lowering
200-extern-function-pointer-indirect-call
200-floating-compound-assign-integral-rhs
200-floating-condition-declaration-negative-zero
200-floating-logical-branch
200-floating-return-integral-conversion
200-for-iteration-discards-void-comma-rhs
200-function-reference-static-cast-call
200-functional-reference-typedef-cast
200-global-address-reinterpret-cast-initializer
200-global-pointer-array-null-fill
200-global-pointer-array-nullptr-init
200-goto-case-block-entry-label
200-goto-case-block-label-after-statement
200-included-namespace-global-definition
200-literal-logical-short-circuit-omits-unreachable-call
200-local-function-type-typedef-reference
200-namespace-default-argument-declaration-lookup
200-nested-conditional-array-decay
200-qualified-namespace-overload-definition-symbol
200-reinterpret-enum-to-pointer
200-reinterpret-reference-conditional-materialization
200-return-void-call-expression
200-scalar-reference-static-cast-return
200-scoped-enum-global-constant-init
200-scoped-enum-unsigned-high-bit
200-unscoped-enum-promotion-overload
200-variadic-float-argument-promotes-to-double
200-wide-unscoped-enum-promotion
300-return-empty-braces-scalar
```

The named baseline-to-current delta is **47 newly passing failures**:

```text
100-array-cv-rvalue-reference-overload
100-c-linkage-reference-declaration-metadata
100-condition-declaration-variable-rvalue
100-extern-unknown-bound-array-reference
100-global-function-pointer-argument-call
100-global-variable
100-sizeof-local-value-shadows-type-name
100-subscript-sizeof
100-unary-logical-conditional
100-unary-plus-array-decay
100-using-directive-imported-value-function-body
200-address-of-local-const-integral-uses-storage
200-comma-expression-lvalue-address
200-compound-assignment-evaluates-lhs-once
200-conditional-array-decay-subscript
200-const-cast-reference-similar-pointer
200-extern-c-internal-header-const
200-for-init-assignment-expression
200-generated-slot-name-collision
200-global-array-bitwise-or-enum-init
200-global-array-conditional-cast-initializer
200-global-array-decay-compare
200-global-array-element-address-initializer
200-global-array-one-past-end-pointer
200-global-array-scalar-cast-init
200-global-array-static-const-byte-init
200-global-object-address-initializer
200-global-pointer-array-subscript-load
200-inferred-local-array-bound
200-integral-multiply-compound-assignment
200-local-direct-init-array-subscript
200-local-int-slot-width
200-local-lvalue-reference-alias-init
200-lvalue-conditional-address
200-lvalue-conditional-reference-return
200-partial-local-array-zero-initialization
200-pointer-compound-assignment-scale
200-pointer-deref-byte-load
200-pointer-operator-array-decay
200-postfix-incdec-evaluates-lhs-once
200-prefix-incdec-lvalue-address
200-prefix-pointer-decrement-reference-argument
200-reference-parameter-temp-name-collision
200-scalar-assignment-address-lvalue
200-scoped-enum-underlying-type
200-signed-enum-compare-lowering
200-switch-case-nested-inside-if
```

## 3. Active Checkpoint

Retained and validated scope:

- Namespace/global scalar and pointer storage identity, object addresses,
  global array element-address initializers, local scalar slots, and
  declaration-only C-linkage/extern objects, including unknown-bound extern
  arrays as declarations with valid LowIR representation.
- Local bounded and inferred arrays, partial local-array zero initialization,
  array-to-pointer decay, subscript addresses/loads, pointer dereference,
  address-of, one-past array pointers, and pointer arithmetic/compound
  assignment with element-size scaling.
- Reference bindings, parameters, arguments, and returns as referent
  addresses; direct reference aliases and the validated global function-pointer
  argument/call form, with no double address/load.
- RHS-before-LHS simple assignment, value-category-preserving comma and
  conditional lvalues, compound assignment, prefix/postfix increment and
  decrement, and integral compound arithmetic, with the LHS address evaluated
  exactly once.
- Typed `sizeof` value lowering, value/type shadow handling, unary-plus array
  decay, integral logical/conditional materialization, selected enum underlying
  type/compare cases, and validated integral/pointer/reference-compatible cast
  subsets.

Nonclaims are exactly the 41 current failures above: floating lowering and
floating argument/conversion cases; string escape classification; enum
constant/promotion/overload cases not in the validated subset; unresolved
function/reference/extern indirect-call and static-cast-call variants; broad
reinterpret/const-cast materialization; global pointer-array null fill;
nested conditional array decay; namespace/default-argument/qualified-overload
edges; goto/case entry; unreachable short-circuit behavior; void-call return;
the comma-expression xvalue reference return; and empty-brace scalar return.
No unvalidated breadth is represented as a passing claim.

Owner/data flow is typed and one-way: PA12 produces `SemanticFact` records and
typed constant-initializer facts; PA15 indexes `BindingId`/`TypeId` ownership
once, creates global/local storage and ABI records, then lowers each typed
expression through `lower_address` or `apply_conversions`. Reference storage
contains the referent pointer, so a reference value is already an address and
is not loaded or addressed again. Global declaration status, linkage, and
unknown array bounds come from PA12 facts and control whether LowIR emits a
declaration or definition.

Complexity and correctness constraints are explicit. `LoweredValue` carries
the physical LowIR type in O(1); there is no reverse scan of the current block
per comparison. The reverse typed-symbol-to-spelling index is an O(log n)
`std::map`, and global/function/scope indexes are built once rather than
rescanned per expression. `lower_address` evaluates a complex LHS once and
reuses its address for mutation. Array projections use the declared LowIR
element type; pointer arithmetic performs one element-size scaling and then
uses the byte-index representation. Local array initialization writes all
elements, including implicit zeroes.

The implementation follows the typed-boundary and complexity requirements in
`spec.md` and the typed address/index/operator forms in `pa13/lowir.md`.
Ordinary PA15 lowering does not use `const_cast` or recompute PA12 semantic
facts. PA12 records direct scalar/element constant initializer results once at
the owner boundary; PA15 consumes those typed results.

Validation already completed for this checkpoint:

- `make -C dev cppgm++`: exit 0.
- The retained focused correction matrix, including the original focused 3
  and the requested arrays, references, address/lvalue, mutation, pointer,
  and zero-initialization cases: **28/28 passed**; log:
  `/tmp/pa15-perf-evidence.jpwpRj/final-focused-28-authoritative.log`.
- `make test-pa15`: exit 2 only because the 41 residual failures remain;
  authoritative summary is **68/109**, with all 109 cases covered.
- The required prior gate passed: `n=15; ... make test-report-through-pa14`
  exited 0 with `===== ALL TESTS PASSED SUCCESSFULLY! (1058 / 1058) =====`;
  log: `/tmp/pa15-perf-evidence.jpwpRj/through-pa14-final-authoritative.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` exited 0;
  it reported four pre-existing bad-division warnings plus one new
  `pa15_lowering.h` declaration/header-division warning, recorded in
  `/tmp/pa15-perf-evidence.jpwpRj/file-audit-final-authoritative.log`.

## 4. Performance Evidence

The measurement artifacts are in `/tmp/pa15-perf-evidence.jpwpRj`. The
immutable executable is
`/tmp/pa15-perf-evidence.jpwpRj/cppgm++-immutable`, mode 555, SHA-256
`2f46889f4d790a7f071ed0c0aa6ea5822df0b84138b1052f8f0a9e348171455f`.
`timings.tsv` contains 56 interleaved samples: seven samples for each of two
families at four sizes. Medians are nanoseconds:

| family | n=32 | n=64 | n=128 | n=256 |
|---|---:|---:|---:|---:|
| long scalar expression | 17,039,168 | 18,469,857 | 21,447,438 | 27,481,421 |
| many globals | 17,643,801 | 18,895,565 | 21,298,332 | 27,406,945 |

The structural probe in `probe.out` reports long-expression LowIR line counts
106/202/394/778 and binary-add counts 32/64/128/256 for n=32/64/128/256,
respectively (`3n+10` lines, no global rescans). The many-global family has
line counts 38/70/134/262, zero binary adds, and 32/64/128/256 global
identities (`n+6` lines). These exact linear structural counts and the
repeated/interleaved medians corroborate near-linear scaling over the tested
family. `single-eval.lowir` contains exactly one `call ptr @lhs()` and one
`addr @value`, corroborating one LHS/subtree evaluation for the mutation
shape.

## 5. Checkpoint Ledger

| Status | Evidence |
|---|---|
| Complete | Baseline captured at 21/109; typed PA12-to-PA15 address/value boundary implemented without text parsing or ordinary semantic recomputation. |
| Complete | Global/local storage, references, arrays/decay/subscripts, pointer scaling, lvalue categories, mutation single-evaluation, declarations, and validated conversion subsets covered by the retained code; zero-fill and index scaling were explicitly corrected. |
| Complete | Focused correction matrix: 28/28 passed; all 109 PA15 cases remain covered. |
| Complete | Full PA15: 68/109 passed, 41 failures, a strict improvement of 47 failures from the baseline 88. |
| Complete | Prior PA1–PA14 gate: 1058/1058 passed. |
| Complete | File audit: exit 0 with four pre-existing warnings plus one new `pa15_lowering.h` declaration/header-division warning; no test/ref/fixture/harness/coverage file is in the intended diff. |
| Complete | `git diff --check`: exit 0; the final intended diff contains implementation files and this plan only. |
