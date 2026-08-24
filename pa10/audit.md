# Current Checkpoint Review

This is the bounded audit of landed increment
`87f0b94bc50c0f3658c94d1dbb9215ace5296140` (`PA10: route trailing parameter
attributes`), based on parent/audit base `7a35c9a9`.  The review covers only the
attribute-start query, balanced scanner, and parameter-declarator routing in
`dev/src/pa10_parser_support.{h,cpp}` and `dev/src/pa10_ast.cpp`, plus the
documentation update.  It used `pa10/README.md`, root `spec.md` §§1, 2, 4,
and 7, the declarator/direct-declarator/declarator-suffix and
parameter-clause/parameter-declaration productions in `pa10/pa10.gram`, the
landed diff, the complete affected ownership path, both fixed tests, and
attributed/declarator sibling tests.  No test, reference, status fixture,
grammar, harness, Makefile, renderer, residual-family source, or unrelated
stage surface was changed.  The audit found one same-path ambiguity and
corrected it in the three affected `dev/src` files; this document and
`pa10/plan.md` are the only other changed files.

## Contract and ownership trace

The representative facts follow one typed forward path:

```text
phase-7/posttoken facts
    -> PA10Token collection
       (fixed token kinds, typed contextual AttributeIntroducer, cold spelling)
    -> support-owned attribute_specifier_start query
       and skip_attribute_specifiers / bounded balanced scan
    -> parameter-declaration routing
       (optional declarator or pack -> trailing attributes -> default argument)
    -> canonical PA10 AST
    -> unchanged deterministic renderer
```

The posttoken collector in `pa10_parser_support.cpp` classifies
`__attribute__` and `__attribute` once as
`PA10ContextualIdentifierKind::AttributeIntroducer`; the source spelling is
retained only as presentation data.  `attribute_start_at` recognizes the
typed contextual identifier, `KW_ALIGNAS`, or a raw standard `[[` candidate.
The support-owned `standard_attribute_wrapper_at` then uses the existing
`delimiter_close_index_` facts to require that the close owned by the second
`[` is immediately followed by the close owned by the first.  The public
`attribute_specifier_start` query exposes the final GNU/alignas or true
standard-wrapper predicate to the parameter parser without moving token
classification into the AST parser.

`skip_attribute_specifiers` owns the consuming boundary.  GNU and `alignas`
introducers require their following `(`; a standard candidate must first pass
the indexed `[[...]]` wrapper check.  A balanced but non-wrapper square group
is scanned to a bounded endpoint and rejected, so an array suffix is never
silently accepted as an attribute.  For a true wrapper, the scanner delegates
delimiter nesting to `skip_balanced_delimiters`, which tracks parentheses,
square brackets, and braces and advances one token at a time.  It publishes
both `after` and `consumed` on success and failure.  A present mismatched close
or `End` token is included in `consumed`, while an out-of-range cursor is
bounded without inventing a token.  The parser wrapper charges every
published position, fails closed on an invalid scan, and only then advances to
`after`.  Attribute contents are not semantically interpreted and no
attribute AST node is created because the checked PA10 references omit them.

The parser path aligns with the relevant grammar boundary as follows:

- A named parameter such as `int x [[...]]` parses the declaration specifier
  and identifier through the ordinary declarator path.  At the trailing `[[`,
  the parameter-only root stops before the existing array-suffix loop; the
  support scanner consumes the attributes, and default-argument parsing starts
  afterward.
- A no-declarator or abstract parameter such as `int [[...]]`, `int *
  [[...]]`, or `int && [[...]]` reaches the same scanner.  The initial
  attribute-start guard prevents construction of an empty declarator, while
  the optional pointer/reference spine remains canonical.
- The `OP_DOTS` pack branch is handled before the scanner, so `int ...
  [[...]]` retains its `ParameterPack` node and then consumes the attributes.
- A normal `[3]` suffix is not an attribute start and remains an
  `ArraySuffix`.  The empty-capture `[[ ](){ return 1; }]` spelling (written
  without whitespace as `[[](){ return 1; }]`) is also an `ArraySuffix`: the
  indexed close facts reject it as a standard wrapper, and the existing
  expression/lambda path preserves its `LambdaExpression` and `[]`
  introducer.  Function suffixes, array suffixes, and nested declarator groups
  are consumed by the existing canonical path until the complete root
  parameter declarator is reached; only then does the parameter-only boundary
  stop at trailing attributes.  The stop flag is not recursively propagated:
  nested `parse_declarator(true)` calls must finish their owned group before the
  outer parameter root can route trailing attributes.
- Attributes are skipped before, and only before, the existing default
  argument parser.  Repeated GNU/standard introducers are consumed in one
  support-owned sequence.

This is a syntax-only compatibility path for the checked attribute inputs.  It
does not add lookup, attribute semantics, source-text reparsing, trial ASTs,
backtracking, or a second parser.  All non-parameter `parse_declarator`
callers retain the default `stop_at_parameter_attributes == false` behavior.
The AST construction and renderer are unchanged, so the canonical output
remains deterministic and attribute-free.

## Findings and bounded correction

The audit found one real same-path defect in the landed increment.  The
parameter stop queried the raw lexical `[[` pair, and the generic balanced
scanner consequently treated `[ [ i ] ( ) { } ]` as one standard attribute.
The emitted AST then dropped the entire array suffix, including its lambda
expression.  PA10 is syntax-only and `pa10/README.md` requires structured
preservation for grammar-accepted syntax even when later semantic validation
could reject a non-integral bound, so this was inside the checkpoint.

The bounded correction is:

- `standard_attribute_wrapper_at` requires the delimiter-indexed close of the
  inner `[` to be immediately followed by the close of the outer `[`.  The
  public parameter-start query and the standard branch of the consuming path
  use this predicate; a balanced non-wrapper group is scanned for bounded
  accounting and then rejected.  GNU and `alignas` retain their existing
  parenthesis-owned behavior, and repeated true wrappers still scan in order.
- The parser now leaves the lambda-leading array shape in the declarator loop,
  and the existing `parse_expression()`/`parse_lambda_expression()` path
  preserves the already-supported empty-capture lambda.  No lambda parser,
  array-bound helper, capture spelling helper, or context-specific capture
  state is added.  A temporary proposal to permit nonempty captures only in
  this parameter-array context was rejected as a partial expansion of the
  named residual lambda-capture family; nonempty captures remain unmodified
  and outside this checkpoint.
- The existing canonical AST and deterministic renderer remain the only output
  path.  There is no source-text reparse, trial AST, backtracking, duplicate
  parser, or test-specific workaround.

The correction is confined to `dev/src/pa10_ast.cpp` and
`dev/src/pa10_parser_support.{h,cpp}`.  No fixture or reference change is
needed.

## Focused evidence

The implementation rebuilt successfully:

```text
make -C dev cppgm++
  exit 0
```

The focused checked-in matrix passed exact status/output comparison:

```text
make -C pa10 check TEST='tests/general/200-trailing-parameter-carries-dependency-attribute.t tests/general/200-trailing-parameter-vendor-attribute.t tests/general/200-attributed-partial-specialization-current-class-constructor.t tests/general/200-attributed-virtual-destructor-member.t tests/general/200-template-qualified-inline-attribute-constructor-definition.t tests/general/200-malformed-function-parameter-list.t tests/spec/100-array-declarator.t tests/general/100-function-pointer-typedef-parameter.t tests/general/200-member-pointer-function-declarator.t'
  exit 0; pa10 check: PASS (9/9)
```

The temporary paired source probes were outside the repository and used only
the implementation binary.  The following comparisons were exact:

```text
/home/vishvananda/work/v3multi/dev/cppgm++ --emit-ast ... /tmp/pa10_attribute_cases_first.t
/home/vishvananda/work/v3multi/dev/cppgm++ --emit-ast ... /tmp/pa10_attribute_cases_first_stripped.t
cmp -s ...
  named/no-declarator/abstract forms: exact AST match

/home/vishvananda/work/v3multi/dev/cppgm++ --emit-ast ... /tmp/pa10_attribute_cases_second.t
/home/vishvananda/work/v3multi/dev/cppgm++ --emit-ast ... /tmp/pa10_attribute_cases_second_stripped.t
cmp -s ...
  pack/array/function/nested/default forms: exact AST match
```

The probe included repeated GNU and standard attributes, `int x[3]` followed
by an attribute, function and nested function/array suffixes, and default
arguments after attributes.  The two malformed source probes exited failure.
The corrected empty-capture lambda-leading probe
`int x[[](){ return 1; }]` exited successfully and contained this canonical
path:

```text
declarator
  identifier x
  array-suffix
    lambda-expression
      lambda-introducer []
```

An observation-only support harness was compiled outside the repository and
passed:

```text
g++ -std=gnu++11 -Wall -Wextra -O0 -ffunction-sections -fdata-sections \
  -Idev/src /tmp/pa10_attribute_support_harness.cpp \
  dev/src/pa10_parser_support.cpp -Wl,--gc-sections \
  -o /tmp/pa10_attribute_support_harness
/tmp/pa10_attribute_support_harness
  pa10 attribute support harness: PASS
```

The harness asserted true standard-wrapper adjacency versus the array-plus-
empty-lambda shape, valid nested attribute arguments, GNU/alignas and
repeated scans, ordinary array-start rejection, malformed mismatch, truncated
and missing-close failures, delimiter-index reset/sentinel behavior, and exact
`after`/`consumed` values.  It did not invoke a reference binary or host
compiler to produce required compiler output.

## Structural performance and safety evidence

No timing, RSS, or aggregate-complexity claim is made.  The evidence-supported
bounds are:

| work | bound and owner |
| --- | --- |
| contextual start query | constant-size typed-token and indexed-close checks in support code |
| one attribute sequence | one forward cursor; nested delimiter stack is bounded by the token vector |
| malformed scan | stops at the first mismatched/failing/end token and publishes that work |
| parser accounting | every published scanner position is charged against the existing `96 * token_count + 2048` ceiling |
| AST/lambda path | existing expression and empty-capture lambda parser remain untouched; no context-specific capture work is added |

The scanner does not re-render source text, reparse a failed source span, build
a trial AST, or retry the whole translation unit.  The support API has one
consuming owner, and the parser has one canonical AST path.  The constant-time
start query itself is a bounded lookahead like the parser's existing fixed
token predicates; the charged scanner work is the potentially variable part.
No stronger performance claim is justified without timing and aggregate
counter measurements, which this checkpoint does not require and did not run.

## Baseline, progress rule, and residual map

The authoritative turn-start evidence supplied for this audit was:

```text
make test-pa10
  exit 2; 154/159 passing; 159 discovered
  residual identities: exactly the five listed below
```

The two identities resolved by `87f0b94b` are exactly:

```text
pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t
pa10/tests/general/200-trailing-parameter-vendor-attribute.t
```

The remaining five identities are untouched and remain the residual map:

```text
pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t
pa10/tests/general/200-lambda-capture-forms.t
pa10/tests/general/200-member-template-parameter-value-vs-template-name.t
pa10/tests/general/200-qualified-enumerator-call-argument.t
pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t
```

The exact progress rule is: retain discovery of all 159 tests; both named
checkpoint targets must match their checked-in success status and AST bytes;
and no new or replaced failure identity may appear.  A newly passing test may
not mask a new failure.  Prior-through-PA9 evidence remains the recorded
`457/457`.  Fresh broad validation produced:

```text
make test-pa10
  exit 2; 154/159 passing; 159 discovered
  failures: exactly the five residual identities listed above

n=10; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
  exit 0; ===== ALL TESTS PASSED SUCCESSFULLY! (457 / 457) =====

perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src
  exit 0; File audit passed for pa10 with 1 warning(s).
  [warning][bad-division] dev/src/cpp_semantic_core.h:1: header contains substantial implementation body; prefer .cpp ownership

git diff --check
  exit 0
```

The warning is the known pre-existing `bad-division` warning; the fatal source
size check was cleared with a formatting-only reduction to 2999 lines in
`dev/src/pa10_ast.cpp`.

## Risks and next checkpoint

The corrected delimiter-indexed wrapper check removes the observed `[[` lexical
overlap for the tested empty-capture lambda-leading array-bound shape.
Remaining uncertainty is limited to malformed or unsupported syntax around
that boundary: attribute contents are structurally skipped rather than
semantically validated, and attribute positions inside a declarator rather
than after the complete parameter root are not newly claimed.  Nonempty lambda
captures remain in the pre-existing named residual family and were not
expanded.  No performance claim beyond the structural bounds above is made.

The next checkpoint is the separately assigned residual-family audit, not a
further review of this checkpoint.  The five residual identities remain
outside this bounded correction.

## Historical checkpoint ledger

Historical rows are retained below.  The final row is the single current audit
row for `87f0b94b`.

| checkpoint | review result | owner action | validation state |
| --- | --- | --- | --- |
| `a2b82dcb` template/angle ownership | historical | retain typed template components and bounded close ownership | historical 106/157 baseline |
| `27623d64` declarator/member boundary | historical | retain unified declarator/member path and bounded shape | historical focused evidence |
| `b9b58b9c` declarator audit | historical | retain nearest-derived-operator and member-pointer bounds | historical 123/157; through-PA9 457/457 |
| `08c38115` structured names/special members | historical | retain one typed name/special-member path and validated sidecars | historical local 135/157; course 1/1 |
| `25f784873f2a852fd825316b2188d9f157f8eae5` typed postfix checkpoint | historical (audited and committed) | use one exact cast-keyword predicate, initialize indexes, route all RShift consumers through the marker, validate synthetic renderer nodes, and retain the 3000-line source bound | historical fresh 142/159 with exact original 17 failures; through-PA9 457/457; file audit exit 0 with one pre-existing warning |
| `d24f8e1689130b0449e19654ffd9e9f3dfc3b853` structured new expressions | historical (prior checkpoint audited) | retain the typed indexed abstract-group fact, distinguish definite from identifier-led nested parameter clauses under pointer/member-pointer spines, route complete abstract-declarator consumption through the canonical parser, validate inline initializer sidecars, and preserve global/placement/pack ownership | historical fresh 159/145 with exactly the original 14 residuals; through-PA9 457/457; file audit exit 0 with one pre-existing warning; focused 55/55 + exact refs 4/4 + warning/index/renderer harnesses |
| `c16e04ef82e93bb0c628d2f495cc7132d47dd749` elaborated-type boundary | historical completed; residuals remained | publish/charge malformed attribute scan work, stop colon lookahead at immediate owners, reject invalid template closes, preserve one canonical class/enum AST path and semicolon owner | historical focused 3/3 + 16/16; PA10 148/159 with exact 11 residuals; through-PA9 457/457; file audit exit 0 with one known warning |
| `90e9687c759a39d9b4844cdc01deed3ab1e80250` declaration/declarator ambiguity | completed historical; seven residuals remain | publish one typed parenthesized-group owner; preserve root-only parameter preference, contextual single-name/follower routing, and the fixture-bound reference exception; classify named member-pointer groups through the same owner | historical focused 21/21; corrected member-pointer harness PASS; PA10 152/159 with exactly the seven identities; through-PA9 457/457; file audit exit 0 with one known warning; diff check exit 0 |
| `87f0b94bc50c0f3658c94d1dbb9215ace5296140` trailing parameter attributes | audited current; bounded same-path correction made; lambda parser intentionally unchanged | require indexed true `[[...]]` wrapper recognition, preserve empty-capture lambda array suffixes, retain typed scanner/parameter-root stop, pack/abstract/default routing, and attribute-free canonical AST; leave nonempty captures to the residual-family audit | focused 9/9, exact paired AST probes, empty-capture lambda AST, and expanded wrapper/accounting harness PASS; PA10 154/159 with exactly the five residuals and 159 discovered; through-PA9 457/457; file audit exit 0 with one pre-existing `bad-division` warning; diff check exit 0 |
