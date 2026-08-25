# PA15 Typed Null Initializer Checkpoint

## Current Stage Design and Ownership

The active PA15 design follows the PA11-to-PA12-to-PA15 typed boundary in
`spec.md` §§2, 4, 5, and 7, with §3 identity ownership where LowIR symbols are
introduced:

- PA11 owns canonical `BindingId`, `TypeId`, `ScopeId`, declaration, linkage,
  storage, function, and scope facts.
- PA12 owns `SemanticFactId` expression identity, selected bindings, value
  categories, conversions, typed `sizeof`, one-time integral constant values,
  and one-time namespace-initializer relocation facts. `ConversionFact` owns
  null-pointer and null-integer-to-pointer decisions; `ConstantAddressFact` has
  explicit `evaluated`/`valid` state and canonical target/addend/projection/
  index fields. Its resolver carries typed `Value`, `ObjectAddress`, and
  `ArrayDecay` context: variable IdExpressions become relocations only for
  recorded array decay or explicit address-of operands; function identity
  remains a function relocation.
- PA15 builds deterministic binding/declaration/function-scope/global/local
  indexes once and consumes typed facts for global declarations/definitions,
  local/global addresses, references as referent addresses, arrays, decay,
  subscripts, projections, pointer scaling, address/value conversion,
  assignment/comma/conditional category preservation, compound/prefix/
  postfix single LHS evaluation, and scalar/structured pointer zero
  initialization. Missing and typed-null array elements share one coalesced
  zero-data path. The only adjacent fixture correction is decoded string
  literal address materialization for pointer-array `ArrayToPointer` facts.
- The PA15 README nominally places string literals outside this milestone, but
  both selected checked-in fixtures require their typed literal backing globals.
  This tension is contained to already-decoded `LiteralData` plus a recorded
  `ArrayToPointer` fact, with one symbol per `SemanticFactId`; it does not add
  arbitrary string-expression lowering or a parallel string-init model.
- LowIR remains one typed in-memory `Program`; `IK_INDEX` preserves typed
  element/projection information and `lowir_model.cpp` is only the serializer.
  `frontend_source_sets.mk` wires `pa12_semantic_facts.cpp` and the PA15
  lowering units. No token/text lookup, fixture-name logic, host compiler, or
  reference/compiler shell-out is used in the source-to-LowIR path.

The repaired address path is ordinary linear work over its typed expression
facts, with the existing deterministic indexes bounded by `O(n log n)`. The
former PA15 recursive `constant_address` reconstruction and second
`find_address_subscript` traversal are gone, and PA15 cannot reinterpret a
bare scalar/pointer lvalue as its storage address.

## Exact Failure Map and Coverage

Historical evidence preserves the pre-increment `21/109` passing baseline and
the resulting `21` to `68` progress. The turn-start full-stage result was
`68/109` passing, exactly 41 failures, all `109` covered; its authoritative log
is `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The fresh final log is `/tmp/pa15-null-final-full.log`, with the sorted
turn-start and final inventories retained at
`/tmp/pa15-null-turn-start-failures.txt` and
`/tmp/pa15-null-final-failures.txt`. The final 39-name map below is byte-for-byte
equal to the turn-start map after removing exactly the two selected target
names; `70 + 39 = 109`, so coverage is complete and no replacement failure was
introduced.

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

No residual failure is counted as passing, and no new failure substitutes for
one of these names. The removed names are
`200-global-pointer-array-null-fill` and
`200-global-pointer-array-nullptr-init`; both pass in the focused and full
gates below.

## Focused and Final Validation

The focused and final results are:

| command or case | result |
|---|---|
| `make -C dev cppgm++` | exit `0` |
| target pair: `200-global-pointer-array-null-fill`, `200-global-pointer-array-nullptr-init` | pass, `2/2` |
| named 8-case address/array/scalar-zero matrix | pass, `8/8`; log: `/tmp/pa15-null-final-regression-matrix.log` |
| stdin scalar pointer probe (`int* = nullptr`, `int* = 0`) | exit `0`; both globals serialize as `zero`; log/output under `/tmp/pa15-scalar-pointer-null-zero-final.*` |
| `make test-pa15` | exit `2`, `70/109`, 39 residual failures, all `109` covered; log: `/tmp/pa15-null-final-full.log` |
| exact `n=15` through-PA14 gate | exit `0`, `1058/1058`; log: `/tmp/pa15-null-final-through-pa14.log` |
| `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` | exit `0`, five nonfatal header warnings; log: `/tmp/pa15-null-final-file-audit.log` |
| `git diff --check` | pass |

The named regression matrix is `100-global-variable`,
`200-comma-expression-lvalue-address`,
`200-compound-assignment-evaluates-lhs-once` (scalar zero),
`200-global-object-address-initializer`,
`200-global-array-element-address-initializer`,
`200-global-array-one-past-end-pointer`,
`200-global-pointer-array-subscript-load`, and
`200-global-array-decay-compare`; the target pair above is also checked
separately.

The focused target logs are `/tmp/pa15-null-fill-final-focused.log` and
`/tmp/pa15-nullptr-init-final-focused.log`. The prior checkpoint's `20/20`
address/value matrix and probes remain retained historical evidence.

The exact through-PA14 command was:

```sh
n=15; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
```

The five existing file-audit warnings are the header-division findings for
`abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`, `pa11_semantic_model.h`,
and `pa15_lowering.h`.

## Representative Performance Evidence

The immutable corrected executable is
`/tmp/pa15-typed-relocation-correction.6EMMbb/context-perf/cppgm++-corrected`,
mode `0555`, size `1,924,688` bytes, SHA-256
`69b7221e16dffec0e266c91b651cfcab6851fcd8678906941d5845882f4cfe77`.
Inputs, LowIR, semantic dumps, raw timings, structural counts, and medians are
retained in `context-perf`. Its `timings.tsv` contains seven interleaved
samples per family and size, each with 20 repeated compilations. The exact
order in rounds 1, 3, 5, and 7 is
`long-32, many-32, long-64, many-64, long-128, many-128, long-256,
many-256`; rounds 2, 4, and 6 reverse the size order. The executable and
generated evidence files are immutable after generation.

Structural counts from `structure.tsv` are:

| family | n | input bytes/lines | LowIR lines | semantic lines | LowIR globals | binary facts |
|---|---:|---:|---:|---:|---:|---:|
| long-expression | 32 | 131/3 | 9 | 75 | 2 | 32 |
| long-expression | 64 | 195/3 | 9 | 139 | 2 | 64 |
| long-expression | 128 | 324/3 | 9 | 267 | 2 | 128 |
| long-expression | 256 | 580/3 | 9 | 523 | 2 | 256 |
| many-global | 32 | 1403/65 | 69 | 168 | 64 | 0 |
| many-global | 64 | 2811/129 | 133 | 328 | 128 | 0 |
| many-global | 128 | 5711/257 | 261 | 648 | 256 | 0 |
| many-global | 256 | 11727/513 | 517 | 1288 | 512 | 0 |

Per-compilation medians from `medians.tsv` are wall/user/system seconds and
peak RSS KiB:

| family | n | wall | user | system | RSS KiB |
|---|---:|---:|---:|---:|---:|
| long-expression | 32 | 0.003500 | 0.001500 | 0.002000 | 5152 |
| long-expression | 64 | 0.003500 | 0.001500 | 0.002000 | 5128 |
| long-expression | 128 | 0.004000 | 0.001500 | 0.002000 | 5388 |
| long-expression | 256 | 0.004500 | 0.002000 | 0.002500 | 5676 |
| many-global | 32 | 0.004500 | 0.002000 | 0.002500 | 5672 |
| many-global | 64 | 0.006000 | 0.003500 | 0.002500 | 6164 |
| many-global | 128 | 0.010000 | 0.005500 | 0.004000 | 6892 |
| many-global | 256 | 0.016500 | 0.010500 | 0.006000 | 8604 |

The long-expression family retains a constant-size serialized relocation while
its typed binary facts grow from 32 to 256. The many-global family grows from
64 to 512 LowIR globals and from 168 to 1288 semantic lines. These counts and
the timing medians provide representative bounded evidence, not a universal
performance proof.

Checkpoint performance evidence uses immutable candidate
`/tmp/pa15-null-pointer-perf.QgTN8n/cppgm++-candidate` (mode `0555`, SHA-256
`c3588fb8bf456126470bba224e96213b4c61b878898408650e0109796e73223a`), generated
ordinary decoded string/null/missing pointer arrays, and five interleaved
samples of twenty compilations per size. Raw timings are in
`/tmp/pa15-null-pointer-perf.QgTN8n/timings-batch.tsv`; structural counts are in
`/tmp/pa15-null-pointer-perf.QgTN8n/structure.tsv`; medians are in
`/tmp/pa15-null-pointer-perf.QgTN8n/medians.tsv`.

| elements | input bytes/lines | LowIR lines | globals (string backing + array) | array addr/zero items | median wall/user/system s | median RSS KiB |
|---:|---:|---:|---:|---:|---:|---:|
| 32 | 340/28 | 118 | 13 (12 + 1) | 12/12 | 0.004000/0.001500/0.002500 | 7376 |
| 128 | 1311/124 | 512 | 61 (60 + 1) | 60/60 | 0.004500/0.002000/0.002500 | 7568 |
| 512 | 5343/508 | 2240 | 253 (252 + 1) | 252/252 | 0.008000/0.004500/0.003500 | 7220 |

The changed array path is one pass over the bound, coalesces adjacent missing
and typed-null pointer bytes, and performs one typed conversion range scan per
initialized pointer; the string-address side map is `O(log m)` for `m` distinct
literal facts and emits each decoded payload once. Structural output grows
linearly with the generated elements and the bounded timings show no
superlinear trend; process-startup cost dominates the smallest sample, so this
is evidence for the changed surface, not a universal performance claim.

## Active Checkpoint

The typed global pointer null/zero initializer checkpoint is complete. Its
owner chain is PA12 `ConversionFact` for
`nullptr`/integer-zero conversion -> PA15 scalar `INIT_ZERO` or structured
`ITEM_ZERO` -> typed LowIR serialization. Top-level pointer cv is discarded by
the value conversion while pointee qualification remains checked. The next
bounded checkpoint is typed enum scalar lowering, selected from the residual
`200-enum-class-scalar-lowering` failure and its related scoped/unscoped enum
residuals (`200-scoped-enum-global-constant-init`,
`200-scoped-enum-unsigned-high-bit`, and
`200-unscoped-enum-promotion-overload`).

## Checkpoint Ledger

| status | checkpoint | completed evidence |
|---|---|---|
| Complete | PA15 full-stage / typed global pointer null-zero lowering | PA12 top-level pointer-object cv correction; PA15 typed null conversion consumer for scalar and structured globals; narrow pointer-array decoded-literal address materialization required by the two fixtures; target `2/2`, named regression matrix `8/8`, scalar pointer probe exit `0`, build exit `0`; final PA15 `70/109` with exactly the two target names removed and all `109` covered; through-PA14 `1058/1058`; file audit passed with five existing warnings; performance evidence recorded; diff-check passed. |
| Next | PA15 / typed enum scalar lowering | Selected from fresh residual `200-enum-class-scalar-lowering` and related enum failures named above. |
| Complete | PA15 full-stage / checkpointAudit — typed address/value ownership | Amended PA12 relocation ownership with explicit `Value`/`ObjectAddress`/`ArrayDecay` context, rejecting bare pointer/scalar lvalue relocations while preserving object, array, one-past, function, and array-element forms; focused `20/20` plus probes, through-PA14 `1058/1058`, PA15 `68/109` with the exact unchanged 41 names and all `109` covered, fresh immutable `n=256` performance evidence, file audit pass, and diff-check pass. |
