# PA12 checkpoint plan

## 1. Stage design and spec alignment

The implementation remains one typed route:

`PPPreprocessingSession -> PPTokenBuffer -> PA10Ast -> PA11SemanticModel -> PA12 typed facts -> cold deterministic dump`

PA10 owns structured statement identities and child order. PA11 owns canonical
types, bindings, ordinary block scopes, and lookup. PA12 extends that owner
with pointer-keyed `StatementFact` preparation, hidden control/substatement
scopes, condition/for-init `DeclarationFact` records, typed contextual
conversions, and `SemanticFact` nodes for compound, if/else, switch/case/
default, while, do, for, break, and continue.

Condition declarations are bound in their control scope; for-init declarations
are bound in the for scope; unbraced selection/iteration bodies get distinct
child scopes; and loop/switch depth is carried lexically for jump validation.
The PA12-only scopes are not attached to the PA11 dump tree. Rendering consumes
typed facts and PA10 producer identities once: there is no rendered-text
intermediate, parallel analyzer, reference-tool call, arena retry, or
hash-order output. Preparation and semantic traversal are linear in statements,
declarations, and expressions, with lookup work bounded by lexical nesting.
Per-switch duplicate labels use a small typed `FlatIndex` context; no sibling
or whole-arena scan is used.

## 2. Failure map and coverage

The clean turn-start baseline was **90/166 passing, 76 failures**, with all 166
tests covered. Final partition counts are:

| checked-in partition | total | passed | failed |
| --- | ---: | ---: | ---: |
| `tests/spec/100-*.t` | 12 | 12 | 0 |
| `tests/general/100-*.t` | 25 | 25 | 0 |
| `tests/spec/200-*.t` | 9 | 8 | 1 |
| `tests/general/200-*.t` | 33 | 19 | 14 |
| `tests/spec/300-*.t` | 10 | 6 | 4 |
| `tests/general/300-*.t` | 77 | 33 | 44 |
| **all PA12 tests** | **166** | **103** | **63** |

The 13 repaired baseline cases are the 12 focused statement positives:

- `spec/200-if-control-flow`, `spec/200-do-statement`, `spec/200-for-loop`,
  `spec/200-switch-statement`, `spec/300-condition-declaration-scope`, and
  `spec/300-nullptr-pointer-conversion`;
- `general/200-condition-declaration`,
  `general/200-if-substatement-sibling-declaration-scopes`,
  `general/200-loop-jumps`, `general/200-switch-case-declaration`,
  `general/200-switch-default-declaration`, and `general/200-while-loop`.

The additional repaired baseline case is
`general/300-qualified-using-directive-enumerator-case`.

The five checked-in negative guards remain passing: bad default outside
switch, bad scoped-enum if condition, break outside loop, continue outside
loop, and nonconstant case label. Honest residuals include the independent
builtin-call, other using/call, scoped-enum/call, overload and
function-pointer/reference, class/constructor, array/subscript, floating,
pointer/reference, cast, namespace, and parser families shown by the 63-test
partition above; all 100-level tests are now green.

Normalized failure-path comparison against
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` found
**63** failures in the current run and **76** in the baseline: **13
baseline-only** repairs (the 12 focused statement positives plus the
qualified-using/enumerator case), **0 current-only** regressions, and 63
common failures.

## 3. Active checkpoint

The completed checkpoint implements the shared structured-statement boundary:
typed scope preparation, copy-init conditions, contextual-to-bool checks
(including pointer/nullptr), integral/enumeration switch conditions, typed
constant case labels with duplicate detection, deterministic statement-fact
shape, and lexical jump validation. `reparent_scope` was removed: PA11-created
compound scopes are either new under their final parent or are checked against
that parent in preparation. Hidden statement scopes are created once by
pointer-keyed AST identity and are not dumped.

Focused validation:

`make -C pa12 check TEST='tests/spec/200-if-control-flow.t tests/spec/200-do-statement.t tests/spec/200-for-loop.t tests/spec/200-switch-statement.t tests/spec/300-condition-declaration-scope.t tests/spec/300-nullptr-pointer-conversion.t tests/general/200-condition-declaration.t tests/general/200-if-substatement-sibling-declaration-scopes.t tests/general/200-loop-jumps.t tests/general/200-switch-case-declaration.t tests/general/200-switch-default-declaration.t tests/general/200-while-loop.t tests/general/300-bad-default-outside-switch.t tests/general/300-bad-scoped-enum-if-condition.t tests/general/300-break-outside-loop-bad.t tests/general/300-continue-outside-loop-bad.t tests/general/300-nonconstant-case-label-bad.t'` — **17/17 passed**.

Temporary synthesized probes — not repository fixtures — were **12/12** at
their expected exit statuses: duplicate case, duplicate default, normalized
duplicate case, nested-switch independence, condition visibility in both
branches, condition name absent afterward, for visibility, for name absent
afterward, sibling unbraced redeclaration, nested loop/switch jumps, and
continue-in-switch rejection. Empty `for (;;)` succeeded and two cold dumps
were byte-identical.

The three checkpoint regressions
`general/100-string-literal-array-type`,
`general/100-variadic-call-fixed-prefix`, and
`general/200-paren-argument-list-call` were also rerun and passed. The fix is
that ordinary literals retain their source spelling: the final
`semantic_literal` has no generic `has_literal_value` capture at all. The
completion audit also found and restored the ordinary hex/source-spelling
regression in `general/300-floating-literal-classification` (`0xfeedL` had
rendered as `65261`) alongside those three string/call regressions. Only the
literal fact explicitly synthesized by `semantic_case_label` sets
`has_literal_value` and `literal_value` for canonical case-label rendering and
duplicate detection.

## 4. Performance evidence

The current `dev/cppgm++` was rebuilt by `make -C pa12`, independently hashed,
and copied to a fresh temporary location. The source binary is mode **775**
and the immutable copy is mode **555**; both are **1,133,096 bytes** and both
have SHA-256
`d9683d357e23c632502bc9c6b43ae4f2f9fde423274d8e18d0d0a34ce713a6aa`.
Generated workloads and outputs were outside the repository. Structural
statement/control counts below count exact semantic-dump labels: statements
are compound, declaration, return, expression, structured-control, case,
default, break, and continue nodes; control nodes are the structured-control,
case/default, and break/continue subset.

| workload | source lines | source bytes | functions | statements | control nodes | dump lines |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 200 functions | 4,200 | 89,090 | 200 | 4,600 | 2,200 | 20,404 |
| 800 functions | 16,800 | 356,690 | 800 | 18,400 | 8,800 | 81,604 |

Seven `/usr/bin/time -f '%e'` samples were interleaved (small/large order
alternated each round). Samples were 0.09, 0.09, 0.09, 0.09, 0.09, 0.09,
0.09 seconds for 200 and 0.39, 0.39, 0.39, 0.39, 0.39, 0.40, 0.39 seconds
for 800; medians were **0.09s** and **0.39s**. The 4x structural increase
produced a roughly 4.3x median time increase at meaningful 0.01s resolution.
The source structure supports O(statements + declarations + expressions)
preparation and semantic traversal, O(lexical nesting) context lookup, and
expected-O(1) per-label duplicate checks in each switch context. No sibling
scan, whole-arena retry, or output-order dependence remains in this route.

## 5. Checkpoint ledger

| row | result |
| --- | --- |
| turn-start baseline | Clean worktree; PA12 **90/166**, 76 failures; earlier through PA11 **685/685**. |
| reviewed/corrected milestone | Five implementation files only; focused **17/17** and synthesized probes **12/12**; no tests, refs, harnesses, grammar, or generated workloads changed. |
| final validation | `make test-pa12`: **103/166**, **63 failures**, all 166 covered; through-PA11: **685/685**; file audit passed with 2 known warnings; `git diff --check` passed; failure-set comparison had 13 baseline-only and 0 current-only cases. |
| final result / commit | **HEAD** (`pa12: add structured statement semantics`); the final immutable object ID is reported with the validation handoff. |
