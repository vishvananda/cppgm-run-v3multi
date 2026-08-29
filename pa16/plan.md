# PA16 implementation plan

## Current authority

Clean starting authority for this checkpoint is c39d45634bb029a02c938c190f8ac703bd275050,
PA16: preserve typed canonical truth boundaries. Its turn-start result is
199/243 identities passed, exactly 44 failed, and 243/243 identities were
covered. The final gate must remain at least 199/243 with no final-only
identity and no coverage loss. The older e92 194/243, 49-failure result is
retained only as historical context and is not current authority.

## Spec alignment and stage design

PA11 owns canonical typed identities: SemanticFactId, BindingId,
FunctionFactId, typed expression ownership, and recorded ConversionFact
ranges. PA12 builds all retained facts and function bodies in deterministic
source order. PA12 then runs one finalize_canonical_truth pass after
construction; set_semantic_children and calls do not demand bodies or publish
provenance. PA15 consumes the published fact/conversion owners and emits
typed LowIR.

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

## Exact final failure map

The final `make test-pa16` result is 199/243 passed, 44 failed, with all
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
- pa16/tests/general/200-mutable-member-const-method.t
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

Compared with the turn-start
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`,
baseline-only is 0, final-only is 0, missing coverage is 0, and unexpected
coverage is 0. The final residual is therefore unchanged, not offset by
extra passes.

## Focused evidence

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

## Structural/performance evidence

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

## Validation and next checkpoint

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

PA16 remains incomplete by the unchanged 44-identity residual. The next
checkpoint should select one residual while preserving this single typed
finalizer, explicit result-edge ownership, conservative may-provenance,
canonical function mapping, and per-conversion PA15 reset.

## Checkpoint ledger

| checkpoint | status |
| --- | --- |
| c39d4563 plus typed canonical-truth finalizer checkpointAudit | Completed/current: bounded finalizer and hardening are in the six authorized source owners; clean build and protected five pass, focused probes pass, final PA16 is 199/243 with the exact unchanged 44-failure map and 243/243 coverage, through-PA15 is 1167/1167, and file audit passes with five known warnings. |
