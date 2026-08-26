# PA16 checkpoint audit

## Current Checkpoint Review

This review covers only landed commit `37265733` relative to parent
`4e73af06`: typed lowering of direct and nested non-static data-member
projections for `.` and `->`.  Methods, calls, lifetime, constructors,
bit-fields, packing, ADL, inherited projection, and other PA16 surfaces are
not audited here.

The owned path is:

```text
PA10 MemberExpression syntax
  -> PA12 SemanticFact with typed object child and selected BindingId
  -> PA11 RecordLayout::member_offsets keyed by BindingId for the object's
     canonical NamedRecordId
  -> PA15 lower_member_address -> LowIR IK_INDEX [projection=field]
  -> existing lvalue, load, store, address, and reference consumers
```

### Findings

- Ordinary member facts publish the selected `BindingId` at the PA12 semantic
  owner.  PA15 derives the record from the typed object or pointee type and
  consumes the exact `member_offsets` entry; it does not recover a name or
  construct a second layout map.  A base/using binding absent from that direct
  layout fails explicitly instead of receiving an inferred offset.
- Dot lowering takes the object address once; arrow lowering evaluates the
  pointer expression once.  PA12 already records the required lvalue-to-rvalue
  conversion for an arrow object, while PA15 reuses the existing conversion
  and lvalue machinery for nesting, stores, loads, address-of, and reference
  arguments.
- A real defect was found in `member_access_type`: a reference-returning object
  retained cv-qualification in its referred type, but `cv_qualifiers` was
  called on the reference itself and lost it.  It now normalizes with
  `expression_object_type` before applying member cv.
- `semantic_injected_member` still publishes its typed selected binding for
  existing PA12 semantic behavior, but that binding is an injected alias and
  is not a direct layout owner for this checkpoint.  PA15 now rejects an
  injected binding with backing storage.  Class-scope anonymous-union
  injection is rejected at PA11 before its aliases can enter the enclosing
  direct layout.  Namespace/local anonymous-union semantic behavior from
  earlier assignments is unchanged.
- `lower_member_address` requires a complete class layout, checks the offset
  before converting it to the signed LowIR i64 operand, and emits an i8 field
  projection.  The `emit_index` bool-to-`IndexProjectionKind` refactor
  preserves array-element, pointer-byte-offset, and initialization call sites;
  field projection is the only new kind used here.
- `pa15_lowering_member.cpp` is the single cppgm++ implementation and is
  registered once in `dev/frontend_source_sets.mk`.

## Focused Evidence

- Before repair, the new boundary regression reproduced both defects: the
  class anonymous-member case and the write through `const Cell&` both exited
  success.  After repair,
  `sh cppgm.tests/course/pa16/401-typed-member-projection-boundary-regression.sh`
  exits `0`.
- The focused PA16 command covering simple/nested members, local/empty/large/
  self-pointer layouts, and the arrow fixture exits `2` with `6/7` passing.
  The only mismatch is the known omitted load of unused `on_immediate` in
  `100-function-pointer-nested-param-name-shadow.t`; all six member/layout
  cases pass.
- `make -C pa11 check TEST='tests/spec/200-namespace-anonymous-union-injected-members.t'`
  exits `0`, confirming the earlier namespace semantic path was preserved.
- `make -C dev cppgm++` and the boundary regression both exit `0`; the
  sequential through-PA15 gate exits `0` with `1167/1167` passing.
- `make test-pa16` exits `2` with `38/243` passing and `205` failing.  The
  complete log is preserved at
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-post-repair-validation.log`.
  Against the immutable turn-start/latest landed HEAD `37265733`, the exact
  post-repair comparison has `0` added failure identities, `0` removed
  failure identities, `0` turn-start pass regressions, and `243/243` covered.
  The parent checkpoint baseline `4e73af06` was `35/243` passing and
  `208` failing; the immutable landed evidence was `38/243` passing and
  `205` failing.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0`.
  It reports the five documented pre-existing `bad-division` header warnings
  and no new audit failure.  `git diff --check` exits `0`.

## Performance and Boundaries

The projection path uses expected/amortized O(1) `FlatIndex` lookup with
bounded table probing by typed `BindingId` after the PA11 class binding scan;
this is not an absolute worst-case O(1) claim.  It has one typed object/pointer
normalization and one LowIR projection per member access.  The existing
large-layout focused case passes, but no timing, RSS, allocation, or
work-counter measurement was taken, so no numerical performance claim is
made.  Inherited, using-imported, anonymous/injected, static, method,
reference-field, and other non-direct projection cases remain explicit
boundaries for this increment.

## Audit ledger

| checkpoint | result and disposition |
| --- | --- |
| `37265733` typed member projection audit/repair | Direct/nested dot and arrow ownership is traced through PA12, PA11 `RecordLayout::member_offsets` keyed by the object's canonical `NamedRecordId`, and PA15 LowIR; the reference-cv and class anonymous-injection defects are repaired. Broad validation and exact identity/coverage checks pass their bounded invariants; PA16 remains incomplete with the existing 205 failures. |
