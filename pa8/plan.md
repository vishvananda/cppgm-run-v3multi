# Stage Design

The production path is one typed pipeline: preprocessing produces one
`PPTokenBuffer` per TU, `cpp_declaration_syntax` publishes typed actions,
and `CppSemantic::SemanticCore` is the single owner of NameId, TypeId,
NamespaceId, EntityId, canonical TypeKey records, namespace/entity lookup,
linkage-independent declaration identity, and compact flat indexes. PA7 is an
action/rendering adapter over that core; PA8 is an adapter that adds
expressions, value facts, linkage, layout, relocation, and image facts.
Qualified names retain typed component IDs. TU order and first-declaration
vectors are separate from lookup indexes. Anonymous namespaces reuse only
within the current TU; named namespaces may reopen across TUs.

This follows pa8/README.md and spec.md Purpose/§§1-4: one production
pipeline, typed fact continuity, stable compact identities, deterministic
order independent of hash iteration, and dense/flat hot storage. `nsinit`
owns only CLI, preprocessing, model invocation, and binary I/O. Valid PA8
errors return `EXIT_FAILURE`, never `EXIT_NOT_IMPLEMENTED`.

# Failure Map

Baseline was 0/60, all checked-in categories stopping at
`EXIT_NOT_IMPLEMENTED`. The current full gate is 34/60, with 26 remaining:

- References/conversions: 300-bad-ref1/2/3, 300-uninit-ref, 450-reference,
  450-cv-dropping-reference-bad, 450-lvalue-to-rvalue-reference-bad,
  700-reference-to-reference.
- Arrays/cv/static assertions: 310-array-str-lit, 340-array-const,
  500-static-assert, 500-static-assert3, 300-cv-through-typedef-constant.
- Namespace/linkage validation: 400-namespace-alias-misuse,
  400-namespace-alias-to-self, 410-namespace-conflict1 through 6,
  300-function-typedef-definition-bad.
- Qualified/cross-TU pointer work: 600-qualified-redeclaration and 2,
  120-constexpr-pointer-cross-tu, 120-constexpr-qualified-pointer.

These are later expression/reference/string/conversion/diagnostic/linkage
categories outside the first coherent slice. No fixture or reference was
changed.

# Active Checkpoint

The first typed PA8 slice remains intact: real `nsinit` production
preprocessing and posttoken/action flow, empty declarations and function
stubs, scalar zero initialization, representative fundamental literal
conversion, deterministic alignment/order, PA8 magic, and initial
storage/linkage records. The shared-core correction is complete:
`name_texts` is the sole canonical name arena; PA7 rendering and audit use
it, and the dead spelling-vector state is removed. Lookup caches are cleared
at each TU boundary. Anonymous namespace traversal is restricted to the
current TU, internal-linkage entity candidates are filtered by TU, while
named/external entities remain visible across TUs. The focused regression
floor remains 19/19 PA8, 3/3 batch, and 1/1 PA7.

# Performance Evidence

The shared core uses open-addressed flat indexes with insertion-order entry
vectors for names, canonical types, namespace members, entity buckets, and
lookup caches; it has no per-namespace node-based maps. Hot lookup and
interning are expected amortized O(1), namespace traversal/parsing O(n), and
entity/layout emission linear in first-declaration order.

The checked-in 600-level deep-namespace fixture is 18,552 bytes / 1,800
lines and emits a 2,404-byte image. Three direct runs measured about 0.01s
and 6,672--6,916 KB max RSS. For 20-run scale probes derived from that
fixture, measurements were:

| namespace levels | input bytes | elapsed / 20 | max RSS |
| ---: | ---: | ---: | ---: |
| 100 | 2,981 | 0.08s | 4,360 KB |
| 300 | 9,217 | 0.14s | 5,372 KB |
| 600 (checked-in) | 18,552 | 0.23s | 6,936 KB |

This shows no materially superlinear trend over the measured range; it is
bounded evidence, not a proof for arbitrary inputs. The file audit passed
with one existing-style warning that the header contains substantial
implementation body.

# Checkpoint Ledger

| checkpoint | result | evidence |
| --- | --- | --- |
| baseline at `186b31cc` | 0/60 | authoritative baseline log; every test returned `EXIT_NOT_IMPLEMENTED` |
| first typed PA8 slice | 19/19 focused PA8, 3/3 batch, 1/1 PA7 | checked-in focused tests |
| shared-core correction | focused slice preserved; TU probes pass | canonical `name_texts`, flat indexes, TU-cleared caches, visibility filtering |
| broad checkpoint gate | 34/60; 26 remaining | `make test-pa8`; no coverage reduction |
| prior gate | 339/339 through PA7 | exact `n=8` command |
| file audit | pass, 1 warning | `cppgm_file_audit.pl --stage pa8 --paths dev/src` |
| performance | measured linear-scale evidence | checked-in deep fixture plus 100/300/600 level 20-run probes |
