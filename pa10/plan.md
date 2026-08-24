# PA10 Checkpoint Plan and Evidence

## Spec alignment and ownership

```text
typed phase-7 tokens
    -> PA10Token collection with contextual AttributeIntroducer facts
    -> support-owned constant-time attribute-start query
       (indexed true [[...]] wrapper) and bounded balanced attribute scan
    -> parameter-declaration routing: declarator/abstract/pack,
       trailing attributes, then optional default argument
    -> canonical PA10 AST
    -> unchanged deterministic renderer
```

PA10 remains a syntax-only AST stage.  The active grammar boundary is
`declarator` -> `direct-declarator` -> `declarator-suffix` and
`parameter-clause` -> `parameter-declaration`; `pa10.gram` wins for accepted
syntax.  The token collector owns typed contextual classification.  The
support layer owns `attribute_specifier_start` and
`skip_attribute_specifiers`, including indexed standard-wrapper recognition,
balanced delimiter traversal, and failure accounting.  The parser owns only
contextual parameter routing and the canonical AST path.  A lambda-leading
array bound is kept in that path when the indexed facts show it is not a true
standard wrapper; the existing empty-capture lambda parser preserves its
structured node.  Attributes are intentionally omitted from the AST because
the checked references omit them.

This satisfies `spec.md` §§1, 2, 4, and 7: one forward pipeline, typed fact
continuity, one owner for the attribute boundary, bounded work, and no source
text downgrade, reparse, trial AST, backtracking, duplicate parser, or
unsupported semantic attribute interpretation.  The new declarator stop flag
is passed only at the parameter root; all other declarator callers retain their
prior behavior.  Review rejected a context-specific nonempty-capture parser
expansion; the final lambda parser and array-bound expression calls are
unchanged.

## Exact failure map and coverage

The authoritative turn-start evidence supplied for this checkpoint was
**154/159 passing; 159 discovered**, with exactly these five residuals:

- `pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t`
- `pa10/tests/general/200-lambda-capture-forms.t`
- `pa10/tests/general/200-member-template-parameter-value-vs-template-name.t`
- `pa10/tests/general/200-qualified-enumerator-call-argument.t`
- `pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t`

The two resolved checkpoint identities are:

- `pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t`
- `pa10/tests/general/200-trailing-parameter-vendor-attribute.t`

The exact progress rule is unchanged: retain all 159 discovered tests, keep
both resolved identities at their checked-in success status and exact AST
bytes, and introduce no new or replaced failure identity.  Prior-through-PA9
evidence remains the recorded `457/457`.  Fresh broad results are PA10
`154/159`, `159 discovered`, with exactly the five listed residual identities;
the through-PA9 report exits 0 at `457/457`.

## Active path checks

- Named, no-declarator, abstract pointer/reference, and `OP_DOTS` pack
  parameters route attributes after the complete optional declarator shape.
- `[3]` remains an array suffix; function, array, nested-function, and
  nested-array suffixes finish before the parameter-root stop.
- Repeated GNU/standard attributes are consumed before default arguments.
- Malformed and truncated delimiters fail closed; the support scanner publishes
  `after` and `consumed` on both success and failure, including a present
  failing token.
- Indexed close facts distinguish a true standard `[[...]]` wrapper from the
  empty-capture lambda-leading array-bound shape `int x[[](){...}]`; the latter
  emits `ArraySuffix` with the existing `LambdaExpression`.  Nonempty lambda
  captures remain in the named residual family and are not expanded here.
  Malformed/truncated candidates still fail closed.

## Focused evidence

```text
make -C dev cppgm++
  exit 0

make -C pa10 check TEST='tests/general/200-trailing-parameter-carries-dependency-attribute.t tests/general/200-trailing-parameter-vendor-attribute.t tests/general/200-attributed-partial-specialization-current-class-constructor.t tests/general/200-attributed-virtual-destructor-member.t tests/general/200-template-qualified-inline-attribute-constructor-definition.t tests/general/200-malformed-function-parameter-list.t tests/spec/100-array-declarator.t tests/general/100-function-pointer-typedef-parameter.t tests/general/200-member-pointer-function-declarator.t'
  exit 0; pa10 check: PASS (9/9)

/tmp paired implementation-only probes
  named/no-declarator/abstract: exact AST match after attribute stripping
  pack/array/function/nested/default: exact AST match after stripping

/tmp/pa10_attribute_cases_array.t
  exit 0; lambda_array parameter contains ArraySuffix -> LambdaExpression
  -> LambdaIntroducer []

/tmp/pa10_attribute_support_harness
  PASS: true-wrapper versus array+empty-lambda recognition, nested/repeated valid
  scans, GNU/alignas, reset/sentinels, malformed/truncated failure, and exact
  after/consumed accounting
```

No reference binary or host compiler was used to produce required compiler
output.  The file audit exits 0 with the known pre-existing
`[warning][bad-division] dev/src/cpp_semantic_core.h:1` warning; no fatal size
finding remains.

## Structural performance evidence

The start query is constant-size typed-token plus indexed-close lookahead.
Each attribute scan uses one forward cursor and a delimiter stack bounded by
the token vector; malformed input stops at the first failing/end token.  The
parser charges published scanner positions against its existing
`96 * token_count + 2048` ceiling.  No timing, RSS, or aggregate-O(n) claim is
made; the evidence is a structural bound and accounting harness only.  The
lambda parser and array-bound expression calls remain unchanged.

## Next checkpoint

The next checkpoint is the separately assigned residual-family audit.  Do not
expand the named nonempty-capture family in this checkpoint.

## Checkpoint ledger

| checkpoint | status | compact evidence/state |
| --- | --- | --- |
| `a2b82dcb` template/angle ownership | landed historical | typed components/sidecars and bounded close ownership; historical 106/157 |
| `27623d64` declarator/member boundary | landed historical | unified declarator/member path and bounded shape |
| `b9b58b9c` declarator audit | landed historical | historical 123/157; through-PA9 457/457; one pre-existing audit warning |
| `08c38115` structured names/special members | landed historical | removed 12 prior residuals; retained course boundary fixture |
| `017eb658` structured-name audit | historical starting point | clean at 158/136 with 22 failures |
| `25f784873f2a852fd825316b2188d9f157f8eae5` typed postfix checkpoint | landed historical | historical 142/159 with exact original 17 failures; prior focused postfix evidence |
| `d24f8e1689130b0449e19654ffd9e9f3dfc3b853` structured new expressions | landed historical | bounded indexed abstract-declarator correction; final gates retained in history |
| `c16e04ef82e93bb0c628d2f495cc7132d47dd749` elaborated-type boundary | completed historical; residuals remain | focused 3/3 + 16/16; historical PA10 148/159 with exact 11 residuals |
| `90e9687c759a39d9b4844cdc01deed3ab1e80250` declaration/declarator ambiguity | completed historical; seven residuals remain | focused 21/21; PA10 152/159 with exact seven identities; through-PA9 457/457 |
| `87f0b94bc50c0f3658c94d1dbb9215ace5296140` trailing parameter attributes | completed landed increment; audit current with bounded correction | focused 9/9, exact pairs, empty-capture lambda AST, and expanded wrapper/accounting harness; lambda parser unchanged; PA10 154/159 with exact five residuals; through-PA9 457/457; file audit exit 0 with one pre-existing `bad-division` warning |
