# PA10 Checkpoint Plan and Evidence

## Stage Design

```text
phase-7 typed tokens/indexes -> PA10ParserSupport typed elaborated classification
    -> PA10Parser canonical class/enum/specifier owner -> deterministic renderer
```

`PA10ParserSupport::classify_elaborated_specifier` owns the bounded context
decision over typed tokens and existing delimiter/template/RShift indexes.  It
returns context, body/colon facts, and exact `charged_work`; the parser wrapper
charges that count before AST construction and semicolon ownership.  The
support template-follower API uses the same contract.  The renderer remains a
cold presentation boundary.  No source reparse, retry/backtracking parser,
duplicate AST path, semantic lookup expansion, host/reference shortcut, or
fixture/reference edit is in scope.

## Failure Map

Turn-start state is **159 discovered, 145 passed, 14 failed**.  The residual
identities remain grouped by owning boundary:

| owning boundary | status | exact residuals |
| --- | --- | --- |
| elaborated class/enum specifier ownership | **active** | `200-elaborated-enum-member-declarators.t`, `200-friend-type-declaration.t`, `200-sizeof-elaborated-class-type-id.t` |
| general declaration/declarator ambiguity | inactive | `200-global-struct-paren-declaration.t`, `200-local-typedef-paren-declaration.t`, `200-mock-type-declaration-ambiguity.t` |
| template/name/expression ownership | inactive | `200-forward-unknown-nested-template-in-ctor-body.t`, `200-friend-function-template-declaration.t`, `200-member-template-parameter-value-vs-template-name.t`, `200-qualified-enumerator-call-argument.t`, `200-template-member-definition-inherited-typedef-cast.t` |
| lambda capture ownership | inactive | `200-lambda-capture-forms.t` |
| trailing-parameter attribute ownership | inactive | `200-trailing-parameter-carries-dependency-attribute.t`, `200-trailing-parameter-vendor-attribute.t` |

No inactive family is being widened into by this checkpoint.

## Active Checkpoint

Completed bounded increment; PA10 residuals remain.  Scope is the
elaborated-type boundary in `dev/src/pa10_parser_support.cpp/.h`, parser
ownership in `dev/src/pa10_ast.cpp`, and this plan.  The typed support result
distinguishes non-elaborated, embedded/declarator-bearing, standalone forward,
and standalone definition contexts; the parser constructs the canonical AST
and owns semicolons.  `sizeof(struct X)` owns a `type-id` and
`type-specifier-seq`.  Enum forwards with an underlying type and no body are
direct enum declarations; enum bodies followed by declarators remain simple
declarations.

Invariants are exact existing AST rendering for standalone forwards, named and
anonymous class definitions, scoped and defined enums, class members, and
unrelated declarations/type traits; one production is consumed once; no
semantic lookup is introduced; and no test, reference, status fixture, or
grammar file is changed.  The correction explicitly covers unscoped/scoped
underlying-type forwards and preserves body/declarator routing.  Remaining
uncertainty is limited to unusual attributes, base/underlying-type clauses,
and declarator-bearing anonymous forms; inactive residual families remain out
of scope.

## Checkpoint Evidence

```text
make -C pa10 check TEST='tests/general/200-elaborated-enum-member-declarators.t tests/general/200-friend-type-declaration.t tests/general/200-sizeof-elaborated-class-type-id.t'
```

Exit 0; stdout: `pa10 check: running 3 tests`, `pa10 check: PASS (3/3)`.
The harness compared generated `.check` ASTs and `.check.exit_status` files
to the checked-in refs; stale `.my` files are separate scratch output.

```text
make -C pa10 run INPUT=/tmp/pa10-elaborated-enum-boundary.t RUN_OUTPUT=/tmp/pa10-elaborated-enum-boundary.out
```

Exit 0; stdout rendered `enum-specifier E`, `enum-specifier Scoped`, the
defined scoped enum, and a `simple-declaration` containing
`DefinedWithDeclarator` and `value`.

```text
make -C pa10 check TEST='tests/general/100-class-alignas-after-class-key.t tests/general/100-scoped-enum-underlying-type.t tests/general/100-structured-type-id.t tests/general/100-typedef-anonymous-enum.t tests/general/100-typedef-anonymous-union.t tests/general/100-typedef-struct-union.t tests/general/200-dependent-sizeof-pointer-type-id.t tests/general/200-function-throw-typed-specification.t tests/general/200-sizeof-zero-arg-functional-cast.t tests/spec/100-enum.t tests/spec/200-class-bases-and-ctor-init.t tests/spec/300-type-id-expression-contexts.t tests/general/100-conditional-sizeof.t tests/general/100-new-delete-traits.t'
```

Exit 0; stdout: `pa10 check: running 14 tests`, `pa10 check: PASS (14/14)`.
No refs or fixtures were edited.

```text
make test-pa10
```

Exit 2 because residual tests remain; all 159 tests were discovered and the
summary was `===== TEST SUMMARY: 148 / 159 TESTS PASSED =====`.  The exact
remaining 11 identities are:

```text
pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t
pa10/tests/general/200-friend-function-template-declaration.t
pa10/tests/general/200-global-struct-paren-declaration.t
pa10/tests/general/200-lambda-capture-forms.t
pa10/tests/general/200-local-typedef-paren-declaration.t
pa10/tests/general/200-member-template-parameter-value-vs-template-name.t
pa10/tests/general/200-mock-type-declaration-ambiguity.t
pa10/tests/general/200-qualified-enumerator-call-argument.t
pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t
pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t
pa10/tests/general/200-trailing-parameter-vendor-attribute.t
```

These are exactly the 11 inactive turn-start residuals; the three active
elaborated-type identities passed and no new failure appeared.

```text
n=10; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
```

Exit 0; `===== ALL TESTS PASSED SUCCESSFULLY! (457 / 457) =====` through PA9.

```text
perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src
```

Exit 0; `File audit passed for pa10 with 1 warning(s).`  The sole warning is
the known pre-existing `dev/src/cpp_semantic_core.h:1` bad-division warning;
there is no new audit finding.

```text
git diff --check
```

Exit 0.  The clean syntax/build check `make -C dev cppgm++` also exited 0
with no compiler warnings.

## Performance Evidence

`classify_elaborated_specifier` scans only the current header, jumps over
matched parentheses/brackets and template argument groups through existing
indexes, and stops at the class/enum body opener without scanning the body.
Attribute tokens, cursor steps, indexed template closes, RShift nested-close
pieces, and delimiter jumps contribute to its returned `charged_work`; the
parser charges exactly that value.  `template_follow_is_valid` returns the
same exact work contract.  There is no new global index or cache: disjoint
headers give aggregate O(n) lookahead work and existing indexes remain O(n)
storage.  This is a structural bound only; no comparative timing claim is
made.

## Checkpoint Ledger

| checkpoint | status | compact evidence/state |
| --- | --- | --- |
| `a2b82dcb` template/angle ownership | landed historical | typed components/sidecars and bounded close ownership; historical 106/157 |
| `27623d64` declarator/member boundary | landed historical | unified declarator/member path and bounded shape |
| `b9b58b9c` declarator audit | landed historical | historical 123/157; through-PA9 457/457; one pre-existing audit warning |
| `08c38115` structured names/special members | landed historical | removed 12 prior residuals; retained course boundary fixture |
| `017eb658` structured-name audit | historical starting point | clean at 158/136 with 22 failures |
| `25f784873f2a852fd825316b2188d9f157f8eae5` typed postfix checkpoint | landed historical | historical 142/159 with exact original 17 failures; prior focused postfix evidence |
| `d24f8e1689130b0449e19654ffd9e9f3dfc3b853` structured new expressions | landed historical | bounded indexed abstract-declarator correction; final gates and evidence retained in repository history |
| current elaborated-type boundary | **completed; residuals remain; committed** | typed support classification/follower ownership; `dev/src/pa10_ast.cpp` 2945 lines; 148/159 with exactly 11 inactive residuals; focused 3/3, sibling 14/14, ad hoc exit 0, through-PA9 457/457, audit exit 0 with one known warning, diff check clean |
