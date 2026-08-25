# PA15 Audit

## Current Checkpoint Review

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

## Fresh Evidence

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

## Representative Performance Evidence

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
| Current | PA15 full-stage / checkpointAudit — typed global pointer null/zero initializers | Hardened transactional PA12 literal snapshots and binding invariants; kept PA15 on `ConstantAddressFact` identity/ranges; tightened terminal/destination-safe typed null chains and pointer cv behavior; corrected lvalue loads and runtime `nullptr`-to-bool typing; target matrix `10/10`, narrow regression pass, final PA15 `70/109` with the exact unchanged 39-name set and all `109` covered, through-PA14 `1058/1058`, file audit pass, diff-check pass, and fresh immutable performance evidence above. |
