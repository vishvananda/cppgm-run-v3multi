# PA16 implementation plan

## Stage design/spec alignment and owner/data flow

This checkpoint keeps one typed production path from PA10 call syntax through
PA12 semantic facts, PA15, and LowIR.  The member-call probe handles both the
existing `CallExpression(MemberExpression)` form and an unqualified `id(...)`
inside a non-static member body.  For the latter, typed lookup checks nearer
block/function declarations, type declarations, the direct class scope, and
then the ordered single direct-base chain before ordinary enclosing lookup can
see a namespace value.  At each scope the value declaration set is probed
before the tag/type set, so a same-scope ordinary method hides a same-spelled
class or enum tag.  A type-only first declaration set returns its owning
`TypeId` as an explicit outcome and is consumed by the existing functional-cast
producer; it cannot reopen enclosing namespace/ADL value lookup.  A found
class/base declaration set suppresses unrelated namespace/ADL candidates; a
base-owned value set with no supported non-static method is rejected by its
nonempty typed base path, while an unsupported imported origin is an explicit
`Blocked` outcome.  Both cases fail closed.
Nearer lexical/direct-class values retain the ordinary resolver's existing
fallback.  No declaration is recovered from text.

`prepare_pa12_member_parameter` owns the exact synthetic first `this`
parameter in the member Function `Scope`.  A successful call publishes the
typed `this` value as semantic child zero with `OP_ARROW`, followed by
converted/defaulted explicit arguments in source order.  Its callable Function
type has the hidden object pointer first, while the original member Function
type remains the ABI/signature owner.  The unqualified helper reuses the
already-selected `BindingId` when creating child zero, so semantic lookup is
performed once and no name/text recovery is needed.

`direct_base_chain` is keyed by `NamedRecordId`, validates class-scope
back-references and consistent direct-base metadata, and checks
invalid/virtual/cyclic relations with a bounded walk.  It supplies the
ordered lookup path.  The unqualified lookup passes that path to the shared
member selector, so an inherited call does not re-walk it during semantic
candidate selection.  The same-owner path returns before chain allocation;
inherited selection uses the one typed path produced by lookup.  Value/type
ownership is represented by an explicit outcome (`None`, `Value`, `Type`, or
`Blocked`) plus the selected scope/type, not by an invalid `ScopeId` sentinel.
A base-owned value set with no supported ordinary non-static method is blocked
by the nonempty typed base path, while a value origin from an unsupported
import is `Blocked`; ordinary enclosing lookup cannot reopen in either case.
Nearer lexical/direct-class values keep their existing ordinary-resolver
fallback.  The selector admits only ordinary non-static
methods from the owner scope, performs
the existing implicit-object
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

The supervisor-supplied turn-start evidence at assignment HEAD `b1a9e589` is
`48/243` passing, `195` failing, `243/243` covered, and `make test-pa16` exit
`2`.  Its complete failure map is preserved in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.

The required through-PA15 command
`n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
exits `0` with `1167/1167` passing.  The PA16 file audit exits `0` with five
pre-existing `bad-division` warnings.  `make test-pa16` exits `2` with
`48/243` passing, `195` failures, and `243/243` coverage.  Comparing its exact
failure identities with the turn-start `195`-failure map in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` gives
added set `∅` and removed set `∅`; the complete failure identity set is
unchanged.  No fixture or reference was changed.

## Active checkpoint

The active selector checks implicit-object qualification compatibility before
ranking and reuses the existing explicit-argument conversion/default logic.
The unqualified probe searches nearer lexical scopes, probing values before
types, then the direct class and the first matching declaration set in the
direct-base chain.  It admits only ordinary non-static methods and then
publishes the selected `BindingId`, its owner `ScopeId`, the hidden-object
callable `Function TypeId`, and typed children consumed by the existing PA15
`lower_call`.  A type-only result instead carries its selected `TypeId` into
the existing functional-cast producer, including the focused direct and
inherited scalar cases, and unsupported targets fail closed.  A base-owned
non-callable value or unsupported imported value is likewise `Blocked` and
fails closed; nearer lexical/direct-class values still use the existing
ordinary fallback.  A successful
inherited call therefore carries actual `D*` in child zero while its selected
callable requires owner `B*`; PA15 validates and emits the typed
base-subobject projection between them.  The semantic tail guard rolls back
an unsuccessful probe, so no failed speculation can publish a call fact or
demand edge.

This checkpoint intentionally does not add inherited fields, qualified base
calls, protected/friend/using-imported access, constructors/lifetime, aggregate
initialization, ADL or operators, static calls, virtual/ref-qualified methods,
or broader user-defined conversions.  The supported relation remains one
non-virtual direct base per class.  The bounded three-level reduction emitted
two ordered offset-zero base projections before `@A__f`, while multiple
inheritance and virtual inheritance remain outside this checkpoint.

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

Measured evidence is the focused six-test command (`1/6`, exit `2`), the
three direct/inherited controls (`3/3`, exit `0`), course regressions 400, 401,
and 402 (each exit `0`), the new 402 typed tag/method and direct/base type-only
LowIR assertions plus the rejected inherited-field/outer-call regression, the
successful bounded reductions, through-PA15
`1167/1167`, the full PA16 `48/243` with `243/243` coverage, and the five
file-audit warnings listed in the audit record.  No timing, RSS, allocation,
or structural-counter measurement is claimed; the performance evidence is
the bounded structural work and these exact control results.

## Next checkpoint

The next bounded checkpoint should own the separately deferred inherited-field
semantics and qualified-base boundaries if supervisor review confirms their
scope.  This checkpoint only makes inherited non-callable value sets fail
closed; it does not implement inherited fields.
Protected/friend/using access, operators/ADL, constructors/lifetime, virtual
dispatch, and broader conversion expansion remain separate checkpoints; this
record does not claim full PA16 completion.

## Checkpoint ledger

| checkpoint | status |
| --- | --- |
| `b1a9e589` direct + inherited unqualified member-call checkpointAudit | Completed bounded audit and narrow repairs for canonical owner/hidden-`this` facts, value-before-type lookup with explicit `Type`/`Blocked` outcomes, fail-closed base metadata and inherited value-set ownership, shared selection, and PA15 base-subobject validation. Focused six-test result is `1/6` on the same five prerequisite identities; through-PA15 is `1167/1167`, file audit exits `0` with five pre-existing warnings, and full PA16 is `48/243` with `195` failures and `243/243` coverage, with zero failure-identity additions or removals. |
| `37265733` typed member projection audit/repair | Completed bounded audit/repair; direct/nested dot and arrow ownership remains traced through PA12, PA11 `RecordLayout::member_offsets`, and PA15 LowIR, with PA16 still incomplete at that checkpoint's existing failure baseline. |
| `b1e8272d` + PA16 typed implicit-object boundary | Prior landed checkpoint record preserved: canonical Function-scope hidden-object ownership, fail-closed viability, typed demand indexing, direct PA15 lowering, focused `6/6` + `2/2`, and reported final `47/243` with `243/243` coverage. |
| `0b534f2f` typed direct member-call checkpointAudit | Completed bounded audit/repair: implicit-object cv subset ranking, N3485 variadic comparison, single-owner typed PA15 reachability, dense FunctionFact/fact metadata, declaration-only member declarations with hidden-object/cv ABI boundaries, hidden-object call formation, and source-file sizing are repaired. Focused controls and course regressions pass; broad gates record through-PA15 `1167/1167`, final PA16 `47/243` with `196` failures and `243/243` coverage, zero failure-identity additions/removals, and PA16 still incomplete. |
