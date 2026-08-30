# PA16 implementation plan

## Stage Design

The maintained pipeline is one typed path:

1. PA10 lexes/parses names, member/operator syntax, expressions, declarations,
   and source-point information into the AST.
2. PA11 builds canonical scopes, bindings, types, records, base relations,
   access facts, using/friend publication, and typed `ValueRef`/semantic-model
   facts. Lookup and ADL consume those facts, not recovered source text.
3. PA12 walks the AST once, creates typed expression/declaration facts, computes
   value category and lifetime, collects typed candidates, ranks conversions,
   selects one callable, and publishes the selected binding, callable type,
   argument conversions, base paths, and ownership on the fact graph.
4. PA15 lowers those published facts to typed LowIR: layout/address paths,
   constructor/destructor actions, ABI types, references, conversions, and
   demand-driven function bodies. It must not redo lookup or selection.

PA11 owns canonical declarations, scopes, types, records, base/access facts,
and lookup/publication. PA12 owns expression typing, value category/lifetime,
candidate selection, conversion ranking, and publication of the selected
binding, callable type, argument conversions, and base-subobject paths. PA15
consumes those facts for typed lowering and representation; PA10 owns syntax.

## Failure Map

The audit-turn starting checkpoint is `220/243` with `23` failures and full
`243/243` identity coverage.  The parent baseline was `219/243` with `24`
failures; that `24 -> 23` comparison establishes the landed increment only.
The final audit gate must preserve all `243` identities, introduce no new
failure identity, and remain at or below this current `23`-failure set.

The current residuals below are recorded for boundary control, not re-audited
by this checkpoint:

- PA10 syntax/name formation: `200-elaborated-member-forward-type.t`,
  `300-user-defined-string-literal-operator.t`.
- PA11 typed lookup/publication and layout:
  `200-friend-derived-private-base-defaulted-constructor.t`,
  `200-friend-intermediate-derived-protected-base-method.t`,
  `200-unnamed-namespace-hidden-friend-single-definition.t`,
  `300-callable-field-hides-private-base-method.t`,
  `300-using-base-static-same-signature-derived-preferred.t`.
- PA12 typed construction, conversion, and call/operator selection:
  `200-external-ctor-overload-nonfirst-argument.t`,
  `200-nested-braced-member-aggregate-init.t`,
  `200-reference-member-class-init.t`,
  `200-string-literal-does-not-convert-to-mutable-void-pointer.t`,
  `300-operator-nullptr-t-from-zero.t`,
  `300-overloaded-deref-user-assignment.t`,
  `300-nested-enum-hidden-friend-bitmask-adl.t`.
- PA15 typed lowering, emission, and LowIR representation:
  `100-function-pointer-nested-param-name-shadow.t`,
  `200-const-subobject-member-call.t`,
  `200-local-default-class-array-lifecycle.t`,
  `200-reference-indexed-pointer-member-access.t`,
  `300-enum-class-nonmember-operator-bitand.t`,
  `300-friend-function-definition-skip.t`,
  `400-bit-field-prefix-postfix-increment.t`,
  `400-signed-bit-field-read.t`,
  `400-signed-enum-bit-field-read.t`.

The selected identity `pa16/tests/general/300-prvalue-derived-base-friend-
operator.t` is absent because the landed increment fixed it.  The two signed
bit-field cases remain preservation controls; their checked-in fixture and
README requirement are intentionally not changed here.

## Active Checkpoint

Spec alignment: PA16 supports ordinary non-template calls/operators, single
inheritance, typed constructor temporaries, and references.  The final audit
preserves that boundary; it does not open PA17 class-by-value transfer,
copy/move, general temporary materialization, multiple inheritance, templates,
or unrelated semantics.

The bounded audit covers landed commit
`e470e9dfed07ca09a373d227640f3c8042cc2cbf` (`PA16 enable prvalue derived-base
reference binding`) relative to parent `f3afe9d5`.  Its source change is in
`dev/src/pa12_semantic_resolution.cpp`; PA12 fact publication and PA15
lowering consumers are read-only ownership surfaces for this audit.

Two directly caused defects required repair in that source boundary.  First,
the new non-lvalue `Derived` to cv-qualified `Base&` choice admitted volatile
lvalue references, so `volatile Base&` and `const volatile Base&` could bind
to a temporary.  The derived-to-base branch is now limited to exactly
const/nonvolatile lvalue references, and target-directed constructor fallback
is disabled for volatile lvalue-reference targets.  Rvalue references and
ordinary const lvalue-reference binding remain in scope.

Second, the new `Base` candidate could outrank an exact `const Derived&`
candidate because standard conversion comparison gives the derived-to-base
choice precedence over a non-derived conversion at the same broad category.
The exact same-class temporary reference choice now receives exact rank in
this branch, while genuine base, cv, access, and non-base distinctions retain
their typed comparison.  This does not open class-by-value conversion.

The existing typed path owns the rest of the operation:

```text
source fact/category/type + target reference cv
  -> conversion_for viability and typed ranking
  -> derived_base_choice / derived_base_relation access and path
  -> selected ConversionChoice
  -> add_conversion canonical ConversionFact and path arena
  -> PA15 apply_derived_base_conversion
  -> validated direct-base address projection and reference call argument
```

`derived_base_relation` walks canonical single-inheritance records and
checks the supplied access scope.  `add_conversion` validates the endpoint,
distance, access metadata, and canonical path; it rejects base metadata on
other conversion kinds.  PA15 validates the same fact and layout path before
emitting typed `base_subobject` projection.  Constructor actions already
produce addressable temporary storage, so no PA15 change or duplicate
materialization is needed.

Scope is limited to the landed increment and these two direct correctness
repairs.  No tests, fixtures, `.ref` files, sidecars, harnesses, comparators,
generated outputs, coverage/source-set rules, or unrelated stage code changed.

## Focused Evidence

Sequential focused validation after the repair is:

- `make -C dev cppgm++ CXX=g++`: status `0`.
- `make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/300-prvalue-derived-base-friend-operator.t tests/spec/200-conditional-derived-base-lvalue-reference.t tests/spec/200-const-reference-binds-derived-pointer-prvalue.t tests/general/300-const-method-array-member-binds-const-reference.t tests/general/300-basic-operator-overloads.t tests/general/200-derived-pointer-overload-prefers-base-over-void.t tests/spec/300-inherited-const-method-base-pointer-cv-bad.t tests/spec/200-derived-base-reference-overload-rank.t'`: status `0`, `PASS (8/8)`.
- Ephemeral typed probes outside the repository: exact-derived overload and
  nearer-base overload both select correctly (status `0`); xvalue direct
  derived binding is accepted (status `0`); volatile and const-volatile base
  lvalue-reference bindings are rejected (status `1`); inaccessible private
  base is rejected outside its friend and accepted in the friend; class by
  value, non-const base reference, and non-base reference cases are rejected.
- The target LowIR shape retains one constructor action per operand, two
  addressable temporary objects, two canonical base projections, and one
  hidden-friend operator call.  No fixture or test was added.
- `git diff --check`: status `0`.
- Required final gates: the exact prior-through command exits `0` at
  `1167 / 1167`; the PA16 file audit exits `0` with five known warnings; and
  `make test-pa16` exits `2` at `220 / 243` with `23` residual failures.
- Durable final evidence is under
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-e470-checkpoint-audit-20260830/`;
  its identity check reports authority/fresh failures `23/23`, fresh-only and
  authority-only `0/0`, and discovered/reference/fresh coverage `243/243/243`
  with all missing/unexpected counts `0`.

## Performance Evidence

The new path performs one typed direct-base walk of height `H` for each
eligible conversion candidate; with `C` typed candidates and `A` arguments,
the bounded selection work is O(C*A*H).  It adds no global scan, textual
recovery, retry pipeline, host/reference shortcut, or unbounded allocation.
Representative structural evidence is the target's two constructor actions,
two temporary slots, two base-subobject projections, and one operator call;
there is no timing/RSS claim from this focused milestone.

## Checkpoint Ledger

- Parent baseline (provenance only): `219/243`, `24` failures.
- Audit-turn starting checkpoint: `220/243`, `23` failures, `243/243` identities;
  the final result may not regress to the parent `24`-failure set.
- Completed row:
  `e470e9dfed07ca09a373d227640f3c8042cc2cbf` — bounded source repair and
  documentation audit complete; focused evidence passes, the required broad
  gates meet the current `23`-failure limit, no fresh-only failure identity
  appears, and all `243` identities remain covered.
- Next checkpoint: later PA16 work may select one of the same 23 residual
  identities.  This increment remains complete and its source/docs write set
  contains no test, fixture, reference, harness, or unrelated stage change.
