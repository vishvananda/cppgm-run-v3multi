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

The final design follows N3485 8.5.2 for character-array completion, encoding
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
unchanged. The final through-PA8 report covers all **94/94** PA8 cases, so
the complete map has zero remaining failures.

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

# Final Status

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

The final required gates passed: `perl scripts/cppgm_file_audit.pl --stage
pa8 --paths dev/src` exit 0; `make test-report-through-pa8` exit 0 with
433/433 and 8/8 stages; and `git diff --check` exit 0. The one
`cpp_semantic_core.h` audit warning is non-blocking organizational debt at
this boundary: it is one shared production implementation, with no duplicate
model or path. No correctness, architecture, performance, self-containment,
timeout, or file-audit blocker remains.

# Performance Evidence

The string-array conversion reuses one typed scalar view and one conversion
buffer; each element copies only its canonical element slice. Three interleaved
runs of immutable before/after `nsinit` binaries measured median wall time
from 0.18s to 0.01s at 65,536 code units and from 2.65s to 0.05s at 262,144,
with identical deterministic image hashes. The 4,096- and 16,384-unit cases
were below the timer's 0.01s resolution. This is process wall/user/RSS
evidence, not a formal asymptotic proof or phase/allocation counter.

The bounded probes exercised four namespace directions and three
array-linkage cases: global→A 0, A→A 0, descendant-B→A 1, child→global 1,
incomplete→unknown 0, incomplete→`[2]` 0, and mismatched known bounds 1; the
checked non-enclosing case was 1/1. Array compatibility walks only parallel
array-layer chains; linkage walks indexed bucket candidates; image planning is
three linear entity passes plus one linear relocation pass. An external
alias-deep layout probe through 16,384 canonical array layers remained
successful and approximately linear; no adjacent scaling repair was
indicated, and no profile was required because this curve and the repaired
string-array curve have no remaining unexplained cost. The process-level
measurements are not a formal complexity proof.

# Checkpoint Ledger

| checkpoint | result | evidence |
| --- | --- | --- |
| `pa8-baseline` at `97a0f724` | 85/94; 9 failures; exactly 94 cases covered | supervisor-provided `make test-pa8`; earlier-through-PA7 and file audit passed |
| `typed-pa8-semantic-boundary` | affected 14/14; nearby 22/22; checked non-enclosing 1/1 | focused `make -C pa8 check` groups; `make -C pa8` exit 0 |
| `qualified-containment-and-array-linkage` | all bounded edge probes matched; no fixtures changed | direct `dev/nsinit` probes: valid 0, invalid 1 as listed in Performance Evidence |
| `pa8-final-gates` | PA8 94/94; through PA7 339/339; through PA8 433/433; audit exit 0; diff check exit 0 | final worker commands listed in Final Status |
| `final-worker-checkpoint` | linear string-array conversion repair, scalar value-category correction, focused 13/13, broad gates passed, and final audit complete | `dev/src/pa8_semantic.cpp`, `pa8/plan.md`, and `pa8/audit.md` |
