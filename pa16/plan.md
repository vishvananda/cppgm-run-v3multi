# PA16 hidden-friend emission-demand checkpoint

## Stage Design

PA11 owns the canonical `BindingId`, `BindingSidecar`, namespace owner, and
linkage facts.  A hidden friend is therefore identified by its typed sidecar
relation and its namespace-owned `FunctionFact`, not by a rendered operator
name.  PA12 gives every definition a typed `body_fact` so semantic validation
can cover an unused body without making it an emitted definition.

PA15 scans all semantically validated namespace function bodies to discover
typed member, constructor, destructor, and global-root dependencies, but
`collect_functions` filters only class-owned facts.  The checkpoint adds a
dense, per-`FunctionFact` namespace-definition demand result to that same
bounded pass.  Non-hidden namespace definitions and C/internal-linkage hidden
friends remain roots; an externally linked C++ hidden friend is emitted only
when a typed call, address/reference, or ordinary-visibility edge reaches its
binding.  The scan includes bodies that will not themselves become plans;
this semantic dependency traversal is distinct from emitted-caller
reachability.  Existing class/lifecycle demand edges stay unchanged.  The
audit additionally makes malformed namespace fact identities fail closed,
without changing valid demand.  The result is consumed before plan creation,
with no render/reintern lookup, second semantic owner, or body lowering for
skipped definitions.

This is the `spec.md` Purpose and §§1–5/§7 boundary: typed facts retain their
identity and canonical owner from PA11 through PA12 and PA15; semantic body
validation is separate from runtime reachability and emitted-definition
demand; roots and edges are typed; and the demand pass uses bounded dense
worklists with an architecture trace to `FunctionPlan` and LowIR emission.

## Failure Map

The supplied current authority is from clean HEAD
`544904edcd691ea7fc77599236a63fe9b40f1bd3`, with `make test-pa16` exiting `2`
at `237/243`; all `243/243` test identities are covered and exactly these six
failures remain:

1. `pa16/tests/general/200-local-default-class-array-lifecycle.t`
2. `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
3. `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
4. `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
5. `pa16/tests/general/400-signed-bit-field-read.t`
6. `pa16/tests/general/400-signed-enum-bit-field-read.t`

The repaired `300-friend-function-definition-skip.t` is no longer a failure.
The reverse-array, typed-index-widening, instruction-order, and signed
bit-field owners in the six residuals are out of scope and must not be
repaired here.  Fresh final `make test-pa16` reproduces `237/243` and the same
six identities: authority-only, fresh-only, and new failures are `0/0/0`.
Discovered source tests, checked-in reference exit-status sidecars, and fresh
output exit-status sidecars are each `243`, with zero identity-set deltas.
Reference and fresh sidecar statuses are each `224` `EXIT_SUCCESS` and `19`
`EXIT_FAILURE`, with zero status mismatches.

## Active Checkpoint

The active source creates two namespace-owned hidden-friend `FunctionFact`s:
`operator==` and `operator!=`.  Before this checkpoint, namespace ownership
allowed both definitions through the class demand filter.  The landed demand
split scans both validated bodies, while the second body's typed call edge
demands the first; the audit preserves that fixture-defined distinction even
though the caller itself is not planned.  The narrow owner boundary keeps the
first while suppressing the unrooted second body.

The implementation traces direct calls through
`selected_binding -> function_binding_fact_index_ -> FunctionFact`, and
function references through `IdExpression`/`Variable` binding facts, including
global semantic roots.  Ordinary namespace free functions, exported/C-linkage
or otherwise required definitions, internal hidden-friend definitions, and
hidden friends reached by ADL or an address remain demanded.  Lifecycle roots
continue to use the existing constructor/destructor facts and base-entry
relations.  Demand bits are deduplicated by fact identity and processed in
stable existing worklist order.  No friend is suppressed from semantic body
validation, and no decision uses a rendered spelling.

Focused controls cover the active skip, used hidden-friend/ADL operators,
ordinary namespace operators and free functions, friend body calls, function
address/reference roots, member calls, and the unnamed-namespace hidden-friend
lifecycle case.  No additional regression was needed: the repair is an
internal malformed-fact guard and the existing controls express the owner
boundary.

## Performance Evidence

The supplied authority and fresh final `make test-pa16` from clean HEAD
`544904edcd691ea7fc77599236a63fe9b40f1bd3` are both exit `2` at `237/243`,
with complete `243/243` identity coverage and the exact six residuals above.
The exact prior gate exits `0` at `1167/1167`.  The exact file audit exits `0`
with six known nonfatal `bad-division` warnings at line 1 in
`dev/src/abi_mangle.h`, `dev/src/cpp_semantic_core.h`,
`dev/src/lowir_model.h`, `dev/src/pa11_semantic_model.h`,
`dev/src/pa12_semantic_selection.h`, and `dev/src/pa15_lowering.h`.

Fresh focused evidence is `make build` exit `0`, the PA15
deleted-declaration control `PASS (1/1)`, the exact active handout `PASS
(1/1)`, a representative affected matrix `PASS (14/14)`, course regressions
430 and 431 both `PASS`, and `git diff --check` exit `0`.  The matrix covers
address/reference, ordinary/static member and namespace functions, friend
declarations/definitions, ADL operators, mixed/nullptr and derived-base friend
operators, and qualified friend access.  The evidence is structural: dense
fact-indexed worklists and one bounded typed scan per relevant fact domain are
retained, with no timing, RSS, allocation, or generated-program performance
claim.

## Checkpoint Ledger

- Start: clean `dff21435b183d2bb123c508522627ed4b5e20421`; supplied baseline
  `236/243`, seven named failures, `243/243` identities.
- Investigation: complete; owner/data flow is `BindingId`/sidecar -> namespace
  `FunctionFact` -> PA12 `body_fact` -> PA15 demand bit -> `FunctionPlan`.
- Plan: rewritten for this hidden-friend/non-member checkpoint.
- Landed increment: committed as
  `8c60e658572a1f73aa0387ffaf6dc5546e5c2bda` (`PA16: demand hidden friend
  definitions`), relative to `dff21435b183d2bb123c508522627ed4b5e20421`.
- Focused evidence: `make build` exit `0`; exact active handout `PASS (1/1)`;
  representative matrix `PASS (14/14)`; PA15 deleted-declaration control
  `PASS (1/1)`; course 430 and course 431 `PASS`; `git diff --check` exit `0`.
- Current authority/fresh result: `make test-pa16` exits `2` at `237/243`,
  complete `243/243` coverage, and exactly the six residual identities listed
  above; authority-only/fresh-only/new failures are `0/0/0`.  Discovered,
  reference-sidecar, and fresh-sidecar identities are `243/243/243`, with
  zero identity deltas; sidecar statuses are `224` success and `19` failure
  on both sides, with zero mismatches.
- Required gates: exact through-PA15 exits `0` at `1167/1167`; exact file
  audit exits `0` with six known nonfatal header-division warnings.
- Checkpoint/audit: complete and committed for this bounded scope.  The repair
  in `dev/src/pa15_lowering_calls.cpp` hardens malformed namespace definition
  and typed-edge identities while preserving declaration-only and
  member/lifecycle demand; `pa16/audit.md` records the final trace, evidence,
  and residual boundary.
- Next checkpoint: `pa16/tests/general/200-local-default-class-array-lifecycle.t`
  as a separately scoped residual owner.
