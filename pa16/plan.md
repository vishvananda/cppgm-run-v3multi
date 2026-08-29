# PA16 implementation plan

## Stage Design

PA11 remains the typed owner of canonical member access in the binding
sidecar.  This checkpoint extends that owner/data flow with a sparse typed
friend-class relation (`NamedRecordId` owner to `NamedRecordId` friend and its
reverse index) and a typed access-view fact on `ValueEntry`: the
`MemberAccess` override and the publishing class `ScopeId` travel together.
A class using-declaration therefore publishes a view without changing the
canonical binding/origin owner.  PA12 carries both view facts through
`ValueRef`, `MemberLookup`, and member/operator candidates; it validates the
publishing class and object lookup/base path, while `member_accessible` keeps
the canonical owner for binding, type, and projection and evaluates a
private/protected view relative to the view owner.  PA15 consumes the resulting
typed member/base facts without a friendship-specific boundary: every
validated direct base edge remains a LowIR projection.  No name, rendered
text, whole-TU rescan, parallel access table, or retry loop is used.

The language behavior is sourced from the PA16 README and `doc/n3485.txt`
§§7.3.3 paragraphs 17--18, 11.2 paragraphs 4--6, 11.3 paragraphs 1--10,
and 11.4 paragraph 1: friendship is neither inherited nor transitive; private
access remains restricted; protected access requires an eligible class/friend
context and the object-expression rule; a using-declaration requires each
named base declaration to be accessible and gives the alias the usual access
of its member-declaration context.
The typed-owner, identity, bounded-work, and lowering constraints are from
`spec.md` §§2--5 and 7.  Single non-virtual inheritance is the only base model
here; virtual/multiple inheritance, templates/variadics, and later
value-semantics families remain excluded.

The earlier inheriting-constructor increment at `30d69fc3` is historical, not
the active wording of this checkpoint.  Its typed wrapper implementation,
constructor probe, and broad evidence remain summarized in the ledger below.

## Failure Map

Turn-start authority is commit `0fb73ad4`: `176/243` PA16 identities pass,
`67` fail, and `243/243` identities are covered.  The exact 67 failures below
are copied from `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:

```text
pa16/tests/general/100-function-pointer-nested-param-name-shadow.t
pa16/tests/general/100-global-aggregate-nested-array-initializer.t
pa16/tests/general/100-global-reference-incomplete-referent.t
pa16/tests/general/100-object-member-enumerator-constant.t
pa16/tests/general/200-aliased-base-mem-initializer-match.t
pa16/tests/general/200-const-subobject-member-call.t
pa16/tests/general/200-defaulted-constructor-still-aggregate.t
pa16/tests/general/200-deleted-constructor-still-aggregate.t
pa16/tests/general/200-destructor-body-local-before-base-destruction.t
pa16/tests/general/200-elaborated-member-forward-type.t
pa16/tests/general/200-extern-class-object-declaration.t
pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t
pa16/tests/general/200-friend-derived-access-inherited-protected-field.t
pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t
pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t
pa16/tests/general/200-global-constructor.t
pa16/tests/general/200-global-function-style-constructor.t
pa16/tests/general/200-local-default-class-array-lifecycle.t
pa16/tests/general/200-member-call-hides-outer-type-declaration.t
pa16/tests/general/200-member-object-lifetime.t
pa16/tests/general/200-mutable-member-const-method.t
pa16/tests/general/200-nested-braced-member-aggregate-init.t
pa16/tests/general/200-nested-out-of-class-constructor-enclosing-type.t
pa16/tests/general/200-nonliteral-field-condition-not-folded.t
pa16/tests/general/200-placement-new-expression-aggregate-brace.t
pa16/tests/general/200-placement-new-expression-constructor-call.t
pa16/tests/general/200-pointer-subscript-class-reference-return.t
pa16/tests/general/200-protected-member-typedef-access-bad.t
pa16/tests/general/200-qualified-friend-function-member-access.t
pa16/tests/general/200-reference-indexed-pointer-member-access.t
pa16/tests/general/200-reference-member-class-init.t
pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t
pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t
pa16/tests/general/300-adl-associated-namespace-does-not-climb-parents.t
pa16/tests/general/300-adl-using-declaration-source-point.t
pa16/tests/general/300-callable-field-hides-private-base-method.t
pa16/tests/general/300-class-using-declaration-reexposes-protected-field.t
pa16/tests/general/300-compound-assignment-adl-nonmember-after-member-reject.t
pa16/tests/general/300-const-pointer-explicit-destructor-call.t
pa16/tests/general/300-enum-class-nonmember-operator-bitand.t
pa16/tests/general/300-explicit-destructor-call-enclosing-namespace-type.t
pa16/tests/general/300-friend-function-definition-skip.t
pa16/tests/general/300-header-static-class-init.t
pa16/tests/general/300-member-vs-nonmember-operator-implicit-object-cv-rank.t
pa16/tests/general/300-mixed-member-free-shift-stress-chain.t
pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t
pa16/tests/general/300-operator-nullptr-t-from-zero.t
pa16/tests/general/300-operator-shift-stress-chain.t
pa16/tests/general/300-overloaded-deref-user-assignment.t
pa16/tests/general/300-packed-class-layout.t
pa16/tests/general/300-pragma-pack-followed-by-endif.t
pa16/tests/general/300-prvalue-derived-base-friend-operator.t
pa16/tests/general/300-scalar-pseudo-destructor-call.t
pa16/tests/general/300-static-class-member-object-definition.t
pa16/tests/general/300-synthesized-array-member-lifecycle.t
pa16/tests/general/300-thread-local-synthetic-symbol-family-isolation.t
pa16/tests/general/300-unary-address-of-builtin-fallback.t
pa16/tests/general/300-user-defined-string-literal-operator.t
pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t
pa16/tests/general/300-using-declaration-function-hides-tag.t
pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t
pa16/tests/general/400-bit-field-constructor-member-init.t
pa16/tests/general/400-bit-field-member-access-bad.t
pa16/tests/general/400-bit-field-prefix-postfix-increment.t
pa16/tests/general/400-bitfield-aggregate-init.t
pa16/tests/general/400-signed-bit-field-read.t
pa16/tests/general/400-signed-enum-bit-field-read.t
```

The selected existing family is the five access identities named above:
friend access through a derived object, friend access to a private-base
defaulted constructor, friend access through an intermediate derived class,
protected typedef rejection, and public using re-exposure.  Preservation
controls are the seven PA16 identities `100-private-method-bad`,
`200-private-base-static-cast-bad`, `200-private-base-static-cast-member`,
`200-protected-base-method`, `200-friend-function-member-access`,
`200-nested-class-private-enclosing-access`, and
`spec/200-nested-class-enclosing-access`, plus course controls 405 and 411.
The new reduced course control 419 covers the same typed using/friend boundary
without counting as stage progress.  All other residual identities, and all
out-of-scope inheritance/language families, are excluded.  No handout test,
`.ref`, exit-status reference, harness, comparator, or existing fixture is
changed.  The final broad result is `179/243` passing, `64` failures, and
`243/243` identities covered; comparison with the exact turn-start map has
only the three repaired baseline identities below and no final-only identity.

## Active Checkpoint

The final landed access-control scope is exactly these eight
implementation files, one new reduced course control, and this plan:

- `dev/src/pa11_semantic_model.h`
- `dev/src/pa11_semantic_core.cpp`
- `dev/src/pa11_semantic.cpp`
- `dev/src/pa11_semantic_types.cpp`
- `dev/src/pa12_semantic_selection.h`
- `dev/src/pa12_semantic.cpp`
- `dev/src/pa12_semantic_calls.cpp`
- `dev/src/pa12_semantic_member.cpp`
- `cppgm.tests/course/pa16/419-typed-using-access-regression.sh`

`pa11_semantic.cpp` publishes the class access view on using declarations,
records its publishing `ScopeId`, validates the canonical declaration and
accessible base path at that point, and records the friend-class reverse
relation.  `pa11_semantic_core.cpp` and the model header own the
identity-bearing sidecars/indexes and carry the paired view facts through typed
lookup.  `pa11_semantic_types.cpp` is required because the protected typedef
failure is a type-lookup access boundary with no object expression.
`pa12_semantic_selection.h` is required because `ValueRef` is the typed
candidate sidecar; `pa12_semantic.cpp` carries its view owner through implicit
member data access, and `pa12_semantic_calls.cpp` does the same for
member/operator candidate access.  `pa12_semantic_member.cpp` owns member
lookup, view-provenance checks, using-import validation,
friend/private/protected/object checks, and the selected canonical member/base
context.  The new 419 script is a reduced public-boundary regression for
re-exposure and friendship; it does not alter the handout suite or count as
progress.  `dev/src/pa15_lowering_calls.cpp` is deliberately unchanged:
friendship affects semantic access only, so the existing lowerer retains the
complete validated direct-single-base chain and per-edge projection convention
required by course 405.

Final state: this typed access-control checkpoint is validated as the landed
increment.  The focused 14-test set is `12/14`: the defaulted-constructor
fixture and the intermediate friend-call fixture semantically accept but retain
checked-in LowIR shape residuals.  The former adds the defaulted derived/base
constructor call; the latter retains both validated base projections after the
PA15 workaround was removed.  The three selected identities with fully
matching existing fixtures are the friend-derived protected field, protected
typedef rejection, and public using re-exposure.  The two using-declaration
preservation controls, seven PA16 preservation controls, and courses 405/411/419
pass; no access boundary is reopened by the residual LowIR work.

## Performance Evidence

The material risk is accidentally turning access checks into unrelated-scope
or whole-TU scans.  The implementation uses the sparse reverse relation only
for lexical class identities, walks lexical ancestry with a scope-vector
bound, and validates only the relevant direct-single-base chain with identity
and cycle guards.  Access-view publication/lookup also compares the typed
owner to the publishing class and the object path; this is bounded structural
reasoning, not a timing claim.

Deterministic external evidence is under
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-access-boundary-probe-20260829`.
The 40-line access/using case and the same case with 64 unrelated empty class
declarations (105 lines) both compile with the repository `dev/cppgm++` to
byte-identical LowIR, SHA-256
`a994e25767151654c710b2724364f1b5f3d9b071c3b9326aef284b962a1b2fd6`, with
six base projections each.  The protected-typedef negative pair both exits 1
and has identical stderr SHA-256
`37e6f8ed897d209b62c1b3b33e831cb114a86a06e792e0aa8c0645df156d3fd3`.
This is representative structural/noise-isolation evidence only; no speedup,
timing, RSS, or allocation claim is made, and no host compiler or reference
binary was used.  The refreshed run and statuses are recorded in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-access-boundary-probe-validated-20260829.log`.

Historical constructor evidence remains at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-inheriting-constructor-probes-20260828`:
19-line versus 84-line typed inputs produced identical 68-line outputs with
hash `02de5e72c8fccc4a0aa6304231b0bed5d29cbeabea1c9b33bd35a67827d78592`.
That evidence belongs to the landed constructor checkpoint, not this active
access boundary.

## Validation

- `make -C dev cppgm++` — exit `0` after final source compaction.
- Focused `make -C pa16 check CPPGM_SKIP_DEV_REBUILD=1 TEST='tests/general/300-private-base-using-method-call.t tests/general/300-using-declaration-public-private-base-member.t tests/general/300-class-using-declaration-reexposes-protected-field.t tests/general/200-friend-derived-access-inherited-protected-field.t tests/general/200-friend-derived-private-base-defaulted-constructor.t tests/general/200-friend-intermediate-derived-protected-base-method.t tests/general/200-protected-member-typedef-access-bad.t tests/general/100-private-method-bad.t tests/general/200-private-base-static-cast-bad.t tests/general/200-private-base-static-cast-member.t tests/general/200-protected-base-method.t tests/general/200-friend-function-member-access.t tests/general/200-nested-class-private-enclosing-access.t tests/spec/200-nested-class-enclosing-access.t'` — `FAIL (12/14)`.
- The focused command exits `2` only for the two documented LowIR residuals;
  all other selected existing identities match, including the three repaired
  pre-existing identities named above.  Durable output:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-access-control-focused-validated-20260829.log`.
- The three fully repaired pre-existing selected identities are
  `200-friend-derived-access-inherited-protected-field.t`,
  `200-protected-member-typedef-access-bad.t` (expected failure), and
  `300-class-using-declaration-reexposes-protected-field.t`.  The selected
  constructor and intermediate-call sources both now record `EXIT_SUCCESS`,
  but their checked-in LowIR comparisons remain residuals: the former emits
  the defaulted derived/base call, and the latter retains both validated base
  projections after the PA15 workaround was removed.
- The two using-declaration preservation controls and all seven listed PA16
  preservation controls pass.  Direct `sh` execution of course controls 405,
  411, and expanded new 419 also passes (all exit `0`); 419 covers private
  view use inside/outside its declaring class, protected view use from a
  further-derived context with derived/further-derived versus base/unrelated
  objects, public/protected re-exposure, private-source rejection/friend
  access, non-transitivity, and friend protected-object restrictions.
- Broad `make test-pa16` exits `2` at `179/243` with `64` failures and
  `243/243` identity coverage.  Durable log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-access-control-broad-validated-20260829.log`.
  The sorted final identities, exact baseline, and comparison are in
  `pa16-access-control-validated-identities-20260829.txt`,
  `pa16-access-control-baseline-identities-20260829.txt`, and
  `pa16-access-control-validated-identity-diff-20260829.txt` in that same
  directory.  The comparison is baseline-only for the three repairs and
  final-only is empty.
- The exact through-PA15 gate exits `0` at `1167/1167`; durable log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-access-control-through-pa15-validated-20260829.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0`
  with exactly the five known warnings for `abi_mangle.h`,
  `cpp_semantic_core.h`, `lowir_model.h`, `pa11_semantic_model.h`, and
  `pa15_lowering.h`; durable log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-access-control-file-audit-validated-20260829.log`.
- Courses 405, 411, and expanded 419 each exit `0`, and `sh -n` for 419 exits
  `0`; durable log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-access-control-course-validated-20260829.log`.
- The read-only scope check exits `0`: only the eight implementation files,
  new 419, and this plan are changed; no existing handout test, reference,
  exit-status file, harness, comparator, or course control is changed.  Durable
  record:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-access-control-scope-validated-20260829.log`.
- `git diff --check` exits `0`; the refreshed structural probe also exits `0`.

## Next Checkpoint

Next checkpoint: the remaining PA16 semantic/lifecycle/layout residual family
outside typed access control.  It must preserve this landed access boundary,
the exact 64-identity residual set, full per-edge lowering, and the
direct-single-base/non-template exclusions; no access-view or friendship model
reopening is planned.

## Checkpoint Ledger

| checkpoint | result |
| --- | --- |
| `36b93869` historical aggregate handoff | `159/243` PA16 passing, `84` failures, `243/243` covered; PA1--PA15 `1167/1167`. |
| `dea01c52` aggregate implementation | Historical aggregate increment: `164/243` passing, `79` failures, `243/243` covered; through-PA15 and audit evidence preserved. |
| `3c2114b6` typed-builtin turn-start | Historical clean state at `164/243`, `79` failures, `243/243` covered; exact residual map carried forward. |
| `d7ed98aa` typed builtin boundary | `167/243` passing, `76` failures, `243/243` covered; typed builtin semantic/lowering owner added. |
| `f290784f` typed builtin audit | Historical clean baseline: `167/243`, `76` failures, `243/243` covered; PA1--PA15 and audit pass. |
| `3b7d8e6a` qualified-type checkpoint | `173/243` passing, `70` failures, `243/243` covered; prior broad evidence preserved. |
| `working tree after 3b7d8e6a` historical constructor first stop | Selected constructor family and preservation controls passed before the later constructor increment. |
| `30d69fc3` landed inheriting-constructor checkpoint | Historical landed increment: `176/243` passing, `67` failures, `243/243` covered; typed N3485 wrapper/default/DMI/copy/order-independent evidence and durable broad/identity/probe logs are retained above. |
| `0fb73ad4` PA16 access turn start | Clean authority for this checkpoint: `176/243` passing, `67` failures, `243/243` covered; through-PA15 `1167/1167`, audit with five known header-division warnings. |
| `PA16 typed access-control checkpoint` | Landed typed friend-class relations and declaration-point-validated using access views, carrying the paired publishing-owner `ScopeId` through `ValueEntry`/`ValueRef`/`MemberLookup`/candidates while retaining canonical binding/origin and full PA15 per-edge lowering. Final PA16 is `179/243` with `64` failures and `243/243` identities; exact comparison to the `67`-failure turn-start map has the three baseline-only repairs and no final-only identities. The selected set is `12/14` only on the two documented LowIR residuals; through-PA15 is `1167/1167`, file audit exits `0` with the five known warnings, courses 405/411/419 and `sh -n` pass, structural probe is noise-isolated, and no existing handout/test/reference/harness/comparator changed. |
