# PA16 implementation plan

## Current authority

The current committed repository state at HEAD contains the bounded
checkpointAudit. Its source-bearing checkpointAudit commit is
`6924315a7190aa64b68dfbe029e47ed8d6f868a9` (`PA16: complete checkpointAudit`),
based on checkpoint input commit
`05a3252fa2f649d7962545af92f041ba801ebd16` (`PA16: fix cv-qualified member
object semantics`). The checkpoint input authority recorded 200/243 identities
passed, exactly 43 failed, and 243/243 identities were covered. The validated
checkpoint result after the approved bounded repair is also 200/243 with
exactly 43 failures and 243/243 coverage. The authoritative checkpoint-input
primary log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The final checkpoint log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-final-20260829.log`.
Its parent checkpoint `88d15835` had 199/243 passed and exactly 44 failures;
the landed delta is baseline-only
`pa16/tests/general/200-mutable-member-const-method.t`, with final-only empty
and coverage unchanged. Relative to checkpoint input `05a3252f`, the audit repair has
baseline-only `0`, final-only `0`, and no coverage change; the exact comparison
is preserved at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-identity-comparison-20260829.log`.

## Current checkpoint scope and spec alignment

This checkpoint audits only the PA11 declaration-specifier and mutable
`BindingSidecar` path, PA12 member-subobject cv propagation, and mixed
member/nonmember operator implicit-object viability/ranking through the
existing explicit hidden-`this` call boundary. `[dcl.stc]` 7.1.1 paragraph 10
requires `mutable` to name a non-static class data member and rejects const,
static, reference, and unnamed-bit-field ownership. `[basic.type.qualifier]`
3.9.3 and `[expr.ref]` 5.2.5 preserve volatile while removing the containing
object's const for a mutable member. `[over.match.oper]` 13.5.3 and
`[over.best.ics]` 13.3.3 compare the member implicit-object conversion with
the nonmember's first explicit operand.

PA11 carries `SpecFact` and the canonical `BindingId`; named mutable members,
including bit-fields, write one sidecar fact. PA12 passes that same binding to
`member_access_type` for explicit dot/arrow, implicit-this, and injected
anonymous-union views. The helper removes only const from the object cv set.
Member calls use the typed object conversion and then the existing hidden-this
callable type. Mixed operators keep bounded member/nonmember candidate lists;
the nonmember parameter-zero qualification delta is represented in the typed
conversion score. No textual recovery, duplicate model, broad invalidation,
or PA15 widening is in scope.

The mutable condition-declaration spelling is rejected at the parser boundary,
before `process_condition_declaration`; it is parser-excluded and was not
newly guarded there or widened into PA10.

## Inherited predecessor architecture and retained evidence

PA11 owns canonical typed identities: SemanticFactId, BindingId,
FunctionFactId, typed expression ownership, declaration cv/mutable metadata,
and recorded ConversionFact ranges. PA12 builds all retained facts and
function bodies in deterministic source order. Member access consumes the
typed owning BindingId to qualify a member subobject; member selection and
operator ranking consume the typed implicit-object cv. Selected member calls
then cross the existing explicit hidden-this function boundary. PA12 then
runs one finalize_canonical_truth pass after construction; set_semantic_children
and calls do not demand bodies or publish provenance. PA15 consumes the
published fact/conversion owners and emits typed LowIR.

The finalizer is one ephemeral local graph. A local ResultNodeId wraps its
dense ordinal ranges for retained semantic facts, BindingId conservative
may-summaries, and canonical defined FunctionFact summaries; these IDs are
discarded after publication. Append-only edge records use dense source
heads/next links. Direct PA12 member/object-derived and justified
implicit-this facts seed the graph. Explicit result edges are limited to
variable/return initializers, unary except address-of, postfix/cast,
non-comma binary operands, comma RHS, assignment result operands,
conditional result arms, and subscript object. Calls receive provenance only
from the selected canonical defined function summary; call arguments and
indirect callees, comma-left, conditions, constructor arguments, member
objects, control/statement edges, and other non-value edges are excluded.

Variable/assignment definitions point to a BindingId summary and typed
variable IdExpression uses read that summary. Retained ReturnStatement
results are paired with their owning function fact; return results feed the
defined-function summary and the summary feeds selected calls. A single
deduplicated monotonic queue converges recursive summaries without body
retry, whole-program retry, broad invalidation, or a second production model.
Declaration-only/external calls have no definition edge and fail closed to
Materialize. Malformed retained return ownership and graph endpoints fail
closed with runtime_error; domain additions are checked before allocation.

The BindingId result is a conservative whole-retained-program may-summary.
Branches, loops, and reassignment may retain earlier possible provenance;
this plan does not claim precise reaching definitions or unconditional
reassignment clearing. Shadowing remains separate because BindingIds differ.
Speculative tails finish before finalization, so discarded type-only facts
cannot update the graph and no new graph rollback state is required.

CanonicalTruthPolicy is conversion-owned. PA12 publishes Preserve for each
owned conversion whose semantic source is bool, including bool-to-int/zext;
otherwise the existing Materialize disposition remains. PA15 resets
LoweredValue from the current ConversionFact before each conversion, so a
Preserve conversion cannot become sticky across a later Materialize record.

## Final failure map

The exact final `make test-pa16` result is 200/243 passed, 43 failed, with all
243 identities covered. The exact sorted residual identities are:

- pa16/tests/general/100-function-pointer-nested-param-name-shadow.t
- pa16/tests/general/200-aliased-base-mem-initializer-match.t
- pa16/tests/general/200-const-subobject-member-call.t
- pa16/tests/general/200-destructor-body-local-before-base-destruction.t
- pa16/tests/general/200-elaborated-member-forward-type.t
- pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t
- pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t
- pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t
- pa16/tests/general/200-local-default-class-array-lifecycle.t
- pa16/tests/general/200-member-object-lifetime.t
- pa16/tests/general/200-nested-braced-member-aggregate-init.t
- pa16/tests/general/200-nonliteral-field-condition-not-folded.t
- pa16/tests/general/200-placement-new-expression-aggregate-brace.t
- pa16/tests/general/200-placement-new-expression-constructor-call.t
- pa16/tests/general/200-reference-indexed-pointer-member-access.t
- pa16/tests/general/200-reference-member-class-init.t
- pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t
- pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t
- pa16/tests/general/300-adl-associated-namespace-does-not-climb-parents.t
- pa16/tests/general/300-adl-using-declaration-source-point.t
- pa16/tests/general/300-callable-field-hides-private-base-method.t
- pa16/tests/general/300-compound-assignment-adl-nonmember-after-member-reject.t
- pa16/tests/general/300-enum-class-nonmember-operator-bitand.t
- pa16/tests/general/300-friend-function-definition-skip.t
- pa16/tests/general/300-member-vs-nonmember-operator-implicit-object-cv-rank.t
- pa16/tests/general/300-mixed-member-free-shift-stress-chain.t
- pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t
- pa16/tests/general/300-operator-nullptr-t-from-zero.t
- pa16/tests/general/300-operator-shift-stress-chain.t
- pa16/tests/general/300-overloaded-deref-user-assignment.t
- pa16/tests/general/300-packed-class-layout.t
- pa16/tests/general/300-pragma-pack-followed-by-endif.t
- pa16/tests/general/300-prvalue-derived-base-friend-operator.t
- pa16/tests/general/300-synthesized-array-member-lifecycle.t
- pa16/tests/general/300-user-defined-string-literal-operator.t
- pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t
- pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t
- pa16/tests/general/400-bit-field-constructor-member-init.t
- pa16/tests/general/400-bit-field-member-access-bad.t
- pa16/tests/general/400-bit-field-prefix-postfix-increment.t
- pa16/tests/general/400-bitfield-aggregate-init.t
- pa16/tests/general/400-signed-bit-field-read.t
- pa16/tests/general/400-signed-enum-bit-field-read.t

Relative to parent `88d15835`, the landed delta is baseline-only
`pa16/tests/general/200-mutable-member-const-method.t`, final-only empty, and
coverage remains `243/243`. Relative to checkpoint input `05a3252f`, the final audit
repair has baseline-only `0`, final-only `0`, and coverage delta `0`.

Focused identities and ownership outcomes:

- `200-const-subobject-member-call.t`: PA12 already selects
  `Table::f() const` through the const `Map` subobject and exits successfully;
  the residual is a PA15 empty-aggregate no-op projection in LowIR, excluded
  from this cv/member-call checkpoint.
- `200-mutable-member-const-method.t`: the clean landed authority now passes;
  the audit also found and repaired invalid mutable consumers in function
  definitions, parameters, type-ids, and bit-field declarations.
- `300-member-vs-nonmember-operator-implicit-object-cv-rank.t`: PA12 records
  the nonmember object's cv delta in its typed conversion score; the mixed
  candidate set selects the member and exits correctly. Its remaining checked
  difference is the known unused-`$period` LowIR shape residual.

The exact final sorted comparison is preserved at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-identity-comparison-20260829.log`;
both logs contain the same 43 failure identities.

## Inherited focused evidence

- `make clean` followed by `make -j2` exits 0 after the structural extraction.
- The protected five command exits 0 with `pa16 check: PASS (5/5)`.
- Current ephemeral probes call-after, call-forward, call-before,
  cycle-observable, result-edges, call-argument, binding-observable,
  type-only, conversion-sequence, two-conversion, nested-calls, and
  nested-calls-reversed all exit 0.
- call-after and call-forward LowIR compare byte-for-byte. The adversarial
  A/B/A cycle reaches the later direct member-derived seed through the one
  worklist. The result-edge probes show direct member comparison zext,
  procedural/call-argument/comma-left comparison trunc, and no propagation
  from a condition into literal result arms.
- Branch/reassignment uses observe conservative may-provenance; a shadowed
  inner BindingId has the member-derived shape while the outer binding stays
  procedural. The type-only probe leaves exactly one retained base projection
  and Base::get call, with the discarded fact absent from LowIR.
- Typed LowIR shows the member-derived bool-to-int path as cmp plus zext,
  while a plain procedural comparison retains trunc. Temporary policy tracing
  recorded `[14->28:Preserve]` then `[28->29:Materialize]` on one fact;
  diagnostics and counters were removed before the clean rebuild.
- The fresh twelve-probe LowIR outputs are byte-identical to the
  pre-extraction outputs; the focused log is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-structural-refactor-focused-20260829.log`.

## Current checkpoint evidence

Command:
`make -C pa16 check TEST='tests/general/200-const-subobject-member-call.t tests/general/200-mutable-member-const-method.t tests/general/300-member-vs-nonmember-operator-implicit-object-cv-rank.t tests/general/200-const-member-call-prefers-const-object-overload.t tests/general/200-const-object-nonconst-member-call-bad.t tests/general/200-member-call-implicit-object-cv-overload.t tests/general/200-member-call-implicit-this-cv-overload.t tests/general/200-method-cv-overload-preference.t tests/general/300-mutable-anonymous-member.t'`

The corrected build succeeds and the focused matrix is `FAIL (7/9)`: the five
member/cv controls, `200-mutable-member-const-method.t`, and
`300-mutable-anonymous-member.t` pass. The operator test has successful PA12
selection and exit status but still has a fixture-only LowIR difference: the
checked-in reference contains an extra unused `addr $period`; emitting that
would be an output-shape workaround. The const-subobject test still has the
known extra empty-aggregate field projection. These remain the only two
focused residuals and are outside this PA11-to-PA12 repair.

Temporary `--emit-semantics` probes after the repair reject mutable function
definitions, parameters, type-ids, anonymous bit-fields, and mixed
named/anonymous bit-field declarations. Named mutable and mutable-volatile
bit-field writes through const objects, plus `mutable const T*`, accept. The
condition-declaration reduction is rejected by the parser before its typed
consumer; it is parser-excluded and was not newly guarded in
`process_condition_declaration`.

The required full-stage gate exits 2 at `200/243`, with `43` failures and
`243/243` coverage; its durable log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-final-20260829.log`.
The exact through-PA15 command exits 0 at `1167/1167`; its durable log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-through-pa15-20260829.log`.
The required file audit exits 0 with the five existing header warnings listed
in the audit record. `git diff --check` exits 0.

## Current structural/performance bounds

The mutable repair adds one canonical `BindingSidecar` write per named member
and one keyed read at `member_access_type`; no duplicate model, scan, retry,
or invalidation is introduced. Mixed operator selection remains bounded by
the member/nonmember candidate lists and explicit arguments, approximately
`O(C * A)` for `C` candidates and `A` arguments. These are structural bounds
only; no timing, RSS, allocation, or speedup claim is made. The file audit
reports no relevant function-size, file-size, or packed-statement defect from
the five-file change.

## Inherited predecessor structural/performance evidence

No timing, RSS, allocation, or speedup claim is made. Representative
finalizer structural samples are:

- conversion-sequence: 71 semantic, 21 BindingId-summary, and 6
  defined-function nodes; 58 appended edges; 43 reachable edge visits;
  44 queue pushes and 44 true nodes;
- cycle-observable: 30 semantic, 7 BindingId-summary, and 3
  defined-function nodes; 22 appended edges; 17 reachable edge visits;
  17 queue pushes and 17 true nodes.

Graph construction is O(V+E) in retained nodes plus generated typed result
edges. Dense queued/true state bounds each node's queue transition to once;
the linked edge arena is traversed once for each reachable edge. The local
graph is discarded after publishing semantic/function/conversion owners.
Reversing nested function definitions changes only deterministic function
presentation order, not the owned result path.

The extracted finalizer methods are below the 240-line function limit. The
source audit also confirms the existing 2400-line header and 3000-line source
limits remain satisfied, with no newly introduced source line packing multiple
statements.

## Current checkpoint boundary and next residual checkpoint

Scope is the PA11-to-PA12 typed cv flow for non-static member subobjects,
including the mutable exception, and the PA12 implicit-object conversion score
used by mixed member/nonmember operators. Invariants are BindingId ownership,
cv qualification only at the member-object type boundary, typed candidate
viability/ranking, the existing explicit hidden-this call type, and the single
typed canonical-truth pipeline. The empty aggregate's zero-work field
projection and the unused `$period` shape are excluded because their semantic
selection/exit behavior is already correct and they are not caused by this
typed cv repair. The checkpoint is complete with the required broad validation
and exact unchanged failure set.

The operator candidate work remains bounded by the relevant member and
nonmember candidate lists and their argument lists; no global retry or
whole-program scan is introduced. The mutable bit is stored once on the
canonical member BindingId sidecar and read only for that member access.

The mutable sidecar lookup and cv mask are one keyed access per member access.
Existing operator selection remains an O(candidates * arguments) traversal;
the focused mixed operator exercises the two candidate families and its typed
hidden-object boundary. No timing or allocation claim is made.

## Inherited predecessor validation evidence

Full PA16 log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-structural-refactor-test-pa16-20260829.log`.
Through-PA15 was run exactly with n=16 and exits 0 at `1167 / 1167`; log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa15-through-structural-refactor-20260829.log`.
The file audit exits 0 with five existing header bad-division warnings in
abi_mangle.h, cpp_semantic_core.h, lowir_model.h, pa11_semantic_model.h,
and pa15_lowering.h; log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-structural-refactor-file-audit-final-20260829.log`.
The exact identity comparison is preserved at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-structural-refactor-identity-compare-20260829.log`.
`git diff --check` exits 0 after the document update.

The full evidence above belongs to the predecessor checkpoint; final evidence
for this audit is recorded in the current checkpoint sections above and below.

## Final validation evidence

The final `make test-pa16` exits `2` with `200/243` passed, `43` failures, and
`243/243` identities covered. Its durable log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-final-20260829.log`.
The exact sorted comparison against the supplied checkpoint-input log is preserved
at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-identity-comparison-20260829.log`:
baseline-only `0`, final-only `0`, and coverage delta `0`.

The exact required through-PA15 command exits `0` at `1167/1167`; durable log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-through-pa15-20260829.log`.
The exact file audit exits `0`; durable log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-file-audit-20260829.log`.
It reports only the five existing `bad-division` warnings for
`abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
`pa11_semantic_model.h`, and `pa15_lowering.h`; no relevant file-size,
function-size, or packed-statement defect was introduced. `git diff --check`
exits `0`.

The only reviewed files are `dev/src/pa11_semantic.cpp`,
`dev/src/pa11_semantic_types.cpp`, `dev/src/pa12_semantic_facts.cpp`,
`pa16/audit.md`, and `pa16/plan.md`. No test, fixture, reference, harness,
comparator, coverage, source-set, or unrelated file changed.

## Checkpoint ledger

| checkpoint | status |
| --- | --- |
| c39d4563 plus typed canonical-truth finalizer checkpointAudit | Completed predecessor: bounded finalizer and hardening are in the six authorized source owners; clean build and protected five pass, focused probes pass, final PA16 is 199/243 with the exact unchanged 44-failure map and 243/243 coverage, through-PA15 is 1167/1167, and file audit passes with five known warnings. |
| `05a3252f` plus checkpointAudit | Completed bounded PA11-to-PA12 audit and repair: reachable `SpecFact` consumers reject invalid `mutable` ownership, named bit-fields publish the canonical `BindingSidecar`, and parser-excluded mutable condition declarations remain outside `process_condition_declaration`. Member cv propagation and mixed operator scoring remain typed through the hidden-`this` boundary. Final PA16 is `200/243` with `43` failures and `243/243` coverage; exact comparison with checkpoint input `05a3252f` has baseline-only `0`, final-only `0`, and coverage delta `0`. Through-PA15 is `1167/1167`; file audit exits `0` with the five existing header warnings; diff-check exits `0`; the focused matrix is `7/9` with the two approved LowIR-shape residuals. Only the three bounded source files and two PA16 records changed. |
