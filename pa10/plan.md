# PA10 Checkpoint Plan and Evidence

## Stage Design

```text
typed phase-7 tokens
    -> support-owned constant-time attribute-start predicate/query over typed
       tokens, plus bounded balanced scan
    -> parameter-declaration routing: optional declarator/pack, attribute scan,
       optional default argument
    -> canonical PA10 AST
    -> deterministic renderer
```

The token collector classifies GNU attribute introducers with the existing
typed contextual-identifier classification.  The support-owned
`attribute_specifier_start` predicate/query is constant-time over those typed
tokens and reuses that classifier.  `skip_attribute_specifiers` owns the
complete bounded attribute recognition and delimiter scan.  The parser's one
wrapper charges the published scan count and advances only after a valid scan;
attributes are
intentionally absent from the canonical AST because the checked refs omit
them.  The renderer is unchanged.

`parse_parameter_declaration` routes the scanner after every applicable
optional declarator form, including an abstract/no-declarator form and the
existing pack form, and before default-argument parsing.  Its declarator call
has one parameter-only boundary flag: the declarator suffix loop stops before
an attribute-start predicate matches rather than misreading `[[` as an array
suffix.  All other declarator callers retain their prior behavior.  No
source-text downgrade, reparse, trial AST, or backtracking was added.

This aligns the PA10 grammar boundary with `spec.md` §1's single forward
pipeline, §2's typed-fact continuity and single owner, §4's bounded work, and
§7's conformance/measurement requirements.  It remains syntax-only; no name
lookup or semantic attribute interpretation is introduced.

## Failure Map

The authoritative turn-start result was **152/159 passing; 159 discovered**.
The final PA10 result is **154/159 passing; 159 discovered**, with exactly the
five residual identities below.  Prior through PA9 is **457/457**.  The two
checkpoint targets are resolved in the final result; the exact turn-start
identity map is:

- **resolved checkpoint target:**
  `pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t`
- **resolved checkpoint target:**
  `pa10/tests/general/200-trailing-parameter-vendor-attribute.t`
- **residual:** `pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t`
- **residual:** `pa10/tests/general/200-lambda-capture-forms.t`
- **residual:** `pa10/tests/general/200-member-template-parameter-value-vs-template-name.t`
- **residual:** `pa10/tests/general/200-qualified-enumerator-call-argument.t`
- **residual:** `pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t`

The focused matrix was diagnostic evidence only; the final broad count is
authoritative and no added test was used to inflate it.  The five residual
identities remain untouched.

## Active Checkpoint

- **Invariant:** after the complete optional parameter declarator, one
  support-owned scanner consumes zero or more valid attributes before `=` or
  the parameter-list delimiter; malformed recognition fails closed and all
  scanned positions are charged.
- **Implementation boundary:** `dev/src/pa10_ast.cpp` adds the parameter-only
  declarator stop and boundary routing; `dev/src/pa10_parser_support.{h,cpp}`
  exposes only the shared typed start predicate/query.  No AST or renderer
  attribute node is introduced.
- **Uncertainty/risk:** `[[` conflicts lexically with the existing array-suffix
  loop, so the stop is deliberately scoped to parameter declarators.  GNU
  special-member attribute paths remain covered by the focused sibling checks;
  the unrelated named-pack/lambda residual is not expanded here.
- **Exact progress rule:** the two named target identities must match their
  checked-in success refs exactly, 159 tests must remain discoverable, and no
  new or replaced failure identity may appear.  Final evidence satisfies this
  rule: both targets pass broadly and only the five listed residuals fail.

## Performance Evidence

The support scanner advances a single cursor through each contiguous
attribute sequence and delegates nested delimiters to the existing balanced
scan.  Its `consumed` result includes a present failing token and is published
on success and failure.  The parser charges every published position, while
the start query is a constant-size typed-token check; malformed delimiters
remain bounded by the token vector.  The parser's existing ceiling remains
`96 * token_count + 2048`, with recursion and nesting limits unchanged.

Focused exact check: `make -C pa10 check TEST='...'` ran 9 cases and passed
9/9, including both targets, default arguments, malformed parameters, and
GNU/standard attributed special-member siblings.  A temporary uncommitted
paired probe compared exact AST bytes for named, unnamed/no-declarator,
abstract, pack, array-then-attribute, and default-after-attribute forms; all
six pairs were identical.  Malformed GNU and malformed `[[...]]` probes both
exited failure, with the GNU case reporting `unterminated attribute`.
Final gates were: `make test-pa10` exit 2 with 154/159 and exactly the five
residuals; the through-PA9 report exit 0 with 457/457; the PA10 file audit
exit 0 with one known pre-existing `bad-division` warning; and
`git diff --check` exit 0.  No timing or aggregate-complexity claim is made;
this is structural bounded scan/counter evidence only.

## Checkpoint Ledger

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
| current trailing-parameter attribute boundary | **completed in this commit** | based at audit commit `7a35c9a9`; shared support predicate/query plus parameter routing; focused 9/9; final PA10 154/159 with exactly five residuals; through-PA9 457/457; audit exit 0 with one known warning; diff check exit 0 |
