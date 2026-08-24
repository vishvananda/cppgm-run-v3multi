# PA12 audit record

## Current Checkpoint Review

This bounded `checkpointAudit` reviews landed commit `61b60cb1a311bfdbb30eb57e4c684db7d92dc83a`
(`pa12: preserve block lookup provenance`) relative to `994a700040545debc8b2ed200a7652d49a093a43`.
The supplied full primary log gives the exact starting baseline: **120/166
passing**, **46 failures**, all 166 covered. The final broad run is identical:
**120/166 passing**, **46 failures**, all 166 covered, with **0 current-only**
and **0 baseline-only** paths. The earlier through-PA11 gate is **685/685**;
the seven checkpoint paths listed in `pa12/plan.md` remain successful.

Ownership trace: the semantics driver creates one `PPPreprocessingSession` per
input, passes its typed `PPTokenBuffer` to `parse_pa10_ast`, and passes one
`PA10Ast` through one `PA11SemanticModel`. The preprocessor's typed token
identity/source-location buffer is retained through PA10; PA10 owns source-node
identity, qualified-name components, literal decoding, and child order. Its
landed declaration boundary uses token identities plus the existing constant
declaration-follow predicate, not rendered spelling. PA11 owns canonical
`TypeId`, `ScopeId`, `BindingId`, `NamePath`, function types, parameter-pack
shape, declaration facts, and deterministic lookup.

The block path forms `SimpleDeclaration`, `AliasDeclaration`, namespace alias,
using-directive, and using-declaration nodes once in source order. Value lookup
entries are typed `(BindingId, origin ScopeId)` pairs, so a using-declaration
retains the canonical source binding and provenance without cloning a semantic
declaration. Raw using-directive edges remain available for qualified,
transitive, inline, and cyclic graph traversal. Effective entries are placed at
the scope-tree common ancestor; one lexical ancestor mark pass filters them for
an unqualified query, while generation marks terminate repeated graph edges and
preserve source order. Direct declarations and nominated overloads are merged
at their common ancestor; direct/type precedence remains level-directed.

The direct-substatement audit found that PA10 accepts declaration starts on
the `parse_statement` path (`dev/src/pa10_ast.cpp:2360-2361`); `--emit-ast`
confirmed `using-directive`, `using-declaration`, and `alias-declaration`
children under unbraced branches and case/label edges. PA12 now forms those
nodes through `process_declaration` in the implicit substatement scope, while
`prepare_pa12_compound` skips direct declaration children already formed by the
PA11 source-order pass. Case/default/label recursion reaches the same formation
path without double-processing compound children. A valid labeled-alias probe
still reaches the pre-existing unsupported `LabeledStatement` semantic path;
an invalid labeled target is rejected during preparation.

`process_using_declaration` now distinguishes typed function entries from
non-function collisions. Existing and incoming function bindings merge in one
same-scope overload set while retaining each `(BindingId, origin ScopeId)`;
exact repeated pairs are skipped, and one dump view is retained per imported
canonical binding. A non-function collision with a local value, another
non-function import, a function/non-function mix, or an imported type remains an
error.

Direct initialization uses PA11 target lookup and PA12's
`semantic_expression_for_target`/conversion path. The selected function fact
retains canonical `BindingId` and source `ScopeId`; the target conversion and
result category are recorded in typed PA12 facts. The cold renderer walks fact
ranges and renders typed IDs only at the requested output boundary.

This final audit preserves the two earlier bounded repairs: `Scope::depth` is
carried by PA12's separate `create_internal_scope`, and top-level aliases use a
cached canonical `TypeAlias` fact instead of dump-time AST type reconstruction.
It adds the direct-substatement declaration formation and typed using-function
merge/dedup repairs above. No tests, references, fixtures, grammar, harness,
script, or generated repository file was changed.

Focused validation:

- `make -C pa12` passed.
- The seven fixed checkpoint paths passed **7/7** with exact dumps.
- The seven checkpoint paths plus seven nearby ownership/lookup controls passed
  **14/14**. The additional qualified-`decltype` control
  `pa12/tests/general/300-qualified-direct-function-hides-using-directive.t`
  remains its supplied residual failure and was not repaired here.
- Valid direct selection/iteration, alias, namespace-alias, case, nested-scope,
  direct-target, same-scope using-function merge, and repeat-import probes
  exited 0. Invalid target, collision, and post-substatement non-leakage probes
  exited 1. The merge dump selected both `left::f` and `right::f`; repeated
  `--emit-types` showed one imported block `f` view, while the two-namespace
  merge showed two.
- Three immutable repeated runs for nested depth, direct-target, and
  cyclic/transitive probes each produced exactly one output hash. The valid
  labeled probe remains the known unsupported labeled semantic control; its
  invalid-target counterpart fails during declaration preparation.
- `make test-pa12` exited 2 with **120/166** passed and **46** failures. Its
  complete failure-path set normalized against
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` as
  **46 baseline / 46 current / 0 current-only / 0 baseline-only**; no checkpoint
  gain regressed.
- The exact through-PA11 command passed **685/685**.
- `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src` passed with
  exactly two known `[warning][bad-division]` findings: substantial
  implementation bodies in `dev/src/cpp_semantic_core.h:1` and
  `dev/src/pa11_semantic_model.h:1`.
- `git diff --check` passed, and the changed-path audit from `61b60cb1` found
  exactly the four approved paths with no tests, refs, fixtures, grammar,
  harnesses, scripts, or generated repository artifacts.

The refreshed audit-repair performance sample uses immutable executable
`/tmp/pa12-checkpoint-audit-immutable-61b60cb1-direct-using`, SHA-256
`2fc6d0b184ed8a5a6aea86224607b9ef4ad3b04f93975040e3aab0b2156a680b`, and the
equivalent hashed inputs/structural counters in `pa12/plan.md`. Five
interleaved rounds timed ten invocations per case: 50/50 successful
invocations per case, one unique dump hash per case, and refreshed median batch
wall/user/system/RSS of **1.06/0.03/0.06/7206 KiB** (small) and
**1.06/0.03/0.06/7194 KiB** (large). The coarse timing supports bounded
lexical-mark/effective-entry and reachable-graph work, but is not a fine-grained
scaling coefficient or a full-suite performance claim.

Residual risks are the exact 46 final paths, including later expression/control,
anonymous-namespace, enum, and `decltype` families; no attempt was made to
re-audit those. The valid labeled probe remains outside this repair because
`semantic_statement` has no `LabeledStatement` case. The final broad
normalization and the two known file-audit warnings are recorded above. A
future residual-family pass is separate from this completed checkpoint audit.

Next checkpoint: a separately scoped residual-family audit, if undertaken; this
block/alias/using/direct-initialization checkpoint is complete.

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
| 2026-08-24 PA12 final bounded `checkpointAudit` from `61b60cb1` | Traced the complete PPPreprocessingSession → PPTokenBuffer → PA10Ast → PA11 canonical identity/lookup → PA12 typed-fact/cold-dump path. Confirmed one-pass source-order block declaration formation, direct unbraced/label/case preparation without compound-child duplication, `(BindingId, origin ScopeId)` provenance, typed same-scope function-using merge/dedup, common-ancestor effective directives, generation-marked transitive/cyclic lookup, target-directed direct initialization, and deterministic typed rendering. Preserved internal-scope depth propagation and top-level alias fact rendering. | Final `make test-pa12`: **120/166**, **46 failures**, all 166; normalized **46 baseline / 46 current / 0 current-only / 0 baseline-only**; focused checkpoint **7/7**, expanded controls **14/14**; valid direct selection/iteration/alias/case/nested/merge/repeat probes exited 0; invalid target/collision/non-leakage probes exited 1; merge selected `left::f` and `right::f`, repeat `--emit-types` had one imported block view; refreshed immutable five-round interleaved probes were 50/50 successful per case with one unique dump hash per case; median small/large wall/user/system/RSS **1.06/0.03/0.06/7206 KiB** and **1.06/0.03/0.06/7194 KiB**. Through-PA11 was **685/685**; file audit passed with exactly the two known header-division warnings; diff/path audit passed for exactly the four approved paths. | The exact 46 final residual paths, qualified-`decltype` control, and valid labeled semantic control remain excluded from this bounded checkpoint. The coarse performance timing is structural evidence, not a fine-grained coefficient; no full-suite performance claim is made. | Exact `make test-pa12`; supplied-log normalization; exact through-PA11 command; exact file audit; exact 7-test and 14-test focused checks; out-of-tree direct/collision/merge probes; immutable repeated probes; refreshed five interleaved 10-run samples; `git diff --check`; changed-path audit from `61b60cb1`; final checkpoint-audit commit. |
