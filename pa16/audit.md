# PA16 checkpoint audit

## Current Checkpoint Review

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

## Focused Evidence

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

## Performance and Boundaries

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
| `b1a9e589` direct + inherited unqualified member-call checkpointAudit | Bounded audit completed with four narrow fixes: exact synthetic-`this` BindingId reuse, value-before-type lookup with explicit `Type`/`Blocked` outcomes, fail-closed direct-base metadata validation, and inherited value-set ownership blocking. Direct/inherited/parenthesized reductions, the three focused controls, and course regressions pass; the six-test handout probe remains `1/6` on the same five prerequisite identities. Through-PA15 is `1167/1167`, the file audit exits `0` with five pre-existing warnings, and full PA16 is `48/243` with `195` failures and `243/243` coverage, with zero failure-identity additions or removals. |
| `37265733` typed member projection audit/repair | Direct/nested dot and arrow ownership is traced through PA12, PA11 `RecordLayout::member_offsets` keyed by the object's canonical `NamedRecordId`, and PA15 LowIR; the reference-cv and class anonymous-injection defects are repaired. Broad validation and exact identity/coverage checks pass their bounded invariants; PA16 remains incomplete with the existing 205 failures. |
| `0b534f2f` typed direct member-call checkpointAudit | Completed bounded audit/repair: implicit-object cv subset ranking, N3485 variadic comparison, single-owner typed reachable member demand, dense PA15 reachability metadata, declaration-only member declarations with hidden-object/cv ABI boundaries, hidden-object call formation, and source-file sizing are repaired. Focused PA16/PA15 controls and all relevant course regressions pass; through-PA15 is `1167/1167`, the file audit passes with five pre-existing warnings, and full PA16 remains `47/243` with `196` failures and `243/243` coverage, with zero failure-identity additions or removals. |
