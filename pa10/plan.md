# PA10 Checkpoint Plan and Evidence

## Scope and specification alignment

This checkpoint audits landed increment
`d24f8e1689130b0449e19654ffd9e9f3dfc3b853` (parent `a35dfc17`), which parses
structured new expressions.  The owned path is:

```text
phase-3 source -> typed posttokens/indexes -> PA10Parser new-expression path
    -> GlobalScope/NewPlacement/NewExpression/TypeId/Initializer/PackExpansion
    -> deterministic renderer
```

The path is aligned with the PA10 README, the new-expression/type-id,
placement, complete abstract-declarator/direct-abstract-declarator,
initializer, and pack grammar, and root `spec.md` §§1-4 and §7.  The parser
consumes the selected production once.  `delimiter_close_index_`, template
close/RShift facts, and the one-byte-per-token
`new_abstract_declarator_group_` are typed indexed facts; the new fact is
consulted only when `parse_type_id(..., new_expression_context=true)` reaches
the canonical new-expression abstract-declarator path.  Non-new
`looks_like_parameter_clause()` and declarator lookahead remain unchanged.
The renderer is a cold presentation boundary.  There is no source reparse,
retry/backtracking, textual shortcut, host/reference shortcut, or
fixture/reference edit.

The correction covers pointer/reference and qualified member-pointer
operators, nested parenthesized abstract declarators, array/function suffixes,
parameter clauses, and function suffixes while preserving `new T(*p)`,
`new T((x))`, the unparenthesized `new T(int())` initializer, explicit
parenthesized type-ids such as `new (int())`, placement/type-id selection,
global scope, and pack ownership.  The canonical `parse_abstract_declarator`
remains the sole AST-producing grammar path.

## Exact failure map

Fresh `make test-pa10` evidence is **159 discovered, 145 passed, 14 failed**;
the exit is 2.  The exact failures are:

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
pa10/tests/general/200-qualified-enumerator-call-argument.t
pa10/tests/general/200-sizeof-elaborated-class-type-id.t
pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t
pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t
pa10/tests/general/200-trailing-parameter-vendor-attribute.t
```

This is exactly the supplied residual set: no new failure, no coverage
reduction, and no work entered any listed family.

## Completed/current checkpoint row

| checkpoint | status | compact evidence/state |
| --- | --- | --- |
| `d24f8e1689130b0449e19654ffd9e9f3dfc3b853` structured new expressions | completed bounded correction; final gates pass | context-confined indexed abstract-declarator decision; exact charged index work; canonical parser consumption; renderer sidecar guard; focused 32/32 and exact refs 4/4; PA10 159/145 with exact original 14; through-PA9 457/457; file audit exit 0 with one pre-existing warning; immutable final SHA-256 `bfc4058782989d23df54a173a9d7321facba3592c7176602dbd83759d9afa8c7` |

## Focused and broad evidence

Final focused commands/results:

```text
make -C dev cppgm++ CXX=g++                                  exit 0
four direct .t/.ref AST comparisons                          4/4 exact
positive/sibling/negative/malformed new matrix                32/32
renderer malformed-sidecar invariant harness                 exit 0
build_indexes reset/reuse/index harness                      exit 0
g++ -std=gnu++11 -Wall -Wextra -Werror syntax checks          3/3 pass
git diff --check                                              exit 0
```

The exact reference comparisons were
`200-parenthesized-new-type-vs-placement.t`,
`200-placement-new-identifier-led-initializer.t`,
`200-placement-new-pack-init.t`, and `100-new-delete-traits.t`.

The authorized through command exited 0 with `457 / 457` through-PA9:

```text
n=10; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
```

The authorized file audit exited 0:

```text
perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src
```

It reported one pre-existing warning only:
`dev/src/cpp_semantic_core.h:1`, `bad-division` (implementation body in a
header).  No new warning or fatal finding was reported.

## Performance and structural evidence

The first milestone's 20-run immutable-executable characterization for
`200-placement-new-pack-init.t` and
`200-parenthesized-new-type-vs-placement.t` is retained as historical
d24-only evidence; it is not reused as a comparative claim for the corrected
source.

The corrected final executable was copied to an immutable path and hashed
before and after the repeated runs.  Both hashes were:

```text
bfc4058782989d23df54a173a9d7321facba3592c7176602dbd83759d9afa8c7
```

Twenty invocations per equivalent input, timed as one aggregate loop,
produced:

| input | elapsed | user | sys | peak RSS |
| --- | ---: | ---: | ---: | ---: |
| `200-placement-new-pack-init.t` | 0.06 s | 0.03 s | 0.02 s | 4364 KB |
| `200-parenthesized-new-type-vs-placement.t` | 0.05 s | 0.02 s | 0.03 s | 4368 KB |

These are single-executable, process-launch-dominated characterization
measurements, not comparative claims.  Structurally, the abstract-group fact
is reset and filled once per token buffer, is one byte per token, and is
consumed by the parser rather than recomputed per new-expression.  A
current-source reset/reuse harness returned identical counts; shape inputs
grew from 6 tokens/43 work units to 641/4996, and member-pointer inputs from
10/92 to 1153/11268.  The reverse delimiter-owned spine pass and its
charged work count support amortized linear construction; there is no text
retry or per-new rescan.  The existing global work, recursion, delimiter,
angle, and renderer limits remain in force.

Final affected-source counts observed before the final audit were:

```text
dev/src/pa10_ast.cpp            2999 lines
dev/src/pa10_parser_support.cpp  889 lines
dev/src/pa10_parser_support.h     41 lines
dev/src/pa10_renderer.cpp       1017 lines
```

## Renderer invariant decision

The renderer guard is retained.  Inline new-expression presentation bypasses
normal child dispatch for the initializer and directly accesses its syntax
child.  The guard validates the initializer's and then the syntax child's
sidecar ranges before `front()` and before inline child emission.  The
synthetic malformed-sidecar probe sets an out-of-range syntax name-prefix
sidecar and is rejected (`exit 0`) at that boundary, demonstrating an actual
unchecked-sidecar invariant rather than merely moving a later failure.

## Next checkpoint

The next checkpoint is a separately assigned residual-family audit.  Preserve
the exact 14 identities above and do not widen into lambda, general
declaration/declarator, qualified-name, or unrelated PA10 work.

## Historical ledger

| checkpoint | status | compact evidence/state |
| --- | --- | --- |
| `a2b82dcb` template/angle ownership | landed historical | typed components/sidecars and bounded close ownership; historical 106/157 |
| `27623d64` declarator/member boundary | landed historical | unified declarator/member path and bounded shape |
| `b9b58b9c` declarator audit | landed historical | historical 123/157; through-PA9 457/457; one pre-existing audit warning |
| `08c38115` structured names/special members | landed historical | removed 12 prior residuals; retained course boundary fixture |
| `017eb658` structured-name audit | historical starting point | clean at 158/136 with 22 failures |
| `25f784873f2a852fd825316b2188d9f157f8eae5` typed postfix checkpoint | landed historical | historical 142/159 with exact original 17 failures; prior focused postfix evidence |
