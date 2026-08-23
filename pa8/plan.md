# Stage Design

PA8 uses one production path: preprocessing produces one `PPTokenBuffer` per
TU, `cpp_declaration_syntax` publishes typed actions, and
`CppSemantic::SemanticCore` owns canonical `NameId`, `TypeId`, `NamespaceId`,
`EntityId`, `TypeKey`, namespace/entity identity, lookup, and flat indexes.
PA7 is a rendering adapter over that core; PA8 adds typed expression/value,
linkage, layout, relocation, and image facts.  `nsinit` owns only CLI,
preprocessing, model invocation, and binary I/O.  This aligns with
`pa8/README.md` and `spec.md` Purpose/§§1-4: one pipeline, typed fact
continuity, stable IDs, deterministic order, and compact hot storage.

## Current Checkpoint

The landed typed slice covers real `nsinit` preprocessing and action flow,
empty declarations and function stubs, scalar zero/constant initialization,
fundamental literal conversion, alignment/order, PA8 magic, and initial
cross-TU/linkage records.  This checkpoint retains namespace
first-declaration ownership and entity-cache invalidation, adds
declaration-owned same-TU linkage inheritance, and separates program
identity/link buckets from current-TU source visibility for
entities/namespaces/aliases/using facts.

The compact representation is now explicit: declaration records retain a
sentinel-aware last-declaration TU scalar; external link buckets are indexed
by `NameId`; TU-local link and source buckets are indexed by `(NameId, TU)`;
typed aliases/using facts use the same composite key; and anonymous/inline
children plus using directives use per-TU occurrence ranges.  Uncached entity
lookup uses reusable generation marks and deterministic current-TU ranges.
Declaration-name conflicts use current-TU-visible namespace/type facts while
redeclaration matching may still use program-wide external identity.  Its LIFO
worklist pushes directives before children so child traversal precedes
using-directive traversal.
The PA7 adapter and PA8 value/layout/relocation/image path remain on this
shared typed core.

The checkpoint adds course probes for valid static-to-unspecified linkage
inheritance, the invalid external-to-static inverse, TU2 entity
declaration visibility/merging, namespace visibility, and non-leaking
typedef, namespace-alias, using-declaration, and using-directive facts.  The
10 new probes remain documented as such; the generation-reuse and cross-TU
linkage regressions are additional guards.  No existing PA8 fixture was
edited; both inverse failure sidecars are standard-informed after the
documented reference path exposed reference divergences.

## Failure Map

The authoritative turn-start full gate is 34/60, with exactly 26 failures;
the final full gate is 46/72 with exactly the same 26 failure identities and
no additional failure.  Its preserved map is:

- References/conversions: `300-bad-ref1/2/3`, `300-uninit-ref`,
  `450-reference`, `450-cv-dropping-reference-bad`,
  `450-lvalue-to-rvalue-reference-bad`, `700-reference-to-reference`.
- Arrays/cv/static assertions: `310-array-str-lit`, `340-array-const`,
  `500-static-assert`, `500-static-assert3`,
  `300-cv-through-typedef-constant`.
- Namespace/linkage validation: `400-namespace-alias-misuse`,
  `400-namespace-alias-to-self`, `410-namespace-conflict1` through
  `410-namespace-conflict6`, and `300-function-typedef-definition-bad`.
- Qualified/cross-TU pointer work: `600-qualified-redeclaration`,
  `600-qualified-redeclaration2`, `120-constexpr-pointer-cross-tu`, and
  `120-constexpr-qualified-pointer`.

The exact through-PA7 gate passed at 339/339.  All 12 new focused PA8 tests
passed in the expanded denominator; no original passing test regressed.
The file audit passed with one existing `bad-division` warning for the
substantial implementation body in `dev/src/cpp_semantic_core.h`.

## Performance Evidence

The core uses open-addressed typed indexes, insertion-order ownership vectors,
per-TU occurrence ranges, and reusable generation-marked dense lookup scratch.
An uncached entity-set lookup visits reachable current-TU namespaces and
candidate buckets, with O(1)-expected entity/namespace dedup marks and
deterministic source order.  Link matching consults the external `NameId`
bucket and only the current TU's internal `(NameId, TU)` bucket; no prior-TU
internal occurrence scan or speculative overload-set cache is claimed.

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
and is not a formal asymptotic proof.  The earlier single-TU deep-namespace
measurement is retained only as context; the table is the material cross-TU
evidence for this checkpoint.

## Next Checkpoint

For the next checkpoint, select one smallest remaining failure
cluster—reference/array/value or the existing namespace/linkage diagnostics—
without expanding this completed audit into those deferred clusters.

## Checkpoint Ledger

| checkpoint | result | evidence |
| --- | --- | --- |
| `checkpointAudit` at `8b56021c` | completed coherent bounded audit/repair/documentation milestone; final gates passed | focused ref provenance and 10 new probes plus two guards; PA8 12/12 focused, final PA8 46/72 with the same 26 failure identities, representative PA8 5/5, PA7 9/9 focused and 339/339 through-PA7, file audit passed with one existing warning, typed compact-index/traversal review, and many-TU scale evidence |
