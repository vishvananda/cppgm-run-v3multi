# PA15 Audit

## Current Checkpoint Review

This review covers `d8d925563ea16945fa92a566f86fb743590e81c5`
(`PA15: lower typed floating scalar conversions`), parent `ea846ea4`, and
the bounded checkpoint-audit repairs. PA11 remains the owner of canonical
types and constants, PA12 the owner of selected `ConversionFact` chains,
call defaults, and constant-address publication, PA15 the typed LowIR
lowering owner, and PA13 the serializer/validator contract.

The earlier audit repairs remain intact: `lower_call` follows each recorded
argument chain and lets `ReferenceBinding` own pointer-prvalue temporary
storage and function-reference address flow; PA12 publishes same-type
lvalue-to-rvalue facts for supported variadic integral, floating, and pointer
scalars while leaving fixed arguments, decay, promotions, and addresses under
their existing owners. Floating lowering retains signed/unsigned conversion
operators and f32/f64/f80 truth/conversion widths. `lower_logical` compares
each RHS in its physical scalar type, with no floating-source gate. Only
condition consumers omit final bool materialization; value consumers use
ordinary `lower_expression`, including initializer, assignment, return, call,
and unary value paths.

The final repair restores the typed boundary after a canonical comparison or
logical result. Before dispatching any non-Identity recorded conversion,
including `ReferenceBinding`, when its source is semantic bool,
`apply_conversions` emits `convert trunc u8 i64` from canonical physical truth,
replaces the result with physical/semantic u8, and then dispatches the recorded
conversion (for example a typed reference store, `zext i32 u8`, or
`sitofp f64 u8`). It never retags the i64 operand or removes the semantic
conversion. Identity bool materialization and condition-only omission remain
separate paths. 404 now scopes this rule through bool-to-integer,
bool-to-floating, direct variadic, const-bool-reference, and bool-rvalue-
reference consumers, in addition to bool stores/returns and the existing
floating/integral coverage.

Exactly six existing PA15 refs changed for this canonical-bool-to-nonbool
correction: `100-enum-default-argument-constant-fold.ref`,
`200-extern-c-internal-functions-stay-distinct.ref`,
`200-extern-c-internal-header-const.ref`,
`200-pointer-operator-array-decay.ref`,
`200-postfix-incdec-evaluates-lhs-once.ref`, and
`200-prefix-incdec-lvalue-address.ref`. Each change is limited to the explicit
bool materialization, its recorded conversion, and deterministic temp-number
shifts. Current enum/extern/header/prefix refs pass `lowir2cy86`; pointer and
postfix full files retain unrelated pre-existing validator diagnostics
(`invalid binary type` and `scalar global initializer type mismatch`). Their
reduced current sequences pass, while the reduced old sequences fail exactly
with `conversion operand type mismatch`. The complete current/old transcript
and reduced probes are retained at
`/tmp/tmp.YWSo4HvvH9/summary.tsv`. No reference tool, unrelated fixture, or
presentation-only edit was used. The complete amended-checkpoint fixture list
is these six plus the prior four refs:
`100-unary-logical-conditional.ref`,
`200-reference-parameter-temp-name-collision.ref`,
`200-function-reference-static-cast-call.ref`, and
`200-floating-logical-branch.ref`.

This ordering repair changed no existing `.ref`; the six refs above remain the
only fixtures changed by the canonical-bool-to-nonbool correction. A reduced
reference-binding LowIR proof is retained at
`/tmp/pa15-audit-reference-order-proof.0NdxJb`: the corrected sequence
validates, while the pre-bridge `store u8` of physical i64 is rejected with
`store value type mismatch`.

Final gates: focused six is `PASS (6/6)`
(`/tmp/pa15-audit-focused-reference-order.log`); owner probes 400–404
all exit zero (`/tmp/pa15-audit-owner-probes-reference-order.log`).
`make test-pa15` exits 2 only for the authoritative residual set and reports
`98/109`, with `109/109` covered. The mechanical proof is
`/tmp/pa15-audit-failure-set-reference-order.log`:
failure count `11`, missing `0`, new/replacement `0`. The exact required
through-PA14 command reports `1058/1058`
(`/tmp/pa15-audit-through-pa14-reference-order.log`). File audit passes
with five known header warnings
(`/tmp/pa15-audit-file-audit-reference-order.log`), and
`git diff --check` passes (`/tmp/pa15-audit-diff-check-reference-order.log`).

The refreshed §7 artifact is
`/tmp/pa15-checkpoint-audit-perf-reference-order.cgmxrA`. Its immutable
`cppgm++` is mode 555 with SHA-256
`2d2310eaecaa41fd623c317788a5de85615b4d6b13a25dcf1fbda4ff6924347d`.
Seven bounded inputs retain LowIR, validator, structural-counter, raw-timing,
median, and hash artifacts. Five interleaved forward/reverse rounds used 20
compilations per sample; candidate-only medians are `0.0030s` per invocation
for each retained selected input and `0.0060s` for `owner_404`. RSS medians
are `5132–5360 KiB` for the selected inputs and `5956 KiB` for `owner_404`.
Selected structural rows range from 19–89 lines and 0–5 conversions;
`owner_404` is 569 lines, 32 conversions, 6 calls, 13 comparisons, and 70
slots. These are bounded affected-path measurements, not universal or
comparative performance claims.

The exact remaining uncertainty is the unchanged 11-name set in the plan:
overload/category, string decoding, unnamed-parameter storage, comma/xvalue,
control-flow labels/iteration, literal short-circuit, nested array decay,
void-call return, and empty-brace scalar return remain outside this checkpoint.

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
| Current | PA15 full-stage / checkpointAudit — typed floating scalar conversion ownership (`d8d925563ea16945fa92a566f86fb743590e81c5` + bounded audit repair) | Final `98/109`, all `109` covered, exact authoritative 11-name set retained, failure count `11`, missing `0`, new/replacement `0`; focused matrix `6/6`, owner probes 400–404 including const/rvalue bool-reference value flow, no new fixture changes, through-PA14 `1058/1058`, file audit pass with five known warnings, diff-check pass, and immutable candidate-only interleaved performance artifact `/tmp/pa15-checkpoint-audit-perf-reference-order.cgmxrA`. PA12 publishes same-type variadic scalar lvalue-to-rvalue facts; PA15 materializes canonical truth as typed u8 before every recorded non-Identity conversion, including ReferenceBinding, and keeps condition-only omission. |
