# PA12 audit record

## Current Checkpoint Review

This final `checkpointAudit` reviews landed commit
`613b9cb62e7664eda1462f2a4dedf53f08223101`
(`PA12: close declaration lifetime and template residuals`), parent
`fbb9dd85`. It closes the three residual fixture paths and finds no further
source blocker in the deliberately narrow PA12 bridge. The final audit is
documentation-only; the implementation under review is the clean `613b9cb6`
baseline.

### Pipeline and typed ownership

The production route is `run_emit_semantics_mode` ->
`PPPreprocessingSession` -> classified `PPTokenBuffer` ->
`PA10Ast` -> one `PA11SemanticModel` -> PA12 facts ->
requested cold dump. `PA11SemanticModel` owns canonical
`TypeKey`/`TypeId`, `BindingId`, `ScopeId`, `NamedRecordId`,
`TemplateFunctionId`, `TemplateSpecializationId`, `SemanticFactId`, and
`ConversionFactId`. PA10 owns syntax and indexed lookahead; PA11 owns
declaration, type, scope, and lookup identity; PA12 owns expression,
conversion, lifetime, and rendering facts. The source contains no reference or
host-toolchain shell-out, rendered-text reparse, parallel semantic model, or
whole-program retry in this path. Text is retained only as source/presentation
data and is rendered at the requested dump boundary.

### Representative ownership paths

1. **Global-qualified local extern.** At
   `PA10Parser::declaration_start`, a leading `::` is classified by
   `PA10ParserSupport::qualified_declaration_start` from the prebuilt
   delimiter and parenthesized-group facts. Its reported work is charged once;
   the ordinary parser consumes the declaration. PA11 resolves the declaration
   scope, applies the one-pass declarator prefix/suffix split, normalizes
   embedded function types and adjusted parameters, and preserves the
   unadjusted source type in `BindingSidecar` when PA11 presentation needs it.
   PA12 then records the local function binding and pointer call from typed
   facts. The checked-in local-extern dump matches byte-for-byte.

2. **Canonical record display.** A global-qualified type occurrence reaches
   `remember_type_display_path`, which first checks
   `canonical_type_display_path` against the `NamedRecordId` owner/name
   chain. Only that canonical path can populate the sparse named-record
   sidecar; typedef and namespace-alias spellings are rejected. The cold
   renderer follows the typed sidecar and never uses an alias spelling as
   record identity. Alias-only, canonical-only, and both encounter-order
   probes produced the expected `struct S` versus `struct n::S` views.

3. **Local record lifetime.** `semantic_declaration` checks block scope,
   defined non-union class kind, empty class scope, and no initializer before
   calling `semantic_constructor_action`. The action obtains the
   `NamedRecordId`, creates at most one sparse-sidecar synthetic constructor
   `BindingId`, attaches the constructor call/action to the variable fact, and
   lets the cold dump emit the action and synthetic definition. Anonymous unions
   remain on their existing constructor route. This is a narrow lifetime fact,
   not constructor overload resolution.

4. **Template bridge.** Template declarations form `TemplateFunctionFact`
   records and a `NameId` candidate index. Explicit type arguments and direct
   `T`-deduction remain `TypeId` facts. Selection uses the exact
   `TemplateSpecializationKey(TemplateFunctionId, vector<TypeId>)` flat index.
   Each indexed fact is retained through explicit `NotStarted`,
   `InProgress`, `Complete`, or `Failed` transitions; a retained failed
   key does not repeat substitution. The completed binding carries a sparse
   specialization sidecar, and semantic facts retain the selected binding and
   scope through conversions to cold rendering.

   The supported exception is intentionally narrow: one-component names bound
   to global template declarations. The checked-in residual fixture uses the
   unqualified `&hello<stream>` spelling; a focused out-of-tree probe with the
   global-qualified `&::hello<stream>` spelling also passed. The direct `take(T)`
   demand is likewise bound to the global template declaration. The candidate
   list is name-indexed and each candidate is checked against the visible scope
   ancestry. Namespace-qualified template names and nested same-name template
   hiding are not claimed by PA12; focused probes rejected those forms (the
   nested case reported ambiguity) rather than producing an unowned or
   text-recovered answer. General templates, namespace-alias
   template lookup, dependent lookup, non-type arguments, and
   template-aware overload ranking remain out of scope.

### Spec alignment and findings

- §§1-2: the above is one forward production model with typed continuity;
  display strings are cold presentation only.
- §3: selected declarations, conversions, value categories, lifetime actions,
  and the narrow specialization binding are recorded at their owners. Exact
  specialization keys and all four lifecycle states are explicit.
- §4: parser classification uses indexed facts and one charge; declarator
  splitting is one child pass; display walking follows canonical owner depth;
  lifetime work is constant per eligible object; template work is limited to
  the indexed same-name list, visible-scope checks, type traversal, and exact
  key lookup. No global specialization-fact scan or whole-program retry is
  present.
- §7: structural counters, immutable repeated dumps, exact output comparisons,
  stage totals, and file-audit evidence are recorded without claiming timing,
  asymptotic, or generated-code results that PA12 dump mode cannot establish.

No additional source repair, test, reference, harness, grammar, or script
change was justified. The only documentation blocker was this stale review
header and ledger; it is corrected here. The remaining uncertainties are the
deliberately excluded general template/candidate cases and the absence of
generated-object evidence in a semantics-dump assignment.

### Focused validation and performance evidence

The focused build passed with `make -C pa12 -j2`. Direct checked-in-output
comparisons passed for the exact residual trio
`300-local-extern-function-declaration.t`,
`300-reference-binding-pointee-const-pointer.t`, and
`300-static-cast-overloaded-function-template-argument.t` (`3/3`),
six nearby PA12 controls (`6/6`), two PA10 qualified-declarator controls
(`2/2`), and four PA11 type/declarator/record controls (`4/4`). The nine
PA12 dumps each had exit `0` and `cmp` exit `0`. Current layout probes
report `SemanticFact=136`, `TemplateSpecializationFact=48`,
`BindingSidecar=48`, and `TemplateSpecializationKey=32` bytes.

The fresh immutable structural probe generated two global templates
`hello(T)` and `take(T)`, `n` distinct empty records, and `n` direct
`take(static_cast<void(*)(Ti)>(&hello<Ti>))` demands. Thus each scale has
`2n` distinct specialization demands and complete specialization declaration
lines. Three runs per scale produced:

| distinct records `n` | demands / complete facts | output bytes | triplicate SHA-256 |
|---:|---:|---:|---|
| 4 | 8 / 8 | 3046 | `f9c9eb757b400d36b809884e9c6119abbeb01381a282d2097aa54e17c2db603ea4` |
| 8 | 16 / 16 | 5930 | `a9cd17c294bfa227b440c54a87f4d9be173c72f398b0ef7acf119e68990b85e5` |
| 16 | 32 / 32 | 11752 | `58ab8d80f01744073bf9b102dea2b4672fe308d7be6aebc08f5d57d038a38dcc` |
| 32 | 64 / 64 | 23432 | `4ca4ef49f7baf3d315307cffade765e7650c36ccdad3e3963fb7da5008cb9223` |

The demand/fact counts are structural observations for this generated shape;
the source trace confirms each demand uses the exact flat index rather than a
global fact scan. The hashes are dump determinism evidence only. No timing,
RSS, asymptotic coefficient, generated-code, or self-hosting claim is made.

The authoritative source checkpoint for this audit is commit
`613b9cb62e7664eda1462f2a4dedf53f08223101` (`613b9cb6`), with the source tree
clean before this documentation change. Fresh Phase 2 validation of this
two-file audit diff reported: `perl scripts/cppgm_file_audit.pl --stage pa12
--paths dev/src` exit `0`, with exactly two retained `bad-division` warnings
at `dev/src/cpp_semantic_core.h:1` and `dev/src/pa11_semantic_model.h:1`;
`make test-report-through-pa12` exit `0`, `851/851` across all `12/12` stages;
and `git diff --check` exit `0`. The exact changed-path audit found only
`pa12/audit.md` and `pa12/plan.md`, with no test, reference, harness, grammar,
script, or source path changed; the pre-commit short status contained only
those two documentation paths.

## Prior checkpoint context

The immediately preceding audit row at `4f890322` established the typed
member-pointer target path and recursive array qualification repair. Earlier
rows remain preserved below; the immediately prior row at `115aff98` is
historical context, not the current review.

## Audit ledger

| audit row | findings and ownership trace | evidence | uncertainties / residual exclusions | exact validation |
| --- | --- | --- | --- | --- |
| 2026-08-25 PA12 final `checkpointAudit` at `613b9cb6` | Closed the global-qualified local-extern routing, narrow local empty-class lifetime action, and typed function-template residuals. Rechecked one forward pipeline, canonical record display ownership, source-type side storage, exact four-state specialization lifecycle, selected binding continuity, and cold deterministic rendering. No further source blocker was found; this audit corrects the template wording to distinguish the checked-in unqualified spelling from the separately probed global-qualified spelling. | Focused build; exact residual trio `3/3`; six nearby PA12 controls `6/6`; PA10 `2/2`; PA11 `4/4`; layout `136/48/48/32` bytes; fresh 4/8/16/32 structural scales emitted exact `2n` demands/facts with one hash per scale across three runs; fresh through-PA12 `851/851` across `12/12` stages; file audit exit `0` with exactly two retained `bad-division` warnings at `dev/src/cpp_semantic_core.h:1` and `dev/src/pa11_semantic_model.h:1`; `git diff --check` exit `0`; exact two-document path audit. | Namespace-qualified and nested same-name template probes are deliberately rejected and remain out of scope; PA12 dump mode provides no generated-code evidence. The final audit commit is complete; its identity and post-commit clean-tree proof are reported in the handoff rather than self-referenced in this record. | `make -C pa12 -j2`; direct checked-in comparisons; `make -C pa10 check` (`2/2`); `make -C pa11 check` (`4/4`); immutable structural/determinism probe; fresh `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src` (exit `0`); fresh `make test-report-through-pa12` (exit `0`, `851/851`, `12/12`); `git diff --check` (exit `0`); exact changed-path audit. |
| 2026-08-24 prior PA12 audit at `eee242c6` | Shared typed semantic owner, qualification safety, canonical function redeclaration/definition state, and lexical dump views were recorded before the structured-statement increment. | Prior checkpoint record: PA12 90/166 with 76 residual failures; through-PA11 685/685; performance and file-audit evidence preserved. | Broader PA12 residual families were explicitly excluded. | See the prior committed audit record in history. |
| 2026-08-24 PA12 `checkpointAudit` at `47ca58be` | Complete ownership path reviewed; shared fixed-target promotion, exact converted-case representability, scoped-enum legality, compact 64-bit case payload continuity, empty control substatements, child order, lexical jump validation, deterministic cold rendering, bounded indexes, and final file-audit shape are repaired or confirmed. | Final PA12 103/166, 63 failures, all 166 covered; through-PA11 685/685; focused 22/22; temporary probes 17/17 expected outcomes; normalized failure paths: 63 baseline, 63 current, 0 current-only, 0 baseline-only; `SemanticFact` layout 144-to-136 bytes. | Future residual-family work requires separate authorization; PA12 is not complete. The two header-division warnings are preserved known findings. No tests, refs, harnesses, grammar, or scripts changed. | `make test-pa12` (exit 2, 103/166); exact through-PA11 command; `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src` (pass, 2 warnings); `git diff --check` (pass); changed-path audit. |
| 2026-08-24 PA12 `checkpointAudit` at `5fa28b37` | Callable ownership traced from PA10 IDs through PA11 lookup/types/bindings to PA12 deferred `FunctionIdResolution`, fact-free candidate scoring, selected conversions, call result categories, and deterministic cold rendering. Repaired ordinary `nullptr_t` alias lookup precedence and nested named-pack detection; confirmed qualified/parenthesized target IDs. | Post-repair PA12 113/166, 53 failures, all 166; through-PA11 685/685; focused checked-in 28/28; out-of-tree target/pack/alias probes passed; fresh immutable C/A/D/F evidence was 200/200 successful per case with byte-identical dumps; normalized comparison 53 baseline, 53 current, 0 current-only, 0 baseline-only. | The exact 53 residual paths remain outside this checkpoint; overloaded address-of and overloaded parenthesized-callee forms are not claimed. PA12 is not complete. | `make test-pa12` (exit 2, 113/166); exact through-PA11 command; `make -C pa12`; exact 28-test `make -C pa12 check`; fresh 5x40 interleaved immutable probe; file audit passed with the two known warnings; `git diff --check`; changed-path audit. |
| 2026-08-24 PA12 final bounded `checkpointAudit` from `61b60cb1` | Traced the complete PPPreprocessingSession -> PPTokenBuffer -> PA10Ast -> PA11 canonical identity/lookup -> PA12 typed-fact/cold-dump path. Confirmed one-pass source-order block declaration formation, direct unbraced/label/case preparation without compound-child duplication, `(BindingId, origin ScopeId)` provenance, typed same-scope function-using merge/dedup, common-ancestor effective directives, generation-marked transitive/cyclic lookup, target-directed direct initialization, and deterministic typed rendering. Preserved internal-scope depth propagation and top-level alias fact rendering. | Final `make test-pa12`: 120/166, 46 failures, all 166; normalized 46 baseline / 46 current, 0 current-only / 0 baseline-only; focused checkpoint 7/7, expanded controls 14/14; valid direct selection/iteration/alias/case/nested/merge/repeat probes exited 0; invalid target/collision/non-leakage probes exited 1; merge selected `left::f` and `right::f`, repeat `--emit-types` had one imported block view; refreshed immutable five-round interleaved probes were 50/50 successful per case with one unique dump hash per case; median small/large wall/user/system/RSS 1.06/0.03/0.06/7206 KiB and 1.06/0.03/0.06/7194 KiB. Through-PA11 was 685/685; file audit passed with exactly the two known header-division warnings; diff/path audit passed for exactly the four approved paths. | The exact 46 final residual paths, qualified-`decltype` control, and valid labeled semantic control remain excluded from this bounded checkpoint. The coarse performance timing is structural evidence, not a fine-grained coefficient; no full-suite performance claim is made. | Exact `make test-pa12`; supplied-log normalization; exact through-PA11 command; exact file audit; exact 7-test and 14-test focused checks; out-of-tree direct/collision/merge probes; immutable repeated probes; refreshed five interleaved 10-run samples; `git diff --check`; changed-path audit from `61b60cb1`; final checkpoint-audit commit. |
| 2026-08-24 PA12 `checkpointAudit` at `47a64a66` | Re-audited the scalar/conversion increment end to end. Preserved the array-conditional decay and source-owned conversion facts; repaired object-only pointer-to-cv-void conversion, complete same-object-type subtraction, and the limited `const T*`/`volatile T*` conditional composite; added the contiguous conversion-range guard. Confirmed enum promotion/common arithmetic, reference-preserving assignment, declaration-shaped assignment ambiguity, single-argument selection, anonymous-enum identity, block-enum publication, deterministic child order, and cold typed rendering. | Fresh post-repair build; exact active family 20/20; exact nine controls 9/9; five neighboring controls 5/5; object/function/void/subtraction/composite `/tmp` probes matched expected exits; retained pre-supervisor immutable structural probes were 0/0 with byte-identical outputs and 8/8/7 versus 64/64/63 counters; prior performance and progress evidence preserved. Fresh broad PA12 142/166, exactly 24 paths, all 166 covered; normalized supplied/fresh 24/24 with 0-only on either side; through-PA11 685/685; file audit passed with exactly two known warnings. | The exact 24 residual paths and excluded builtin, declaration/anonymous-union, member-pointer/cast/reference-binding, lookup/namespace, and scoped-enum functional-cast/control families remain outside scope. The composite helper is intentionally limited to same unqualified pointee identity with direct cv union; no general nested composite-pointer completeness is claimed. No timing/RSS/scaling claim is made. | Fresh `make -C pa12 -j2`; exact 20-path, nine-control, and five-neighbor checks; bounded valid/invalid probes; exact `make test-pa12`; supplied-log normalization against `/tmp/pa12-final-test.log`; exact prior-through command; exact file audit; `git diff --check`; final changed-path audit. |
| 2026-08-24 PA12 `checkpointAudit` at `f8b8c49b` with final bounded repair | Landed intrinsic ownership was traced from exact builtin spelling through PA11 model identity, constexpr declaration facts, and safe integral folding to PA12 call/literal/conversion facts and cold rendering. Repaired typed-zero propagation into conversion ranking, narrowed fold suppression to `NonConstantExpression`, added four-vector `SemanticTailGuard` rollback, and bounded multiply/shift/signed-divide folding. | Fresh final PA12 `146/166`, exactly 20 failures, all `166/166` covered; normalized against the supplied post-landed log with `0` current-only and `0` supplied-only paths; focused active `4/4`, controls `13/13`, evaluator/typed-zero/invalid probes passed their expected statuses; through-PA11 `685/685`; file audit passed with exactly two known header warnings. | Exact 20 residual paths remain excluded; qualified/global/parenthesized builtin source-shape variants and nonintegral query semantics beyond the required fold set are not broadened. Retained structural measurements are landed/historical only and do not prove repaired behavior. PA12 is not complete. | Fresh `make test-pa12` (exit 2), exact prior-through command, exact file audit (exit 0), focused commands and `/tmp/pa12-checkpoint-final-test.log`; `git diff --check` and exact five-path status audit pass before the single authorized commit. |
| 2026-08-24 PA12 `checkpointAudit` at `e45d0795` with bounded cv repair | Traced PA10 source/node identity through PA11 canonical record/scope/binding formation, sparse storage/member/constructor sidecars, PA12 construction/member/copy facts, and cold deterministic rendering. Repaired ordinary cv-qualified anonymous record objects by publishing the unqualified `NamedRecordId` type for the record name while preserving typedef alias qualification; confirmed cv propagation, invalid member rejection, generated identities, and no integral/constexpr poisoning. | Fresh final PA12 `149/166`, exactly 17 failures, all `166/166`; normalized against the supplied post-landed set with `0` current-only and `0` supplied-only paths. Fresh build and focused PA12 controls passed `12/12`; PA11 anonymous-union control passed `1/1`; cv, reuse, invalid-operand/member, and constexpr probes had expected exits; five interleaved immutable rounds produced one hash per representative shape and median peak RSS `4360/4376/4384 KiB` with timer-resolution-limited `0.00` wall/user/system values. Through-PA11 passed `685/685`; file audit passed with exactly two known header-division warnings. | The exact 17 residual paths remain outside this checkpoint: local extern/parser, member-pointer/cast/reference, and namespace/lookup families. No broad timing or asymptotic claim is made; the two file-audit warnings are retained known findings. | `make test-pa12` exit `2`; exact through-PA11 command exit `0`; exact file audit exit `0`; `make -C pa12 -j2`; exact 12-test PA12 check; exact PA11 control check; bounded valid/invalid `/dev/fd` probes; immutable five-round structural/determinism probe at `/tmp/pa12-record-checkpoint.PJtyG8`; `git diff --check`; exact changed-path audit; one final commit containing only `dev/src/pa11_semantic_core.cpp`, `pa12/audit.md`, and `pa12/plan.md`, followed by clean `git status --short`. |
| 2026-08-24 PA12 `checkpointAudit` at `1a150235` with bounded namespace/type audit | Traced PA10 source identities and points through the sole PA11 namespace/type/binding/lookup owner into PA12 selection and cold rendering. Preserved stable unnamed-namespace reopening and implicit visibility; retained direct-vs-using hiding, source-order value/relation filtering, conflict and redeclaration ownership, cycle marks, and bounded call-shaped `decltype`. Repaired namespace aliases with sparse declaration points, inline-child traversal, namespace-owned type declaration-point filtering for `types`/`using_types`, stable originating `BindingId` candidate identity (rather than `TypeId`) for ambiguity/deduplication, optional identity propagation through qualified/unqualified `lookup_type_path` into type-using formation, including transitive and graph-exposed chains, and later-inline-marker source-order leakage. The new type and inline facts are sparse and the hot `Scope`, `Binding`, and `NamedRecord` layouts are unchanged; the sidecar helpers remain in the PA11 formation `.cpp` and the lookup core is 3000 lines. | Final PA12 `155/166`, exactly 11 failures, all `166/166` paths covered; normalized supplied/final failure sets match with `0` current-only and `0` supplied-only; focused PA12 `19/19`; focused PA11 `12/12`; nine late-type probes rejected; distinct same-`TypeId` declarations were ambiguous, repeated same-origin paths, transitive chains, and graph-exposed type-using imports deduplicated, direct hiding passed, distinct inline siblings were ambiguous, and late/early inline-marker probes had expected exits; four repeated namespace-shape dumps had `cmp` exit `0` with one hash each; layout was `Binding 80`, `NamedRecord 120`, `Scope 440`, `NamespaceAliasRelation/List 16/24`, `TypeDeclarationRelation/List 24/24`, and `SourcePoint 8`; through-PA11 `685/685`; file audit passed with exactly two known warnings. | The exact eleven residual paths remain outside this checkpoint: decltype functional-cast, local-extern, member-pointer, reference-binding, scoped-enum-cast, and static-cast families. No broad conversion/overload semantics, timing/scaling claim, or new test/reference/harness/grammar/script change is claimed; the two header-division warnings remain known findings. | Final `make test-pa12` (exit `2`); exact residual normalization and 166-path count; exact through-PA11 command; `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src` (pass, 2 warnings); focused PA12/PA11 checks and bounded `/dev/fd` probes; deterministic `cmp`/SHA-256 and layout probes; `git diff --check`; exact five-path audit; amended existing checkpoint commit followed by clean `git status --short`. |
| 2026-08-24 PA12 `checkpointAudit` at `4f890322` with bounded array-cv repair | Traced PA10 declarator/source identities through `pointer_op`, typed `DeclaratorOp` prefix/suffix application, and canonical `TypeKey`/`TypeId` formation into PA12 exact member-pointer target selection, address-of facts, synthetic `this`, static-member sidecars, and cold member-definition rendering. Confirmed exact class-owner/function-type matching, function cv retention, data/function member-pointer distinction, static ordinary-function behavior, recursive array qualification, and deterministic typed output. Repaired `cv_qualifiers` to retain cv from wrappers, pointer/member-pointer nodes, and nested array elements, preventing const-array or const-pointer objects from converting to unqualified `void*`. | Fresh focused build; five affected PA12 tests plus fifteen nearby PA12 controls `20/20`; four checked-in PA10 member-pointer controls `4/4`; twelve valid/invalid target, cv, data/function, array, bound, static, and void-pointer probes with expected statuses; nested prefix/suffix and static/non-static rendering assertions passed. Fresh broad PA12 `160/166` with all `166/166` covered and exactly the six turn-start residuals; through-PA11 `685/685`; five interleaved performance rounds passed; file audit passed with two known warnings. | The six exact residual paths remain outside this audit. General class-aware calls/templates remain out of scope; local-extern, reference-binding, and overloaded-function-template selection remain residual. No tests, refs, harnesses, grammar, or scripts changed. | `make -C pa12 -j2`; direct checked-in-output/status comparisons for focused `20/20 + 4/4`; bounded `/dev/stdin` probes; five-round immutable performance probe; exact through-PA11 command; `make test-pa12` exit `2` with `160/166`; file audit exit `0`; `git diff --check`; exact authorized-path audit; checkpoint-audit commit followed by clean `git status --short`. |
| 2026-08-24 PA12 `checkpointAudit` at `115aff98` | Complete PA10 call-shaped classification/charging -> PA11 canonical type/value lookup -> PA12 functional/explicit cast conversion ownership -> cold typed rendering trace; no source repair. | Fresh `make test-pa12` `163/166`, all 166 covered; fresh through-PA11 `685/685`; fresh through-PA12 `848/851`; focused build exit `0`, active PA12 `3/3`, PA12 controls `8/8`, PA10 parser controls `4/4`; six repeated dump pairs had status `0/0` and `cmp` `0`; fresh file audit passed with the two retained header warnings; diff/path checks passed. | Exact residuals remain outside this checkpoint: `300-local-extern-function-declaration.t`, `300-reference-binding-pointee-const-pointer.t`, and `300-static-cast-overloaded-function-template-argument.t`. No class-aware/template repair, test/ref/harness/grammar/script change, or source repair was made. | `make test-pa12` exit `2`; exact prior-through command exit `0`; `make test-report-through-pa12` exit `2`; file audit exit `0` with 2 warnings; `git diff --check` exit `0`; exact path audit pass. |
