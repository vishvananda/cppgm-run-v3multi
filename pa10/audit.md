# Current Checkpoint Review

This is the bounded Phase A and Phase B review of landed increment
`90e9687c759a39d9b4844cdc01deed3ab1e80250` (`PA10: classify declaration
ambiguity boundary`), whose parent is `163fa9e1`.  The review also covers one
same-path correction in this checkpoint's final working diff in
`dev/src/pa10_parser_support.cpp`: a named member-pointer spine now receives
the existing `NamedDeclarator` group fact.  The review used `pa10/README.md`,
the declaration/declarator/abstract-declarator/parameter/new-expression
productions in `pa10/pa10.gram`, root `spec.md` §§1, 2, 4, and 7, N3485 §§6.8
and 8.2, the checkpoint diff, current owned-path code, and focused checked-in
tests.  No test, reference, status fixture, grammar, harness, build file,
renderer, or residual-family source was changed.

## Contract and ownership trace

The representative facts follow one typed forward path:

```text
phase-7/posttoken typed tokens
    -> collect_tokens PA10Token stream (fixed kinds and cold spellings)
    -> build_indexes template/RShift/delimiter facts plus one typed
       PA10ParenthesizedGroupKind fact per parenthesized group
    -> declaration_start / parse_declarator / parameter and abstract-
       declarator routing
    -> one canonical PA10 AST construction path
    -> existing renderer, observation only
```

`build_indexes` resets and sizes every side index on each invocation.  Its
delimiter index uses the token-count sentinel for missing closes; its template
and split-RShift indexes remain typed side facts.  The reverse group pass
publishes `None`, `AbstractDeclarator`, `ParameterClause`,
`NestedParameter`, or `NamedDeclarator` into the single typed vector.  The
support pass is the sole producer of that group fact.  The parser owns the
vector as an index and applies bounded contextual routing around it.  In
particular, `parenthesized_declaration_start_at` has an exact
single-identifier shape check and an immediate declaration-follower check.
Those contextual decisions do not duplicate the support-owned multi-shape
group classification; there is no second implementation of that
classification in the parser.

The declaration path consumes the fact as follows:

- `declaration_start` handles fixed declaration starts and direct
  identifier-led pointer/reference/cv spines with a bounded immediate-follower
  check.  It also asks the indexed group owner about a single identifier or a
  named/abstract nested declarator.  Declaration followers are limited to the
  grammar continuations needed by this boundary (`;`, `,`, `=`, `{`, `(`, and
  `[`); `->`, `++`, and the shift-expression form are not declaration
  followers.
- `parse_declarator` preserves the canonical nested-declarator AST path.
  Only the root of a parameter declaration receives the
  `prefer_parameter_clause_at_root` preference, which is the §8.2 redundant
  parameter-parentheses boundary; recursive nested declarators do not inherit
  that preference accidentally.
- `parse_parameter_declaration` and `parse_abstract_declarator` consume the
  same parameter/group fact.  The bounded parameter helper recognizes the
  syntax-only mock list `(_It, _It, _It)` while rejecting a literal-led nested
  group such as `double(3)` as a parameter declarator.
- `parse_type_id` and the `new` path reuse the same enum through
  `new_parenthesized_abstract_declarator_start`.  Placement detection and
  parenthesized type-id parsing remain context decisions over the indexed
  group, not a second group parse.

For the active forms this matches the PA10 syntax-only AST contract and N3485
§6.8: a construct that can be parsed as a declaration is declaration-routed,
while the specified expression followers disambiguate the expression cases.
There is an explicit checked-contract exception at the reference-led group
boundary.  `T(&x);` is a syntactically valid reference declarator and general
§6.8 declaration preference would choose a declaration, but PA10 deliberately
preserves the checked `function(&spawned_thread);` expression AST because this
syntax-only stage does not perform lookup.  The correction therefore covers
raw-star and member-pointer named groups, while direct unparenthesized
reference spines remain declaration-routed.

## Findings and bounded correction

The landed ownership split is sound, but review found one concrete omission in
the owned boundary.  `parenthesized_group_kind_at` recorded a named group only
when its pointer spine contained `*`.  A member-pointer operator (`C::*`) is
also a declarator pointer operator under the grammar, so `T(C::*p);` was not
declaration-routed and could fail in expression parsing.  The correction
records a `NamedDeclarator` when the scanned spine contains a raw
`*` or a successfully indexed member-pointer operator.  It intentionally does
not broaden the established reference-led `(&name)` fixture boundary.

The correction is bounded and semantic: it changes only group ownership, not
the parser grammar, AST node shape, or renderer.  The observed repair is
`T(C::*p); -> simple-declaration` with a nested `ptr-operator C::*` and
identifier `p`; `T(C::*)();` remains an abstract nested declarator.  Direct
`T &name;` and `T &&name;` use the existing direct spine route.  The existing
§6.8 star matrix continues to classify `T(*d)(int)`, `T(e)[5]`,
`T(f) = {1,2}`, and `T(*g)(double(3))` as declarations, while `T(a)->m = 7`,
`T(a)++`, and `T(a,5)<<c` remain expression statements.

No duplicate parser or AST path, source-text downgrade/reparse, trial AST,
backtracking, semantic lookup, host/reference/previous-solution shortcut, or
unbounded retry was introduced.  The renderer was not changed; source strings
remain cold presentation spellings and the fixed/group facts remain typed.

## Phase A focused evidence

The implementation rebuilt successfully:

```text
make -C dev cppgm++
  exit 0
```

The original checkpoint matrix and sibling boundaries, expanded with member
pointer and new-expression cases, passed exact checked-in output/status
comparison:

```text
make -C pa10 check TEST='tests/general/200-global-struct-paren-declaration.t tests/general/200-local-typedef-paren-declaration.t tests/general/200-mock-type-declaration-ambiguity.t tests/general/200-friend-function-template-declaration.t tests/spec/300-declaration-statement-ambiguity.t tests/spec/100-nested-declarator.t tests/spec/100-params.t tests/general/100-function-pointer-typedef-parameter.t tests/general/200-function-type-alias-declaration.t tests/general/200-member-pointer-function-declarator.t tests/general/200-member-pointer-data-declarator.t tests/general/200-member-pointer-const-function-declarator.t tests/general/200-qualified-result-parenthesized-member-pointer-declarator.t tests/general/200-parenthesized-new-type-vs-placement.t tests/general/200-placement-new-identifier-led-initializer.t tests/general/200-placement-new-pack-init.t tests/general/200-sizeof-zero-arg-functional-cast.t tests/general/200-constructor-declaration-identifier-parameter.t tests/general/200-malformed-function-parameter-list.t tests/general/100-operators-pm.t tests/general/100-pointer-cv-qualifier.t'
  exit 0; pa10 check: PASS (21/21)
```

The checked malformed function-parameter test retained its expected failure
status.  A temporary observation-only probe at
`/tmp/pa10_phase_a_decl_ambig_probe.t` exited 0 and showed the complete matrix
above, direct pointer/reference/cv spines, the member-pointer correction, and
the expression followers in their expected AST node families.  It also showed
that `T(&x);` remains the established expression-safe reference-led boundary.

The temporary typed-index harness was compiled outside the repository and
passed:

```text
g++ -std=gnu++11 -Wall -Wextra -O0 -ffunction-sections -fdata-sections -Idev/src /tmp/pa10_group_index_harness.cpp dev/src/pa10_parser_support.cpp -Wl,--gc-sections -o /tmp/pa10_group_index_harness
/tmp/pa10_group_index_harness
  pa10 group index harness: PASS
```

It contains a representative `(C::*p)` token sequence and asserts the exact
`groups[13] == PA10ParenthesizedGroupKind::NamedDeclarator` value.  It also
checked typed group values for named raw-pointer, reference-safe, parameter,
and abstract groups; reset/reuse after a shorter token vector; missing-close
sentinels; and a truncated group.  No reference binary or generated fixture
was used.

## Structural performance and safety evidence

No timing or comparative performance claim is made.  The
structural bound is:

| work | bound/owner |
| --- | --- |
| ordinary indexes | one forward token/delimiter pass; all vectors are reset and missing facts are sentinels |
| group facts | one reverse token visit; each pointer/member-pointer or mock-name scan stops at its current delimiter owner and uses indexed template closes |
| parser routing | constant-time group-fact lookup plus the bounded contextual single-name/follower check and current-spine/group consumption |
| accounting | `build_indexes` returns its instrumented predicate/scan counter and the parser charges that returned count against `96 * token_count + 2048`; this is an accounting ceiling, not a timing or tight aggregate-overlap proof |

Malformed groups fail closed through the token-count sentinel and then follow
the existing parser failure path.  Each individual support scan is finite and
input-bounded; this review makes no stronger aggregate-O(n) claim for all
possible overlapping nested-group shapes.  No source-text scan is repeated as
a reparse, and no trial AST is constructed.  No new source file was added and
the AST file was not touched.

## Phase B broad evidence and residual map

The authoritative turn-start baseline was:

```text
make test-pa10
  exit 2; 152/159 passing, 159 discovered
```

The exact seven identities are preserved and remain outside this checkpoint:

```text
pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t
pa10/tests/general/200-lambda-capture-forms.t
pa10/tests/general/200-member-template-parameter-value-vs-template-name.t
pa10/tests/general/200-qualified-enumerator-call-argument.t
pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t
pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t
pa10/tests/general/200-trailing-parameter-vendor-attribute.t
```

Fresh Phase B results were:

```text
make test-pa10
  exit 2; TEST SUMMARY: 152 / 159 TESTS PASSED; 159 discovered

n=10; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
  exit 0; ===== ALL TESTS PASSED SUCCESSFULLY! (457 / 457) =====

perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src
  exit 0; File audit passed for pa10 with 1 warning(s).
  [warning][bad-division] dev/src/cpp_semantic_core.h:1: header contains substantial implementation body; prefer .cpp ownership

git diff --check
  exit 0; no output
```

The fresh PA10 run retained exactly the seven turn-start identities above:
every one remained an expected-`EXIT_SUCCESS`/actual-`EXIT_FAILURE` result;
there was no coverage reduction and no new or replaced failure identity.  The
through-PA9 report remains `457/457`.  The file-audit warning is the known
pre-existing `dev/src/cpp_semantic_core.h:1 [bad-division]` warning; no new
warning was introduced.  The identity rule remains the PA10 contract: failure
tests compare only exit status, while successful tests require exact AST output
and exit status.  Additional passes cannot mask a new failure.

## Risks and next checkpoint

The main uncertainty is the intentionally preserved reference-led nested-group
boundary: `T(&x);` is syntactically a reference declaration and general §6.8
preference would select it as a declaration, but the checked PA10 contract
keeps `function(&spawned_thread);` as an expression because the syntax-only
stage does not perform lookup.  This is an explicit fixture-bound exception,
not a universal §6.8 routing claim.  Complex identifier-led parameter
declarations and followers beyond the focused matrix remain unclaimed.  The
member-pointer correction has focused probe coverage and the fresh broad
identity check above preserves the exact baseline failure set.

The next checkpoint is the separately assigned residual-family audit.  The
seven listed residual families remain untouched; any future new failure or
identity change must stop that checkpoint rather than be absorbed by the
residual count.

## Historical checkpoint ledger

Historical rows are retained below.  The final row is the single current audit
row for `90e9687c`.

| checkpoint | review result | owner action | validation state |
| --- | --- | --- | --- |
| `a2b82dcb` template/angle ownership | historical | retain typed template components and bounded close ownership | historical 106/157 baseline |
| `27623d64` declarator/member boundary | historical | retain unified declarator/member path and bounded shape | historical focused evidence |
| `b9b58b9c` declarator audit | historical | retain nearest-derived-operator and member-pointer bounds | historical 123/157; through-PA9 457/457 |
| `08c38115` structured names/special members | historical | retain one typed name/special-member path and validated sidecars | historical local 135/157; course 1/1 |
| `25f784873f2a852fd825316b2188d9f157f8eae5` typed postfix checkpoint | historical (audited and committed) | use one exact cast-keyword predicate, initialize indexes, route all RShift consumers through the marker, validate synthetic renderer nodes, and retain the 3000-line source bound | historical fresh 142/159 with exact original 17 failures; through-PA9 457/457; file audit exit 0 with one pre-existing warning |
| `d24f8e1689130b0449e19654ffd9e9f3dfc3b853` structured new expressions | historical (prior checkpoint audited) | retain the typed indexed abstract-group fact, distinguish definite from identifier-led nested parameter clauses under pointer/member-pointer spines, route complete abstract-declarator consumption through the canonical parser, validate inline initializer sidecars, and preserve global/placement/pack ownership | historical fresh 159/145 with exactly the original 14 residuals; through-PA9 457/457; file audit exit 0 with one pre-existing warning; focused 55/55 + exact refs 4/4 + warning/index/renderer harnesses |
| `c16e04ef82e93bb0c628d2f495cc7132d47dd749` elaborated-type boundary | historical completed; residuals remained | publish/charge malformed attribute scan work, stop colon lookahead at immediate owners, reject invalid template closes, preserve one canonical class/enum AST path and semicolon owner | historical focused 3/3 + 16/16; PA10 148/159 with exact 11 residuals; through-PA9 457/457; file audit exit 0 with one known warning |
| `90e9687c759a39d9b4844cdc01deed3ab1e80250` declaration/declarator ambiguity | completed; seven residuals remain | publish one typed parenthesized-group owner; preserve root-only parameter preference, contextual single-name/follower routing, and the fixture-bound reference exception; classify named member-pointer groups through the same owner | focused 21/21; corrected member-pointer harness PASS; §6.8 probe PASS; PA10 152/159 with exactly the seven turn-start identities; through-PA9 457/457; file audit exit 0 with one known warning; diff check exit 0 |
