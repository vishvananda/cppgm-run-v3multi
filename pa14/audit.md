# PA14 Full-Stage Architecture Audit

## Final current review

This audit covers the complete PA14 source surface at
`f156044bfa2c1e0d2b598838d84405bb4e096cdd` (`PA14: complete typed ABI name
boundary`), from PA13 parent `bd4bf655`.  The stage history is
`a9572906`, `1a05c250`, `16d775c4`, `0c654318`, `12eaf37b`, `490d1ec7`,
`623abbe4`, `3e333caa`, `bf99cd9c`, and `f156044b`.  The reviewed production
surface is `dev/abimangle.cpp`, `dev/src/abi_mangle.h`,
`dev/src/abi_mangle.cpp`, and `dev/frontend_source_sets.mk`; course additions
remain under `cppgm.tests/course/pa14`.

The final PA14 result is `100=25/25`, `200=25/25`, `300=37/37`, `400=4/4`,
`500=13/13`, and `600=7/7`, or `111/111`.  The complete root result is
`through-PA13=947/947` and `through-PA14=1058/1058`, both exit 0.  The seven
600 failures listed later are historical results from the pre-`f156044b`
checkpoint, not current failures.

The ownership path is one typed chain:

```text
fact line -> DefinitionInterner/parse_type_words
          -> AbiFactCase + dense AbiDefinitionId references
          -> FactEncoder structural identity/substitution state
          -> direct append-based Itanium spelling
```

## Representative ownership traces

- `DefinitionInterner` owns file-local dense IDs; `parse_type_words` builds
  typed `AbiType` records; `canonicalize_case` resolves references before
  `parse_fact_text` hands a case to `mangle_abi_fact_case`.  Labels are a
  diagnostic sidecar, not an encoder identity.
- Qualified type/function facts use `type_identity`, the name trie, and
  `append_named_type_name`/`append_function_name`.  Enum-owned operator and
  special-member terminals are typed enum cases.  TLS wrappers and ordinary
  and virtual thunks enter the same target dispatch and direct encoder path.
- Template specialization, entity, and member-owner facts retain typed owner,
  member, argument, and child identities in `type_identity`,
  `argument_identity`, and `entity_identity`.  Owner prefixes publish before
  operands; complete specializations publish only after operands.  Pending
  function prefixes are consumable only in the explicit function-template
  role, so owner and nested lists cannot consume that candidate accidentally.
- Dependent expressions decode into typed operator/cast/access/value fields
  and child references.  `expression_identity` includes those fields and
  `append_expression` traverses the same graph directly.  Decltype preserves
  `ABI_DECLTYPE_EXPRESSION` versus `ABI_DECLTYPE_ID_OR_MEMBER` in identity and
  emission (`DT` versus `Dt`).
- Nested external entities use a separate target collection and swap a fresh
  `SubstitutionState` around `encode_entity_symbol`, restoring it on success
  and exception.  Raw symbols and normalized raw context fragments are the
  explicit ABI text boundaries; they do not enter the qualified-name trie.
- The final 600 paths directly cover local class/lambda context spelling,
  direct versus explicit-substitution template-parameter publication,
  template-parameter-template typed parsing/serialization/identity/emission,
  owner-component constructor terminals, inline-namespace/std substitution
  ordering, and pack/reference constructor traversal.  Empty and invalid
  template-parameter-template forms are rejected at typed boundaries.

## Architecture, findings, and spec alignment

`AbiFactCase` is the production semantic model for this standalone stage, and
`FactEncoder` is the single reusable production encoder.  `StructuralKey`
carries typed domains, enums, interned source components, scalars, and child
structural IDs.  `StructuralId` equality and dense definition-vector access
are O(1).  Structural/name interning and substitution lookups are map-backed
O(log n); constructing and comparing a key is proportional to its typed
operand width.  These distinctions avoid treating map-backed interning as a
constant-time lookup.

Qualified names remain component sequences.  Rendered ABI text is never parsed
back or used as a semantic key.  Only source components, raw bounds/symbols,
raw contexts, and cold diagnostic text cross a textual boundary.  Constructor
indexing, target selection, function fact/name collection, and parameter
emission make a constant number of bounded linear passes over
`fact_case_.records`: ordinary collection is O(n), while map-backed indexing
has ordinary O(n log n) behavior for fixed-width typed operands.  Candidate
publication is left-to-right through the ABI grammar.  Active definition and
identity scopes, pending function-prefix scope, and template-depth scope are
RAII-cleaned; active-cycle checks reject recursive malformed models.

The adapter boundary is deliberately cold: parse and serialize live in
`dev/abimangle.cpp` at the standalone adapter boundary.  The reusable
production encoder is `dev/src/abi_mangle.cpp` and never calls serialization;
the mangle path goes directly from parsed typed facts to
`mangle_abi_fact_case`.  Serialization is a diagnostic/round-trip adapter, not
a production semantic owner.

There is no retry-until-stable or repeated whole-case rescan loop, rendered-key
vector, duplicate production model, reference/host compiler shell-out, or
test-specific answer.  Typed validation rejects malformed enum values and
empty template-template arguments before ABI output.  The seven final repairs
are bounded to the PA14 600 boundary; this audit found no additional source
correctness or architecture defect.  No handout test, reference, harness, or
fixture was changed.

## Final-600 changes

The final source checkpoint repaired the following independently traced gaps:

1. Local class/lambda contexts now spell the Itanium local-name context without
   an extra nested `N...E`.
2. Direct and explicit-substitution template parameters share identity while
   `TemplateArgumentEmissionScope` controls publication mode and depth.
3. Template-parameter-template arguments are typed, serialized, validated,
   identity-keyed, and emitted; empty and invalid forms fail cleanly.
4. Constructor terminals accept all owner components, including a mixed
   owner/specialization chain.
5. Inline namespace and adjacent standard substitutions retain typed ordering.
6. Pack/reference constructor operands traverse the typed owner and parameter
   records directly.

## Measurement and performance evidence

The durable scaling result uses the immutable final-600 candidate copied after
the final source build: mode `0555`, size `484664`, SHA-256
`ae56d2130e54a63fca117b624230f309bcdeaaf097b9821a9775f91b63c800d`.
Fixed-width 1024/2048/4096 fact cases were sampled seven times each in
interleaved order with `/usr/bin/time -f '%e %U %S %M'`:

| scale | fact lines | input bytes | output bytes | median wall | user | sys | max RSS |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1024 | 3,075 | 93,256 | 19,468 | 0.09 s | 0.05 s | 0.03 s | 24,724 KiB |
| 2048 | 6,147 | 186,440 | 38,924 | 0.19 s | 0.11 s | 0.08 s | 45,648 KiB |
| 4096 | 12,291 | 372,808 | 77,836 | 0.39 s | 0.21 s | 0.16 s | 87,500 KiB |

This immutable 1024/2048/4096 run is the scaling evidence.  Fixed-width input
and output growth has near-doubling wall medians; source corroboration is the
scalar template-depth counter and map-backed structural/name tables.  It is
evidence for the measured workload, not a general timing or asymptotic proof.

A secondary coarse probe repeated seven interleaved runs of the immutable
`600-function-local-class-template-arg` case at 64/128/256 cases.  Medians
were respectively input/output `8,704/1,984`, `17,408/3,968`, and
`34,816/7,936` bytes; wall `0.00/0.00/0.01 s`; max RSS
`3,832/3,884/3,848 KiB`.  Its coarse timers make it corroboration only, not
the durable scaling claim.

## Validation record

Focused checks completed serially:

```text
make -B -C dev abimangle                                      exit 0
g++ -std=c++11 -Wall -Wextra -Werror -Idev/src -fsyntax-only \
  dev/src/abi_mangle.cpp dev/abimangle.cpp                    exit 0
make -C pa14 check TEST='tests/abi/600-*.t'                   exit 0, 7/7
make -C pa14 check TEST='../cppgm.tests/course/pa14/*.t'      exit 0, 10/10
representative 100/200/300/400/500 cases                    exit 0, 14/14
CXX=${CXX:-g++} sh cppgm.tests/course/pa14/400-public-typed-model-regression.sh
                                                               exit 0
```

Final serial gates are:

```text
make test-report-through-pa14                              exit 0, 1058/1058
perl scripts/cppgm_file_audit.pl --stage pa14 --paths dev/src
                                                               exit 0
git diff --check                                             exit 0
```

The file audit reports four known nonfatal header `bad-division` warnings at
`dev/src/abi_mangle.h:1`, `dev/src/cpp_semantic_core.h:1`,
`dev/src/lowir_model.h:1`, and `dev/src/pa11_semantic_model.h:1`.  No new
warning was introduced.  The final commit and post-commit status are recorded
in the ledger below.

## Uncertainties and nonclaims

The raw external-symbol boundary emits its producer-supplied symbol and does
not cross-check it against typed owner/member facts.  Cold serialization
covers the represented normalized subset and is not the semantic transport;
this is not a claim that every manually constructed enum has a text form.
There is no PA15 compiler-source integration, object-emission, generated-code
quality, arbitrary-width key-bound, or timing claim here.  The four file-audit
warnings above are known and nonfatal.

## Historical progression and checkpoint ledger

| checkpoint | historical result | durable outcome |
|---|---:|---|
| `a9572906` / `1a05c250` foundation | initial typed boundary | introduced and audited typed records, dense IDs, one encoder, cycle/state guards |
| `16d775c4` wide values | foundation preserved | widened typed validation and rejected malformed wide values without textual fallback |
| `0c654318` / `12eaf37b` typed 200 | 100/200 coverage | added and audited typed qualifiers, terminals, contexts, TLS, and thunks |
| `490d1ec7` / `623abbe4` typed 300 | 88/111; 23 historical failures | added and audited structural substitution, value normalization, entity isolation, and owner order |
| `3e333caa` / `bf99cd9c` dependent 400/500 | 104/111; seven historical 600 failures | added and audited dependent expressions, decltype identity, serializer boundary, and malformed guards |
| `f156044b` final typed 600 | 111/111 | repaired all seven final-600 ownership paths; current full-stage result is 111/111 |

The historical seven-failure set at the `bf99cd9c` boundary was:
`600-function-local-class-template-arg`,
`600-function-template-local-class-arg`,
`600-function-template-local-lambda-arg`,
`600-inline-namespace-basic-string-param`,
`600-nested-helper-owner`,
`600-template-param-template-type-substitution`, and
`600-template-parameter-pack-reference-constructor`.  They are retained here
only to explain the final repair progression; all seven now pass.

| ledger checkpoint | result and evidence |
|---|---|
| typed-300 boundary | historical 88/111; 100–300 was preserved and through-PA13 was 947/947 |
| dependent 400/500 boundary | historical 104/111; 400/500 focused coverage was 17/17 and 100–300 was 87/87 |
| final typed-600 boundary | current PA14 111/111; final root gate exit 0 at 1058/1058; file audit exit 0 with four known warnings |
| final audit commit | exact message `PA14: finalize full-stage architecture audit`; post-commit `git status --short` is empty |

No source repair beyond the landed final-600 implementation was necessary for
this audit.  The only audit changes are the compact durable records in
`pa14/audit.md` and `pa14/plan.md`.
