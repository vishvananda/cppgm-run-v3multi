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

The preceding checkpoint's historical baseline was 70/109 passing, with all
109 tests covered and exactly these 39 failures:

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

The fresh final PA15 stage result for this checkpoint is 79/109 passing with
all 109 tests covered. The exact final 30-name failure set is:

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

The final set comparison against the incoming primary log is explicit: removed
(9), with no new or replacement failures:

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

Thus the final set is exactly the 39-name baseline minus those nine names;
coverage remains 109. The fresh final log is
/tmp/pa15-final-checkpoint-test-pa15.log. Mechanical comparison with the
incoming primary full-stage log
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log gives
zero names in final-minus-incoming and zero names in incoming-minus-final.

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

Final checkpoint proof:

    make -C dev cppgm++
    make -C pa15 check TEST='tests/general/100-enum-default-argument-constant-fold.t tests/general/100-scoped-enum-braced-assignment.t tests/general/100-scoped-enum-previous-enumerator-bitwise-or.t tests/general/100-scoped-enum-no-implicit-int-bad.t tests/general/200-enum-class-scalar-lowering.t tests/general/200-global-array-bitwise-or-enum-init.t tests/general/200-namespace-default-argument-declaration-lookup.t tests/general/200-scoped-enum-global-constant-init.t tests/general/200-scoped-enum-underlying-type.t tests/general/200-scoped-enum-unsigned-high-bit.t tests/general/200-signed-enum-compare-lowering.t tests/general/200-unscoped-enum-promotion-overload.t tests/general/200-wide-unscoped-enum-promotion.t'
    ./cppgm.tests/course/pa15/402-typed-enum-boundary-regression.sh
    make test-pa15                         # exit 2, 79/109, all 109 covered
    n=15; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
    perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src
    git diff --check

The build exits 0, the focused harness is 13/13, and the durable regression
exits 0. The full PA15 gate exits 2 only for the unchanged exact 30-name
residual set; coverage remains 109. The exact through-PA14 gate exits 0 with
1058/1058. The file audit exits 0 with its five pre-existing header-division
warnings, and the final diff check exits 0. Temporary probes cover
declaration-only and interleaved default ownership, fixed and implicit enum
boundaries, unsigned global wrap, signed/unsigned operator selection, enum
pointer offsets, promoted-width shift rejection, conditional common-type
conversion, and bool normalization. The generated witnesses include i64 for
the enum-class scalar function, u8 for the high-bit scoped comparison, i32
for the signed scoped/global comparison, a default call @f(0), and the correct
int/unsigned overload selections.


## Historical Performance Context

Enum declaration processing is one linear pass over enumerators; promotion and
scoped-comparison checks use canonical type IDs and NamedRecord access; default
arguments use contiguous per-function fact ranges. The changed path retains
O(n) declaration work, O(1) canonical type equality, and no whole-program
retry.

The historical table below is preserved for continuity only. The temporary
candidate/artifact directory referenced by the preliminary note was
unavailable in this workspace, so no missing path, hash, or supporting file is
claimed as present or as final evidence.

| n | input bytes/lines | semantic bytes/lines | LowIR bytes/lines | median wall/user/sys s | median max RSS KB |
|---:|---:|---:|---:|---:|---:|
| 16 | 13271/193 | 40977/904 | 30590/1284 | 0.01/0.00/0.00 | 7208 |
| 64 | 53879/769 | 163953/3592 | 122702/5124 | 0.04/0.03/0.01 | 14812 |
| 256 | 220835/3073 | 659445/14344 | 494738/20484 | 0.19/0.13/0.05 | 47224 |
| 512 | 444835/6145 | 1321205/28680 | 991890/40964 | 0.41/0.29/0.11 | 90612 |

The earlier structural and timing interpretation remains historical context,
not a final performance claim after the Phase 1 repairs.

## Fresh Performance Evidence

Fresh evidence was generated from the immutable final candidate
/tmp/pa15-final-enum-perf.postgates.bHzCH8/cppgm++-final-immutable, mode 0555,
SHA-256
dc945f3cfd2116ea26610b58ac5a4382e6d242d01a2f5ef8515062b3a6c5d555.
The artifact directory is /tmp/pa15-final-enum-perf.postgates.bHzCH8 and contains
candidate.sha256, candidate.mode, structure.tsv, timings.tsv, medians.tsv,
the bounded source inputs, and the corresponding semantic/LowIR outputs.
Inputs at sizes 16, 64, 256, and 512 exercise enum declaration/value
processing, default-argument ownership, unscoped promotion, scoped
comparison, conditional common-type conversion, bool conversion, signed and
unsigned operators, promoted-width shifts, global constants, and enum pointer
offsets. Five timing rounds alternate ascending and descending size order.

| n | input bytes/lines | semantic bytes/lines | LowIR bytes/lines | enumerator bindings | LowIR globals/functions | median wall/user/sys s | median max RSS KB |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 16 | 1644/51 | 5185/121 | 4544/167 | 16 | 20/7 | 0.00/0.00/0.00 | 5356 |
| 64 | 3612/147 | 8353/217 | 7424/215 | 64 | 68/7 | 0.00/0.00/0.00 | 5896 |
| 256 | 12108/531 | 21337/601 | 19412/407 | 256 | 260/7 | 0.01/0.00/0.00 | 7492 |
| 512 | 23628/1043 | 38745/1113 | 35540/663 | 512 | 516/7 | 0.02/0.01/0.01 | 10076 |

These are bounded measurements of the affected path, not a universal
performance claim. The fresh artifact shows linear enumerator/global
structural growth across the sampled sizes, with five interleaved runs and
the recorded medians in timings.tsv and medians.tsv.

## Checkpoint Ledger

| status | checkpoint | evidence or next proof |
|---|---|---|
| Historical | PA15 typed null/global initializer boundary | Prior plan recorded the earlier 70/109 baseline and typed-null work; it is historical context, not evidence for this enum increment. |
| Current | PA15 typed enum scalar boundary (`3bf82dbe45fcc77af7246331b9c6a88674ed43ff`) | Build `0`, compact focused proof `13/13`, durable `402` regression `0`, final PA15 `79/109` with all `109` covered and the exact unchanged `30`-name residual set, through-PA14 `1058/1058`, file-audit exit `0` with five pre-existing warnings, diff-check exit `0`, and fresh post-gate immutable performance evidence under `/tmp/pa15-final-enum-perf.postgates.bHzCH8`. |
| Next | PA15 residual boundary | `200-reinterpret-enum-to-pointer` is the next enum-adjacent residual boundary; continue only in a later checkpoint. |
