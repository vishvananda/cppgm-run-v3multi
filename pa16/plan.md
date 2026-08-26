# PA16 implementation plan

## Spec alignment

This checkpoint keeps one production path from PA10 member syntax through
PA12 typed semantic facts, PA11 `RecordLayout::member_offsets` keyed by the
selected `BindingId` under the object's canonical `NamedRecordId`, and PA15
typed LowIR.  `selected_binding` is the semantic identity; `member_offsets`
is the sole direct-field offset owner; lowering performs no name recovery or
layout reconstruction.  Dot and arrow direct/nested fields reuse existing cv,
value-category, lvalue, load/store, address, and reference-consumer
machinery.  Static members, methods/calls, inherited/base projection,
anonymous/injected expansion, lifetime, constructors, bit-fields, packing,
and ADL remain outside this increment.

## Failure map

The parent checkpoint baseline at `4e73af06` was `35/243` passing and
`208` failing.  The immutable turn-start/latest landed evidence at HEAD
`37265733` was `38/243` passing and `205` failing, all `243` covered, with
the three removed identities
`200-simple-class-member-object-access.t`,
`200-nested-class-member-object-access.t`, and
`300-lazy-class-lookup-ignores-later-using-directive.t`, and no additions.
The post-repair `make test-pa16` result is also `38/243` passing and `205`
failing, with `0` added failure identities, `0` removed failure identities,
`0` turn-start passing-test regressions, and `243/243` covered.  The required
threshold is satisfied without relying on offsetting additional passes.

Focused post-repair evidence is `6/7`: simple/nested member access and the
four representative layout cases pass; the arrow fixture retains only its
pre-existing unused-`on_immediate` load mismatch.  The new course boundary
regression passes and guards the two repaired earliest-owner cases.

The sequential through-PA15 gate passes `1167/1167`.  The PA16 file audit
passes with the five documented pre-existing `bad-division` header warnings;
no new file-audit failure is present.

## Current checkpoint

PA12 now preserves cv when a member object is reached through a reference.
PA15 validates the selected binding, exact complete direct layout entry,
checked offset conversion, and `IPK_FIELD` i8 projection.  Class-scope
anonymous-union aliases are rejected before they can masquerade as direct
fields; PA15 rejects the already-supported local injected alias form without
changing earlier PA12 namespace/local semantic behavior.  PA16 completion is
not claimed.

## Performance evidence

The typed binding lookup uses expected/amortized O(1) `FlatIndex` lookup with
bounded table probing after the existing per-record layout scan; this is not
an absolute worst-case O(1) claim.  There is no textual keying, source
re-rendering, duplicate layout map, or whole-program retry.  The large-layout
focused test preserves a representative case.  No timing, RSS, allocation,
or structural-counter measurement was collected, so performance evidence is
structural only.

## Next checkpoint

The next narrow slice is the typed implicit-object/`this` foundation for one
direct, non-overloaded, non-static member call selected from the exact class
owner by `.` or `->`: preserve the typed object fact, validate its cv/object
compatibility, prepend the single typed object pointer as the implicit `this`
argument, and lower the direct call.  Overload sets, inherited methods,
operators, virtual dispatch, constructors, and broader method cv/ref rules are
deferred.  This boundary is adjacent because PA12 already inserts a typed
`this` parameter in member-function scopes, while its call path and PA15
`lower_call` currently do not form or prepend the implicit object.

## Checkpoint ledger

| checkpoint | status |
| --- | --- |
| `37265733` typed member projection audit/repair | Completed bounded audit/repair; focused and broad invariants pass, exact failure identity and coverage are unchanged from landed HEAD, and PA16 remains incomplete with 205 existing failures. |
