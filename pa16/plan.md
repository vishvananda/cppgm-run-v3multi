# PA16 implementation plan

## Stage Design

This checkpoint owns the typed standard-conversion boundary for the PA16
non-virtual single-inheritance subset. PA11 remains the owner of `TypeId`,
`NamedRecordId`, validated direct-base metadata, record layout, and access
facts. PA12 scores a `DerivedToBase` `ConversionChoice` with compact typed
endpoints, bounded direct-base distance, cv metadata, and access scope; final
`ConversionFact` publication rechecks that relation and owns the path slice.
PA15 consumes that fact and emits the already-established offset-zero typed
base projection. No rendered names, duplicate type model, multiple/virtual
inheritance, class-by-value transfer, or conversion operators are added.

The governing material is spec.md §§2--5 and §7 and N3485 [conv.ptr],
[conv.qual], [dcl.init.ref], and [over.ics.rank]. The existing PA16 contract
requires ordinary object pointer/reference conversions and single inheritance
at offset zero; inaccessible base paths must remain rejected.

## Failure Map

The authoritative clean-HEAD baseline is commit `052b47b99da3` and
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log:
`132/243` passed, `111` failed, and all `243/243` identities were covered.
The baseline has 61 expected-success exit mismatches, 2 expected-failure exit
mismatches, and 48 LowIR comparison mismatches.

The focused conversion command covers these ten identities. It is currently
`8/10`; the eight baseline failures removed are
`general/200-derived-pointer-member-init.t`,
`general/200-derived-pointer-overload-prefers-base-over-void.t`,
`general/200-function-reference-return-expression-type.t`,
`general/300-overloaded-unary-deref-base-ref-return.t`,
`general/300-out-of-class-member-trailing-return.t`,
`spec/200-const-reference-binds-derived-pointer-prvalue.t`,
`spec/200-derived-base-reference-overload-rank.t`, and
`spec/200-conditional-derived-base-lvalue-reference.t`.
The two unchanged baseline failures are
`general/200-pointer-subscript-class-reference-return.t` (scalar-index
LowIR scaling) and `general/200-reference-indexed-pointer-member-access.t`
(nested-array initializer boundary).

The focused access/rank/parser control command is currently `7/9`. Its seven
passing controls are `spec/300-inherited-const-method-base-pointer-cv-bad.t`,
`general/200-private-base-static-cast-bad.t`,
`general/200-private-base-static-cast-member.t`,
`general/300-using-base-same-signature-derived-preferred.t`,
`general/200-multilevel-qualification-conversion-bad.t`,
`general/200-const-cast-pointer-reference-alias.t`, and
`general/300-alignas-derived-base-layout.t`. The two remaining baseline
failures are `general/300-using-base-static-same-signature-derived-preferred.t`
and `general/200-string-literal-does-not-convert-to-mutable-void-pointer.t`.
The final full gate is `144/243` passed and `99` failed, with all `243/243`
identities covered. The twelve baseline-only failures are
`general/200-const-member-call-prefers-const-object-overload.t`,
`general/200-derived-pointer-member-init.t`,
`general/200-derived-pointer-overload-prefers-base-over-void.t`,
`general/200-function-reference-return-expression-type.t`,
`general/200-private-base-static-cast-member.t`,
`general/300-derived-shift-prefers-free-char-pointer.t`,
`general/300-member-function-pointer-field-call.t`,
`general/300-out-of-class-member-trailing-return.t`,
`general/300-overloaded-unary-deref-base-ref-return.t`,
`spec/200-conditional-derived-base-lvalue-reference.t`,
`spec/200-const-reference-binds-derived-pointer-prvalue.t`, and
`spec/200-derived-base-reference-overload-rank.t`; final-only identities are
empty. Tests and fixtures remain unchanged.

## Active Checkpoint

Disposition: completed typed conversion checkpoint. The canonical flow
is PA11 `TypeId`/`NamedRecordId` plus the validated direct-base relation; PA12's
transient `ConversionChoice` carries category, legacy rank, cv addition, base
distance, typed endpoints, and the access scope that proved the relation, but
no path allocation. At final `ConversionFact` publication PA12 performs one
count/path walk with that stored scope and distance, then owns the copied
slice in `conversion_base_paths_`. PA15 validates only that stored slice
against typed identities and complete offset-zero, non-virtual layout, then
emits the required `IPK_BASE_SUBOBJECT` projections. It does not rediscover a
conversion through `derived_base_path`, names, or rendered text. Pointer
values, including null literals, remain values; object references use
addressable storage and retain alias identity; class-by-value transfer remains
rejected.

The rank comparator first compares standard rank category when a class
adjustment participates, then derived-vs-other ordering, nearer base distance,
and cv subset metadata. Pairs without a derived adjustment retain their
legacy numeric rank ordering. Ordinary calls, operators, member calls,
function-id/template resolution, and constructor probes use this typed score.
Structural pointer-common discovery is deliberately scope-free and is used
only to form a candidate common type; every resulting branch is committed by
the scoped `conversion_for`/`record_builtin_conversion` path, so it cannot
fall through as a semantic conversion.

The work is bounded by one count-only direct-base walk per scored class
conversion, one access recheck, and one path-collect/copy operation for each
published derived-base fact. The shared validated edge helper makes walks
cycle-bounded by the named-record arena; no candidate path vectors, global
rescans, retries, or per-occurrence rendered names were added.

Surface audit disposition:

- `dev/src/pa10_parser_support.cpp`: the 26-line change is a general
  delimiter-bounded PA10 indexed discriminator for an identifier followed by
  actual cv qualifiers and, where present, `*`, `&`, or `&&`; pointer/reference
  tokens alone remain on the existing ambiguity path, so `(x * y)` is not
  classified as a definite parameter clause. It is not keyed to a fixture name.
  It implements the accepted `pa16.gram` production
  `parameter-clause -> '(' parameter-declaration-list? ')'`, with
  `parameter-declaration -> decl-specifier-seq declarator` and
  `declarator -> ptr-operator* direct-declarator` (`pa16.gram:287-345`).
  Without it, the ordinary `Base const *` parameter in the direct overload
  test is misclassified as an initializer before PA12 sees the pointer type.
- `dev/src/pa11_record_layout.cpp`: propagates the existing narrow
  zero-storage summary through a direct base at offset zero; PA15 needs that
  typed layout fact for address/alias demand.
- `dev/src/pa11_semantic_core.cpp`: initializes the path arena and separates
  scope-free structural pointer-common discovery from semantic convertibility;
  generic `pointer_convertible` cannot reclassify an inaccessible base path.
- `dev/src/pa11_semantic_model.h`: declares the typed score/fact/path arena,
  scoped conversion APIs, and the selected-call/cast scope plumbing.
- `dev/src/pa12_semantic.cpp`: threads access scope through
  initialization/assignment/return/conditional and pointer common-type
  boundaries, and keeps class-glvalue conditionals as references.
- `dev/src/pa12_semantic_calls.cpp`: applies typed scores to ordinary calls
  and operator candidate sets, including implicit constructor probes.
- `dev/src/pa12_semantic_construction.cpp`: supplies constructor lexical scope
  to member-initializer conversions, which is required for allowed private-base
  member context.
- `dev/src/pa12_semantic_facts.cpp`: owns final conversion-fact publication,
  access recheck, and one copied base-path slice; it also retains the scoped
  unqualified `&member` fact correction.
- `dev/src/pa12_semantic_member.cpp`: owns the shared validated direct-base
  edge/relation walk, count-only access proof, and member-call object score
  while reusing existing member/base facts.
- `dev/src/pa12_semantic_resolution.cpp`: owns scope-free pointer-common
  discovery, classifies pointer/reference conversions, stores compact selected
  endpoints/scope, threads scope through casts/returns, and uses typed
  comparison for function-id/template resolution.
- `dev/src/pa12_semantic_selection.h`: defines the shared conversion kind,
  category, secondary metadata, and comparison boundary.
- `dev/src/pa15_lowering.cpp` and `dev/src/pa15_lowering.h`: dispatch and
  declare the final typed conversion path; no lookup or rendered-name recovery
  occurs at lowering.
- `dev/src/pa15_lowering_member.cpp`: consumes only the final fact's owned
  path slice, validates direct edges/layout/access, and emits offset-zero
  pointer/reference projections while preserving null and alias identity.
- `dev/src/pa15_lowering_flow.cpp`: treats a derived-to-base alias like a
  reference for global demand, admits only non-virtual direct-base summaries,
  and preserves address-context conditional branches.

The explicit class-to-void cast acceptance is retained because the repaired
member-initializer identity contains `(void)iter`; the unqualified `&member`
correction is retained because it is exercised by the repaired function-return
and trailing-return identities. Both are narrow conversion-boundary
prerequisites, not new general class semantics. Nearby controls include the
direct overload, private-base rejection/allowed-member pair, inherited cv
rejection, exact qualification, and overload-rank controls listed above.

Scope plumbing removed during audit was incidental for unary arithmetic,
member-arrow self-retargeting, indirect function-pointer calls, and
condition-only boolean conversion; those targets cannot perform this class
base adjustment.

The final PA15 prior-through gate is `1167/1167`; the exact file audit passes
with five unchanged header-division warnings, and `git diff --check` passes.
The two focused holdouts are deliberately outside this slice: scalar index
scaling/array-initializer lowering. The remaining control failures are the
unchanged static same-signature and string-literal pointer-rejection paths.
Ephemeral parser controls for `(x * y)`, `Base const *`, and `Base volatile &`
all compile successfully; explicit inaccessible implicit and private
`static_cast` probes reject while the member-context private conversion is
accepted. No multiple/virtual inheritance, class-by-value transfer,
conversion operators, or unrelated residual work is included here.

## Performance Evidence

Durable structural evidence is at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-typed-conversion-evidence-final-v1`.
The frozen `cppgm++` SHA-256 is
`5347a2abb876d9492501f70e6fa8fa9f6d3c27f2da0c35283f702d4a2652ab81`.
`results.tsv` records nine immutable probes (depths `1/8/32`, candidates
`2/16/64`), each run twice: all 18 exit statuses are zero, all nine repeated
LowIR hashes match, and `lowir.sha256`/`probes.sha256` records the artifacts.
Declared base edges are the depth; candidate counts are exact. Source sizes
range from 595 to 5215 bytes, LowIR from 83/2112 to 703/18222 lines/bytes,
and observable base projections are 4, 18, or 66 for depths 1, 8, or 32.
The cases include direct and transitive pointer/reference conversion, nearer
base selection, and pointer-to-const-void competition. The evidence supports
allocation-free compact candidate choices, one bounded count/access walk per
scored candidate, and one bounded path collection/copy for each published
selected fact; it makes no timing, RSS, speedup, or asymptotic claim.

## checkpoint ledger

| checkpoint | result |
| --- | --- |
| `2d93a5e9` ordinary non-template overloaded-operator audit | Final PA16 `127/243`, `116` failed, `243/243` covered; through-PA15 `1167/1167`; focused `29/32` with three documented holdouts; deterministic performance evidence hash `e5ffb4e9869c619552f193e16ef063ab2feba7c27f809887ebdd187960196580`. |
| `da4252b6` typed bit-field boundary audit | Final PA16 `131/243`, `112` failed, `243/243` covered; through-PA15 `1167/1167`; focused `5/11`; deterministic performance evidence hash `c98edbf143904e0b09b451310de38e7966149b4374ad912b55a1b9f8c96aaf02`; timing/RSS medians remain historical evidence only. |
| `9718b987` member-function-definition declarator audit | Final PA16 `132/243`, `111` failed, `243/243` covered; through-PA15 `1167/1167`; course 413 and file audit passed; focused `5/7`; excluded out-of-class special-member forms remain out of scope. |
| Typed single-inheritance standard-conversion boundary | Completed: PA16 `144/243`, `99` failed, `243/243` covered; twelve baseline-only failures and no final-only identities; focused matrix `8/10`, controls `7/9`, PA15 conditional controls `2/2`; through-PA15 `1167/1167`; audit passed with five unchanged warnings; durable evidence `pa16-typed-conversion-evidence-final-v1`, compiler SHA-256 `5347a2abb876d9492501f70e6fa8fa9f6d3c27f2da0c35283f702d4a2652ab81`. |
