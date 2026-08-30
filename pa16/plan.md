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

The turn-start oracle is 219/243 with these 24 failures, grouped by the
currently suspected owning boundary:

- PA10 syntax/name formation: `200-elaborated-member-forward-type.t`,
  `300-user-defined-string-literal-operator.t`.
- PA11 typed lookup/publication and layout: `200-friend-derived-private-base-defaulted-constructor.t`,
  `200-friend-intermediate-derived-protected-base-method.t`,
  `200-unnamed-namespace-hidden-friend-single-definition.t`,
  `300-callable-field-hides-private-base-method.t`,
  `300-using-base-static-same-signature-derived-preferred.t`.
- PA12 typed construction, conversion, and call/operator selection (the
  active family is marked **selected**): `200-external-ctor-overload-nonfirst-argument.t`,
  `200-nested-braced-member-aggregate-init.t`,
  `200-reference-member-class-init.t`,
  `200-string-literal-does-not-convert-to-mutable-void-pointer.t`,
  `300-operator-nullptr-t-from-zero.t`,
  `300-overloaded-deref-user-assignment.t`,
  **`300-prvalue-derived-base-friend-operator.t`**,
  `300-nested-enum-hidden-friend-bitmask-adl.t`.
- PA15 typed lowering, emission, and LowIR representation: `100-function-pointer-nested-param-name-shadow.t`,
  `200-const-subobject-member-call.t`,
  `200-local-default-class-array-lifecycle.t`,
  `200-reference-indexed-pointer-member-access.t`,
  `300-enum-class-nonmember-operator-bitand.t`,
  `300-friend-function-definition-skip.t`,
  `400-bit-field-prefix-postfix-increment.t`,
  `400-signed-bit-field-read.t`,
  `400-signed-enum-bit-field-read.t`.

The two signed bit-field cases remain unchanged preservation controls. The
README requirement and the observed checked-in fixture output conflict; this
checkpoint leaves the required sign-extension implementation intact.

## Active Checkpoint

Contract alignment: PA16 supports ordinary non-template calls/operators,
single-inheritance base access, typed temporary construction, and references;
it does not introduce PA17 class value semantics, copy/move operations, or
general class-by-value transfer.

Root cause and owner: in `conversion_for`, a non-lvalue `Derived` bound to a
cv-qualified `Base&` was sent through the by-value class-conversion branch.
That made the hidden-friend `operator-(const Base&, const Base&)` non-viable
for `Derived()-Derived()`, despite the existing typed derived-to-base choice
and the PA15 addressable constructor-temporary representation. The fix adds
the missing typed derived-to-base reference choice at PA12. The selected
conversion and base path then continue through the existing semantic fact and
PA15 projection owners.

Exclusions: no class-by-value conversion, temporary copy/materialization,
copy/move assignment, parser recovery, lookup retry, fixture/test change, or
changes to the other 23 failures. No lowerer gap was observed, and the PA15
source remained unchanged; the existing typed temporary/base projection path
lowered the conversion successfully.

Focused validation confirmed that the constructor action remains addressable
when the new conversion is lowered and that access/path publication is
unchanged. The broad PA16 and through-stage gates now confirm the same result
without introducing another failure identity.

## Performance Evidence

The new decision is a constant amount of work around the existing typed
reference branch. Existing base-relation/path construction is proportional to
the inheritance path height; candidate selection remains O(C*A) for C typed
candidates and A arguments, with no new global scan, retry engine, or textual
recovery. The checkpoint adds no unbounded work or allocation-heavy traversal;
the broad 243-test run and exact identity comparison are representative
evidence, so a timing sample is not warranted for this bounded branch.

## Checkpoint Ledger

- Turn-start baseline: 219/243, 24 failures, full 243/243 coverage.
- Intended delta: fix the selected prvalue-derived-base operator failure
  (+1), with no coverage reduction or fixture changes.
- Focused delta: +1 existing failure, with all 7 selected/control tests
  passing. The proof set is
  `300-prvalue-derived-base-friend-operator.t`,
  `200-conditional-derived-base-lvalue-reference.t`,
  `200-const-reference-binds-derived-pointer-prvalue.t`,
  `300-const-method-array-member-binds-const-reference.t`,
  `300-basic-operator-overloads.t`,
  `200-derived-pointer-overload-prefers-base-over-void.t`, and the expected
  rejection `300-inherited-const-method-base-pointer-cv-bad.t`.
- Broad final result: `make test-pa16` exited `2` as expected with residual
  failures; `220/243` tests passed. The stage-progress gate is satisfied by
  `24 -> 23`, with the exact fresh residual set equal to the Failure Map
  minus `300-prvalue-derived-base-friend-operator.t`: baseline-only `0`,
  fresh-only `0`, and no new failures.
- Identity coverage: discovered `243`, reference `243`, fresh `243`; missing
  and unexpected identities are all `0`.
- Prior gate: `make test-report-through-pa15` passed `1167/1167`. File audit
  passed with the five repository-known header-division warnings. Durable
  evidence is in `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-derived-base-broad-20260830.log`,
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-derived-base-through-pa15-20260830.log`,
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-derived-base-file-audit-20260830.log`,
  and `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-derived-base-identity-comparison-20260830.log`.
- Disposition: focused, broad, prior-through-PA15, file-audit, and diff checks
  completed; the checkpoint is committed. Residual 23 remain for later
  checkpoints.
