# PA10 Checkpoint Plan and Evidence

## Stage Design

```text
typed tokens -> build_indexes delimiter/template facts plus
    vector<PA10ParenthesizedGroupKind> parenthesized_group_kind
    -> declaration_start / parse_declarator / parameter routing
    -> one canonical AST path -> observation-only renderer
```

`PA10ParserSupport::build_indexes` is the sole producer of the bounded
parenthesized-group fact.  Its public `enum class
PA10ParenthesizedGroupKind : unsigned char` has named `None`,
`AbstractDeclarator`, `ParameterClause`, `NestedParameter`, and
`NamedDeclarator` values; `PA10Parser` owns the typed vector but not a second
classifier.  The reverse indexed pass classifies each parenthesized group once
using delimiter/template indexes.  `declaration_start` consumes the fact for
single-name and named/abstract pointer shapes; `parse_declarator` preserves
the nested-declarator path and `parse_parameter_declaration` enables the
root-only parameter-clause preference needed by §8.2 p7.  New-expression
routing consumes the same named enum values through its existing context.

This follows N3485 §6.8: a construct that can syntactically be a declaration
is routed as one, while the immediate post-group token rejects expression
continuations such as `->` and `++`.  The bounded parameter classifier also
rejects a literal-led nested group such as `double(3)` as a parameter
declarator, leaving it as the §6.8 paren initializer.  PA10 remains syntax-only
and retains the course mock-name convention; there is no lookup, typedef
table, source-text downgrade/reparse, retry/backtracking, renderer change, or
fixture edit.

## Failure Map

Turn-start evidence was **159 discovered, 148 passing, 11 failing**.  The
complete identities and ownership at turn start were:

```text
RESIDUAL pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t
ACTIVE   pa10/tests/general/200-friend-function-template-declaration.t
ACTIVE   pa10/tests/general/200-global-struct-paren-declaration.t
RESIDUAL pa10/tests/general/200-lambda-capture-forms.t
ACTIVE   pa10/tests/general/200-local-typedef-paren-declaration.t
RESIDUAL pa10/tests/general/200-member-template-parameter-value-vs-template-name.t
ACTIVE   pa10/tests/general/200-mock-type-declaration-ambiguity.t
RESIDUAL pa10/tests/general/200-qualified-enumerator-call-argument.t
RESIDUAL pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t
RESIDUAL pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t
RESIDUAL pa10/tests/general/200-trailing-parameter-vendor-attribute.t
```

The exact active four are the only checkpoint-owned identities.  The seven
`RESIDUAL` rows are separate families and remain out of scope.

## Active Checkpoint

The implementation extends the indexed parameter classifier for the
syntax-only mock list `(_It, _It, _It)`, so the friend declaration owns a
`parameter-clause`.  `declaration_start` scans one contiguous direct
pointer/reference spine or one indexed parenthesized declarator shape and
checks its immediate follow token.  This routes `C**** pointer{};`, `foo(x);`,
and the named pointer forms in the §6.8 matrix as declarations without
turning `foo(x)->...`, `foo(x)++`, or `foo(x,5)<<...` into declarations.

Invariants:

- the support header/API is the typed ownership boundary for the group fact;
- each source construct is consumed once by the existing parser and AST path;
- no semantic lookup, source reparse, trial AST, host/reference invocation, or
  test/ref/grammar/harness edit is introduced;
- malformed/truncated groups fail closed through sentinel indexes and the
  existing parser work, recursion, and nesting limits.

The direct spine route requires a declaration follow token, so an operator
expression such as `a & b | c ^ d` remains an expression statement; this was
checked by `pa10/tests/general/100-operators-pm.t` after the first broad run.

Uncertainties are limited to broader §6.8 follow-token combinations and
complex identifier-led parameter declarations.  Reference-led named groups
retain the prior expression-safe boundary used by the checked declaration-
statement sibling; the checkpoint specifically covers the required named
pointer forms and does not widen the seven residual families.

## Performance Evidence

No timing or comparative performance claim is made.  Structurally, index
construction is one token/delimiter pass plus one reverse parenthesized-group
pass.  Pointer-spine and bare-name scans are bounded by the current group or
the current contiguous spine; accepted scans do not recursively rescan nested
groups.  Parser routing is O(1) after indexing except for that bounded spine,
with the existing global work limit still charged.  No source text is reparsed
and no trial AST is built.

Representative focused evidence:

```text
make -C dev cppgm++
  PASS
make -C pa10 check TEST='tests/general/200-global-struct-paren-declaration.t tests/general/200-local-typedef-paren-declaration.t tests/general/200-mock-type-declaration-ambiguity.t tests/general/200-friend-function-template-declaration.t tests/spec/300-declaration-statement-ambiguity.t tests/spec/100-nested-declarator.t tests/spec/100-params.t tests/general/100-function-pointer-typedef-parameter.t tests/general/200-function-type-alias-declaration.t tests/general/200-member-pointer-function-declarator.t tests/general/200-parenthesized-new-type-vs-placement.t tests/general/200-sizeof-zero-arg-functional-cast.t tests/general/200-malformed-function-parameter-list.t tests/general/100-operators-pm.t'
  PASS (14/14)
```

The temporary N3485 §6.8 matrix `/tmp/pa10-n3485-68-probe.t` was inspected as
AST: `T(*d)(int)`, `T(e)[5]`, `T(f) = {1,2}`, and `T(*g)(double(3))` are four
`simple-declaration` nodes; `d` has a nested pointer plus parameter clause,
`e` an array suffix, `f` a braced initializer, and `g` a nested pointer plus
paren initializer containing `double(3)`.  `T(a)->m = 7`, `T(a)++`, and
`T(a,5)<<c` are three `expression-statement` nodes with assignment/member,
postfix, and shift shapes respectively.  The checked malformed boundary was
`pa10/tests/general/200-malformed-function-parameter-list.t` and remained an
expected exit failure.

```text
timeout 10s ./dev/cppgm++ --emit-ast -o /tmp/pa10-long-pointer-valid-final-gate.ast /tmp/pa10-long-pointer-valid.t
  exit 0
timeout 10s ./dev/cppgm++ --emit-ast -o /tmp/pa10-long-pointer-truncated-final-gate.ast /tmp/pa10-long-pointer-truncated.t
  exit 1 (expected malformed/truncated failure)
```

The generated probes contain 256 `*` tokens.  This is representative bounded
valid/truncated evidence, not a timing comparison.

## Broad Validation

```text
make test-pa10
  exit 2; TEST SUMMARY: 152 / 159 TESTS PASSED
n=10; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
  exit 0; ===== ALL TESTS PASSED SUCCESSFULLY! (457 / 457) =====
perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src
  exit 0; File audit passed for pa10 with 1 warning(s)
git diff --check
  exit 0
```

The exact seven residual identities after `make test-pa10` are:

```text
pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t
pa10/tests/general/200-lambda-capture-forms.t
pa10/tests/general/200-member-template-parameter-value-vs-template-name.t
pa10/tests/general/200-qualified-enumerator-call-argument.t
pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t
pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t
pa10/tests/general/200-trailing-parameter-vendor-attribute.t
```

The only audit warning is the pre-existing
`dev/src/cpp_semantic_core.h:1 [bad-division]` substantial-implementation-body
warning.  The size fatal was cleared at the 3,000-line `dev/src/pa10_ast.cpp`
limit.

## Checkpoint Ledger

| checkpoint | status | compact evidence/state |
| --- | --- | --- |
| `a2b82dcb` template/angle ownership | landed historical | typed components/sidecars and bounded close ownership; historical 106/157 |
| `27623d64` declarator/member boundary | landed historical | unified declarator/member path and bounded shape |
| `b9b58b9c` declarator audit | landed historical | historical 123/157; through-PA9 457/457; one pre-existing audit warning |
| `08c38115` structured names/special members | landed historical | removed 12 prior residuals; retained course boundary fixture |
| `017eb658` structured-name audit | historical starting point | clean at 158/136 with 22 failures |
| `25f784873f2a852fd825316b2188d9f157f8eae5` typed postfix checkpoint | landed historical | historical 142/159 with exact original 17 failures; prior focused postfix evidence |
| `d24f8e1689130b0449e19654ffd9e9f3dfc3b853` structured new expressions | landed historical | bounded indexed abstract-declarator correction; final gates and evidence retained in history |
| `c16e04ef82e93bb0c628d2f495cc7132d47dd749` elaborated-type boundary | completed; residuals remain | focused 3/3, sibling/malformed/index 16/16, PA10 148/159 with exact 11 residuals, through-PA9 457/457, file audit exit 0 with one known warning |
| declaration/declarator ambiguity checkpoint | validated implementation increment | typed group ownership corrected; focused 14/14; §6.8 AST matrix and 256-pointer valid/truncated probes pass; PA10 152/159 with exact seven residuals; through-PA9 457/457; audit passes with one known warning; diff check passes |
