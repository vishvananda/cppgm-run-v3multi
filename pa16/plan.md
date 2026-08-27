# PA16 implementation plan

## Stage design/spec alignment and owner/data flow

This checkpoint keeps one typed production path from PA10 call syntax through
PA12 semantic facts, PA15, and LowIR.  The member-call probe handles both the
existing `CallExpression(MemberExpression)` form and an unqualified `id(...)`
inside a non-static member body.  For the latter, typed lookup checks nearer
block/function declarations, the direct class scope, and then the ordered
single direct-base chain before ordinary enclosing lookup can see a namespace
value.  A found class/base declaration set suppresses unrelated namespace/ADL
candidates; no declaration is recovered from text.

`prepare_pa12_member_parameter` owns the exact synthetic first `this`
parameter in the member Function `Scope`.  A successful call publishes the
typed `this` value as semantic child zero with `OP_ARROW`, followed by
converted/defaulted explicit arguments in source order.  Its callable Function
type has the hidden object pointer first, while the original member Function
type remains the ABI/signature owner.

`direct_base_chain` is keyed by `NamedRecordId`, checks invalid/virtual/cyclic
metadata with a bounded walk, and supplies the ordered lookup path.  The
unqualified lookup passes that path to the shared member selector, so an
inherited call does not re-walk it during semantic candidate selection.  The
same-owner path returns before chain allocation; inherited selection uses the
one typed path produced by lookup.  The selector admits only ordinary
non-static methods from the owner scope, performs the existing implicit-object
cv ranking and explicit argument/default handling, and publishes one canonical
fact.  Its child zero is the synthetic `this` value, with `OP_ARROW`; later
children are converted/defaulted explicit arguments.  The selected owner
remains canonical: an inherited `B::f` has selected scope `B` and a callable
Function type whose hidden pointer is `B*` (with the selected cv).

The only member emission-demand edge is a successful typed member
`CallExpression` with its selected `BindingId` in a reachable emitted
FunctionFact body.  PA15 follows that existing typed binding-to-FunctionFact
edge, validates the hidden-object ABI and direct symbol, and in `lower_call`
projects a `D*` through each validated direct-base `NamedRecordId`/complete
`RecordLayout` pair at offset zero before passing the owner pointer.  Dot uses
one object address and arrow uses one object expression before explicit
arguments.  Parenthesized unqualified ids use the already-unwrapped probe
node, so the semantic helper does not re-read source spelling.

This matches the root specification's one production pipeline, typed fact
continuity, canonical semantic owners, typed demand roots/edges, bounded
worklists, and typed lowering.  There is no parallel textual lookup/demand
model or second canonical member-call selector.

## Failure map and coverage identity

The immutable turn-start evidence at assignment HEAD `25e80541` was `47/243`
passing, `196` failing, `243/243` covered, and `make test-pa16` exit `2`.  Its
complete failure map is preserved in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.

The authorized full `make test-pa16` run is recorded in
`/tmp/v3multi-pa16-final.log`: it exited `2` (the suite remains incomplete)
with `48/243` passing and `195` failing.  All `243/243` checked-in identities
were covered.  Compared with the immutable baseline, exactly one failure was
removed and no failure was added; the original `47` passing identities have
zero regressions.  The focused six-test command measured `2/6` before the
inherited extension and `3/6` after it.  The retired identity is
`200-inherited-member-call-hides-outer-type.t`; the remaining focused failures
are `200-member-call-implicit-this-cv-overload.t`,
`200-implicit-member-call-suppresses-adl.t`, and
`200-local-class-direct-init-inherited-member-call.t`.  The four-test direct/
inherited control set is `4/4`, and the selected PA15 free/indirect control
set is `3/3`.  Bounded stdin reductions for direct, parenthesized, and local
name-shadow calls exited successfully and are not additional suite coverage.
The exact removed-failure set is
`pa16/tests/general/200-inherited-member-call-hides-outer-type.t`; the added-
failure set is empty.

## Active checkpoint

The active selector checks implicit-object qualification compatibility before
ranking and reuses the existing explicit-argument conversion/default logic.
The unqualified probe searches nearer lexical scopes, the direct class, and
the first matching declaration set in the direct-base chain.  It admits only
ordinary non-static methods and then publishes the selected `BindingId`, its
owner `ScopeId`, the hidden-object callable `Function TypeId`, and typed
children consumed by the existing PA15 `lower_call`.  A successful inherited
call therefore carries actual `D*` in child zero while its selected callable
requires owner `B*`; PA15 validates and emits the typed base-subobject
projection between them.  The semantic tail guard rolls back an unsuccessful
probe, so no failed speculation can publish a call fact or demand edge.

This checkpoint intentionally does not add inherited fields, qualified base
calls, protected/friend/using-imported access, constructors/lifetime, aggregate
initialization, ADL or operators, static calls, virtual/ref-qualified methods,
or broader user-defined conversions.  The supported relation remains one
non-virtual direct base per class.  The bounded three-level reduction at
`/tmp/v3multi-pa16-three-level.cc` passed compilation and emitted two ordered
offset-zero base projections before `@A__f`, while multiple inheritance and
virtual inheritance remain outside this checkpoint.

## Performance evidence and uncertainties

Same-owner object validation returns in `O(1)` before allocating or walking a
base vector.  For an inherited unqualified call, lookup performs one bounded
`O(depth)` `NamedRecordId` walk and passes its typed path to the shared
selector; candidate viability/ranking remains `O(C * (P + A))` for `C`
candidates, `P` parameters, and `A` explicit arguments.  PA15 cannot reuse a
semantic lookup vector, so it independently constructs/validates one bounded
`O(depth)` path from the actual object to the selected owner and then performs
the corresponding bounded projection/layout checks.  Thus semantic lookup and
lowering retain separate safety validation; no exactly-once cross-phase walk
is claimed.  The three-level reduction is structural evidence of two ordered
projection steps, not a benchmark.  No whole-program scan or repeated class
completion was added.

Measured evidence is the exact inherited handout (`1/1`), the focused six-test
command (`3/6` after, with the inherited identity removed), the direct/
inherited controls (`4/4`), the PA15 free/indirect controls (`3/3`), the two
PA12 functional-cast controls (`2/2`), the successful bounded stdin
reductions, and the emitted inherited LowIR shape containing
`index i8 [projection=base_subobject] ... , 0` before `@B__f`.  The full PA16
run is `48/243` with `195` failures and `243/243` coverage; through-PA15 is
`1167/1167` with exit `0`; and the file audit exits `0` with five pre-existing
header-division warnings.  No timing, RSS, allocation, or structural-counter
measurement is claimed.

## Next checkpoint

The next bounded checkpoint should own the separately deferred inherited-field
and qualified-base boundaries if supervisor review confirms their scope.
Protected/friend/using access, operators/ADL, constructors/lifetime, virtual
dispatch, and broader conversion expansion remain separate checkpoints; this
record does not claim full PA16 completion.

## Checkpoint ledger

| checkpoint | status |
| --- | --- |
| `current checkpoint (completed)` direct + inherited unqualified member-call boundary | Completed typed lexical/direct-base lookup, canonical owner/hidden-`this` facts, shared explicit-member selection with path reuse, and validated PA15 base-subobject projection. Focused six-test result is `3/6`; full PA16 is `48/243` with `195` failures and `243/243` coverage, exactly one removed failure and no additions; through-PA15 is `1167/1167`; file audit passes with five pre-existing warnings. |
| `37265733` typed member projection audit/repair | Completed bounded audit/repair; direct/nested dot and arrow ownership remains traced through PA12, PA11 `RecordLayout::member_offsets`, and PA15 LowIR, with PA16 still incomplete at that checkpoint's existing failure baseline. |
| `b1e8272d` + PA16 typed implicit-object boundary | Prior landed checkpoint record preserved: canonical Function-scope hidden-object ownership, fail-closed viability, typed demand indexing, direct PA15 lowering, focused `6/6` + `2/2`, and reported final `47/243` with `243/243` coverage. |
| `0b534f2f` typed direct member-call checkpointAudit | Completed bounded audit/repair: implicit-object cv subset ranking, N3485 variadic comparison, single-owner typed PA15 reachability, dense FunctionFact/fact metadata, declaration-only member declarations with hidden-object/cv ABI boundaries, hidden-object call formation, and source-file sizing are repaired. Focused controls and course regressions pass; broad gates record through-PA15 `1167/1167`, final PA16 `47/243` with `196` failures and `243/243` coverage, zero failure-identity additions/removals, and PA16 still incomplete. |
