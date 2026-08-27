# PA16 implementation plan

## Stage Design/spec alignment and owner/data flow

PA12 owns one typed MemberLookup boundary. It inspects the actual class
scope, then the ordered non-virtual direct-base chain; the first declaration
set hides all bases. The result carries the lookup kind, canonical BindingId,
owning class ScopeId, selected type, and typed NamedRecordId base path. Values
precede types; ambiguous declaration sets and imported/unsupported ownership
are blocked.

Explicit dot/arrow fields and non-static member calls consume that result.
Unqualified and parser-supported qualified inherited fields and calls in
member bodies use the exact synthetic this binding. Calls retain the selected
owner/path through the shared call selector, including cv and argument ranking.
Access is checked at the selected owner. Public and same-owner access retain
their existing behavior; protected access walks the bounded lexical class
scopes and accepts an eligible scope only when its typed class derives from the
owner. For non-static members, the actual object must also be that access class
or a further-derived class; protected static object spelling has no such
second relation. Private and unrelated access remain rejected. This preserves
the N3485 §11.7 nested-class member access right without changing owner or
binding identity. The lexical walk is bounded by the scope-vector size and
fails closed if a valid cursor remains after that bound; ordinary invalid
parent termination and same-owner access retain their existing outcomes.

PA15 consumes the selected owner for member addresses, validates the actual
object-to-owner direct-base path and every complete RecordLayout edge at
offset zero, emits one projection=base_subobject per edge, and then uses the
owner layout's member_offsets for projection=field. Dot/arrow objects are
evaluated once. The ownerless semantic_injected_member producer remains the
separate storage-backed anonymous-union path; PA15 checks that backing marker
before requiring a selected owner.

Ordinary local implicit default construction now validates a bounded typed
walk over direct bases and non-static member TypeKey/sidecar facts, with an
active-record cycle guard. A state-free inherited chain publishes no
constructor action, synthetic helper, or demand edge; a base/member/default
initializer or unsupported subobject fails closed. Anonymous-union storage
keeps its separate constructor action. The pre-existing no-base empty-class
constructor fact is retained for the PA12 semantic contract
(300-reference-binding-pointee-const-pointer.t); this is not a blanket
derived-construction restriction.

The active static-function path follows root `spec.md` §§2--5: PA11's
`FunctionFact`/`BindingId` and `BindingSidecar::static_member` remain the sole
typed owners. PA12 resolves a class-qualified call through typed class lookup
and `MemberLookup`, so the first owning declaration set hides later bases. A
class-qualified spelling with no implicit object selects only static functions
and publishes the raw callable `TypeId` with no implicit-object child; an
inherited `D::f` therefore retains `B`'s binding and owner. In a non-static
member body, a current/base class qualifier and an ordinary unqualified name
use one candidate set containing both static and non-static functions, ranked
by explicit argument conversions plus typed implicit-object qualification
where applicable. A static winner has no hidden object; a non-static winner
has the exact synthetic `this` BindingId as child zero and an owner-qualified
callable TypeId. In a static body, nearer lexical declarations are checked
first, then the enclosing class/base member set is filtered to static
functions. Parenthesized callees use these same branches; an inherited static
keeps its declaring owner instead of reopening an outer namespace.

PA12 checks access after the selected static or non-static member owner is
known. PA15 builds one dense `BindingId -> class ScopeId` owner index from
class-scope-owned bindings, rejecting duplicate/out-of-range ownership, then
validates each static fact's selected owner in O(1). It follows direct static
call facts from namespace roots into class-owned `FunctionFact` definitions or
bodyless declaration boundaries, recursively scanning reached static bodies
with dense visited worklists. Binding/owner/body identity is validated at the
demand boundary; static function parameters remain source parameters, with
no hidden object. Qualified class claims with no static target fail closed.

## Failure Map and coverage identity

Turn-start baseline is the authoritative log
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log:
PA16 55/243, 188 failures, 243/243 covered. The landed increment's recorded
full-stage delta is added `∅`, removed:
`pa16/tests/general/100-static-member-overload-skips-nonstatic-this.t`,
`pa16/tests/general/100-static-member-qualified-call.t`,
`pa16/tests/general/200-const-cast-pointer-reference-alias.t`,
`pa16/tests/general/200-inherited-static-member-qualified-call.t`,
`pa16/tests/general/200-static-nonstatic-same-pointer-signature.t`, and
`pa16/tests/general/300-lazy-nested-class-enclosing-alias-lookup.t`.

The earlier pre-increment plan records the historical 48/243 to 49/243
improvement from removing pa16/tests/general/200-protected-base-method.t.
That evidence is preserved separately from the landed increment's current
55/243 identity. The six landed removals had no additions and retained
243/243 coverage. The final post-repair `make test-pa16` is exit `2` at
`55/243` with `188` failures and `243/243` coverage; its sorted failure
identity set is exactly the turn-start set, with added `∅` and removed `∅`.

The typed inherited-member checkpoint is landed in 9f158daa. Its prior
evidence carries the actual object TypeId into protected field/call
accessibility, repairs the protected static exception, repairs eligible
enclosing lexical access scopes for nested classes, and expands the
course-405 matrix. The prior through-PA15 result is 1167/1167 and the prior
file audit exits 0 with the five existing header warnings.

The overload primary remains out of this boundary: its exact current error is
PA15 unsupported address expression. A debug lowering breakpoint showed the
first failing address fact is SemanticFactKind::Literal, fact id 9,
array/string literal element count 2, type id 29, with invalid
constant_address; no lower_expression_impl scalar-default breakpoint was hit.
The local string-literal address issue is therefore not a scalar member-call
fact.

Focused current evidence: the new course
`406-typed-static-member-function-audit-regression.sh` exits 0 and covers
parenthesized qualified/unqualified calls, class-qualified non-static
rejection, mixed static/non-static overload ranking in both qualified and
unqualified member-body spellings, inherited static-body owner/hiding,
protected/private access, declaration-only emission, redeclaration identity,
recursion, and the raw static ABI. Course 405 exits 0 and covers the prior protected field/method
matrix, nested `Derived&` access, `Base&` rejection, and the existing PA15
static projection boundary. Courses 401--404 each exit 0; 402 retains the
expected PA12 inherited-member negative diagnostic. The checked-in protected
positive control is 1/1. No handout fixture or `.ref` file changed.

The landed static checkpoint resolves these exact target identities:
`pa16/tests/general/100-static-member-qualified-call.t` and
`pa16/tests/general/100-static-member-overload-skips-nonstatic-this.t` (the
PA15 direct-call emission boundary),
`pa16/tests/general/200-inherited-static-member-qualified-call.t` (PA12
qualified inherited lookup), and
`pa16/tests/general/200-static-nonstatic-same-pointer-signature.t` (the same
PA15 boundary). The seven focused handout tests pass 7/7, including controls
`100-builtin-prefix-static-member-call`, `100-member-methods`, and
`200-protected-base-method`; the focused handout matrix is 7/7 and course
controls 401--406 each exit 0. The final broad identity remains exactly
`55/243`, `188` failures, and `243/243` coverage, with no identity additions
or removals.

## Checkpoint Status

`checkpointAudit` is complete for landed `021ef639` plus the bounded
mixed-overload, owner-index, documentation, and course-regression repairs.
The static call path changes only PA12 class/member lookup, access and fact
publication, and PA15 reachable definition/declaration planning; static
data-member storage remains out of scope. Focused validation and through-PA15
are green; broad PA16 matches the recorded baseline at `55/243` with the same
`188` failure identities and `243/243` coverage, and the file audit passes with
five pre-existing warnings.

Prior inherited field/call selection, owner-path lowering, protected
derived-body access, and state-free construction-demand evidence remains in
landed `9f158daa`/`8b445ee6` and course 405. The next checkpoint is distinct:
static data-member storage/lifetime and its reachable initialization path,
not another audit of this static function lookup/emission boundary. The
overload local string address gap, explicit-constructor parsing/lifetime,
broader initialization, friend/using re-exposure, multiple/virtual
inheritance, operators/ADL, and full PA16 success remain out of scope here.

## Performance evidence and uncertainties

Same-owner member selection is an immediate scope lookup. Protected access
walks lexical scopes of depth `S`, records `L` class scopes, and performs at
most two direct-base paths of depth `D` per candidate, so its worst-case bound
is `O(S + L*D)` (ordinary member bodies have `L=1`); static access can stop
after the owner proof. Static-body lookup adds one bounded lexical walk before
the enclosing class/base lookup; member field/call lowering validates a typed
`O(depth)` path and layout sequence, with no whole-program scan, retry, or
rendered-name recovery. The implicit-default predicate walks only the
reachable typed base/member graph per declaration; its active vector is
bounded by the supported record depth and protects cycles. No cache was
needed for this checkpoint.

The static demand extension reuses dense FunctionFact, BindingId, and
SemanticFact identity tables with monotonic visited marks. PA15 first scans
class-scope-owned binding vectors once to build the typed owner index; this is
`O(B_class)` setup, with duplicate or malformed ownership failing closed.
Each reachable function body and semantic fact is then scanned once, each
typed child/call edge is enqueued only through that scan, and each static
selected-owner check is O(1). Including the existing model-wide namespace-root
enumeration, the honest bound is near-linear:
`O(F_model + B_class + F_reachable + semantic facts_reachable + call edges)`;
there is no per-fact owner-values scan, whole-program retry, or rendered-name
recovery. Qualified static lookup adds only the bounded class/direct-base
chain and candidate set.

The landed-increment measurement used a deterministic temporary non-repository
source with one
reachable static-call chain of 1, 128, and 512 definitions ran five times per
size; all 15 compiler invocations exited 0. Elapsed times were 0.00s, 0.01s,
and 0.03s per size; RSS ranges were 5,296--5,368 KB, 6,912--7,052 KB, and
12,232--12,376 KB respectively. These are modest repeated observations, not a
formal benchmark. In this audit, five runs of
`406-typed-static-member-function-audit-regression.sh` (including its
recursive/inherited static demand chain) took `0.17--0.18s`, with RSS
`7,040--7,320KB`. These are representative smoke measurements, not a formal
benchmark. The seven-test handout probe took `0.20s` and `9,824KB`. A separate
temporary nested-access source with 256
protected-field expressions in one out-of-class nested member used lexical
depths `L=1`, `8`, and `32`, with three invocations per depth; all exited 0,
with maximum elapsed time 0.01s and maximum RSS 7.4MB. These are small
measured bounded-behavior samples, not formal benchmarks. The prior file audit
recorded only the five existing header bad-division warnings; the final file
audit has the same five warnings and no new owner-index or duplication
warning.

The required `make test-pa16` exits `2` only because the unchanged baseline
has `188` failures; through-PA15 exits `0` at `1167/1167`, the PA16 file audit
exits `0`, and the final clean-tree commit is the checkpoint handoff. No
handout fixture or `.ref` file changed.

## Checkpoint ledger

| checkpoint | status |
| --- | --- |
| PA16 typed static member-function lookup/reachable emission | Completed `checkpointAudit`: the mixed static/non-static member-body selector, typed PA15 class-binding owner index, and course-406 reductions are focused-green; final PA16 is `55/243` with the exact turn-start `188` failure identities and `243/243` coverage, through-PA15 is `1167/1167`, and the file audit passes with five pre-existing warnings. The next checkpoint is separately bounded static data-member storage/lifetime. |
| 9f158daa | Landed typed inherited field/call selection and lowering, protected derived-body qualified calls, state-free inherited construction demand, and the 48→49/243, 195→194 identity improvement with 243/243 coverage. Through-PA15 is 1167/1167 and audit exits 0 with five pre-existing warnings. |
| 8b445ee6 protected object access scope checkpointAudit | Completed bounded audit/repair: typed protected object proofs are traced through field/call facts, the static-member exception is repaired, eligible enclosing lexical scopes cover nested-class access, and course 405 covers the required matrix. Final PA16 is 49/243 with 194 failures and 243/243 coverage; exact failure additions/removals are ∅/∅. Courses 401–405, through-PA15 at 1167/1167, and the file audit pass; the next checkpoint is a separately bounded PA16 surface. |
| 0b534f2f typed direct member-call checkpoint | Landed implicit-object cv subset ranking, N3485 variadic comparison, typed PA15 member reachability, dense FunctionFact/fact metadata, declaration-only member ABI boundaries, hidden-object call formation, and source-file sizing. |
| b1e8272d + PA16 typed implicit-object boundary | Landed canonical Function-scope hidden-object ownership, fail-closed viability, typed demand indexing, direct PA15 lowering, and focused direct/member-call controls. |
| 37265733 typed member projection audit/repair | Landed direct/nested dot and arrow ownership tracing through PA12, PA11 RecordLayout::member_offsets, and PA15 LowIR. |
