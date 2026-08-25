# PA15 Typed Enum Scalar Boundary Checkpoint

## Stage Design

This checkpoint carries enum facts across the existing PA11 → PA12 → PA15
boundary. PA11 remains the owner of the enum declaration scope, canonical enum
type identity, enumerator values, and the selected underlying representation.
It now evaluates enumerator initializers in the enum value scope and publishes a
representation for defined enums without an explicit underlying type.

PA12 consumes those facts for unscoped-enum promotion ranking, same-type scoped
enum comparisons, scalar braced assignment, and default arguments. Default
argument facts are stored as a contiguous range on the owning FunctionFact;
calls append the already converted typed fact instead of re-looking up or
re-evaluating its source expression. Binary facts retain a canonical
operation_type; same-type scoped-enum operation types remain canonical enum
TypeIds, and PA15 checks their NamedRecord identity in O(1) to preserve narrow
storage. Enumerator bindings retain source unsignedness and 64-bit bit storage
while semantic facts derive final signedness from the canonical enum underlying
type.

PA15 consumes the enum's selected underlying type through low_type and uses
operation_type and its canonical enum NamedRecord identity to preserve narrow
representation and unsigned C++ operators in comparisons, division, shifts,
and conversions. The 64-bit scalar boundary uses the PA13 i64 spelling required
by the checked-in
enum oracle; signedness comes from canonical TypeId facts rather than that
spelling. No lowering path performs text/name rediscovery, creates a second
enum model, retries a whole program, or shells out to another compiler.

## Exact Failure Map

The turn-start historical baseline was 70/109 passing, with all 109 tests
covered and exactly these 39 failures:

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

The targeted cluster was:

    100-enum-default-argument-constant-fold
    100-scoped-enum-braced-assignment
    100-scoped-enum-previous-enumerator-bitwise-or
    200-enum-class-scalar-lowering
    200-scoped-enum-global-constant-init
    200-scoped-enum-unsigned-high-bit
    200-unscoped-enum-promotion-overload
    200-wide-unscoped-enum-promotion
    200-namespace-default-argument-declaration-lookup

Final current proof is 79/109 passing with all 109 tests covered. The exact
current 30-name failure set is:

    100-const-integral-lvalue-overload-category
    100-function-pointer-ref-call
    100-string-hex-escape-code-unit
    100-unnamed-parameter-storage
    200-comma-expression-xvalue-reference-return
    200-const-cast-pointer-const-drop
    200-const-cast-reference-array-subscript
    200-const-ref-converted-float-argument
    200-extern-function-pointer-indirect-call
    200-floating-compound-assign-integral-rhs
    200-floating-condition-declaration-negative-zero
    200-floating-logical-branch
    200-floating-return-integral-conversion
    200-for-iteration-discards-void-comma-rhs
    200-function-reference-static-cast-call
    200-functional-reference-typedef-cast
    200-global-address-reinterpret-cast-initializer
    200-goto-case-block-entry-label
    200-goto-case-block-label-after-statement
    200-included-namespace-global-definition
    200-literal-logical-short-circuit-omits-unreachable-call
    200-local-function-type-typedef-reference
    200-nested-conditional-array-decay
    200-qualified-namespace-overload-definition-symbol
    200-reinterpret-enum-to-pointer
    200-reinterpret-reference-conditional-materialization
    200-return-void-call-expression
    200-scalar-reference-static-cast-return
    200-variadic-float-argument-promotes-to-double
    300-return-empty-braces-scalar

The baseline-to-current set comparison is explicit: removed (9), with no new
or replacement failures:

    100-enum-default-argument-constant-fold
    100-scoped-enum-braced-assignment
    100-scoped-enum-previous-enumerator-bitwise-or
    200-enum-class-scalar-lowering
    200-namespace-default-argument-declaration-lookup
    200-scoped-enum-global-constant-init
    200-scoped-enum-unsigned-high-bit
    200-unscoped-enum-promotion-overload
    200-wide-unscoped-enum-promotion

    new failures: none

Thus the current set is exactly the 39-name baseline minus those nine names;
coverage remains 109. The exact full-suite log and set extracts are retained at
/tmp/pa15-final-validation-proof.oaRt2w/test-pa15.log,
/tmp/pa15-final-validation-proof.oaRt2w/baseline-39.txt,
/tmp/pa15-final-validation-proof.oaRt2w/current-failures.txt,
/tmp/pa15-final-validation-proof.oaRt2w/removed.txt, and
/tmp/pa15-final-validation-proof.oaRt2w/new.txt.

## Active Checkpoint

Implementation files changed in this coherent increment:

    dev/src/pa11_semantic_model.h
    dev/src/pa11_semantic_core.cpp
    dev/src/pa11_semantic.cpp
    dev/src/pa12_semantic.cpp
    dev/src/pa12_semantic_facts.cpp
    dev/src/pa15_lowering.h
    dev/src/pa15_lowering.cpp
    dev/src/pa15_lowering_flow.cpp

Focused proof currently available:

    make -C dev cppgm++
    make -C pa15 check TEST='tests/general/100-enum-default-argument-constant-fold.t tests/general/100-scoped-enum-braced-assignment.t tests/general/100-scoped-enum-previous-enumerator-bitwise-or.t tests/general/100-scoped-enum-no-implicit-int-bad.t tests/general/200-enum-class-scalar-lowering.t tests/general/200-global-array-bitwise-or-enum-init.t tests/general/200-namespace-default-argument-declaration-lookup.t tests/general/200-scoped-enum-global-constant-init.t tests/general/200-scoped-enum-underlying-type.t tests/general/200-scoped-enum-unsigned-high-bit.t tests/general/200-signed-enum-compare-lowering.t tests/general/200-unscoped-enum-promotion-overload.t tests/general/200-wide-unscoped-enum-promotion.t'
    git diff --check

The build and diff check exit successfully; the focused harness is 13/13.
The generated witnesses include i64 for the enum-class scalar function, u8
for the high-bit scoped comparison, i32 for the signed scoped/global
comparison, a default call @f(0), and the correct int/unsigned overload
selections. A bounded local probe rejects an implicit-base scoped value above
int; another emits udiv/ult for unsigned-long and fixed unsigned-long-enum
operations, with zext for unsigned-int to unsigned-long conversion.
The same bounded probes accept an implicit unscoped uint64-max value and an
i64-boundary auto-enumerator without signed-overflow failure.

The exact through-PA14 command passes 1058/1058, and the PA15 file audit passes
with five existing header-division warnings and no fatal findings.

## Performance Evidence

Enum declaration processing is one linear pass over enumerators; promotion and
scoped-comparison checks use canonical type IDs and NamedRecord access; default
arguments use contiguous per-function fact ranges. The changed path retains
O(n) declaration work, O(1) canonical type equality, and no whole-program
retry.

The immutable candidate is
/tmp/pa15-enum-perf.5MCQq4/cppgm++-final-immutable, mode 555, SHA-256
30c69d3399f64efd19fa721fde610861d63f20a65b64636ac83ffff8b525289b.
Supporting exact artifacts are
/tmp/pa15-enum-perf.5MCQq4/candidate.sha256,
/tmp/pa15-enum-perf.5MCQq4/candidate.mode,
/tmp/pa15-enum-perf.5MCQq4/structure.tsv,
/tmp/pa15-enum-perf.5MCQq4/timings.tsv, and
/tmp/pa15-enum-perf.5MCQq4/medians.tsv. Inputs include unscoped range
selection, narrow scoped and fixed-underlying comparisons, defaulted calls,
and unsigned high-bit/64-bit division and conversion. Five runs per size were
interleaved in ascending/descending size order.

| n | input bytes/lines | semantic bytes/lines | LowIR bytes/lines | median wall/user/sys s | median max RSS KB |
|---:|---:|---:|---:|---:|---:|
| 16 | 13271/193 | 40977/904 | 30590/1284 | 0.01/0.00/0.00 | 7208 |
| 64 | 53879/769 | 163953/3592 | 122702/5124 | 0.04/0.03/0.01 | 14812 |
| 256 | 220835/3073 | 659445/14344 | 494738/20484 | 0.19/0.13/0.05 | 47224 |
| 512 | 444835/6145 | 1321205/28680 | 991890/40964 | 0.41/0.29/0.11 | 90612 |

The structural counters scale linearly with n; median wall time is near
proportional across these bounded sizes, with no unexpected superlinear signal
in this changed path.

## Checkpoint Ledger

| status | checkpoint | evidence or next proof |
|---|---|---|
| Historical | PA15 typed null/global initializer boundary | Prior plan recorded the earlier 70/109 baseline and typed-null work; it is historical context, not evidence for this enum increment. |
| Complete | PA15 typed enum scalar boundary | 13/13 focused proof; 79/109 full PA15 with exactly nine baseline failures removed and none added; 1058/1058 through-PA14; audit clean; immutable bounded performance evidence recorded. PA15 itself remains incomplete with 30 residual failures. |
| Next | PA15 residual boundary | `200-reinterpret-enum-to-pointer` is the next enum-adjacent residual boundary; continue only in a later checkpoint. |
