# PA8 Checkpoint Audit

## Current Checkpoint Review

This review is bounded to landed commit `657e5559` (parent `affab90c`),
**PA8: implement reference semantics**, and its completed array/reference
repair.  It audits only the reference-semantics ownership path; the nine
turn-start PA8 failures remain outside the slice unless a failure is directly
caused by that path.

The typed fact flow is continuous.  Preprocessing and posttokenization carry
decoded literals, spelling IDs, and declaration syntax into
`CppSyntaxDeclaratorOp`; prefix markers, suffix operations, and bound
expressions are copied into `DeclaratorShape`/`DeclaratorOp` without rendering.
`SemanticCore` then interns canonical `TypeId`s and owns reference collapsing,
pointer-to-reference rejection, reference-to-void rejection, and array-of-
reference rejection.  PA8 evaluates each expression into a `PA8Value` that
retains both the named `EntityId` and the post-dereference referent.  Binding
applies C++11 lvalue/rvalue and cv rules, appends a typed lifetime-extended
`Temporary` record for converted bindings, and records a typed relocation.
Image planning emits named entities in block 1 and temporaries in block 2, then
patches relocations after all offsets are known.

The initial audit found one real formation defect: `array(...)` accepted a
reference child, and flattened prefix application could turn `int& a[2]` or
`int*& a[2]` into an apparently legal outer reference instead of diagnosing an
array of references.  Canonical array formation now owns that invariant for
direct and typedef-mediated forms.  The re-review found a second bounded
adapter defect: `direct_reference` and the then-global array-shape check were
carried across the whole flattened operation vector, while `apply_shape`
processes contiguous prefix runs as distinct layers separated by Function or
Array suffixes.  That falsely rejected legal nested layers such as
`extern int& (*& slot)();` and `int& (*slots[2])();`.  The correction resets
direct-spelling validation at each non-prefix boundary and removes the global
array-shape check; adapter validation is now segment-scoped only for forbidden
same-segment direct reference-to-reference and pointer-to-reference spellings.
This matches N3485 8.3.2 paragraph 5: the forbidden formations are
reference-to-reference, array-of-reference, and pointer-to-reference; the two
cross-layer types are not one of those formations.
Grouped reference-to-array, the two new nested-layer forms, direct
pointer-to-reference rejection, direct reference-to-reference rejection, and
typedef reference collapsing remain distinct and valid where required.

Binding and lifetime facts were traced through direct referent identity,
known scalar conversions, unknown arithmetic conversions, and relocation/image
order.  Direct compatible lvalue bindings preserve the referent; rvalue
references reject lvalues; non-const or volatile-qualified lvalue references
reject prvalues/conversion temporaries; and compatible cv additions bind
directly.  Known conversions retain bytes, while unknown arithmetic
conversions create a zero-initialized, non-constant temporary.  Temporary
append order is first-use order, and the two image passes prevent a temporary
from changing block-1 declaration order.

Focused evidence is `make -C dev nsinit -j2` and the exact repaired,
representative, and durable-regression set at `make -C pa8 check TEST='...'`
(14/14).  It covers both array-of-reference forms, grouped
reference-to-array, the two nested cross-layer function forms,
reference-to-pointer/function, typedef collapse, reference-to-void, referent
identity, and known/unknown temporary relocation.  The reduced probes
produced the expected success/failure statuses and deterministic images.  The
documented reference workflow accepted both new legal nested-layer cases with
`EXIT_SUCCESS` and generated their two fixtures.  The earlier reference accepts
the two standard-invalid array-of-reference cases, so their checked-in exit
sidecars pin `EXIT_FAILURE` and their failed payloads are not retained, while
the grouped valid fixture is retained exactly.  No handout test changed.  The
original turn-start broad measurement was PA8 **80/89** over **89 cases**, with
exactly these nine failures:

- `pa8/tests/310-array-str-lit.t.1`, `pa8/tests/340-array-const.t.1`,
  `pa8/tests/500-static-assert.t.1`, `pa8/tests/500-static-assert3.t.1`;
- `pa8/tests/600-qualified-redeclaration.t.1`,
  `pa8/tests/600-qualified-redeclaration2.t.1`;
- `cppgm.tests/course/pa8/120-constexpr-pointer-cross-tu.t.1`,
  `cppgm.tests/course/pa8/120-constexpr-qualified-pointer.t.1`,
  `cppgm.tests/course/pa8/300-function-typedef-definition-bad.t.1`.

The final checked-in PA8 suite is **85/94**: all five added course regressions
pass, and the original 89-case set remains **80/89** with exactly the same nine
failure identities and no new failure.  `make test-pa8` exits 2 for those
deferred cases; the exact through-PA7 report is **339/339**; and
`make test-report-through-pa8` is **424/433** with the same nine identities.
The file audit exits 0 with the existing single header warning recorded below.
Performance evidence is structural only: formation and segment validation are
linear in the current declarator, binding uses indexed identity plus bounded
type/category checks and one temporary append, and image planning remains two
entity passes plus the existing relocation pass.  There is no whole-arena hot
scan, textual downgrade, duplicate semantic model, whole-program retry,
test-spelling shortcut, or compiler/reference shell-out in this path.  No
timing claim is made.

## Prior Checkpoint Review

The prior review was bounded to the landed increment at `8b56021c` and the PA8
`checkpointAudit` slice.  It did not attempt the then-listed 26 features.

The representative ownership path is continuous and typed:

- `nsinit` preprocesses each source into a `PPTokenBuffer`;
  `posttokenize_cpp_tokens` carries token/spelling IDs and decoded `LiteralData`
  into typed declaration-syntax actions.  Semantic facts are not reconstructed
  by rendering source text.
- `SemanticCore` interns spelling/text facts into canonical `NameId`, `TypeId`,
  `NamespaceId`, and `EntityId` arenas.  `TypeKey`, declaration vectors,
  identity/link buckets, source indexes, and value records retain typed facts
  in deterministic insertion order.
- PA7 is a rendering adapter over that core.  PA8 evaluates decoded literals
  and IDs into typed values, records constant bytes or typed relocations,
  computes layout, and emits the PA8 image from those facts.

Findings and bounded repairs:

1. Namespace `children` and entity `variables`/`functions` remain the real
   first-declaration vectors.  Entity and namespace records now keep only a
   sentinel-aware `last_declaration_translation_unit` scalar; each declaration
   marks the canonical owner visible in the current TU and invalidates the
   affected lookup cache.
2. Linkage is inherited at the declaration owner from a declaration already
   marked in the current TU.  A no-storage-class declaration after
   `static void f();` reuses the internal entity, and an external declaration
   repeated in TU2 before `static void f();` still diagnoses the inverse
   inconsistent linkage.  The provisional same-TU conflict fixture was
   replaced by valid-inheritance and standard-informed invalid-inverse
   regressions.
3. Program identity/link matching is separate from source visibility.  External
   link candidates use a typed `NameId` bucket; TU-local link candidates use a
   `(NameId, translation_unit)` bucket.  Current-TU source entities, typedefs,
   namespace aliases, using-declarations, and using-directive/anonymous/inline
   traversal facts use typed `(NameId, translation_unit)` indexes or
   per-TU occurrence ranges.  Thus a prior-TU declaration can be a canonical
   redeclaration candidate without becoming visible to TU2 lookup before TU2
   declares it.  PA8 declaration-name conflict checks likewise consult only a
   namespace visible in the current TU; a hidden canonical namespace remains
   identity/link state, not a source diagnostic fact.
4. `lookup_entities_here` reuses dense generation-marked namespace/entity
   scratch and visits only current-TU reachable ranges plus candidate buckets.
   It no longer zero-fills a namespace-sized mark vector per query, performs
   quadratic entity-ID deduplication, or scans prior-TU internal link buckets.
   Source order remains the insertion order of typed buckets/ranges.  The
   entity generation is monotonic across TU boundaries; wraparound alone
   clears both mark arrays, preventing a first lookup in a later TU from
   colliding with a prior TU's generation.  The LIFO worklist pushes
   using-directive targets before child ranges, so anonymous/inline children
   are visited before using directives, in the shared lookup order.
5. The PA8 entity owner appends new entities to the namespace's real
   first-declaration vector and invalidates entity lookup after both new and
   merged declarations.  The source/link split makes the former ad hoc
   preferred-candidate repair unnecessary, so it is absent.
6. The shared-core header owns its `<algorithm>`, stream, and string-building
   dependencies.  The prior file-audit header-body warning is not expanded in
   this bounded correction.

Focused evidence:

- `make -C dev -B nsinit nsdecl -j2` rebuilt the affected tools.
- The documented `make -C pa8 ref-test TEST='...'` path was used for the 10
  new checkpoint probes and both linkage/generation guards.  The reference
  agrees with valid inheritance and the visibility probes, but accepts both
  N3337-invalid external-then-static inverse cases.  Their generated success
  outputs were discarded; the checked-in failure sidecars are
  standard-informed and the divergence is recorded honestly.
- `make -C pa8 check TEST='...'` passed 12/12 focused ownership probes (the 10
  new probes plus generation-reuse and cross-TU linkage guards).  Five
  representative PA8 image cases passed 5/5.  Focused PA7
  namespace/alias/using/inline/unnamed cases passed 9/9.
- A many-TU probe used one immutable rebuilt `dev/nsinit` with binary sha256
  `d3f51f3235139285af129e3698118ba73b714ab6d5b0b03345c9372d711f94aa`.
  Each zero-padded TU had the equivalent shape
  `static int x = N; int yN = x`: two declarations and one id-expression
  lookup, with the same-spelled TU-local `x`.  Three repetitions at each size
  produced identical output hashes:

  | TUs | declarations/lookups | input manifest sha256 | output bytes / sha256 | timing and max RSS |
  | ---: | ---: | --- | --- | --- |
  | 256 | 512 / 256 | `b04ec9b864823c71d59df8dee1d78d7e74b6cbae1c8f9b2dca21e1ea7b5dfbfd` | 2052 / `e63ac1a80abc5ec87e40a3734477a2d62c8749ec4a4c670fd02fa2859adc3c22` | 0.01s wall, 4,372 KB |
  | 1024 | 2048 / 1024 | `63a3664f45fb22f72823209b980c24a6ed68f1221b461ad0a84ca5bab40e6179` | 8196 / `6e4bb7a5297bf1b2438930b1b69568d4f23a659f8770bdf92c35a170d8a8080a` | 0.03s wall, 5,156 KB |
  | 2048 | 4096 / 2048 | `07f32c39110db73bdcb058bcb3a373bf0baed1c72b6de1f54ae2f5b20b455c85` | 16388 / `95a57c81c1e2d05e2b8eb487e03f9d252a81e2dba8caa70b9b17707ae07e9d2e` | 0.07s wall, 6,136 KB |

  The probe reports generated-shape counts, `/usr/bin/time` wall/user/sys/RSS,
  and deterministic hashes; it has no phase/allocation counters and is not a
  formal asymptotic proof.  The old single-TU deep-namespace run remains only
  contextual evidence, not cross-TU scaling evidence.

The authoritative turn-start gate was 34/60 with exactly 26 failures.  The
final `make test-pa8` gate is 46/72 with exactly those same 26 failure
identities and no additional failure: `300-bad-ref1`, `300-bad-ref2`,
`300-bad-ref3`, `300-uninit-ref`, `310-array-str-lit`, `340-array-const`,
`400-namespace-alias-misuse`, `400-namespace-alias-to-self`,
`410-namespace-conflict1` through `410-namespace-conflict6`, `450-reference`,
`500-static-assert`, `500-static-assert3`, `600-qualified-redeclaration`,
`600-qualified-redeclaration2`, `700-reference-to-reference`,
`120-constexpr-pointer-cross-tu`, `120-constexpr-qualified-pointer`,
`300-cv-through-typedef-constant`, `300-function-typedef-definition-bad`,
`450-cv-dropping-reference-bad`, and `450-lvalue-to-rvalue-reference-bad`.
The 12 new focused tests passed within the expanded denominator; no original
passing test regressed.  The exact prior gate passed at 339/339 through PA7.
The exact file audit passed with one existing warning:
`dev/src/cpp_semantic_core.h:1 [bad-division]` (substantial header
implementation body; prefer `.cpp` ownership).

No landed-increment source-visibility, generation-scratch, linkage, or
multi-entity lookup-bound defect is deferred in this review.  The bounded
deferred scope is the existing PA8 failure map: reference conversions,
array/cv completion, static-assert edge cases, complete reference conversions,
pointer relocation through variables, qualified redeclaration, and the
pre-existing namespace diagnostic family.  The inverse-linkage reference
divergence and the absence of runtime phase/allocation counters are the known
uncertainties.

## Audit Ledger

| checkpoint | result | evidence and scope |
| --- | --- | --- |
| `checkpointAudit` at `8b56021c` | coherent bounded repair/documentation milestone complete; final gates passed | typed ownership trace; current-TU declaration-owned linkage inheritance; scalar TU ownership; typed `(NameId, TU)` source/link indexes and per-TU ranges; monotonic entity scratch and child-before-directive traversal; 10 new focused PA8 probes plus two guards; final PA8 46/72 with the same 26 failure identities, through-PA7 339/339, file audit passed with one existing warning, and many-TU scale evidence above |
| `checkpointAudit` at `bf249b24` | completed after final order-dependent using-entity repair; final amended commit carries the bounded result | typed `cpp_declaration_syntax` -> PA7/PA8 actions -> interned `NameId`/current-TU `NamespaceId` -> `NamespaceRecord` indexes -> invalidating writers -> PA7 rendering/PA8 image and exit behavior; same-target-only namespace-alias redefinition; direct/imported using-entity bucket compatibility with O(k) candidate inspection; both direct-declaration adapters always inspect imported candidates; PA8 focused 36/36, PA7 focused 9/9; original baseline 54/72 and expanded gate 71/89 with exactly the original 18 identities; through-PA7 339/339, through-PA8 410/428; file audit passed with one pre-existing header warning; six standard-informed reference divergences pinned by failure sidecars and valid reference fixtures retained |
| `checkpointAudit` at `657e5559` + array/reference repair | completed bounded ownership-path audit with nested-layer correction; final PA8 85/94, with the original 89-case set still 80/89 and exactly the same nine failures; through-PA7 339/339; through-PA8 424/433 | typed `CppSyntaxDeclaratorOp` -> `DeclaratorShape` -> segment-scoped direct-prefix validation -> canonical reference/array invariants -> `PA8Value` entity/referent -> cv/category binding -> typed lifetime-extended temporaries -> block-1/block-2 image relocation; focused 14/14; five durable course regressions and prescribed fixtures; the two new legal nested-layer cases reference-pass with `EXIT_SUCCESS`; canonical array formation remains the sole array-of-reference rejection owner; the unchanged exact nine identities are `pa8/tests/310-array-str-lit.t.1`, `pa8/tests/340-array-const.t.1`, `pa8/tests/500-static-assert.t.1`, `pa8/tests/500-static-assert3.t.1`, `pa8/tests/600-qualified-redeclaration.t.1`, `pa8/tests/600-qualified-redeclaration2.t.1`, `cppgm.tests/course/pa8/120-constexpr-pointer-cross-tu.t.1`, `cppgm.tests/course/pa8/120-constexpr-qualified-pointer.t.1`, and `cppgm.tests/course/pa8/300-function-typedef-definition-bad.t.1`; all broad gates passed with no new failure; file audit passed with the existing header warning; structural complexity only and no timing claim |
