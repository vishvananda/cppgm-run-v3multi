# Stage Design

PA8 keeps one production path: preprocessing emits typed syntax actions and
`CppSemantic::SemanticCore` owns canonical `NameId`, `TypeId`, `NamespaceId`,
`EntityId`, namespace/entity identity, lookup, and open-addressed indexes.
PA7 and PA8 are adapters over that core; `nsinit` owns only orchestration and
image I/O.  This follows `pa8/README.md` and `spec.md` Purpose/§§1-4:
stable IDs, typed fact continuity, deterministic order, and compact storage.
Existing linkage, current-TU visibility, lookup, layout, and image behavior is
retained; this checkpoint adds only the shared namespace-name boundary.

## Failure Map

Turn-start baseline is PA8 **46/72**, with exactly these 26 failures; added
tests are not counted as progress:

- References/conversions: `300-bad-ref1`, `300-bad-ref2`, `300-bad-ref3`,
  `300-uninit-ref`, `450-reference`, `450-cv-dropping-reference-bad`,
  `450-lvalue-to-rvalue-reference-bad`, `700-reference-to-reference`.
- Arrays/cv/static assertions: `310-array-str-lit`, `340-array-const`,
  `500-static-assert`, `500-static-assert3`,
  `300-cv-through-typedef-constant`.
- Namespace/linkage validation: `400-namespace-alias-misuse`,
  `400-namespace-alias-to-self`, `410-namespace-conflict1`,
  `410-namespace-conflict2`, `410-namespace-conflict3`,
  `410-namespace-conflict4`, `410-namespace-conflict5`,
  `410-namespace-conflict6`, and `300-function-typedef-definition-bad`.
- Qualified/cross-TU pointer work: `600-qualified-redeclaration`,
  `600-qualified-redeclaration2`, `120-constexpr-pointer-cross-tu`, and
  `120-constexpr-qualified-pointer`.

The prior through-PA7 gate was 339/339.  The file audit passed with its one
existing `bad-division` warning in `dev/src/cpp_semantic_core.h`.  No added
test is treated as progress.

Final validation for this checkpoint is PA8 **54/72**, with coverage unchanged
at 72 and exactly these 18 remaining failures:

- References/conversions: `300-bad-ref1`, `300-bad-ref2`, `300-bad-ref3`,
  `300-uninit-ref`, `450-reference`, `450-cv-dropping-reference-bad`,
  `450-lvalue-to-rvalue-reference-bad`, `700-reference-to-reference`.
- Arrays/cv/static assertions: `310-array-str-lit`, `340-array-const`,
  `500-static-assert`, `500-static-assert3`,
  `300-cv-through-typedef-constant`.
- Qualified/cross-TU work: `600-qualified-redeclaration`,
  `600-qualified-redeclaration2`, `120-constexpr-pointer-cross-tu`,
  `120-constexpr-qualified-pointer`.
- Function typedef: `300-function-typedef-definition-bad`.

The exact through-PA8 report is **393/411**, with the same 18 PA8 failures;
the through-PA7 result remains 339/339.  The PA8 file audit passed with the
same one existing `bad-division` warning.

## Active Checkpoint

`on_namespace_begin` and `on_namespace_alias` reach the shared typed
`namespace_name_conflicts` helper before `named_namespace` and
`declare_namespace_alias` mutate canonical state.  The helper probes current-TU
`(NameId, TU)` source entity, type-alias, using-entity, using-type, and
namespace-alias indexes.  Definitions allow the canonical `named_children`
namespace to be reopened, but reject a current-TU alias/entity/type/using
occupant.  Aliases reject a current-TU directly declared namespace or any
entity/type/using occupant, while preserving PA7’s repeated namespace-alias
source-slot behavior.  Thus current-TU source occupancy is separate from
program-wide namespace identity and cross-TU reopening remains valid.

## Performance Evidence

Each validation is a fixed set of expected-O(1) probes into five typed
open-addressed `(NameId, TU)` indexes, plus the existing `NameId` namespace
index for alias-vs-direct-namespace checking.  There are no spelling
render/reparse operations, whole-scope scans, or new caches; total work is
ordinary O(n) expected over declarations.  No new timing measurement was run
for this first focused milestone; the prior checkpoint’s scale probe below
remains the only timing evidence, and this bounded-probe change adds no
material complexity/cache risk.

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

## Checkpoint Ledger

| checkpoint | result | evidence |
| --- | --- | --- |
| `checkpointAudit` at `8ee86ae7` | completed prior bounded audit/repair milestone | turn-start PA8 46/72; through-PA7 339/339 and file audit passed with one existing warning |
| `namespaceNameOccupancy` | completed; committed after broad validation | PA8 54/72 with all eight target identities removed and no regression; coverage 72; through-PA7 339/339; file audit passed with one existing warning; through-PA8 393/411 with the same 18 deferred failures; no fresh timing claim |
