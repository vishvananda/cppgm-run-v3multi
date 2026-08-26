# PA15 Audit

## Current Checkpoint Review

This review covers the landed source checkpoint
`98e75ff9dcf02fe6bd0646330dbc121dfc3c6193`
(`PA15: lower typed discarded and returned expressions`), parent
`2a10382f8abc0ff44ab7712f62c23f530441a63a`, and its final bounded
audit/repair commit. PA11 owns canonical types/constants, PA12 owns typed
conversion and return facts, PA15 owns typed LowIR lowering and declaration
demand, and PA13 owns serialization/validation.

The final `make test-pa15` gate exited 2 at `103/109`, `109/109` covered,
with exactly the six unchanged residuals in the plan. Mechanical comparison
of the sorted names recorded zero added/replacement names and zero missing
names in `/tmp/pa15-final-failure-set-proof.log`. The exact prior gate exited
0 at `1058/1058`; the root through-PA15 gate exited 2 at `1161/1167`, which
is the fully passing prior total plus the 109 PA15 cases and the same six
residuals. Complete logs are `/tmp/pa15-final-checkpoint-test-pa15.log`,
`/tmp/pa15-final-checkpoint-through-pa14.log`, and
`/tmp/pa15-final-checkpoint-through-pa15.log`.
The preserved names are `100-const-integral-lvalue-overload-category`,
`100-string-hex-escape-code-unit`, `100-unnamed-parameter-storage`,
`200-goto-case-block-entry-label`, `200-goto-case-block-label-after-statement`,
and `200-nested-conditional-array-decay`.

The end-to-end ownership trace is coherent. PA12 records `ToVoid`, including
void-to-void casts, and PA15 lowers the discarded source for its side effects
while emitting typed `return void` without an unnecessary scalar load. PA12
rejects a non-void expression in a void return, contextualizes non-void
returns, and represents supported bool/integral/floating/pointer `return {}`
as a typed zero; PA15 preserves the type, cv/category, and literal payload
through LowIR. Comma and discard lowering evaluates the left side once,
preserves a value-producing right lvalue/xvalue when it is consumed, and
avoids materializing a discarded right-hand value in expression statements,
for-init, and iteration paths. The C++11 volatile exception is retained at
the typed boundary: a scalar volatile lvalue glvalue is loaded when discarded,
including through a reference-to-volatile binding; nonvolatile reference
identifiers remain address-only.

The bounded repair closes two late owner gaps found by this audit. A
follow-up volatile audit then narrowed the value-elision shortcut so it cannot
claim a volatile reference is side-effect-free. The
ForInit expression path now calls the common discarded-expression owner. That
owner recursively handles comma expressions and treats a bare discarded
nonvolatile function/reference identifier as a side-effect-free value,
avoiding a storage/value load while retaining assignment/call side effects;
its typed volatile-scalar check routes volatile glvalues through one value
load. Value-context logical
lowering now uses a per-semantic-fact cached proof for literal bool/integer
truth and recursively omits only a provably unreachable RHS; runtime and
unknown paths still lower normally. Condition lowering uses the same recursive
proof, so nested literals cannot leak an unreachable call/address declaration.
The cache fails closed for unknown or malformed facts and avoids repeated
subtree work; there is no textual reconstruction, broad DCE, whole-program
retry, or declaration sweep.

Function declaration demand remains rooted in lowered reachable calls,
function addresses/references, and typed global relocations, with one
deterministic declaration per demanded binding. The focused logical probe
confirms an unreachable `extern` call is not demanded; owner probes 400--404
retain the direct-call/address, typed-global-relocation, reference, and
linkage coverage. The five removed handout tests, adjacent matrix, and one
narrow owner regression passed; no handout test or `.ref` fixture changed.

Focused evidence for this checkpoint is:

- the five residual-owner tests pass `5/5` and the representative adjacent
  matrix passes `7/7`;
- `cppgm.tests/course/pa15/405-typed-discarded-return-regression.sh` passes,
  including typed empty-brace bool/integral/f32/f64/f80/pointer returns,
  void-to-void discard, for-init comma discard, nested/value logical pruning,
  evaluated runtime call/address demand, direct volatile-object and
  reference-to-volatile discard loads, and `lowir2cy86` validation;
- the current `dev/cppgm++` build passes, owner probes 400--404 pass, and the
  five removed inputs plus 405 compile and validate with `lowir2cy86`.

The PA15 file audit exits 0 with five known header-division warnings
(`/tmp/pa15-final-file-audit.log`), and `git diff --check` exits 0. Moving
`lower_logical` to the existing flow owner keeps
`dev/src/pa15_lowering.cpp` at 2945 lines, below the 3000-line limit; no new
source set or parallel lowering implementation was introduced.

Fresh candidate-only performance evidence is retained at
`/tmp/pa15-checkpoint-audit-discarded-final.fRe1n0`: immutable candidate mode
`0555`, SHA-256
`f03860f091c4a646af8937a7e9023e79facd1a6aebb4c885d408d1a81f16d95e`, six
affected-path inputs, five interleaved forward/reverse rounds, 20 compilations
per sample, verified implementation/input/toolchain/LowIR hashes, and zero
compile/validator statuses. Median wall time per invocation is
`0.0025--0.0040 s` and median RSS is `5124--5384 KiB`; these bounded
candidate-only measurements are not comparative or universal claims.

The six unchanged residuals—three `100-*` surfaces, two goto-label cases,
and nested conditional array decay—remain outside this audit and define the
next checkpoint. No unrelated source, handout test, or `.ref` fixture was
changed.

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
  `/tmp/pa15-final-enum-perf.relational-final.ES1ytx/cppgm++-final-immutable`,
  mode `0555`, SHA-256
  `65df5ea3b49bff32cfbf76866001ddc67b89874d18fe7b36bad1704781a3c67e`.
  Five interleaved ascending/descending rounds, proportional selected-chain
  counters, and medians are recorded in that directory's `structure.tsv`,
  `timings.tsv`, and `medians.tsv`.

## Fresh Performance Evidence — selected nested conditional ownership

The immutable candidate is
`/tmp/pa15-final-enum-perf.relational-final.ES1ytx/cppgm++-final-immutable`, mode `0555`,
SHA-256
`65df5ea3b49bff32cfbf76866001ddc67b89874d18fe7b36bad1704781a3c67e`.
The directory contains the candidate hash/mode records, bounded source
inputs, semantic and LowIR outputs, `structure.tsv`, `timings.tsv`, and
`medians.tsv`. Each input retains the bounded enum/default/promotion/operator
coverage and replaces the flat conditional with a selected nested chain of
depth 16, 64, 256, or 512. The `structure.tsv` counters record the selected
chain depth and semantic conditional-node count; five timing rounds alternate
ascending and descending size order.

| depth | input bytes/lines | semantic bytes/lines | LowIR bytes/lines | semantic conditional nodes | LowIR globals/functions | median wall/user/sys s | median max RSS KB |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 16 | 1871/67 | 7885/166 | 4544/167 | 18 | 20/7 | 0.00/0.00/0.00 | 5380 |
| 64 | 4559/211 | 28765/406 | 7424/215 | 66 | 68/7 | 0.00/0.00/0.00 | 6084 |
| 256 | 15935/787 | 250837/1366 | 19412/407 | 258 | 260/7 | 0.01/0.01/0.00 | 8680 |
| 512 | 31295/1555 | 891093/2646 | 35540/663 | 514 | 516/7 | 0.02/0.01/0.01 | 12676 |

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
| Current | PA15 typed discarded/returned-expression ownership at `98e75ff9dcf02fe6bd0646330dbc121dfc3c6193` plus final bounded audit repair | Final `make test-pa15` `103/109`, all `109/109` covered, exact six residuals preserved with zero added/replacement or missing names; through-PA14 `1058/1058` exit 0; through-PA15 `1161/1167` with the same six; focused owner matrix `5/5`, adjacent matrix `7/7`, owner regression 405 (including volatile discard and nested logical demand), owner probes 400–404, and affected-path `lowir2cy86` validation pass. PA12 owns typed `ToVoid`, return conversion, cv/volatile and empty-brace facts; PA15 owns discard/return/logical lowering and demand roots. Fresh immutable candidate-only artifact `/tmp/pa15-checkpoint-audit-discarded-final.fRe1n0` is hash-verified; file audit, diff-check, and checkpoint commit complete. |
