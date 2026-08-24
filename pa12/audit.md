# PA12 audit record

## Current Checkpoint Review

This bounded `checkpointAudit` reviews landed commit
`5fa28b376f3c6f2200b104c94adc817481ef6089` (`pa12: resolve callable targets
and overloads`) relative to `498043c5`. The worktree was clean at turn start.
The post-repair full-stage result is **113/166 passing**, **53 failures**, all
166 covered; the through-PA11 gate is **685/685**. Failure normalization
against `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`
found **53 baseline**, **53 current**, **0 current-only**, and
**0 baseline-only** paths. This review is finalized in the checkpoint-audit
commit.

Fact trace: the driver still creates one `PPPreprocessingSession`, passes its
typed `PPTokenBuffer` to `parse_pa10_ast`, and passes one `PA10Ast` to one
`PA11SemanticModel`. PA10 owns source-node identity, qualified-name components,
literal decoding, and child order. PA11 owns canonical `TypeId`, `BindingId`,
`ScopeId`, function types, parameter-pack shape, and deterministic lookup. The
new `lookup_type_path` fallback consults that type owner first, then supplies
the assignment vocabulary's unqualified `nullptr_t` fundamental only when no
ordinary binding exists.

For a call, `semantic_call_expression` collects a direct candidate vector only
when the callee `IdExpression` resolves entirely to namespace/function
bindings. A variable or parameter of function, reference-to-function, or
function-pointer type instead becomes the typed indirect callee; its callable
function type determines arity, fixed-prefix conversions, and the result's
reference category. Ordinary arguments are analyzed once. An overloaded
function ID in a target slot is held as an invalid `ExprInfo`, so candidate
trials publish no semantic or dump fact. `resolve_function_id_target` performs
typed lookup and conversion ranking only; after the winning outer candidate is
known, one selected `IdExpression` fact is created with its `BindingId` and
`NamePath`, then `apply_context_conversion` attaches the selected
`ConversionFact` (including function decay, null zero/nullptr_t, qualification,
or supported temporary reference binding).

Each viable direct candidate retains one rank per argument. The component-wise
ordering rejects a candidate that is worse in any fixed slot, prefers a
non-variadic candidate when all ranks tie, gives ellipsis a rank above every
supported standard conversion, and rejects incomparable maxima. The selected
call fact retains the winning `BindingId`/`ScopeId`, canonical function result,
and lvalue/xvalue result category. The cold renderer walks typed child ranges
and selected bindings in source/lookup order; it renders names and types only
at the requested dump boundary and never reparses text.

The bounded source repairs in this audit are the `nullptr_t` lookup fallback
and the named-pack walk. A legal user alias such as `using nullptr_t = int;`
now remains the canonical type instead of being shadowed by the synthetic
vocabulary type. Named pack markers under nested pointer/reference declarators
now make the enclosing function variadic, while nested function-type clauses
do not leak that property outward. The callable-resolution implementation
otherwise remains unchanged. No test, reference, grammar, harness, or script
was changed.

Focused validation:

- `make -C pa12` passed.
- The ten checkpoint paths plus 18 nearby positive/negative controls passed
  **28/28** with exact dumps and expected statuses.
- Out-of-tree probes passed for qualified and parenthesized function IDs in
  pointer/reference initialization and arguments, named packs in direct and
  nested pointer/reference declarators, a nested variadic function-type
  parameter control, and `using nullptr_t = int`; the alias dump resolves the
  parameter as `int`.
- The required `make test-pa12` exited 2 with **113/166 passing**, all 166
  covered, and the exact 53 residual paths preserved verbatim in
  `pa12/plan.md`; normalized failure comparison is 53/53 with zero
  current-only and zero baseline-only paths.
- The required `n=12; if [ "$n" -le 1 ]; then ...; else make
  test-report-through-pa$((n - 1)); fi` command printed
  `===== ALL TESTS PASSED SUCCESSFULLY! (685 / 685) =====`.
- `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src` passed with
  exactly the two supplied header-division warnings, and `git diff --check`
  passed. The changed-path audit found only the four approved changed files;
  no tests, references, fixtures, grammar, harnesses, scripts, generated
  artifacts, or unrelated files changed.

Performance evidence was materially stale because the prior cited immutable
artifacts were absent. A fresh immutable replacement uses executable
`/tmp/pa12-cppgm-checkpoint-audit-v2-immutable` (SHA-256
`3e2d092dc1cf8968187179e30d9f9c447079034d297ca8aca635e594000551e7`) and
unchanged out-of-tree inputs with hashes recorded in `pa12/plan.md`. Both
inputs have A=4 and D=1; C/F are (8,4) and (31,32), so C*F grows 31x while
the fixed call shape is equivalent. Five interleaved 40-run samples were
200/200 successful per case; median wall time was 0.14s and 0.27s, with user,
system, RSS samples and byte-identical dumps recorded in the plan. The timing
includes parsing, semantic traversal, and cold rendering, but the measured
work matches the source's C*(A-D) + C*D*F bound and shows no worse-than-bounded
candidate scaling.

Remaining bounded risks are the supplied 53 residual paths, including the
explicit pointer-bool-cast and integer-zero conditional families. Parenthesized
IDs in target contexts pass; address-of an overloaded function and an
overloaded parenthesized callee remain outside this helper's current target-ID
shape and are not claimed by this checkpoint.

Next checkpoint: a separately authorized residual-family pass beginning with
the namespace/using and expression/control paths in the 53-path map. PA12 is
not complete.

## Prior checkpoint context

The preceding audit row at `eee242c6` established the shared PA12 semantic-fact
foundation and recorded the 90/166 checkpoint, its 76 residual failures, the
through-PA11 result, and the prior qualification/function-redeclaration
repairs. That residual family remains outside this bounded review.

## Audit ledger

| audit row | findings and ownership trace | evidence | uncertainties / residual exclusions | exact validation |
| --- | --- | --- | --- | --- |
| 2026-08-24 prior PA12 audit at `eee242c6` | Shared typed semantic owner, qualification safety, canonical function redeclaration/definition state, and lexical dump views were recorded before the structured-statement increment. | Prior checkpoint record: PA12 90/166 with 76 residual failures; through-PA11 685/685; performance and file-audit evidence preserved. | Broader PA12 residual families were explicitly excluded. | See the prior committed audit record in history. |
| 2026-08-24 PA12 `checkpointAudit` at `47ca58be` | Complete ownership path reviewed; shared fixed-target promotion, exact converted-case representability, scoped-enum legality, compact 64-bit case payload continuity, empty control substatements, child order, lexical jump validation, deterministic cold rendering, bounded indexes, and final file-audit shape are repaired or confirmed. | Final PA12 **103/166**, **63 failures**, all 166 covered; through-PA11 **685/685**; focused **22/22**; temporary probes **17/17 expected outcomes**; normalized failure paths: 63 baseline, 63 current, 0 current-only, 0 baseline-only; `SemanticFact` layout 144-to-136 bytes. | Future residual-family work requires separate authorization; PA12 is not complete. The two header-division warnings are preserved known findings. No tests, refs, harnesses, grammar, or scripts changed. | `make test-pa12` (exit 2, 103/166); exact through-PA11 command; `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src` (pass, 2 warnings); `git diff --check` (pass); changed-path audit. |
| 2026-08-24 PA12 `checkpointAudit` at `5fa28b37` | Callable ownership traced from PA10 IDs through PA11 lookup/types/bindings to PA12 deferred `FunctionIdResolution`, fact-free candidate scoring, selected conversions, call result categories, and deterministic cold rendering. Repaired ordinary `nullptr_t` alias lookup precedence and nested named-pack detection; confirmed qualified/parenthesized target IDs. | Post-repair PA12 **113/166**, **53 failures**, all 166; through-PA11 **685/685**; focused checked-in **28/28**; out-of-tree target/pack/alias probes passed; fresh immutable C/A/D/F evidence was 200/200 successful per case with byte-identical dumps; normalized comparison 53 baseline, 53 current, 0 current-only, 0 baseline-only. | The exact 53 residual paths remain outside this checkpoint; overloaded address-of and overloaded parenthesized-callee forms are not claimed. PA12 is not complete. | `make test-pa12` (exit 2, 113/166); exact through-PA11 command; `make -C pa12`; exact 28-test `make -C pa12 check`; fresh 5x40 interleaved immutable probe; file audit passed with the two known warnings; `git diff --check`; changed-path audit. |
