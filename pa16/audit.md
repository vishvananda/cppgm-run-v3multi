# PA16 checkpoint audit

## Current Checkpoint Review

This review covers landed commit `32c4546307b85d19226a07b5619cf6bbc482c404`
relative to parent `a2ac5256`, plus the bounded audit repairs and one
focused course regression.  It audits only typed class-object construction:
local-object roots, scalar/pointer empty-brace and scalar DMI facts, supported
aggregate/class-subobject DMI, implicit and in-class explicitly-defaulted
default constructors, ordered base/member actions, explicit DMI overrides,
demanded synthetic helpers, and constructor call/unwind metadata.  Copy/value
semantics, out-of-class definitions, virtual or multiple inheritance,
parameterized constructor argument lowering, global/TLS lifetime and guards,
destructors outside a directly implicated regression, and unrelated
operators/ADL/access/static-data surfaces remain outside this review.

The authoritative turn-start full-stage state is `80/243` passed, `163`
failed, and `243/243` covered, from
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The final `make test-pa16` exits `2` at `80/243`, with `163` failures and
`243/243` covered; exact failure-identity additions and removals against that
log are both `∅`, and the coverage-identity additions and removals are both
`∅`.  Its complete output is
`/tmp/v3multi-pa16-full-final2.znmVUW.log`.  The exact prior-through-PA15 gate
exits `0` at `1167/1167`, and the required file audit exits `0` with five
pre-existing header-division warnings.

The complete affected ownership path is:

```text
PA10 class member/special-member/local-object syntax
  -> PA11 canonical class ScopeId + field BindingId + constructor BindingId
     + FunctionFact and typed DMI ownership
  -> PA12 semantic initializer facts + ordered ConstructorActionFact ranges
     (base first, then declaration-order fields; explicit initializers win)
  -> PA15 recursive demand/nothrow worklists and completed typed layout
     offsets, with no eager helper emission or textual recovery
  -> LowIR demanded synthetic helper/calls/actions, typed subobject
     projections/stores, and truthful call/unwind metadata
```

### Findings and bounded repairs

- PA11 preserves one canonical class `ScopeId`, field `BindingId`, and
  constructor `BindingId`; `FunctionFact` carries the constructor owner and
  action range, while DMI ownership remains on the field sidecar.  The audit
  found and repaired named empty classes taking a legacy constructor-binding
  path with no `FunctionFact`, which made an empty class-subobject DMI fail at
  PA15 demand time.  The repair is limited to named non-union classes; the
  older anonymous/union representation remains outside this checkpoint.
- Both value-initialization lowering paths validate the selected named-class
  binding, FunctionFact, record, and owner identity before consulting
  `FunctionFact.synthetic`; a missing or contradictory fact fails closed and
  is never treated as an implicit/defaulted constructor.  The canonical
  zero-argument FunctionFact is therefore retained end to end.
- PA12 builds DMI semantic facts before constructor actions publish their
  ranges.  Actions are emitted base-first and then in declaration order;
  explicit mem-initializers win over a field DMI.  The construction snapshot
  copies class member IDs before helper synthesis can grow binding/fact
  vectors, and the audit added subtraction-safe arena checks plus canonical
  owner checks.  No retained reference crosses a vector growth point.
- PA15 demand is recursive but memoized: named-record runtime state and
  constructor/semantic nothrow state each distinguish unseen, in-progress, and
  complete.  Shared DAG edges are reused, cycles are rejected/conservative,
  helper emission is demand-driven and deterministic, and no textual recovery
  or whole-program retry is used.  Function identity, subobject paths, and
  direct-base/member layout offsets remain BindingId/ScopeId/RecordLayout
  facts rather than reconstructed names.
- The audit found that empty class-subobject value-initialization omitted the
  required zero-initialization step.  A typed `value_initialize` fact now
  causes LowIR zeroing only when the selected default constructor is implicit
  or in-class-defaulted (`FunctionFact.synthetic`); a user-provided constructor
  is called without that step.  Aggregate empty-list lowering also now applies
  direct and nested DMIs to omitted members.  Zeroing uses chunks whose widths
  divide the recorded object alignment, with byte fallback, and validates
  complete size/alignment and LowIR offset ranges.
- Constructor calls/actions are emitted exactly once with typed subobject
  projections.  Synthetic constructor unwind is marked `unwind=no` only when
  the cached constructor/action analysis proves the emitted path nothrow;
  user-provided calls retain the throwing boundary unless their declaration
  fact says otherwise.  Range, child-ID, action-operation, function-scope,
  binding-owner, and array/index validation now fail closed before unsafe
  vector access.
- Complexity remains near-linear for the owned path: each reachable record,
  constructor action, and semantic edge is cached/scanned once, each owner and
  layout lookup is O(1), and helper work is proportional to demanded DAG
  edges.  The remaining expensive operations are bounded by declared direct
  inheritance/member depth; no per-use whole-program retry was introduced.

## Focused Evidence

`make -C dev cppgm++` exits `0`.  The copied 11-fixture focused comparison is
`10/11`: all eight prior construction identities, `200-single-inheritance.t`,
and `200-empty-class-member-declaration.t` pass; the one unchanged failure is
`300-value-init-aggregate-with-nontrivial-member.t`, whose remaining
canonical diff is the alignment-safe `i32` zero chunks versus the reference's
single `i64` bulk store, plus the pre-existing boolean conversion difference.
No handout/reference files were used as writable outputs.

Course regressions `400` through `407` each exit `0`, including the extended
course-404 checks for state-free and unused-DMI demand, implicit/defaulted
versus user-provided value-initialization, nested aggregate DMI ordering, and
direct-base/member construction.  `sh -n` on all eight controls exits `0`.
The named empty-class probe exits `0`; a value-initialized class-subobject
probe emits zero stores before the synthetic call and the DMI store in the
callee; a user-provided-constructor probe emits no caller zeroing.  Separate
one-, three-, eight-, and aligned integer-layout probes emit `i8`, `i8`,
`i64`, and alignment-safe `i32` chunks respectively.  A nested aggregate
empty-list probe emits the untouched-field zero followed by nested DMI values.

The bounded 13-link/32-DMI stress probe compiled successfully in five runs:
each was reported as `0.00s`, with maximum RSS from `5824` to `6056KB`; the
timing files are in `/tmp/codex-pa16-stress-final.Tn9MSH` as `stress-1.time`
through `stress-5.time`.  Its current LowIR has 14 constructor helpers, 14
constructor calls, 13 base projections, 45 field projections, and 45 typed
field stores.  This is representative smoke and structural evidence, not a
benchmark; the dense cache/worklist bounds above are the performance claim.
The final stage, through-stage, and file-audit results are recorded above and
in the ledger below.

## Historical Static Member-Function Checkpoint Review

This review covers landed commit `021ef63927293f62e13a29b5b8265c7105fb35a9`
relative to parent `15e133af`, plus the bounded audit repairs and one focused
course regression in that checkpoint.  It is limited to typed
static member-function lookup and reachable emission: qualified and
unqualified calls, class/base hiding and overload filtering, access, canonical
owner/binding/type continuity, PA15 demand, declaration/definition emission,
recursion, and the raw static ABI.  Static data storage, constructors and
lifetime, operators/ADL, broad initialization, friends/using, and
multiple/virtual inheritance remain outside that review.

The authoritative turn-start full-stage state was `55/243` passed, `188`
failed, and `243/243` covered, from
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The final `make test-pa16` exited `2` at the same `55/243`, with `188`
failures and `243/243` coverage; sorted failure-identity comparison was exact,
with no additions or removals.

The affected ownership path was:

```text
PA10 qualified/unqualified IdExpression or parenthesized call
  -> PA12 typed qualifier/class-scope and MemberLookup selection
  -> first owning class ScopeId + canonical BindingId + raw function TypeId
  -> static-only candidates without an implicit object, or one mixed
     static/non-static member set when a non-static body supplies this
  -> access check at the selected owner; raw static or hidden-object fact
  -> PA15 namespace roots and reached static bodies walk typed call facts
  -> FunctionFactId identity chooses definition demand or declaration plan
  -> LowIR symbol uses the retained owner and raw source parameters
```

### Findings

- The landed helper correctly resolves a qualified class type through
  `lookup_type_path`, uses `member_lookup` so the first direct/base
  declaration set hides later bases.  With no implicit object, it filters that
  set to static functions; in a non-static member body, current/base-qualified
  and unqualified calls retain both static and non-static functions in one
  typed overload set.  PA12 call facts keep the selected raw callable `TypeId`
  for a static winner and the owner-qualified hidden-object type for a
  non-static winner; the inherited owner remains the selected `ScopeId`/
  `BindingId`.  PA15's `function_binding_fact_index_` then follows that same
  identity into a definition or declaration boundary, and
  `function_components`/`abi_function_symbol` retain the declaring owner.
- The mixed-set comparator follows N3485 §13.3.1 and §13.3.3: a static
  candidate's implicit-object ICS1 matches any object but establishes no
  conversion sequence, so it is neither better nor worse than another
  candidate on that dimension.  Object qualification is therefore compared
  only between two non-static candidates; explicit argument conversion ranks
  remain the common ranking criteria for static/non-static pairs.
- The audit found a fail-open class-qualified case: if the selected class name
  had only a non-static function, a type, a blocked name, or no static
  candidate, the old boolean result could reopen ordinary value lookup and
  manufacture a no-object call.  The repair separates “class-qualified name
  claimed” from the static candidate vector, unwraps parenthesized callees,
  and rejects the claimed spelling when no static target exists.  A valid
  current/base-qualified call in a non-static member body instead leaves the
  class claim to the unified member selector, so a viable static or non-static
  candidate can win without the first category suppressing the other.
- Static-body unqualified lookup now searches nearer block/function lexical
  declarations first, then the enclosing class's typed direct/base member set.
  An inherited static function therefore retains its base owner even when an
  outer namespace function has the same spelling; a direct non-static
  declaration still hides base declarations before static-only filtering in a
  static body.  In a non-static body, the first owning set supplies both
  categories to the same argument/object ranking.  The lookup is bounded by
  the scope vector and never reopens an outer value set after a class member
  has claimed the name.  Parenthesized callees use the same typed branches.
- The landed access gate covered qualified static candidates only.  The audit
  adds the same `member_accessible` check after selection for every
  class-owned static binding, including an unqualified static-body call.
  Protected inherited static access is granted by the derived access-class
  proof without a non-static object relation; private or unrelated access
  fails closed.
- The landed PA15 demand branch treated every indexed static `FunctionFact` as
  a definition.  Static declarations can have a valid fact with no body, so
  the repair validates binding/owner identity and distinguishes a complete
  function/body scope (definition demand) from a bodyless declaration
  (declaration demand).  Missing or contradictory definition facts, scopes,
  body ranges, owner records, and callable types fail closed.  Recursive
  static edges still use the existing visited function/fact worklists and are
  emitted once.  Before traversal, PA15 builds one dense
  `BindingId -> class ScopeId` index from class-scope-owned bindings; duplicate
  or out-of-range ownership fails closed, and each reached static fact checks
  that index in O(1) instead of scanning the owner's values.
- The added course regression covers parenthesized qualified and unqualified
  static calls, qualified non-static rejection, both directions of mixed
  static/non-static overload ranking in qualified and unqualified member
  bodies, tied-explicit-rank neutral-ICS ambiguity in each of those spellings,
  inherited static-body lookup against an outer same-spelled function,
  protected/private access, an inherited declaration-only static call,
  same-binding redeclaration identity, recursive demand deduplication, and
  raw parameter-only static ABI.  The existing handout matrix covers
  static/non-static filtering and qualified inherited owner retention.

## Focused Evidence

`make -C dev cppgm++` exits `0`.  The focused handout command

```sh
make -C pa16 check TEST='tests/general/100-static-member-qualified-call.t tests/general/100-static-member-overload-skips-nonstatic-this.t tests/general/200-inherited-static-member-qualified-call.t tests/general/200-static-nonstatic-same-pointer-signature.t tests/general/100-builtin-prefix-static-member-call.t tests/general/100-member-methods.t tests/general/200-protected-base-method.t'
```

exits `0` with `7/7` passing.  Course regressions 401--406 each exit `0`; 402's
`PA12 inherited member name is not callable` line is from its expected
negative case.  Course 406 independently rejects the qualified and
unqualified tied-explicit-rank mixed calls with `PA12 ambiguous member call`.
`sh -n` on the new script exits `0`.  No handout test, fixture, or `.ref` file
changed.

Five measured invocations of the new bounded regression after the neutral-ICS
repair (including its recursive/inherited static demand chain) took
`0.31--0.34s`, with RSS `7,064--7,268KB`; the seven-test handout probe took
`0.20s` and `9,824KB`.  These are representative smoke measurements, not a
formal benchmark.  The
new lookup performs only bounded lexical/class/base walks, and PA15 performs
one class-binding owner-index setup followed by O(1) selected-owner checks
and dense visited worklists; no whole-program retry, textual recovery, or
generated-artifact dependency was added.

The existing `pa16/tests/general/200-out-of-class-member-default-argument.t`
still fails in that tree before reaching the static declaration audit; that
pre-existing default-argument merge gap was not part of the landed static
ownership increment.  The focused declaration control consequently used a
bodyless static declaration without a default argument.  The required
through-PA15 command exited `0` at `1167/1167`; the PA16 file audit exited `0`
with five pre-existing header `bad-division` warnings.  `git diff --check`
passed before the final commit, and no handout test, fixture, or `.ref` file
changed.

## Historical Protected-Access Checkpoint Review

This review covers landed commit `8b445ee6e7b7090a1f2d19edebbc96d756f438ad`
relative to parent `9f158daa4cf5fd8326123ca9ccded1b4c59df382`, plus one
narrow source repair, one lexical-access repair, and the course regression
expansion in this audit.  It is limited to protected member access-scope
handling for field and method expressions: protected static object spelling
uses only the access-class/owner proof, while protected non-static members
also impose the object-expression rule.  Lookup, owner-path lowering,
constructors/lifetime, friends/using, operators/ADL, and other PA16 failures
are not re-audited here; PA15 is traced only where it consumes the affected
typed facts or reports the existing static projection boundary.
The bounded audit is complete and committed, and final validation leaves the
working tree clean.

The complete owned path is:

```text
PA10 MemberExpression/CallExpression or member-body IdExpression syntax
  -> PA12 typed actual-record lookup over class/direct-base scopes
  -> selected BindingId + owner ScopeId + typed base path
  -> actual object TypeId (this, dot object, or arrow pointee)
  -> member_accessible(binding, owner, access scope, object)
  -> semantic MemberExpression/CallExpression fact with selected owner
  -> PA15 typed owner/path validation and ordered base-subobject projections
  -> LowIR field address or non-static call with the hidden object pointer
```

### Findings

- The landed call-site changes carry the actual object into all three affected
  non-static paths: implicit member fields pass the typed `this` record,
  explicit fields pass the dot record or arrow pointee, and selected calls pass
  the normalized dot/arrow object.  The selected `BindingId` and owner
  `ScopeId` remain the semantic facts consumed downstream.
- `member_accessible` walks the bounded lexical scope chain and records each
  class scope from innermost to outermost.  For protected members it obtains
  each candidate's canonical named `TypeId` from the typed `TypeKey` index and
  accepts the first candidate that derives from the declaring owner with
  `member_base_path` and, for non-static members, also has the actual object
  type derived from that candidate.  This gives a nested class inside
  `Derived` the enclosing `Derived` access rights required by N3485 §11.7,
  while same-owner access still returns at the existing scope boundary and
  private/unrelated access remains rejected.
- For protected non-static members, the second proof strips the supported
  reference/cv layers from the actual object and requires a named record whose
  direct-base path contains the access class.  Thus a `Derived` or
  further-derived object is accepted in a `Derived` body, while a `Base&`,
  `const Base&`, or `const Base*` object is rejected.  The check is identity-
  based and does not render or recover a type name.
- The audit found one narrow exception in the landed helper: the new object
  proof was also applied to protected static members.  The repair returns
  after an eligible access-class/owner proof for a static binding, because
  C++'s additional object-expression restriction is only for non-static
  members.  The existing PA15 static-member projection boundary remains
  outside this checkpoint.
- The nested-class probe initially reached `PA12 record member is
  inaccessible` with only the innermost `Nested` class considered.  A
  constructor-free out-of-class `Derived::Nested` member definition reaches
  the same helper without widening PA15 nested-function emission; the lexical
  candidate walk then accepts its enclosing `Derived` access class for both a
  protected field and method through `Derived&`.  The corresponding `Base&`
  object reduction remains rejected by the second proof.  An inline nested
  call still encounters the pre-existing `PA15 direct call target was not
  emitted` boundary, so it is not used as a lowering claim here.
- The lexical walk is explicitly bounded by the scope-vector size and now
  requires an invalid cursor on exit.  A valid out-of-range cursor or a valid
  cursor left after cycle exhaustion fails closed before any collected class
  can grant protected access; ordinary invalid-parent termination and the
  same-owner return inside the walk are unchanged.
- Access is checked after member-call overload/cv selection, and field/call
  facts retain their selected binding and owner.  The existing semantic tail
  guard rolls back failed member-call probes; the new helper is const and its
  path walks use only local vectors, so failed accessibility does not publish
  a fact, demand edge, or fallback ordinary-name lookup.
- PA15 independently validates the selected actual-object-to-owner relation
  and complete zero-offset layouts before emitting each typed base-subobject
  projection.  This preserves the existing fact continuity from PA12 through
  the field/call LowIR consumers.

## Focused Evidence

`sh cppgm.tests/course/pa16/405-protected-object-access-regression.sh` exits
`0`.  Its positive source covers same-owner access, implicit and qualified
`this`, explicit dot and arrow on `Derived`, const-reference and pointer
normalization, dot and arrow on a further-derived object, and both field and
method paths.  It checks the expected typed projection counts and
`@Base__protected_method` calls.  Its constructor-free nested `Nested` member
source accepts both protected field and method access through `Derived&`; the
parallel nested `Base&` source returns `EXIT_FAILURE` with the exact PA12
inaccessible diagnostic.  Its separate ordinary field-through-`Base&` and
method-through-`const Base*` sources also return `EXIT_FAILURE`.

The checked-in protected positive control
`make -C pa16 check TEST='tests/general/200-protected-base-method.t'` exits
`0` with `1/1` passing.  The existing course controls
`401-typed-member-projection-boundary-regression.sh`,
`402-typed-member-call-demand-roots-regression.sh`,
`403-typed-inherited-member-field-regression.sh`, and
`404-typed-implicit-default-demand-regression.sh` each exit `0`; 402's
`PA12 inherited member name is not callable` line is the expected diagnostic
from its negative reduction.  No handout test, fixture, or `.ref` file was
changed.

The permanent course-405 protected-static object-spelling source returns
`EXIT_FAILURE` at the pre-existing `PA15 static member projection is
unsupported` boundary, rather than at `PA12 record member is inaccessible`;
this verifies that the access gate no longer imposes the non-static object rule
on static bindings without expanding the static lowering surface.

The turn-start authoritative log records full-stage PA16 at `49/243` passed,
`194` failures, and `243/243` covered.  The authorized final `make test-pa16`
also exits `2` at `49/243`, with `194` failure identities; sorted identity
comparison against the turn-start log gives added `∅` and removed `∅`, and
the `243`-test inventory remains fully covered (`243/243`).  The earlier
pre-increment history is preserved: the plan records the `48/243` to `49/243`
improvement from removing `pa16/tests/general/200-protected-base-method.t`.
The required through-PA15 command exits `0` at `1167/1167`.  The required
file audit exits `0` with the same five existing header `bad-division`
warnings; no handout fixture or `.ref` file changed.

## Performance and Boundaries

Protected access performs one bounded lexical scope walk of depth `S`, records
`L` class candidates, and performs at most two typed direct-base walks of depth
`D` per candidate; its worst-case check is `O(S + L*D)` (with the ordinary
non-nested case `L=1`).  Same-owner access returns before a base walk, and
protected static access can short-circuit after the owner proof.  Selection
remains bounded by the walked scope/inheritance depths and candidate set; the
new check adds no cache, whole-program retry, textual recovery, or mutation on
a failed probe.  PA15 performs one independent typed owner/layout check.

The representative temporary three-level state-free chain timing sample used
five compiler invocations per size with `/usr/bin/time`: for 1, 128, and 512
local declarations, maximum elapsed times were respectively `0.02s`, `0.01s`,
and `0.02s`; maximum RSS was approximately `5.4MB`, `6.1MB`, and `8.7MB`.
A separate temporary nested-access sample placed 256 protected-field
expressions in one out-of-class nested member and used lexical class depths
`L=1`, `8`, and `32`, with three invocations per depth; all exited `0`, with
maximum elapsed time `0.01s` and maximum RSS `7.4MB`.  These are small
bounded-behavior samples, not formal benchmarks or asymptotic timing claims.
Remaining uncertainties are the pre-existing static and inline-nested-call
lowering boundaries, and unrelated protected typedef/friend/using and
broader PA16 surfaces.

## Historical Previous Checkpoint Review

This review covers landed commit `b1a9e58959cb47835362a654283200831e7b99d6`
relative to parent `25e80541`, plus four narrow audit repairs included in
this checkpoint.  It is limited to direct and inherited unqualified
non-static member calls.  Inherited fields, qualified-base calls,
protected/friend/using access, operators/ADL, constructors/lifetime, virtual
or ref-qualified methods, and general conversion work remain outside it.

The owned path is:

```text
PA10 CallExpression(MemberExpression or unqualified IdExpression) syntax
  -> PA12 typed lexical/class/direct-base lookup and member selection
  -> exact Function-scope implicit-object BindingId as semantic child zero
  -> selected BindingId, owner ScopeId, callable Function TypeId, and args
  -> reachable FunctionFact demand edge
  -> PA15 ABI/owner validation and ordered base-subobject projections
  -> LowIR call with the owner pointer followed by explicit arguments
```

### Findings

- `semantic_call_expression` probes the typed member path before functional
  casts and ordinary direct lookup.  The probe unwraps supported
  parenthesized callees, accepts only a plain unqualified id for the new path,
  and never asks namespace lookup or ADL for member candidates.  Its lexical
  walk checks nearer block/function declaration sets, then the direct class
  and ordered direct-base declaration sets.  At every set the value graph is
  probed before the type graph, so a same-scope ordinary method hides a
  same-spelled class/enum tag.  A value-owned class/base set suppresses
  unrelated enclosing candidates.  A base-owned value set with no supported
  non-static method is blocked by its nonempty typed base path, while a
  `ValueRef` origin from an unsupported import returns explicit `Blocked` and
  cannot silently reopen outer value lookup; nearer lexical/direct-class values
  retain the ordinary resolver's existing fallback.  A using-view remains with
  the ordinary resolver.  A type-only first set returns an explicit typed `TypeId` outcome and is
  consumed by the existing functional-cast producer, so it cannot silently
  reopen outer value lookup.  The separate type probe uses a fresh lookup
  generation after the value probe.
- `member_function_candidates_in_scope` retains only ordinary non-static
  functions from the selected class scope.  The selected `BindingId` and
  `ValueRef` owner remain canonical; the callable `Function TypeId` is built
  with the selected owner and its cv-qualified hidden object pointer.  Object
  qualification is checked before the existing explicit-argument conversion,
  default, overload, access, and deleted-function logic.  Equal best choices
  remain ambiguous rather than depending on traversal order.
- `prepare_pa12_member_parameter` owns one synthetic first parameter and its
  exact `BindingId` in the member Function `Scope`.  The unqualified helper
  now passes the already-validated `BindingId` into `semantic_this_expression`,
  rather than resolving the enclosing `this` binding a second time.  Thus the
  successful call has one stable implicit-object fact at child zero, followed
  by converted/defaulted explicit arguments.
- Inherited fields remain deferred, but an inherited value declaration set is
  still owned by the first base scope that contains the spelling.  Empty
  non-static member candidates and unsupported imported-base value origins are
  therefore blocked by the typed base ownership signal and fail closed;
  ordinary outer/ADL lookup cannot be reopened.  A nearer lexical or
  direct-class value still reaches the existing ordinary resolver, preserving
  its direct/static behavior.
- `direct_base_chain` walks `NamedRecordId` edges with Floyd cycle detection.
  The audit repair validates every class-scope back-reference, rejects a
  non-invalid base on `has_base == false`, rejects virtual and union base
  metadata, and preserves the existing single-direct-base parser rejection.
  The semantic path is bounded by inheritance depth and is passed to the
  shared selector without a second semantic walk; same-owner conversion stays
  constant-time.
- A successful typed member call is the only member demand edge.  PA15 follows
  its selected binding through the existing `FunctionFactId` index, validates
  the selected class owner and hidden ABI, and plans declaration-only members
  from the typed callable boundary.  It scans reachable facts once with typed
  worklists; failed guarded probes publish no fact or demand edge.
- `lower_call` independently reconstructs the actual-object-to-owner path as
  a safety check.  For every edge it validates the current class relation and
  a complete `RecordLayout` whose direct base is the expected record at offset
  zero, then emits ordered `IPK_BASE_SUBOBJECT` projections.  Dot takes one
  address and arrow one pointer expression; free and indirect calls retain
  their existing lowering paths.

### Historical Focused Evidence

The six-test handout probe
`make -C pa16 check TEST='tests/general/200-inherited-member-call-hides-outer-type.t tests/general/200-implicit-member-call-suppresses-adl.t tests/general/200-member-call-implicit-this-cv-overload.t tests/general/200-local-class-direct-init-inherited-member-call.t tests/general/200-parenthesized-member-call.t tests/general/200-single-inheritance.t'`
exits `2` with `1/6` passing.  The inherited outer-type control passes.  The
five remaining failure identities are unchanged prerequisite blockers:
`200-implicit-member-call-suppresses-adl.t`,
`200-member-call-implicit-this-cv-overload.t`,
`200-local-class-direct-init-inherited-member-call.t`,
`200-parenthesized-member-call.t`, and `200-single-inheritance.t`.

The focused control
`make -C pa16 check TEST='tests/general/100-member-methods.t tests/general/200-inherited-member-call-hides-outer-type.t tests/general/200-member-call-return-type-overload-arity.t'`
exits `0` with `3/3` passing.  The existing course regressions
`400-typed-layout-boundary-regression.sh`,
`401-typed-member-projection-boundary-regression.sh`, and
`402-typed-member-call-demand-roots-regression.sh` each exit `0`.
The extended 402 script asserts typed LowIR ownership for a base tag/method
collision (zero-offset projection and `@Base__f`) and for direct and inherited
type-only first declaration sets (typed zero cast in `Derived__call`, with no
outer `@f` call).  It also rejects an inherited `Base::f` data member in
`Derived::call` and confirms that no outer `@f` call is emitted.  `sh -n` over
those scripts and `git diff --check` also exit `0`.

Bounded stdin reductions (not additional suite coverage) compile successfully
for direct, inherited, and parenthesized unqualified calls.  The inherited
and parenthesized outputs each contain one zero-offset
`projection=base_subobject` before the base call; a three-level reduction
contains two ordered projections before `@A__f`.  The new 402 reductions show
that a same-scope tag does not hide an ordinary base method, while direct and
inherited type-only declarations return the typed functional-cast zero rather
than calling the unrelated outer function.  The inherited non-callable-value
reduction fails closed rather than reaching the outer function.  A nearer block
variable named like
the method still fails at the local non-callable target and does not fall
through to the base method.

The required through-PA15 command
`n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
exits `0` with `1167/1167` passing.  The required file audit
`perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0` with
these five warnings: `dev/src/abi_mangle.h:1`,
`dev/src/cpp_semantic_core.h:1`, `dev/src/lowir_model.h:1`,
`dev/src/pa11_semantic_model.h:1`, and `dev/src/pa15_lowering.h:1`, each
`bad-division` for a substantial implementation body in a header.
`make test-pa16` exits `2` with `48/243` passing, `195` failures, and
`243/243` coverage.  Comparing the exact failure identities with the
turn-start map in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` gives
added set `∅` and removed set `∅`; all `195` identities are unchanged.  No
fixture, reference, or coverage identity was changed.

### Historical Performance and Boundaries

The lexical/type/value probes inspect only the walked scopes.  Base metadata
validation and lookup are bounded by inheritance depth; same-owner conversion
returns before a base walk.  For `C` candidates, `A` explicit arguments, and
`P` parameters, member viability/ranking remains approximately
`O(C * (P + A))`.  PA15's typed function/fact worklists scan each reachable
node once, and lowering performs one independent bounded path/layout check.
No whole-program retry or textual recovery was added.

No timing, RSS, allocation, or structural-counter measurement was collected;
these are structural bounds only.  Unsupported inherited type construction
and inherited non-callable value calls are fail-closed, while the focused
scalar type-only cases use the existing functional-cast producer.  The
remaining uncertainties are the unchanged
`195`-identity PA16 failure map and the explicitly deferred
protected/friend/using, inherited-field, qualified-base, static,
constructor/lifetime, virtual/ref-qualified, operator/ADL, and broader
conversion slices.

## Audit ledger

| checkpoint | result and disposition |
| --- | --- |
| `32c45463` typed class-object construction checkpointAudit | Completed bounded audit of the landed typed construction increment relative to `a2ac5256`: repaired canonical empty named-class constructor identity, fail-closed FunctionFact ownership, value-initialization zeroing semantics, aggregate DMI fallback, typed range/owner/index validation, demand-driven empty-helper elision, and the course-404 ordering controls. Focused copied handout comparison is `10/11`; course controls 400--407 are green; final PA16 is `80/243` with `163` failures and `243/243` coverage, with exact failure and coverage additions/removals `∅`/`∅`; construction stress smoke is five successful `0.00s` runs with RSS `5824--6056KB` (timings in `/tmp/codex-pa16-stress-final.Tn9MSH/stress-1.time` through `stress-5.time`), 14 constructor helpers, 14 constructor calls, 13 base projections, 45 field projections, and 45 stores. Through-PA15 is `1167/1167`; the file audit passes with five pre-existing header-division warnings; no handout, fixture, reference, or `.ref` changed. |
| `2f130396` typed static-data storage/access checkpointAudit | Completed bounded audit/repair: canonical direct class-owner merging, inherited/nested typed owner retention, initializer-fact preservation, demand-aware class-static/TLS emission, access checks, exactly-once static object evaluation, and PA12 fail-closed class claims are traced and repaired. `make -C dev cppgm++`, course controls 400--407, the focused probe, and exact through-PA15 gate pass their bounded criteria; full PA16 is `61/243` with `182` failures and `243/243` coverage, with failure-identity additions/removals `∅`/`∅`. The file audit exits `0` with five pre-existing warnings; no handout or reference changed. |
| `021ef639` typed static member-function lookup/reachable-emission checkpointAudit | Completed bounded audit/repair: class-qualified lookup fails closed, current/base-qualified and unqualified member-body calls rank one mixed static/non-static set, static-body lookup preserves inherited owner/hiding, access and raw-vs-hidden-object facts remain typed, and PA15 uses a dense class-binding owner index with O(1) selected-owner checks. The focused handout matrix is `7/7`, course controls 401--406 exit `0`, final PA16 is `55/243` with the exact turn-start `188` failure identities and `243/243` coverage, through-PA15 is `1167/1167`, and the file audit passes with five pre-existing warnings. |
| `8b445ee6` protected object access scope checkpointAudit | Completed and committed bounded audit/repair: static protected object spelling stops after the typed access-class/owner proof, nested protected access considers eligible enclosing class scopes while retaining the non-static object proof, and malformed valid scope ancestry fails closed after the bounded walk. Course 405 covers the field/method matrix, nested `Derived&`/`Base&` controls, and the exact existing PA15 static boundary. Final PA16 is `49/243` with `194` failures and `243/243` coverage, with exact failure additions/removals `∅`/`∅`; through-PA15 is `1167/1167`, the file audit passes with five pre-existing warnings, and the final working tree is clean. |
| `b1a9e589` direct + inherited unqualified member-call checkpointAudit | Bounded audit completed with four narrow fixes: exact synthetic-`this` BindingId reuse, value-before-type lookup with explicit `Type`/`Blocked` outcomes, fail-closed direct-base metadata validation, and inherited value-set ownership blocking. Direct/inherited/parenthesized reductions, the three focused controls, and course regressions pass; the six-test handout probe remains `1/6` on the same five prerequisite identities. Through-PA15 is `1167/1167`, the file audit exits `0` with five pre-existing warnings, and full PA16 is `48/243` with `195` failures and `243/243` coverage, with zero failure-identity additions or removals. |
| `37265733` typed member projection audit/repair | Direct/nested dot and arrow ownership is traced through PA12, PA11 `RecordLayout::member_offsets` keyed by the object's canonical `NamedRecordId`, and PA15 LowIR; the reference-cv and class anonymous-injection defects are repaired. Broad validation and exact identity/coverage checks pass their bounded invariants; PA16 remains incomplete with the existing 205 failures. |
| `0b534f2f` typed direct member-call checkpointAudit | Completed bounded audit/repair: implicit-object cv subset ranking, N3485 variadic comparison, single-owner typed reachable member demand, dense PA15 reachability metadata, declaration-only member declarations with hidden-object/cv ABI boundaries, hidden-object call formation, and source-file sizing are repaired. Focused PA16/PA15 controls and all relevant course regressions pass; through-PA15 is `1167/1167`, the file audit passes with five pre-existing warnings, and full PA16 remains `47/243` with `196` failures and `243/243` coverage, with zero failure-identity additions or removals. |
