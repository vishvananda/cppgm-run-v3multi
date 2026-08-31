# PA16 empty-base layout/address-projection checkpoint

## Stage Design

PA10 owns syntax and parser facts; PA11 owns typed `NamedRecordId` identities
and the canonical `RecordLayout`; PA12 owns typed lookup, selected calls, and
member/base-path facts; PA15 consumes those facts for LowIR and ABI lowering.
This checkpoint stays within that flow and does not recover relationships from
spellings or add a parallel semantic model.

`PA11SemanticModel::complete_record_layout` is the sole owner of class size,
alignment, direct-base placement, and member offsets.  A complete layout keeps
the local `empty` fact and the direct base's `zero_size` fact; it deliberately
does not retain a transitive zero-offset closure.  When an empty direct base
can be placed at offset zero, PA11 builds a bounded scratch set from its typed
direct-base chain and queries completed local base/member offsets through the
first storage-capable declaration.  The query unwraps arrays/CV types and
recurses through typed zero-offset nested objects, with per-query visited
identities.  Same-type subobjects therefore remain distinct while differently
typed empty base/member objects may overlap.  Backed/generated anonymous
declarations are not skipped: the canonical member type, backing storage type,
and generated record identity are checked as typed facts.

PA15 validates every typed edge in member-address, member-call, conversion, and
destructor paths.  Since every supported edge is offset zero, it emits at most
one canonical `base_subobject` projection for a nonempty path; identity paths
emit none.  `apply_derived_base_conversion` rejects a zero-length
`DerivedToBase` fact and also guards emission by the path count.

## Failure Map

Turn-start authority was clean HEAD `148ef591`: `make test-pa16` had
`228/243` passing, complete `243/243` coverage, and exactly these 15
failures:

```text
200-friend-intermediate-derived-protected-base-method       RESOLVED: path projection
200-local-default-class-array-lifecycle                    residual
200-nested-braced-member-aggregate-init                    residual
200-reference-indexed-pointer-member-access                residual
200-reference-member-class-init                            residual
200-unnamed-namespace-hidden-friend-single-definition       residual
300-callable-field-hides-private-base-method               RESOLVED: EBO/layout
300-friend-function-definition-skip                         residual
300-nested-enum-hidden-friend-bitmask-adl                   residual
300-overloaded-deref-user-assignment                        residual
300-user-defined-string-literal-operator                   residual
300-using-base-static-same-signature-derived-preferred      residual
400-bit-field-prefix-postfix-increment                     residual
400-signed-bit-field-read                                   residual
400-signed-enum-bit-field-read                              residual
```

Final stage inventory is `230/243`, complete `243/243` coverage, and exactly
the 13 turn-start residuals above.  The two resolved identities are the two
checkpoint targets; no unrelated failure was compensated or displaced.

## Active Checkpoint

The coherent implementation is validated and pending commit.

Invariants:

- `RecordLayout` owns EBO, size, alignment, direct-base offset, and member
  offsets.  Complete class objects remain at least one byte; nonempty bases,
  requested alignment, bit-fields, default member initializers, aggregate and
  lifetime consumers keep their existing layout facts.
- An empty direct base overlaps a first member only when typed zero-offset
  facts prove that no same-type base subobject occurs in that member, including
  arrays and nested base/member paths.  Same-type direct members/arrays/nested
  occurrences disable EBO; a distinct empty member remains eligible.
- All typed base-path edges are validated even when one LowIR projection
  represents an all-zero path.  Identity paths are zero projections and
  nonempty all-zero paths are one projection.

## Focused Validation

The shared compiler builds successfully:

    make -C dev cppgm++

The two checkpoint targets pass together, and 12 representative checked-in
controls (empty class, nonempty base/member offset, inherited access, requested
alignment, destructor/lifetime, and anonymous aggregate controls) pass:

    make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 CPPGM_TEST_JOBS=1 check TEST='tests/general/200-friend-intermediate-derived-protected-base-method.t tests/general/300-callable-field-hides-private-base-method.t'
    pa16 check: PASS (2/2)
    make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 CPPGM_TEST_JOBS=1 check TEST='tests/general/100-empty-class-sizeof.t tests/general/200-empty-class-member-declaration.t tests/general/200-base-field-access.t tests/general/200-inherited-member-call-hides-outer-type.t tests/general/200-local-class-direct-init-inherited-member-call.t tests/general/200-destructor-body-local-before-base-destruction.t tests/general/300-alignas-class-layout.t tests/general/300-alignas-derived-base-layout.t tests/general/300-member-alignas-layout.t tests/general/300-explicit-destructor-call-enclosing-namespace-type.t tests/general/300-anonymous-bitfield-helper-member.t tests/general/300-mutable-anonymous-member.t'
    pa16 check: PASS (12/12)

The selected array-lifecycle control remains the known pre-existing residual:
`200-local-default-class-array-lifecycle.t` has its existing LowIR destruction
order mismatch.  The final through-stage run still passes
`1167/1167` through PA15.  The final file audit passes with the same six
nonfatal header/body warnings.

Disposable LowIR controls compiled successfully.  Same-type direct member,
same-type first array element, and nested zero-offset occurrence produced
`obj<2x1>`, `obj<3x1>`, and `obj<2x1>` with member offsets `1`, `1`, and
nested `1 -> 0`; the distinct empty member produced `obj<1x1>` at offset
`0`.  Requested alignment produced `obj<8x8>`, and the nonempty base retained
a base projection with member offset `4` and `obj<8x4>`.  A deep inherited
member call emitted one base projection; the identity call emitted none.  A
derived-to-base destructor setup emitted one conversion projection, followed
by the identity base-destructor call; generated direct destructor actions each
retained their one direct-edge projection.  The accepted anonymous PA16
controls passed in the 12-test matrix.

## Performance Evidence

Each completed `RecordLayout` retains one direct typed base edge (when
present), local member offsets, and bounded scalar facts; no per-layout
transitive identity vector/index remains.  EBO creates a `RecordTypeSet` only
for an actual empty-base/first-storage intersection query, and the query is
deterministic, memoized through completed layouts, and uses a per-query visited
set for nested object types.  There is no whole-program scan or per-access
semantic lookup.  The persistent layout facts are linear in records plus local
members; scratch ancestry is bounded to the one candidate query.

The representative deep stress probe used 64 inheritance edges (65 records)
and one deepest `E0` member to exercise the same-type ancestry query.  It
compiled successfully (`rc=0`), retained `64` direct edges, `64` empty-chain
facts plus the deepest member layout, and `0` transitive closure entries; the
deepest object was `obj<2x1>` and the inherited call emitted `1` projection.
No timing or RSS claim is made.

## Checkpoint Ledger

| checkpoint | compact result |
|---|---|
| d54e32d1 | Prior authority was `224/243` with 19 failures and complete identity coverage. |
| b58ddd2a | Completed the typed `nullptr_t` carrier path through PA11/PA12/PA15, including ABI and LowIR ownership. |
| e09d8223 | Recorded the nullptr-carrier audit state: 225/243 authority, 18 residual failures, `243/243/243` inventories, and through-PA15 `1167/1167`. |
| d5bf2600 | Completed the typed constructor-overload/lifetime ownership audit; focused matrix passed and PA16 reached `227/243` with 16 residual failures. |
| 29d9c4ce | Completed the PA10 elaborated-member parameter-clause repair and PA15 ABI owner consolidation; PA16 reached `228/243` with this exact 15-item residual set and `243/243/243` inventories. |
| 69bbe800 | Empty-base layout/address projection: post-commit PA16 `230/243`, two target identities resolved, 13 exact residuals retained; through-PA15 `1167/1167`, file audit pass with six warnings. |
