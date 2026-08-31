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
bounded pass.  Non-hidden namespace definitions and internal-linkage hidden
friends remain roots; an externally linked hidden friend is emitted only when
a typed call or address edge reaches its binding.  The scan includes bodies
that will not themselves become plans; this semantic dependency traversal is
distinct from emitted-caller reachability.  Existing class/lifecycle demand
edges stay unchanged.  The result is consumed before function-plan creation,
with no render/reintern lookup, second semantic owner, or body lowering for
skipped definitions.

## Failure Map

Authoritative start is clean HEAD `dff21435b183d2bb123c508522627ed4b5e20421`.
The supplied `make test-pa16` baseline is `236/243`, with all `243` test
identities covered and exactly these seven failures:

1. `pa16/tests/general/200-local-default-class-array-lifecycle.t`
2. `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
3. `pa16/tests/general/300-friend-function-definition-skip.t`
4. `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
5. `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
6. `pa16/tests/general/400-signed-bit-field-read.t`
7. `pa16/tests/general/400-signed-enum-bit-field-read.t`

Only #3 is in this checkpoint.  The reverse-array, typed-index-widening,
instruction-order, and signed bit-field evidence in the other six failures is
out of scope and must not be regressed or repaired here.

## Active Checkpoint

The failing source creates two namespace-owned hidden-friend `FunctionFact`s:
`operator==` and `operator!=`.  PA12 validates both bodies; PA15 presently
emits both because namespace ownership bypasses the class demand filter.  The
canonical mismatch is only the second definition, whose typed body calls the
first.  PA15 scans that second body even though it is not planned, and the
typed call edge demands the first; this is the fixture-defined boundary and
does not imply that the caller was emitted.  The narrow owner boundary keeps
the first while suppressing the unrooted second body.

The implementation will trace direct calls through
`selected_binding -> function_binding_fact_index_ -> FunctionFact`, and
function references through `IdExpression`/`Variable` binding facts, including
global semantic roots.  Ordinary namespace free functions, exported/C-linkage
or otherwise required definitions, internal hidden-friend definitions, and
hidden friends reached by ADL or an address remain demanded.  Lifecycle roots
continue to use the existing constructor/destructor facts and base-entry
relations.  Demand bits are deduplicated by fact identity and processed in
stable existing worklist order.  No friend is suppressed from semantic body
validation, and no decision uses a rendered spelling.

Focused controls will cover the active skip, used hidden-friend/ADL operators,
ordinary namespace operators and free functions, friend body calls, exported
free functions, and the unnamed-namespace hidden-friend lifecycle case.  No
additional regression is planned unless an existing control cannot express
the owner boundary.

## Performance Evidence

At turn start, the supplied authority was `236/243` with complete `243/243`
identity coverage.  Final `make test-pa16` exited `2` with `237/243` passed and
these exact six residuals (the baseline seven minus the repaired active test):

1. `pa16/tests/general/200-local-default-class-array-lifecycle.t`
2. `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
3. `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
4. `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
5. `pa16/tests/general/400-signed-bit-field-read.t`
6. `pa16/tests/general/400-signed-enum-bit-field-read.t`

The source, reference, and fresh exit-status inventories were each `243`, and
both source-vs-reference and source-vs-fresh identity comparisons were empty.
The required through-PA15 command exited `0` at `1167/1167`.  The file audit
exited `0` with six known nonfatal `bad-division` warnings in
`abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`, `pa11_semantic_model.h`,
`pa12_semantic_selection.h`, and `pa15_lowering.h`.  `git diff --check` exited
`0`.  No timing, RSS, or allocation claim is made.  The principal uncertainty
is preserving the fixture's typed dependency from a semantically validated
friend body while not turning an unrooted hidden-friend body into an emitted
definition.

## Checkpoint Ledger

- Start: clean `dff21435b183d2bb123c508522627ed4b5e20421`; supplied baseline
  `236/243`, seven named failures, `243/243` identities.
- Investigation: complete; owner/data flow is `BindingId`/sidecar -> namespace
  `FunctionFact` -> PA12 `body_fact` -> PA15 demand bit -> `FunctionPlan`.
- Plan: rewritten for this hidden-friend/non-member checkpoint.
- Implementation: complete in `dev/src/pa15_lowering.h`,
  `dev/src/pa15_lowering.cpp`, and `dev/src/pa15_lowering_calls.cpp`.
- Focused evidence: `make build` exited `0`; the exact active handout passed
  `1/1`; the hidden-friend/ADL/operator/free-function/internal-linkage set
  passed `13/13`; two address controls passed `2/2`; two friend-declaration
  controls passed `2/2`.
- Broad evidence: `make test-pa16` exited `2` at `237/243` with exactly the six
  residual identities listed in Performance Evidence; all `243` inventories
  matched with no identity differences.  The required prior gate exited `0` at
  `1167/1167`.
- Audit and hygiene: the PA16 file audit exited `0` with the six listed
  `bad-division` warnings; `git diff --check` exited `0`.  No timing/RSS claim
  is made.
- Commit: pending final diff review; intentionally uncommitted until the
  coherent checkpoint increment is committed.
