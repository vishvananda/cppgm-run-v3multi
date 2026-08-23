# Stage Design

PA10 retains one producer-token -> parser -> typed `PA10Ast` -> renderer flow.
`PA10NameComponent` owns producer `PPSpellingId`, the optional `template`
disambiguator, and a range into typed `PA10TemplateArgument` sidecars. Bare
identifier-starting arguments are retained as `Unresolved`; fixed keywords
can remain `TypeId`, and expression syntax remains owned by its AST node. The
renderer composes presentation on demand without flattening or reparsing.
Decltype qualified roots are typed prefix nodes.

The posttoken boundary splits `OP_RSHIFT` into two logical `>` pieces. The
parser's angle/non-angle state consumes one piece per nested close and both
only when expression parsing sees a real shift. Parenthesized regions raise
the non-angle depth, preserving relational `>` inside them.

`build_template_indexes()` performs one charged linear pass over the complete
posttoken stream. It maintains a stack of delimiter frames, with one active
angle stack per frame: every `(`, `[`, or `{` pushes a fresh scope, and only a
matching close records the delimiter and pops/discards that scope. Each RShift
piece is processed independently, so no paired-piece loop can revisit the
second piece. Candidate lookup is O(1); the one-token follow check is charged.
Total auxiliary index work and storage are O(n), bounded by the existing parser
work/nesting guards, with no retry or backtracking loop and no arbitrary
input-length cap. The renderer is in `pa10_renderer.cpp` to keep each
implementation file below the PA10 audit size limit; the build list change is
required for that split.

# Failure Map

Baseline at `43703613`: 157 tests, 77 pass, 80 fail. Final broad PA10
validation at this checkpoint: 157 discovered, 105 pass, 52 fail. Removed
failure identities are exactly:

- `general/100-decltype-qualified-id-expression`
- `general/100-template-condition`
- `general/200-decltype-less-partial-specialization`
- `general/200-dependent-member-template-call`
- `general/200-dependent-template-keyword-nested-angle`
- `general/200-inline-namespace-template-visibility-base`
- `general/200-member-template-if-less-template-call`
- `general/200-mock-template-name-angle-forms`
- `general/200-namespace-alias-using-directive-imported-template-id-type`
- `general/200-nested-qualified-template-id-template-args`
- `general/200-nested-template-return-assignment-operator`
- `general/200-operator-less-followed-by-operator-greater`
- `general/200-parenthesized-qualified-function-template-call`
- `general/200-qualified-base-member-call`
- `general/200-qualified-base-template-id-logical-argument-value-shadow`
- `general/200-qualified-template-id-parameters`
- `general/200-relational-qualified-template-static-calls`
- `general/200-reopened-namespace-template-function-template-name`
- `general/200-switch-case-qualified-template-id-call`
- `general/200-switch-case-template-id-call`
- `general/200-template-id-function-pointer-argument`
- `general/200-template-id-function-pointer-initializer`
- `general/200-template-id-return-type-inline`
- `general/200-template-id-value-if-condition`
- `general/200-using-declaration-imported-template-id-type`
- `general/200-using-directive-imported-template-id-type`
- `spec/200-explicit-specialization-syntax`
- `spec/300-template-id-less-expression`

The complete identity comparison reports 28 removed and zero new identities.
The residual map is the baseline set minus this list; no unrelated family was
added.

# Active Checkpoint

The committed checkpoint owns parser-side typed names/arguments, angle-token
classification, qualified-name use, exact cold rendering, and delimiter-scoped
candidate indexing. No tests, refs, grammar, harnesses, or fixture files
changed. Current implementation files are `dev/src/pa10_ast.cpp`,
`dev/src/pa10_ast.h`, `dev/src/pa10_renderer.cpp`,
`dev/frontend_source_sets.mk`, and this plan. No explicit-instantiation parser
branch or template-template extension was added; existing node labels remain
part of the prior AST contract.

# Performance Evidence

The required focused command passed 11/11, including nine former failures and
the relational/shift guards. External probes, outside tracked files, measured:

| probe | result | elapsed | peak RSS |
| --- | --- | ---: | ---: |
| 140-argument template-id, close beyond 256 tokens | success | 0.00 s | 4340 KB |
| triple adjacent closes plus `x >> 1` | success | 0.00 s | 4120 KB |
| sibling `()`, `[]`, `{}` relational scopes plus nested template args | success | 0.00 s | 4352 KB |
| relational-heavy, 32 comparison pairs | success | 0.00 s | 4372 KB |
| relational-heavy, 128 comparison pairs | success | 0.00 s | 4812 KB |
| relational-heavy, 256 comparison pairs | success | 0.02 s | 5816 KB |

A 512-pair relational probe reached the existing renderer nesting guard at
0.02 s / 7980 KB; it is characterized as a bounded-depth limit, not claimed
as an asymptotic failure. The triple probe rendered `f<a<b<c>>>` and a
separate `OP_RSHIFT:>>` node, confirming angle/shift separation. The probes
were temporary files under `/tmp/pa10-probes`, outside the worktree.

# Checkpoint ledger

| checkpoint | status | evidence / later delta |
| --- | --- | --- |
| `43703613` baseline | recorded | 77/157 pass, 80 failures |
| PA10 template-id / qualified-name / angle correction | committed checkpoint | 105/157 pass, 52 failures, 28 removed, zero new; warning build, through-PA9, audit, focused tests, and probes complete |
| delimiter-scope correction | committed in this amendment | sibling delimiter probes pass; all gate results remain unchanged |
