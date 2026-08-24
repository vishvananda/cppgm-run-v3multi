# PA12 audit record

## Current Checkpoint Review

This bounded full-stage PA12 checkpoint audit started from clean `a859c671`
(`PA12: add shared semantic fact foundation`). The turn-start full evidence
was 85/166 passing and 81 failures; the final audited result is 90/166
passing and 76 failures. The complete 166-test inventory and prior-stage
coverage were preserved, and no test, reference, harness, grammar, or script
file changed.

Ownership trace: the driver reads each source through
`PPPreprocessingSession`, passes the typed `PPTokenBuffer` directly to
`parse_pa10_ast`, and passes the resulting `PA10Ast` to one
`PA11SemanticModel`. PA11 owns canonical `TypeId`/`BindingId`/`ScopeId`
records, lookup, and declaration facts. PA12 appends typed semantic and
conversion facts to that same model. `dump_pa12` renders those facts cold from
typed IDs, enum operators, PA10 producer identities, and source-side spelling;
no rendered text is reparsed or used as semantic identity.

The review verified interned `TypeId` equality, typed `FlatIndex` lookup
tables, deterministic lexical vectors, relevant-scope lookup, and the
candidate-by-argument O(c*a) overload-ranking path. The immutable committed
performance evidence remains the seven interleaved `/usr/bin/time` sample set
for facts-200/facts-800 and pointer-overloads-244/pointer-overloads-1025 in
`pa12/plan.md`. Source structure was rechecked for unchanged fact arenas,
indexes, relevant-scope collection, and ranking; the bounded repair adds only
a same-name scope scan for redeclaration, a canonical-binding lexical dump
view, and pointer-depth qualification decomposition.

Finding 1 was confirmed: the old qualification recursion accepted unsafe
`int** -> const int**`. The repair compares cv at every pointer level,
rejects dropped qualification, and enforces the intermediate target `const`
required by a deeper qualification. The valid deep
`int** -> const int * const *` case remains accepted, as do ordinary
pointer-to-const and cv-preserving void cases; the cv-dropping void case is
rejected.

Finding 2 was confirmed: the old `add_value` created every function binding
anew. The repair normalizes top-level parameter cv in the canonical function
type, reuses one same-scope compatible `BindingId`, rejects a same-parameter-
list return-type conflict, and records one definition state so a duplicate
definition fails. Definition-local parameter bindings retain body-visible cv.
The cold PA12 dump renders canonical function-type parameters for deterministic
declaration output.

The canonical semantic owner is retained while PA11 lexical dump behavior is
preserved: compatible redeclarations add a cold lexical dump view keyed to the
same canonical `BindingId`, so declarations and definitions retain their
required lexical rendering without creating duplicate semantic identities.

## Final Evidence and Residuals

The final PA12 result is 90/166, with complete 100-level sets (12/12 spec and
25/25 general). The remaining counts are 5/9 in `tests/spec/200-*.t`, 20/33
in `tests/general/200-*.t`, 6/10 in `tests/spec/300-*.t`, and 45/77 in
`tests/general/300-*.t`. Residuals remain in broader control-flow,
function-pointer/reference, class/constructor, enum, pointer-arithmetic,
cast, namespace, and parser families. The local-extern case still fails
before semantic analysis in the unchanged PA10 parser
(`expected primary expression at token 49`); these families were not expanded
under this checkpoint.

Exact validation:

- `make test-pa12` — 90/166 passed, 76 failures; coverage remained 166.
- `n=12; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi` — 685/685 through PA11.
- `make -C pa12` — passed; one pre-existing warning in the unchanged cast path.
- The focused repair and neighboring set — 10/10.
- The explicit qualification matrix — 5/5: valid deep qualification, invalid
  deep qualification, ordinary pointer qualification, pointer-to-const void,
  and cv-dropping void-pointer rejection.
- `tests/spec/100-*.t` plus `tests/general/100-*.t` — 37/37.
- `make -C pa11 check TEST=tests/general/200-qualified-namespace-function-definition-parameter-type.t` — 1/1, confirming the lexical dump-view compatibility repair.
- `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src` — passed
  with exactly the two known header-shape warnings for
  `cpp_semantic_core.h` and `pa11_semantic_model.h`.
- `git diff --check` — passed.

## Audit ledger

| audit row | findings and ownership trace | evidence | uncertainties / residual exclusions | exact validation |
| --- | --- | --- | --- | --- |
| 2026-08-24 PA12 current checkpoint, complete | One `PPPreprocessingSession -> PPTokenBuffer -> PA10Ast -> PA11SemanticModel -> PA12 facts -> cold dump` owner confirmed; deep qualification and canonical function redeclaration/definition state repaired; lexical PA11 dump views restored without duplicate semantic function identities. | Final PA12 90/166 with 76 residual failures; through-PA11 685/685; complete 166-test coverage; typed identity/index and complexity structure rechecked; immutable timing evidence preserved; no tests or refs changed. | Residual unsupported/broader families remain outside this bounded checkpoint; no claim of full PA12 completion. Future residual work requires separate authorization. | `make test-pa12`; exact through-PA11 gate; `make -C pa12`; focused 10/10; qualification matrix 5/5; foundational 37/37; PA11 regression 1/1; file audit passed with 2 known warnings; final diff inspection, `git diff --check`, Luna-authored commit, and clean `git status --short`. |
