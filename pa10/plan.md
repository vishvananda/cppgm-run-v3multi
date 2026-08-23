# PA10 Checkpoint Plan and Evidence

## Stage Design

PA10 keeps one production path:

```text
phase-3 source -> typed posttoken facts/indexes
    -> PA10Parser seed -> typed PA10Ast -> cold deterministic renderer
```

For this checkpoint, `PA10ParserSupport::build_indexes` owns the bounded
`delimiter_close_index_` fact.  `PA10Parser` owns new-expression production
selection and the typed tree; the renderer only adapts that tree to the exact
cold dump.  The new-expression children are explicit `GlobalScope`,
`NewPlacement`, `TypeId`, `Initializer`, and `PackExpansionExpression` nodes.
The renderer validates those typed shapes and delegates new-expression inline
presentation to a bounded cold adapter.  There is no source-span fallback,
parser retry, duplicate new production, or host/reference shortcut.

## Failure Map

The supplied turn-start baseline is **159 discovered, 142 passed, 17 failed**.
The exact original residual identities are:

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

The selected owner is the three new-expression failures.  Their data flow is
`KW_NEW`/optional `OP_COLON2` -> indexed first-group close and bounded
type-start follower facts -> one monotonic parser path for placement or
parenthesized type-id -> typed global/placement/type/initializer/pack nodes ->
exact renderer lines.  The allocated `S`/`T` type-id uses an indexed context
proof: only a direct or one-level nested pointer/reference-only parenthesized
abstract form enters that path, while operand-bearing groups remain
new-initializers.  The pointer-shaped declaration start and declarator pack marker are shared syntax
facts required to reach the selected initializer cases; no other residual
family is entered intentionally.

The final PA10 gate is **159 discovered, 145 passed, 14 failed**.  The sorted
current residual identities are exactly:

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

The three selected new-expression identities are absent, and no replacement
failure identity appeared.

## Active Checkpoint

The first parenthesized group is classified in constant time from
`delimiter_close_index_` and bounded follower facts.  An empty group is not
accepted as placement; a direct type-id-start follower selects placement with
an ordinary type-id, and an `OP_LPAREN` follower selects placement with a
parenthesized type-id only when that indexed follower group is nonempty and
its first token is a type-id start.  Otherwise the first group is the
parenthesized type-id and a following group can be its initializer.  The group
is then consumed once by `parse_paren_argument_list`.  This is deliberately a
syntactic policy: an identifier-starting ambiguous group is treated as a
type-id start (for example `(p)(T)` selects placement plus parenthesized
type-id), because PA10 has no semantic lookup at this boundary.  Global scope
is retained as a fixed typed marker, and bounded renderer validation enforces
the global/placement/pack/new child shapes before the cold adapter supplies
`global-scope` and `placement (...)`.  It also requires each present
initializer to contain exactly one `ParenInitializer` or `BracedInitList`.
A single `initializer-clause` owner wraps a trailing `...` in
`PackExpansionExpression`.

The final owner also narrowed the shared declaration-start lookahead to the
pointer form required by `S* p`, preserving operator-expression ownership.
Structural bound: the existing indexed delimiter/template pass is O(n) in
the token stream; new disambiguation is O(1) per new-expression, followed by
monotonic token consumption.  The existing parser work limit and nesting/
recursion ceilings remain the bounds; no long-group speculative retry or
per-node textual storage was added.  The new-context abstract-declarator
policy is intentionally narrow: direct `(*)`, `(&)`, `(&&)`, and one nested
pointer-only group `((*))` are proven from indexed closes; operand-bearing
forms such as `(*p)`, `((1))`, and `((x))` remain initializer syntax.

Final focused and gate evidence:

```text
make -C dev cppgm++ CXX=g++                                  exit 0
make -C pa10 check [five fixtures + operators-pm]             PASS (6/6)
make test-pa10                                                145/159; exact 14 residuals
prior gate n=10                                               PASS (457/457 through PA9)
file audit                                                     PASS; 1 pre-existing warning
warning-clean -Wall -Wextra -Werror affected sources         PASS (3 files)
git diff --check                                              PASS
```

Temporary probes covered `new (p) (int)`, `::new(p)(int)`, `new (int)(1)`,
`new int(*)(int)`, `new int((1))`, `new T((x))`, and `new T(*p)`; their AST
ownership was checked.  The remaining 14 failures are the original residual
families outside this checkpoint; next owner is the subsequent residual-family
checkpoint.  No new PA10 regression is known.  The audit's sole warning is
the pre-existing `cpp_semantic_core.h` division warning.

## Performance Evidence

The immutable `dev/cppgm++` SHA-256 was
`e9d6d9a2f19559431624752b026a090e0fa0fc061556c608c07a060434c3977f` before
and after measurement.  Each row is 20 equivalent repeated runs; values are
aggregate characterization only, not a comparative claim:

```text
200-placement-new-pack-init.t          elapsed 0.06s user 0.02s sys 0.03s peak RSS 4404 KB
200-parenthesized-new-type-vs-placement.t elapsed 0.05s user 0.01s sys 0.03s peak RSS 4420 KB
```

The structural risk remains bounded by one indexed O(n) fact pass, constant-
time follower classification, one parse of the selected group, monotonic
consumption, and the existing work/recursion limits.

## Checkpoint Ledger

| checkpoint | status | compact evidence/state |
| --- | --- | --- |
| `a2b82dcb` template/angle ownership | landed historical | typed components/sidecars and bounded close ownership; 106/157 historical |
| `27623d64` declarator/member boundary | landed historical | unified declarator/member path and bounded shape |
| `b9b58b9c` declarator audit | landed historical | 123/157 historical; through-PA9 457/457; one pre-existing audit warning |
| `08c38115` structured names/special members | landed historical | removed 12 prior residuals; retained course boundary fixture |
| `017eb658` structured-name audit | historical starting point | clean at 158/136 with 22 failures |
| `25f784873f2a852fd825316b2188d9f157f8eae5` typed postfix checkpoint | landed historical | fresh 142/159 with exact original 17 failures; prior focused postfix evidence |
| `PA10 new-expression boundary` | validated and committed | 145/159 with exact 14 original residuals; prior gate 457/457; audit/warning/performance evidence complete |

The checkpoint is committed and the final clean-worktree review is complete.
