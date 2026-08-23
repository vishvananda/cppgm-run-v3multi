# Stage Design

PA8 keeps one production path: preprocessing emits typed syntax actions and
`CppSemantic::SemanticCore` owns canonical `NameId`, `TypeId`, `NamespaceId`,
`EntityId`, namespace/entity identity, lookup, and open-addressed indexes.
PA7 and PA8 are adapters over that core; `nsinit` owns only orchestration and
image I/O.  This follows `pa8/README.md` and `spec.md` Purpose/§§1-4/7:
stable IDs, typed fact continuity, deterministic order, and compact storage.
Existing linkage, current-TU visibility, lookup, layout, and image behavior is
retained; this checkpoint adds only the shared namespace-name boundary.

## Failure Map

The original pre-handoff PA8 universe remains **54/72**, with coverage of all
72 cases and exactly these 18 failures.  Commit `bf249b24` was the same
54/72 starting point for this audit.  The parent’s pre-occupancy result was
46/72 with 26 failures; the eight removed identities are preserved as
historical evidence, not counted as new progress here.  This audit adds 17
reduced course cases (nine from the first milestone, seven supervisor-
requested guards, and one final order-regression guard), all of which pass,
for the expanded current gate of **71/89**.

- References/conversions: `300-bad-ref1`, `300-bad-ref2`, `300-bad-ref3`,
  `300-uninit-ref`, `450-reference`, `450-cv-dropping-reference-bad`,
  `450-lvalue-to-rvalue-reference-bad`, `700-reference-to-reference`.
- Arrays/cv/static assertions: `310-array-str-lit`, `340-array-const`,
  `500-static-assert`, `500-static-assert3`,
  `300-cv-through-typedef-constant`.
- Qualified/cross-TU pointer work: `600-qualified-redeclaration`,
  `600-qualified-redeclaration2`, `120-constexpr-pointer-cross-tu`, and
  `120-constexpr-qualified-pointer`.
- Function typedef: `300-function-typedef-definition-bad`.

The exact through-PA7 gate is **339/339**.  The through-PA8 gate is
**410/428** (= 339 prior cases plus the expanded 89-case PA8 set).  The file
audit passes with its one existing `bad-division` warning in
`dev/src/cpp_semantic_core.h`.  The added reduced probes are guards only and
do not alter this 18-identity failure map.

The prior checkpoint’s historical broad evidence was PA8 **54/72** and
through-PA8 **393/411**, with the same 18 PA8 failures and through-PA7
**339/339**.  That result predates the reduced probes in this audit and is
retained only as historical evidence, not as the current final count.

The known reference divergences are standard-informed failure sidecars for
six reduced invalid cases: direct namespace then typedef, using-type then
entity, using-entity then alias, direct same-signature function then using,
distinct imported variables, and `410-using-entity-direct-overload-order-bad`
(direct overload then matching declaration).
The reference agrees with the alias different-target failure, direct-variable
failure, and the three valid using-entity guards.  Handout tests and fixtures
remain untouched.

## Active Checkpoint

`on_namespace_begin` and `on_namespace_alias` reach the shared typed
`namespace_name_conflicts` helper before `named_namespace` and
`declare_namespace_alias` mutate canonical state.  The alias writer now
rejects a current-TU redefinition whose target has a different canonical
`NamespaceId`, while accepting the same-target duplicate and the PA7
self-alias spelling after resolution.  The reverse writers use
`namespace_name_declared_here` at the same core boundary: PA7/PA8 value and
alias declarations and PA8 using/entity declarations cannot reuse a
current-TU direct namespace or namespace alias.  Imported type/entity facts
also block incompatible later PA8 entity/alias declarations.

`add_using_entity` now probes the current-TU direct and imported same-name
`EntityBucketId` indexes.  Canonical `EntityId` equality preserves repeated
using-declarations; direct functions with the same canonical function
`TypeId` conflict; distinct direct overloads remain allowed; multiple imported
functions, including same signatures from separate using-declarations, remain
allowed; and distinct non-functions or function/non-function mixes fail.  The
helper inspects only those relevant candidate buckets, retaining deterministic
order and current-TU visibility.  Both `declare_value` and the PA8 entity
adapter now always inspect the imported same-name bucket, even after a direct
overload has been recorded; valid direct redeclarations and distinct
imported/direct overloads remain allowed.  Repeated namespace reopening, inline
mismatch rules, per-scope nesting, cache invalidation, and cross-TU hidden
occupants remain unchanged.

## Performance Evidence

The namespace and source-occupancy checks use expected-O(1) probes into the
typed open-addressed `(NameId, TU)` indexes, plus the `NameId` direct-namespace
index.  Using-entity compatibility is not a fixed-O(1) claim: after those
probes it inspects the relevant direct/imported same-name candidate bucket in
O(k), where k is the language-relevant candidate count.  There are no spelling
render/reparse operations, whole-scope scans, or new caches; ordinary total
work remains proportional to the typed facts and the candidate buckets it
actually consumes.  This is aligned with spec.md §3’s typed semantic identity
and hot-path indexing and §4’s ordinary O(n)/O(n log n) work rule; it does not
claim a constant-time compatibility loop.

No fresh timing claim was made for this audit.  The scale probe below uses the
immutable executable from the prior checkpoint and is retained as historical
structural evidence, not as timing evidence for this diff.

The representative cross-TU scale probe used immutable rebuilt `dev/nsinit`
sha256 `d3f51f3235139285af129e3698118ba73b714ab6d5b0b03345c9372d711f94aa`.
Each generated TU had `static int x = N; int yN = x;`, so the structural shape
was two declarations and one lookup per TU.  Three repetitions at 256, 1024,
and 2048 TUs all succeeded and reproduced one output hash per size:

| TUs | declarations/lookups | input manifest sha256 | output bytes / sha256 | wall / max RSS |
| ---: | ---: | --- | --- | --- |
| 256 | 512 / 256 | `b04ec9b864823c71d59df8dee1d78d7e74b6cbae1c8f9b2dca21e1ea7b5dfbfd` | 2052 / `e63ac1a80abc5ec87e40a3734477a2d62c8749ec4a4c670fd02fa2859adc3c22` | 0.01s / 4,372 KB |
| 1024 | 2048 / 1024 | `63a3664f45fb22f72823209b980c24a6ed68f1221b461ad0a84ca5bab40e6179` | 8196 / `6e4bb7a5297bf1b2438930b1b69568d4f23a659f8770bdf92c35a170d8a8080a` | 0.03s / 5,156 KB |
| 2048 | 4096 / 2048 | `07f32c39110db73bdcb058bcb3a373bf0baed1c72b6de1f54ae2f5b20b455c85` | 16388 / `95a57c81c1e2d05e2b8eb487e03f9d252a81e2dba8caa70b9b17707ae07e9d2e` | 0.07s / 6,136 KB |

The probe records generated-shape counts, `/usr/bin/time` wall/user/sys/RSS,
and deterministic output/input hashes, but has no phase/allocation counters
and is not a formal asymptotic proof.  The executable hash, measurements, and
table are historical prior-checkpoint evidence; they are not a fresh timing
claim about the alias/using repair.  The earlier single-TU deep-namespace
measurement is retained only as context.

## Next Checkpoint

The checkpoint-audit review and authorized broad gates are complete.  The next
implementation checkpoint should choose one smallest cluster from the same 18
deferred PA8 failures—reference/conversion, array/cv/static-assert,
qualified/cross-TU pointer, or function-typedef—and must not fold those into
this occupancy audit.  It should begin from the clean amended commit produced
for this audit and preserve the 71/89 PA8 result as its comparison point.

## Checkpoint Ledger

| checkpoint | result | evidence |
| --- | --- | --- |
| `checkpointAudit` at `8ee86ae7` | completed prior bounded audit/repair milestone | turn-start PA8 46/72; through-PA7 339/339 and file audit passed with one existing warning |
| `namespaceNameOccupancy` | completed; committed after broad validation | PA8 54/72 with all eight target identities removed and no regression; coverage 72; through-PA7 339/339; file audit passed with one existing warning; through-PA8 393/411 with the same 18 deferred failures; no fresh timing claim |
| `checkpointAudit` at `bf249b24` | completed after final order-dependent using-entity repair; final amended commit carries the bounded result | spec Purpose/§§1-4/7 alignment; shared typed ownership and invalidation trace; same-target namespace-alias redefinition; using-entity direct/imported bucket compatibility with expected-O(1) probes plus O(k) candidate inspection; both direct-declaration adapters always inspect imported candidates; PA8 focused 36/36 and PA7 focused 9/9; original 54/72 baseline plus 17 passing guards = 71/89; through-PA7 339/339 and through-PA8 410/428; file audit passed with one pre-existing warning; six standard-informed reference divergences retained honestly |
