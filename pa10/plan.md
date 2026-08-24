# PA10 Checkpoint Plan and Evidence

## 1. Stage Design

```text
typed phase-7 tokens
    -> indexed facts: template/rshift closes, delimiters, parenthesized groups,
       and lexical-angle top-level commas
    -> parser owners: class-member context, parameter/declarator routing,
       template-angle ownership, typename decl-specifiers, and lambda facts
       (with bounded token-shape facts supplied by PA10ParserSupport)
    -> canonical PA10 AST and typed side arenas
    -> deterministic renderer
```

This remains one production parser and one canonical AST path (§1).  Existing
PA10Name and PA10TemplateArgument facts continue to own qualified names and
template arguments (§2).  LambdaIntroducer owns a compact typed capture range
(`this`, identifier, reference-identifier, default, and pack facts); the
renderer derives cold spelling on demand.  Class context propagates through
template declarations to the in-class special-member owner, and dependent
`typename` remains a typed qualified decl-specifier.  A leading parameter
ellipsis is standalone only at the parameter separator/end; otherwise the
ordinary declarator owns both pack and identifier.

The checked course fixture convention extends the grammar's non-type template
parameter form with an anonymous built-in parameter such as `int = 0`.  The
parser preserves decoded literal data and marks that default owner with a
typed anonymous-NTP presentation form; only the renderer emits its required
`TT_LITERAL:` leaf spelling.  No source spelling, filename, lookup, or
semantic classification is used.

The existing indexed close/delimiter/parenthesis work is extended with one
byte-sized top-level-comma fact per token.  The parser uses constant-time
indexed ownership checks and one forward capture-list/declarator pass.  Pure
declaration/operator vocabulary, declaration-follow and virt-specifier tests,
new-expression type-routing facts, and the bounded member-pointer shape scan
are owned by `PA10ParserSupport`; the parser charges their published work and
retains all token consumption and AST construction.  The typed lambda
introducer scanner likewise returns capture/default/pack facts plus its exact
consumed and charged counts; the parser publishes those facts into the lambda
side arena.  All index, parser, and bounded lookahead work remains charged
under `96 * token_count + 2048` (§4), with no second parser or source reparse.

## 2. Failure Map

Turn-start authority was 154/159 PA10 tests passing, all 159 discovered, with
exactly these five failures:

- `pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t`
- `pa10/tests/general/200-lambda-capture-forms.t`
- `pa10/tests/general/200-member-template-parameter-value-vs-template-name.t`
- `pa10/tests/general/200-qualified-enumerator-call-argument.t`
- `pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t`

The reviewed uncommitted milestone was focused 15/17: the three qualified or
dependent residuals were exact-green; the forward case had only the anonymous
NTP `TT_LITERAL:0` leaf mismatch; and lambda parsing stopped at token 37.

Final validation has no remaining identities: PA10 is exact `159/159` with
all `159` tests discovered; through-PA9 is `457/457`; through-PA10 is
`616/616`.

## 3. Active Checkpoint

Scope is the complete residual family: attributed in-class special-member
definitions with dependent body names; grammar-listed lambda capture/default/
pack forms; non-type template angle ambiguity; qualified-id primary
expressions; and dependent `typename` decl-specifiers with inherited typedef
casts.

Invariants are typed fact continuity, deterministic exact rendering,
fail-closed malformed syntax, no lookup or semantic classification, no source
reparse/host or reference tool, no trial AST/rollback/parallel parser, and no
language accepted beyond `pa10.gram` except the documented checked-fixture
anonymous-NTP convention.

Validation completed with the unchanged 17-test focused matrix (`17/17`),
including all five residuals and nearby empty-lambda/lambda-declarator,
qualified/template-id, cast/type-id, and special-member-attribute checks.
Direct temporary probes for `[=,]` and `[&,]` both failed closed with rc=1.
Broad validation is exact-green: PA10 `159/159` with all `159` discovered,
through-PA9 `457/457`, through-PA10 `616/616`, and the required full
`dev/src` audit exits 0 with only the known nonfatal
`cpp_semantic_core.h:1` implementation-body warning.  `git diff --check`
also exits 0.

## 4. Performance Evidence

The new template-comma fact is written once during the existing forward index
walk: O(n) storage/work, one byte per token, and one charged constant-time
parser query per candidate.  The member-pointer support probe follows one
qualified component/template-id spine and returns one charged step per
component attempt; the new-expression routing helpers are indexed constant-
time predicates.  Lambda captures and `Args... args` are consumed once by
their owning parser boundaries; renderer work is proportional to the typed
capture range.  No new unbounded scan, source-text pass, trial parse, or
rollback was added.  This is structural bound/accounting evidence only; no
timing or RSS claim is made.

## 5. Checkpoint Ledger

| checkpoint | status | compact evidence/state |
| --- | --- | --- |
| `a2b82dcb` template/angle ownership | landed historical | typed components/sidecars and bounded close ownership; historical 106/157 |
| `27623d64` declarator/member boundary | landed historical | unified declarator/member path and bounded shape |
| `b9b58b9c` declarator audit | landed historical | historical 123/157; through-PA9 457/457; one pre-existing audit warning |
| `08c38115` structured names/special members | landed historical | removed 12 prior residuals; retained course boundary fixture |
| `017eb658` structured-name audit | historical starting point | clean at 158/136 with 22 failures |
| `25f784873f2a852fd825316b2188d9f157f8eae5` typed postfix checkpoint | landed historical | historical 142/159 with exact original 17 failures |
| `d24f8e1689130b0449e19654ffd9e9f3dfc3b853` structured new expressions | landed historical | bounded indexed abstract-declarator correction |
| `c16e04ef82e93bb0c628d2f495cc7132d47dd749` elaborated-type boundary | completed historical | focused 3/3 and 16/16; residuals retained |
| `90e9687c759a39d9b4844cdc01deed3ab1e80250` declaration/declarator ambiguity | completed historical | focused 21/21; PA10 152/159 with exact seven residuals |
| `87f0b94bc50c0f3658c94d1dbb9215ace5296140` trailing parameter attributes | landed historical | focused 9/9; PA10 154/159 with exact five residuals; audit exit 0 with one warning |
| reviewed uncommitted residual structured-syntax milestone | reviewed/superseded | focused 15/17; forward anonymous-NTP presentation mismatch and lambda token-37 boundary remained |
| final residual structured-syntax checkpoint | amended in `HEAD` as `PA10: complete residual structured syntax` | restored readable parser formatting; moved bounded token-shape ownership into `PA10ParserSupport`; focused 17/17 plus malformed-default probes; PA10 159/159; through-PA9 457/457; through-PA10 616/616; audit exit 0 with one known warning; commit hash is recorded in the final handoff |
