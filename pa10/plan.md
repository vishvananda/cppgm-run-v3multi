# PA10 Checkpoint Plan and Evidence

## Stage Design

The production data flow is:

```text
phase-3 source buffer -> typed posttoken stream and one-pass indexes
    -> PA10Parser expression seed -> canonical postfix-suffix consumer
    -> typed PA10Ast -> cold deterministic renderer
```

`PA10Parser` is the canonical AST owner.  `PA10ParserSupport` owns typed token
classification and indexed delimiter/template facts; the new
`rshift_piece1_nested_close` fact distinguishes nested template closes from a
shift pair without retrying or rescanning.  `parse_postfix_expression_seed`
owns ordinary primaries, `typeid` traits, and builtin function-style casts;
`parse_postfix_suffixes` is the one owner for call, member, subscript, and
postfix-increment/decrement suffixes.  Builtin keyword callees are
`IdExpression` nodes whose semantic identity is the fixed `SimpleTokenType`
and whose source spelling is a cold `token_spelling`; they do not use the
generic text field.  The renderer renders that typed seed but never reparses
text.  This keeps one parser/AST/renderer path, typed fact continuity, and the
bounded forward design required by root `spec.md` Purpose and §§1-4 and §7.

## Failure Map

Turn-start authoritative baseline: **158 PA10 tests discovered, 136 passed,
22 failed**; through PA9 was **457/457**.  The 22 exact failures are grouped
by the current investigation owner:

Expression seed/postfix-suffix owner — selected checkpoint (5):

- `pa10/tests/general/200-builtin-function-style-cast-expression.t`
- `pa10/tests/general/200-builtin-function-style-cast-member-body.t`
- `pa10/tests/general/200-conditional-simple-type-shift-return.t`
- `pa10/tests/general/200-template-conditional-simple-type-shift-return.t`
- `pa10/tests/general/200-typeid-postfix-member-suffix.t`

Qualified-name, template-angle, and type-id disambiguation owner (9):

- `pa10/tests/general/200-elaborated-enum-member-declarators.t`
- `pa10/tests/general/200-friend-function-template-declaration.t`
- `pa10/tests/general/200-friend-type-declaration.t`
- `pa10/tests/general/200-member-template-parameter-value-vs-template-name.t`
- `pa10/tests/general/200-mock-type-declaration-ambiguity.t`
- `pa10/tests/general/200-qualified-enumerator-call-argument.t`
- `pa10/tests/general/200-sizeof-elaborated-class-type-id.t`
- `pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t`
- `pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t`

Declaration/declarator and parameter-suffix owner (4):

- `pa10/tests/general/200-global-struct-paren-declaration.t`
- `pa10/tests/general/200-local-typedef-paren-declaration.t`
- `pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t`
- `pa10/tests/general/200-trailing-parameter-vendor-attribute.t`

New-expression placement/initializer owner (3):

- `pa10/tests/general/200-parenthesized-new-type-vs-placement.t`
- `pa10/tests/general/200-placement-new-identifier-led-initializer.t`
- `pa10/tests/general/200-placement-new-pack-init.t`

Lambda capture owner (1):

- `pa10/tests/general/200-lambda-capture-forms.t`

The grouping is an investigation boundary, not a claim that residual cases
share one semantic cause.  Placement-new, lambda, declaration ambiguity, and
the remaining residual families stay outside this checkpoint.

## Active Checkpoint

Scope is the typed expression-seed/postfix-suffix boundary only.  The parser
now represents `bool(true)` and `unsigned(~0)` as the checked structural form
`call-expression(id-expression, paren-argument-list)`, keeps `typeid(int)` as
the typed trait seed, and applies the same suffix loop to `typeid(int).name()[0]`
and ordinary primaries.  The existing shift/template index now rejects a
single-angle `>>` shift as a template close while preserving nested template
close pairs.

Expected reduction was the five selected baseline identities, with no coverage
reduction.  Final focused validation used 24 checked-in PA10 tests:
the five selected failures plus passing call, member/subscript, type-trait,
C-style/keyword-cast, RShift, and nested-template siblings; it passed **24/24**.
No handout test, reference, or new regression fixture was added.  The current
root `make test-pa10` result is **141/158** (exit 2 because 17 residual cases
remain), with all 158 tests still discovered.  The current residual set is
exactly the turn-start 22-item set minus the five selected identities, with no
new identity:

```text
pa10/tests/general/200-elaborated-enum-member-declarators.t
pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t
pa10/tests/general/200-friend-function-template-declaration.t
pa10/tests/general/200-friend-type-declaration.t
pa10/tests/general/200-global-struct-paren-declaration.t
pa10/tests/general/200-lambda-capture-forms.t
pa10/tests/general/200-local-typedef-paren-declaration.t
pa10/tests/general/200-member-template-parameter-value-vs-template-name.t
pa10/tests/general/200-mock-type-declaration-ambiguity.t
pa10/tests/general/200-parenthesized-new-type-vs-placement.t
pa10/tests/general/200-placement-new-identifier-led-initializer.t
pa10/tests/general/200-placement-new-pack-init.t
pa10/tests/general/200-qualified-enumerator-call-argument.t
pa10/tests/general/200-sizeof-elaborated-class-type-id.t
pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t
pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t
pa10/tests/general/200-trailing-parameter-vendor-attribute.t
```

The exact prior gate passed through PA9 at **457/457**.  File audit passed with
one pre-existing warning at `dev/src/cpp_semantic_core.h:1`; `git diff --check`
passed.  This checkpoint is landed in the coherent commit
`PA10: unify typed expression postfix parsing`, containing only the five
listed implementation/plan files.

Risks are limited to builtin type-keyword-plus-parenthesis recognition,
renderer presentation of a fixed-keyword synthetic callee, and the indexed
RShift ambiguity fact.  The implementation does not enter placement-new,
lambda capture, or declaration parsing.

## Performance Evidence

The index builder remains one monotonic O(n) pass and adds one byte-sized
per-token side-index entry (one byte per token).  Seed selection uses constant
lookahead; suffix consumption is one monotonic O(number of suffixes) loop.
There is no text
reparsing, speculative retry, duplicate suffix parser, or new recursion path;
the existing parser work and nesting/recursion bounds remain in force.

The final-source focused executable has SHA-256
`aacb1b09e5cc16eeadd8e2b5a8ade09e52ea79f77bb695cf11d4cd3e3ece1162`.
Characterization on that executable, 32 repeated invocations per input in the
same environment, was:

| input | elapsed | user | sys | peak RSS |
| --- | ---: | ---: | ---: | ---: |
| `200-typeid-postfix-member-suffix.t` | 0.08 s | 0.04 s | 0.04 s | 4408 KB |
| `200-conditional-simple-type-shift-return.t` | 0.08 s | 0.03 s | 0.04 s | 4424 KB |

These are single-executable characterization measurements dominated by
process launch, not comparative performance claims.

## Checkpoint Ledger

| checkpoint | status | compact evidence / state |
| --- | --- | --- |
| `a2b82dcb` template/angle ownership | landed historical | typed template components/sidecars and bounded close ownership; 106/157 historical baseline |
| `27623d64` declarator/member boundary | landed historical | unified declarator/member path and bounded declarator shape |
| `b9b58b9c` declarator audit | landed historical | 157 discovered, 123/157 passed, exact 34-failure historical baseline; through PA9 457/457 |
| `08c38115` structured names/special members | landed historical | removed 12 prior residuals; course fixture added; retained current residual families |
| `017eb658` structured-name audit | starting HEAD | clean turn-start at 158/136 with 22 failures and one pre-existing audit warning |
| typed expression-seed/postfix checkpoint | landed/committed | current 141/158 with exact baseline-minus-five residuals; focused 24/24; through PA9 457/457; audit passed with one pre-existing warning; committed under message `PA10: unify typed expression postfix parsing` |
