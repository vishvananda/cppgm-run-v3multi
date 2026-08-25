# PA15 Checkpoint Plan

## Stage Design

This checkpoint keeps PA12 as the semantic owner and lowers its typed
structured-statement facts directly into the shared typed
`lowir_model::Program`. It does not parse rendered dumps, LowIR text,
fixtures, or reference output. The lowerer covers the bounded structured CFG
slice needed here: `while`, `do`, `for`, `break`, `continue`, condition
declarations, `switch`/`case`/`default`, fallthrough, and direct `&&`/`||`
condition branching.

Block IDs, operands, instructions, slots, and terminators remain typed. Block
allocation uses the existing deterministic stem/counter scheme; final block
presentation follows first CFG/source reachability so normalized LowIR remains
stable. Break searches the innermost loop-or-switch target; continue searches
the innermost loop target while skipping intervening switches. Case labels are
owned by the active switch, including labels nested in its compound or other
substatements.

The stage-wide scope owner index partitions the PA12 scope arena in
`O(S + B + F)` time and `O(S + B)` space (`S` scopes, `B` bindings, `F`
functions), once per translation unit. The ordered maps/sets used by slot
ownership and naming make the honest total slot-ownership bound
`O((S + B + F) log B)`, still without any `O(F*S)` whole-arena scan. Each
owned switch pre-collects its labels with one traversal of its statement
subtree and stops at nested `switch` facts. Thus label collection is `O(F_s)`
for the facts owned by switch `s` before ordered-label bookkeeping, with no
repeated case scans or cross-switch cache; structured lowering otherwise visits
each owned fact a constant number of times. These are structural bounds, not
claims about unsupported expression lowering or the complete frontend.

## Failure Map

The clean turn-start PA15 baseline was **13/109 passing and 96 failing**. The
following baseline triage buckets are exhaustive and sum to 96; names are
grouped by the dominant missing capability, so a few cross-cutting cases have
a secondary theme. The exact baseline inventory is recorded below. The final
broad PA15 run is **21/109 passing and 88 failing**: its exact failure set is
the baseline inventory below minus the eight newly passing cases
`100-bad-switch`, `100-continue-inside-switch-targets-loop`,
`100-do-while-lowering`, `100-for-loop`, `100-nested-switch-cases-stay-inner`,
`100-switch-condition-declaration`, `100-while-break`, and
`200-direct-short-circuit-condition-branch`.

| Category | Count | Checkpoint boundary |
|---|---:|---|
| Structured control: loops, switch, goto, conditions, short circuit | 17 baseline / 9 final | Eight cases addressed; nine remain |
| Globals, arrays, pointers, address/decay, subscripting | 40 | Not claimed |
| Calls, ABI/reference parameters, overloads, linkage, namespace lookup | 18 | Existing scalar/direct-call subset only |
| Enums, `sizeof`, floating cases, literals, conversions | 13 | Existing scalar conversion subset only |
| Extended lvalues, compound assignment, inc/dec, slots, scalar extensions | 8 | Not claimed |
| **Total** | **96 baseline / 88 final** | **No coverage reduction** |

Exact baseline failure inventory (the three checked-in negative tests that
already matched their expected failure—`100-scoped-enum-no-implicit-int-bad`,
`100-switch-label-bypasses-initialization-bad`, and
`200-bad-excess-array-initializer`—are not in this list):

```text
100-array-cv-rvalue-reference-overload  100-bad-switch  100-c-linkage-reference-declaration-metadata
100-condition-declaration-variable-rvalue  100-const-integral-lvalue-overload-category
100-continue-inside-switch-targets-loop  100-do-while-lowering  100-enum-default-argument-constant-fold
100-extern-unknown-bound-array-reference  100-for-loop  100-function-pointer-ref-call
100-global-function-pointer-argument-call  100-global-variable  100-nested-switch-cases-stay-inner
100-scoped-enum-braced-assignment  100-scoped-enum-previous-enumerator-bitwise-or
100-sizeof-local-value-shadows-type-name  100-string-hex-escape-code-unit  100-subscript-sizeof
100-switch-condition-declaration  100-unary-logical-conditional  100-unary-plus-array-decay
100-unnamed-parameter-storage  100-using-directive-imported-value-function-body  100-while-break
200-address-of-local-const-integral-uses-storage  200-comma-expression-lvalue-address
200-comma-expression-xvalue-reference-return  200-compound-assignment-evaluates-lhs-once
200-conditional-array-decay-subscript  200-const-cast-pointer-const-drop
200-const-cast-reference-array-subscript  200-const-cast-reference-similar-pointer
200-const-ref-converted-float-argument  200-direct-short-circuit-condition-branch
200-enum-class-scalar-lowering  200-extern-c-internal-header-const
200-extern-function-pointer-indirect-call  200-floating-compound-assign-integral-rhs
200-floating-condition-declaration-negative-zero  200-floating-logical-branch
200-floating-return-integral-conversion  200-for-init-assignment-expression
200-for-iteration-discards-void-comma-rhs  200-function-reference-static-cast-call
200-functional-reference-typedef-cast  200-generated-slot-name-collision
200-global-address-reinterpret-cast-initializer  200-global-array-bitwise-or-enum-init
200-global-array-conditional-cast-initializer  200-global-array-decay-compare
200-global-array-element-address-initializer  200-global-array-one-past-end-pointer
200-global-array-scalar-cast-init  200-global-array-static-const-byte-init
200-global-object-address-initializer  200-global-pointer-array-null-fill
200-global-pointer-array-nullptr-init  200-global-pointer-array-subscript-load
200-goto-case-block-entry-label  200-goto-case-block-label-after-statement
200-included-namespace-global-definition  200-inferred-local-array-bound
200-integral-multiply-compound-assignment  200-literal-logical-short-circuit-omits-unreachable-call
200-local-direct-init-array-subscript  200-local-function-type-typedef-reference
200-local-int-slot-width  200-local-lvalue-reference-alias-init  200-lvalue-conditional-address
200-lvalue-conditional-reference-return  200-namespace-default-argument-declaration-lookup
200-nested-conditional-array-decay  200-partial-local-array-zero-initialization
200-pointer-compound-assignment-scale  200-pointer-deref-byte-load  200-pointer-operator-array-decay
200-postfix-incdec-evaluates-lhs-once  200-prefix-incdec-lvalue-address
200-prefix-pointer-decrement-reference-argument  200-qualified-namespace-overload-definition-symbol
200-reference-parameter-temp-name-collision  200-reinterpret-enum-to-pointer
200-reinterpret-reference-conditional-materialization  200-return-void-call-expression
200-scalar-assignment-address-lvalue  200-scalar-reference-static-cast-return
200-scoped-enum-global-constant-init  200-scoped-enum-underlying-type
200-scoped-enum-unsigned-high-bit  200-signed-enum-compare-lowering
200-switch-case-nested-inside-if  200-unscoped-enum-promotion-overload
200-variadic-float-argument-promotes-to-double  200-wide-unscoped-enum-promotion
300-return-empty-braces-scalar
```

## Active Checkpoint

- `dev/src/pa15_lowering.cpp` now consumes the typed PA12 statement graph,
  carries loop/switch target stacks, lowers structured terminators, assigns
  condition-declaration storage from PA12-owned scopes, and handles direct
  short-circuit CFG edges.
- `dev/src/lowir_model.cpp` serializes typed `switch` terminators; no textual
  round trip was added.
- Scope ownership is now indexed once per translation unit with typed
  parent-before-child propagation; no function scans the full scope arena.
  Switch lowering records every collected label visit, retains any unvisited
  allocated block deterministically rather than dropping it, and tracks
  label-bearing `if` branches so recovered case paths skip mutually exclusive
  siblings and merge only through typed continuations.
- Focused checked-in validation improved from **0/9** before the change to
  **8/9** for the original set, and is **8/10** including
  `100-condition-declaration-variable-rvalue`: the original eight structured
  cases pass; the two failures are documented scalar-expression nonclaims.
  Both out-of-repository dead-label probes passed; the with-else probe emitted
  `case 2` as `y = 3` followed by a switch-continuation jump, with no `y = 4`
  store, and CY86 execution returned 0 for `f(2) - 3`.
- `200-switch-case-nested-inside-if` remains blocked by its separate `main`
  expression (`run(...) == 2 ? 0 : 1`) and the existing unsupported scalar
  conditional-expression path; its structured `run` function lowers.
  `200-for-init-assignment-expression` likewise remains outside this slice
  because its iteration uses unsupported prefix increment.
- Broad PA15 ran all 109 tests with **21 passing and 88 failing**. The remaining
  structured-control failures are `100-condition-declaration-variable-rvalue`,
  `100-unary-logical-conditional`,
  `200-floating-condition-declaration-negative-zero`,
  `200-for-init-assignment-expression`,
  `200-for-iteration-discards-void-comma-rhs`,
  `200-goto-case-block-entry-label`, `200-goto-case-block-label-after-statement`,
  `200-literal-logical-short-circuit-omits-unreachable-call`, and
  `200-switch-case-nested-inside-if`; the other 79 remain in the
  non-structured baseline categories.
- Nonclaims remain scalar conditional expressions, prefix/inc-dec and other
  extended lvalues, globals/arrays/pointers, indirect/reference-heavy calls,
  unresolved namespace/linkage extensions, enums/floating conversions, and
  goto. No fixture, reference, harness, or coverage change was made.

## Performance Evidence

The immutable freshly built executable was copied outside the repository as
`/tmp/pa15-perf-amend.hd5A6F/cppgm++-pa15-fresh` with SHA-256
`31093ff568ac9d787855b64720c6933b593d2b3d734e7e4b91dba55d71a32972`.
Generated many-function/control-scope and nested-switch inputs were run in an
interleaved sequence with three observations per size. Structural counters
corroborate the one-pass bound; wall time is reported as noisy supporting
evidence, not as a stronger complexity claim.

| Family / size | Median ms | Input B | Output B | Functions | Blocks | Instructions | Cases |
|---|---:|---:|---:|---:|---:|---:|---:|
| many functions / 8 | 5.310059 | 1430 | 8806 | 9 | 89 | 226 | 0 |
| many functions / 16 | 7.253170 | 2836 | 17490 | 17 | 177 | 450 | 0 |
| many functions / 32 | 11.035204 | 5652 | 34866 | 33 | 353 | 898 | 0 |
| nested switches / 4 | 4.029036 | 320 | 1435 | 2 | 18 | 29 | 4 |
| nested switches / 8 | 4.088879 | 656 | 2603 | 2 | 34 | 53 | 8 |
| nested switches / 16 | 4.904032 | 1616 | 4939 | 2 | 66 | 101 | 16 |

The raw measurement table is outside the repository at
`/tmp/pa15-perf-amend.hd5A6F/measurements.tsv`.

## Checkpoint Ledger

| Status | Evidence |
|---|---|
| Baseline recorded | 13/109 passing, 96 failing; exact inventory above from the clean pre-change sidecars |
| Corrections | Stage-wide scope index, complete branch-aware typed label visitation, deterministic all-block retention, corrected complexity bound, and corrected three-negative-test note |
| Focused validation | Original set 8/9; expanded set 8/10; both raw nested-label probes passed, including CY86 execution |
| PA15 gate | `make test-pa15`: 21/109 passing, 88 failing; all 109 covered |
| Prior gate | Through PA14: `1058/1058` passed |
| Audit | Passed with four pre-existing header-division warnings |
| Performance | Fresh immutable executable; interleaved medians and structural counters recorded above |
| Diff sanity | `git diff --check` passed after final source and plan review |
| Commit | Completed checkpoint state; coherent commit amended after final validation, with no completion claim because 88 PA15 failures remain |
