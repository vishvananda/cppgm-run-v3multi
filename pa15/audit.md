# PA15 Audit

## Current Checkpoint Review

This review is for the landed increment `3bf82dbe45fcc77af7246331b9c6a88674ed43ff`
(`PA15: lower typed enum scalars`), parent checkpoint `dea5352e`, and is
bounded to the enum scalar ownership path. The fresh final PA15 stage result
is `79/109` with all `109` covered and exactly `30` residual failures,
recorded in `/tmp/pa15-enum-followup-full-pa15.log`. Comparison with the
incoming primary log at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` has
zero names in final-minus-incoming and zero names in incoming-minus-final; no
failure was added or replaced. The residual failures, including
`200-reinterpret-enum-to-pointer`, remain outside this checkpoint.

The ownership trace is single and typed:

1. PA11 owns enum declaration scope, the canonical `NamedRecord` and enum
   `TypeId`, the selected underlying `TypeId`, and enumerator bindings. Enum
   initializers are evaluated in the enum value scope. Explicit integral
   underlying types are checked against each enumerator's signed range or
   unsigned bit width, while implicit scoped and unscoped representations are
   selected from the complete value range. Binding values retain signedness
   and 64-bit bits for later consumers.
2. PA12 consumes those canonical facts for unscoped integral promotion and
   overload ranking, same-type scoped-enum comparison operation types,
   scalar braced assignment, and default arguments. Default arguments are
   recorded in a contiguous range owned by the canonical `FunctionFact`; a
   declaration, definition, or compatible redeclaration contributes the
   default expression from that declaration context, and calls reuse the
   converted fact. Typed constant-expression folding preserves the operation
   type's width and signedness, including unsigned wrap. Integral-to-bool
   conversion normalizes nonzero values to `1`; fixed enum underlying-type
   representability remains an enum-declaration check at its owner, so it
   still rejects a fixed `bool` enumerator value outside `0`/`1`.
3. PA15 consumes the typed facts through `low_type`, `operation_type`, and
   canonical enum identity. Conditional evaluation reuses the selected
   branch's typed `ConstValue.type`, type-walks only the unselected branch,
   and computes the common/promoted integral type from those two canonical
   TypeIds. Same-type scoped branches retain their enum type. A selected
   nested chain therefore performs one evaluator walk plus one bounded
   unselected-branch type walk per level, without rewalking the whole
   conditional node. Shift counts are checked against the promoted left
   operation type's actual bit width. It emits the PA13 i64 spelling for
   64-bit scalar storage while retaining signedness in the TypeId-driven
   operator selection; comparisons, division, remainder, shifts, compound
   operators, and pointer offsets therefore retain the appropriate signed or
   unsigned LowIR form. No textual downgrade, name rediscovery, second
   semantic model, whole-program retry, or host/reference compiler is
   involved.

The audit repaired the in-scope ownership gaps: declaration-only default facts
were not attached to the canonical function across compatible declarations;
fixed-underlying enum values were not range-checked; typed constant folding
could lose unsigned width or reject defined unsigned wrap; shifts used a
hard-coded width; bool conversion did not normalize nonzero values; and a
conditional expression returned its selected branch without applying its
common type. The evaluator remains in the semantic owner, and the scoped
comparison compatibility context is limited to the existing static-assert
constant-expression use. This follow-up removed the selected-branch
conditional rewalk and kept the common-type helper owned by the same typed
constant evaluator. The existing same-scoped-enum declaration fold remains
supported for the checked enum-body bitwise fixture. No residual failure was
targeted.

The changed path remains bounded: enum processing and default-range filling
are linear in their source facts, canonical identity and same-type checks are
O(1), and the existing ordered identity structures retain their ordinary
O(n log n) behavior. Conditional selected-chain evaluation reuses each
selected typed result and performs only the other branch's type walk at that
level. Fresh immutable/interleaved/median evidence is recorded below from
the nested-chain artifact; it is limited to measured bounded inputs and makes
no universal performance claim.

## Historical Checkpoint Review — typed global pointer null/zero initializers

This review is for the landed increment `dea5352e70fc42b3fa5a56bbe2b17682c581777a`
(`PA15 lower typed global pointer null initializers`) and is bounded to the
typed null/zero initializer path plus the two checked-in pointer-array
fixtures. The residual PA15 enum, floating, goto, and unrelated surfaces are
not re-audited here.

The ownership trace is now single and typed:

1. PA10 decodes a literal and retains its decoded bytes on the AST node as the
   source fact. PA12 validates the array type, element count, byte size, and
   terminal `ArrayToPointer` conversion, then takes one downstream snapshot in
   `constant_address_literal_bytes_` and records its range, element type, and
   count in `ConstantAddressFact::Literal`. The PA12 arena is the canonical
   downstream snapshot; PA15 never reads the PA10 payload, semantic kind, or
   conversion range to relocate a literal.
2. `resolve_constant_address` is a transaction around its recursive typed
   implementation. It records the arena tail before resolution, rolls it back
   on false, exception, or an invalid candidate, and accepts only well-formed
   facts. A `SymbolAddend` must name an in-range variable or function binding;
   an `ArrayElement` must retain its typed target/index relation; a `Literal`
   has no binding target or addend and its byte range must match its element
   type. Unsupported literal arithmetic is rejected before it can transform a
   literal into an invalid symbol addend. A transparent cast wrapper may pass
   `ArrayDecay` to the literal child, preserving the valid literal fact.
3. PA15 maps the recorded `ConstantAddressFact` identity to a LowIR symbol.
   For a literal it materializes one deterministic internal backing global
   from the PA12 byte range and caches only the backend identity mapping from
   constant-address fact to `SymbolId`. This is LowIR identity materialization,
   not a second semantic relocation model. Non-literal mappings require the
   validated binding target. No text, fixture name, source pattern, or
   lowering-time semantic reconstruction is used.
4. Null conversions remain PA12 `ConversionFact` ownership. Scalar pointer
   nulls become `INIT_ZERO`; array elements and omitted slots become coalesced
   `ITEM_ZERO` data. Address items use the same typed constant-address mapper,
   so string-backed addresses, repeated literal facts, and zero slots retain
   deterministic identity and order.
5. PA15 accepts a pointer-zero chain only when there is one null conversion at
   the beginning, every following conversion is linked and pointer-valued,
   the suffix is limited to identity, lvalue-to-rvalue,
   pointer-qualification, or pointer-to-void, and the terminal target matches
   the destination after only an outer cv wrapper is removed. Pointer-object cv
   remains in the pointer `TypeId`; pointee cv remains in its child `TypeId`.
   PA12 value conversion discards top-level pointer-object cv, while
   `qualification_convertible` continues to reject pointee qualification
   drops. The `pa11_semantic_core.cpp` change is retained because its callers
   are value-conversion/common-type contexts, not reference identity binding.
6. PA15 loads an lvalue before pointer qualification or pointer-to-void value
   conversion. Runtime `nullptr` is emitted as a typed pointer copy; its
   `NullptrToBool` conversion uses a typed pointer comparison and boolean
   conversion rather than retagging a pointer temporary as a boolean.
   `LowIR Program` remains the only typed production IR model and the
   serializer only renders that model.

The affected work is linear in the typed fact/range sizes, with ordered
identity maps retaining ordinary `O(n log n)` behavior. Zero data is coalesced
in one pass. No broad string-expression lowering, host compiler, reference
shell-out, duplicate production model, or textual downgrade was introduced.

## Final Checkpoint Evidence

- `make -C dev cppgm++` exited `0` after the bounded source repair; its log is
  `/tmp/pa15-enum-followup-build.log`.
- The exact compact 13-test PA15 matrix from `pa15/plan.md` passed `13/13`;
  its log is `/tmp/pa15-enum-followup-focused.log`.
- The durable earliest-owner regression
  `cppgm.tests/course/pa15/402-typed-enum-boundary-regression.sh` exited `0`;
  its log is `/tmp/pa15-enum-followup-402.log`.
- Bounded temporary probes exited `0` for declaration-only and interleaved
  default ownership, enum signed/unsigned boundaries, global unsigned wrap,
  and typed operator/pointer-offset lowering. Fixed-underlying out-of-range
  values, an implicit scoped value above `int`, a 32-bit promoted-width shift,
  a mixed scoped conditional, and a fixed-bool value outside `0`/`1` exited
  nonzero as required. A selected nested conditional chain of depth 16
  preserved the unsigned boundary (`cmp eq u32 4294967295, 4294967295`); its
  source, LowIR, and log are `/tmp/pa15-followup-conditional-chain.cpp`,
  `/tmp/pa15-enum-followup-conditional-chain.lowir`, and
  `/tmp/pa15-enum-followup-conditional-chain.log`.
- `make test-pa15` exited `2` with `79/109` passing, all `109` covered, and
  exactly the unchanged 30-name residual set; its log is
  `/tmp/pa15-enum-followup-full-pa15.log`. The mechanical failure-set proof
  is `/tmp/pa15-enum-followup-failure-set.log`: current and incoming counts
  are both `30`, current-minus-incoming is empty, and incoming-minus-current
  is empty.
- The exact `n=15` prior gate exited `0` with `1058/1058`:
  `n=15; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`; its log is
  `/tmp/pa15-enum-followup-through-pa14.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` exited `0`;
  its log is `/tmp/pa15-enum-followup-file-audit.log`.
  It emitted only the five pre-existing `bad-division` header warnings for
  `abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
  `pa11_semantic_model.h`, and `pa15_lowering.h`.
- `git diff --check` exited `0`; its log is
  `/tmp/pa15-enum-followup-diff-check.log`.
- Fresh post-correction performance evidence uses
  `/tmp/pa15-final-enum-perf.followup.efvCDA/cppgm++-final-immutable`, mode
  `0555`, SHA-256
  `7aca7c7423be52b33d8966db79ad787aa9569256457821e709406e8365078e60`.
  Five interleaved ascending/descending rounds, proportional selected-chain
  counters, and medians are recorded in that directory's `structure.tsv`,
  `timings.tsv`, and `medians.tsv`.

## Fresh Performance Evidence — selected nested conditional ownership

The immutable candidate is
`/tmp/pa15-final-enum-perf.followup.efvCDA/cppgm++-final-immutable`, mode `0555`,
SHA-256
`7aca7c7423be52b33d8966db79ad787aa9569256457821e709406e8365078e60`.
The directory contains the candidate hash/mode records, bounded source
inputs, semantic and LowIR outputs, `structure.tsv`, `timings.tsv`, and
`medians.tsv`. Each input retains the bounded enum/default/promotion/operator
coverage and replaces the flat conditional with a selected nested chain of
depth 16, 64, 256, or 512. The `structure.tsv` counters record the selected
chain depth and semantic conditional-node count; five timing rounds alternate
ascending and descending size order.

| depth | input bytes/lines | semantic bytes/lines | LowIR bytes/lines | semantic conditional nodes | LowIR globals/functions | median wall/user/sys s | median max RSS KB |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 16 | 1871/67 | 7885/166 | 4544/167 | 18 | 20/7 | 0.00/0.00/0.00 | 5356 |
| 64 | 4559/211 | 28765/406 | 7424/215 | 66 | 68/7 | 0.01/0.00/0.00 | 5884 |
| 256 | 15935/787 | 250837/1366 | 19412/407 | 258 | 260/7 | 0.02/0.01/0.00 | 8632 |
| 512 | 31295/1555 | 891093/2646 | 35540/663 | 514 | 516/7 | 0.04/0.03/0.01 | 12672 |

These are bounded measurements of the selected nested path, not a universal
performance claim. The depth and conditional-node counters scale with the
generated chain; nested semantic rendering contributes its own depth-shaped
text size. Raw interleaved timings and medians are retained in the artifact
directory.

## Historical Evidence — typed global pointer null/zero initializers

- `make -C dev cppgm++` exited `0` after the final source repair.
- The focused affected-path matrix passed `10/10`; its log is
  `/tmp/pa15-final-focused-affected-matrix.log`.
- The narrow regression
  `cppgm.tests/course/pa15/401-typed-pointer-null-cv-regression.sh` exited
  `0`. It verifies scalar keyword/integer/cast nulls, top-level pointer cv
  with a required lvalue load, typed `nullptr`-to-bool comparison, and
  rejection of a pointee qualification drop.
- The transparent literal-cast probe emitted `__strlit__1` and
  `global @value ... = addr @__strlit__1`; the unsupported literal-arithmetic
  probe was rejected as `PA15 nonconstant global initializer`. Logs are
  `/tmp/pa15-final-literal-wrapper.log` and
  `/tmp/pa15-final-literal-arithmetic.log`.
- `make test-pa15` exited `2` with `70/109` passing and all `109` covered. The
  final failure inventory has exactly the 39 turn-start names: zero names
  were added and zero names were removed. The fresh log is
  `/tmp/pa15-final-full.log`; the sorted inventories used for comparison are
  `/tmp/pa15-turn-start-failures-final-audit.txt` and
  `/tmp/pa15-final-failures-final-audit.txt`.
- The exact `n=15` through-PA14 command exited `0` with `1058/1058`; its log
  is `/tmp/pa15-final-through-pa14.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` exited `0`
  with the five pre-existing header-division warnings. Its log is
  `/tmp/pa15-final-file-audit.log`. `git diff --check` passed.

## Historical Performance Evidence — typed global pointer null/zero initializers

Fresh evidence was measured from the final immutable candidate
`/tmp/pa15-final-perf-final.hvQKB0/cppgm++-final-immutable`, mode `0555`,
SHA-256
`4d9ae4004642bdf402118ef3328efe417a5d8d4427de033de6eba700b8658dd9`.
The candidate, generated sources/outputs, structural counts, raw interleaved
batch timings, and medians are retained in that directory:
`candidate.sha256`, `structure.tsv`, `timings.tsv`, and `medians.tsv`.
There are five interleaved rounds, six family/size positions per round, and
20 repeated compilations per timing sample. Odd rounds use sizes `32, 128,
512`; even rounds reverse them; `mixed` and `repeated` families are
interleaved at each size.

| family | n | LowIR globals | address items | zero items | literal facts | median wall s | median RSS KiB |
|---|---:|---:|---:|---:|---:|---:|---:|
| mixed | 32 | 13 | 12 | 12 | 28 | 0.003500 | 5160 |
| mixed | 128 | 49 | 48 | 48 | 100 | 0.004000 | 5396 |
| mixed | 512 | 193 | 192 | 192 | 388 | 0.006000 | 5928 |
| repeated | 32 | 33 | 32 | 0 | 37 | 0.004000 | 5160 |
| repeated | 128 | 129 | 128 | 0 | 133 | 0.005000 | 5648 |
| repeated | 512 | 513 | 512 | 0 | 517 | 0.010500 | 6896 |

The `mixed` inputs exercise repeated string facts, `nullptr`, integer-zero
pointer initializers, and omitted trailing slots. The `repeated` inputs stress
literal identity/materialization. These are bounded affected-path samples,
not a universal performance claim; the medians show no timeout or unexpected
superlinear behavior at the measured sizes.

Historical evidence is preserved separately and is not used as final proof:
the earlier typed address/value candidate and measurements remain under
`/tmp/pa15-typed-relocation-correction.6EMMbb/context-perf`, and the earlier
pre-repair null-pointer candidate and measurements remain under
`/tmp/pa15-null-pointer-perf.QgTN8n`. Their prior progress and timings are
historical only; the final claims above use the immutable candidate from this
checkpoint.

## Audit Ledger

| status | checkpoint | evidence and disposition |
|---|---|---|
| Historical | PA15 full-stage / checkpointAudit — typed address/value ownership | Amended PA12 relocation ownership with explicit `Value`/`ObjectAddress`/`ArrayDecay` context, rejecting bare pointer/scalar lvalue relocations while preserving object, array, one-past, function, and array-element forms; focused `20/20` plus probes, through-PA14 `1058/1058`, PA15 `68/109` with the exact historical 41 names and all `109` covered, immutable `n=256` performance evidence, file audit pass, and diff-check pass. |
| Historical | PA15 full-stage / checkpointAudit — typed global pointer null/zero initializers | Hardened transactional PA12 literal snapshots and binding invariants; kept PA15 on `ConstantAddressFact` identity/ranges; tightened terminal/destination-safe typed null chains and pointer cv behavior; corrected lvalue loads and runtime `nullptr`-to-bool typing; target matrix `10/10`, narrow regression pass, final PA15 `70/109` with the exact unchanged 39-name set and all `109` covered, through-PA14 `1058/1058`, file audit pass, diff-check pass, and immutable performance evidence. Preserved as historical context. |
| Current | PA15 full-stage / checkpointAudit — typed enum scalar ownership (`3bf82dbe45fcc77af7246331b9c6a88674ed43ff`) | Follow-up build `0`, compact focused matrix `13/13`, selected-chain probe `0`, durable `402` regression `0`, final PA15 `79/109` with all `109` covered and the exact unchanged `30`-name residual set, through-PA14 `1058/1058`, file-audit exit `0` with five pre-existing warnings, diff-check exit `0`, and fresh nested-chain immutable performance evidence under `/tmp/pa15-final-enum-perf.followup.efvCDA`. |
