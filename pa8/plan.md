# Stage Design

PA8 uses one typed production path. `CppSyntaxDeclaratorOp` carries decoded
bounds and declarator layers into canonical `TypeId` formation. `SemanticCore`
owns O(1) canonical type identity, pointer cv representation, linkage buckets,
and typed entity records. PA8 owns declaration-context scope, value category,
cv-aware initialization, typed constant/relocation facts, string-literal
storage, and deterministic image planning. Known bytes (`PA8Value::constant`)
are distinct from `is_constant_expression`; no fact is rendered, reparsed,
duplicated, or selected by test spelling.

# Spec Alignment

The checkpoint follows N3485 8.5.2 for character-array completion, encoding
compatibility, and per-element initialization; 5.19 and 7.1.5 for the
distinction between known initializer bytes and constant-expression truth;
3.4.3p3 for qualified declarator scope during bounds and initializer lookup;
3.5 for declaration-owner linkage and redeclaration merging; and the
function-definition restriction on typedef-name function types.

# Failure Map

Turn-start baseline at HEAD `97a0f724`: `make test-pa8` covered exactly 94
cases and passed **85/94**, with exactly these nine failing identities. The
94-case coverage is an invariant: progress requires strictly fewer failures
with the same or greater coverage; handout tests and `.ref` fixtures remain
unchanged. Final `make test-pa8` is **94/94**, so the complete map has zero
remaining failures.

- **fixed** `pa8/tests/310-array-str-lit.t.1` — typed compatible string-array
  completion and per-element conversion.
- **fixed** `pa8/tests/340-array-const.t.1` — cv through typedef-mediated array.
- **fixed** `pa8/tests/500-static-assert.t.1` — known bytes are not expression
  truth.
- **fixed** `pa8/tests/500-static-assert3.t.1` — pointer relocation is a
  distinct constant expression.
- **fixed** `pa8/tests/600-qualified-redeclaration.t.1` and `.t.2` — qualified
  scope/linkage and cross-TU merge.
- **fixed** `cppgm.tests/course/pa8/120-constexpr-pointer-cross-tu.t.1/.t.2`
  — qualified pointer relocation across translation units.
- **fixed** `cppgm.tests/course/pa8/120-constexpr-qualified-pointer.t.1` —
  qualified definition and pointer cv identity.
- **fixed** `cppgm.tests/course/pa8/300-function-typedef-definition-bad.t.1`
  — typed function-typedef definition provenance.

# Active Checkpoint

String literals are typed arrays of const code units with their own
`StringLiteral` entity and block-3 image order. Array initialization completes
unknown bounds before layout, checks encoding-compatible element types, and
converts one element at a time. Top-level constexpr cv is applied after the
declarator, preserving pointer cv canonicality. `EntityRecord` and `PA8Value`
carry separate known-byte, constant-expression, and relocation facts, so
ordinary known bytes do not satisfy `static_assert`, while qualified and
cross-TU constexpr pointer addresses do. Qualified definitions accept only
`is_enclosing_namespace(target, current_)`; compatible array bounds inherit the
same-TU declaration owner's linkage while functions retain exact signature
identity. Incomplete array redeclarations merge typed bounds; named function
typedef provenance rejects `F f {}`. Variable initializer publication is a
separate helper to keep the audited declaration owner bounded.

# Performance Evidence

No timing claim is made. The bounded supervisor probes exercised four namespace
directions and three array-linkage cases: global→A 0, A→A 0,
descendant-B→A 1, child→global 1, incomplete→unknown 0,
incomplete→`[2]` 0, and mismatched known bounds 1; the checked non-enclosing
case was 1/1. String construction appends one entity and array conversion is
bounded by `source.element_count`, with each element sliced using canonical
element layout. Array compatibility walks only parallel array-layer chains;
linkage walks indexed bucket candidates; image planning is three linear entity
passes plus one linear relocation pass. No whole-arena scan or retry loop was
introduced.

# Next Checkpoint

PA8 is complete at this boundary with the verified 94-case coverage and clean
repository gates. Future semantic work starts at PA9.

# Checkpoint Ledger

| checkpoint | result | evidence |
| --- | --- | --- |
| `pa8-baseline` at `97a0f724` | 85/94; 9 failures; exactly 94 cases covered | supervisor-provided `make test-pa8`; earlier-through-PA7 and file audit passed |
| `typed-pa8-semantic-boundary` | affected 14/14; nearby 22/22; checked non-enclosing 1/1 | focused `make -C pa8 check` groups; `make -C pa8` exit 0 |
| `qualified-containment-and-array-linkage` | all bounded edge probes matched; no fixtures changed | direct `dev/nsinit` probes: valid 0, invalid 1 as listed in Performance Evidence |
| `pa8-final-gates` | PA8 94/94; through PA7 339/339; through PA8 433/433; audit exit 0; diff check exit 0 | `make test-pa8`; required `n=8` command; `perl scripts/cppgm_file_audit.pl --stage pa8 --paths dev/src`; `make test-report-through-pa8`; `git diff --check` |
