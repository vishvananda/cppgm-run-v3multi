# PA12 block-scope declaration and lookup-continuity checkpoint

## Stage Design

The production route remains one typed owner/data flow:

`PPPreprocessingSession -> PPTokenBuffer -> PA10Ast -> PA11SemanticModel -> PA12 typed facts -> cold deterministic dump`

PA11 owns canonical `TypeId`, `ScopeId`, `BindingId`, `NamePath`, and lookup.
`process_compound_statement` forms every supported block declaration exactly
once in source order: `AliasDeclaration`, `NamespaceAliasDefinition`,
`UsingDirective`, `UsingDeclaration`, and `SimpleDeclaration`. PA12 consumes
the resulting facts; lookup-only declarations have no statement line, while a
local alias is a typed `TypeAlias` fact and `type-alias` dump line.

Using-declaration entries are typed `(BindingId, ScopeId)` pairs. The binding
is the canonical source declaration and the scope is its source provenance;
the importing scope gets only a lookup entry and a cold dump view, never a
second semantic declaration or a rendered-name key.

Each `Scope` retains raw using-directive target edges for qualified and
transitive graph traversal, while `process_using_directive` computes the
scope-tree common ancestor once using stored scope depths and places an
`EffectiveUsingDirective(target, lexical_scope)` entry at that owner. An
unqualified query marks its lexical ancestor chain once. At each lexical level
it checks direct/inline declarations, then scans only that level's effective
entries and filters applicability by the typed lexical mark. Namespace/type
lookup keeps a direct result at that level; value lookup merges direct values
with reachable nominated graphs, so a direct function and a nominated
overload at their common ancestor remain one typed candidate set.
Generation-marked graph traversal preserves transitive and cyclic termination
and deterministic source order while visiting only relevant
lexical/directive/inline scopes and candidates.

The PA10 change is declaration disambiguation, not a spelling workaround: an
identifier type followed by cv-qualifiers and an identifier, or by a
pointer/reference declarator and identifier, is recognized only when the
existing declaration-follow predicate accepts it. For the ambiguous direct
initializer AST, PA11 uses typed target lookup and PA12 calls
`semantic_expression_for_target` followed by exactly one normal target
conversion. Thus overloaded function IDs and ordinary value operands share
the target-directed path.

Scope formation is O(block children). Effective placement costs O(H) once per
using directive, where H is its scope depth; each query costs one O(H) lexical
mark pass plus scans of effective entries at visited levels and reachable
lookup graph/candidate work. It does not rescan the start ancestry per level or
edge. There is no whole-arena scan, retry loop, rendered-name reparsing, or
hash-order output.

## Failure Map

Turn start was clean at `994a7000`: PA12 **113/166 passing**, exactly **53
failures**, all 166 covered; through PA11 was **685/685**.

| partition | total | passed | failed |
| --- | ---: | ---: | ---: |
| `tests/spec/100-*.t` | 12 | 12 | 0 |
| `tests/general/100-*.t` | 25 | 25 | 0 |
| `tests/spec/200-*.t` | 9 | 9 | 0 |
| `tests/general/200-*.t` | 33 | 23 | 10 |
| `tests/spec/300-*.t` | 10 | 9 | 1 |
| `tests/general/300-*.t` | 77 | 35 | 42 |
| **all PA12 at turn start** | **166** | **113** | **53** |

Checkpoint paths fixed by this increment:

```text
pa12/tests/general/200-block-scope-using-declaration.t
pa12/tests/general/200-local-alias-postfix-cv-declaration.t
pa12/tests/general/200-local-alias-statement.t
pa12/tests/general/200-local-direct-initialization.t
pa12/tests/general/200-local-using-directive-preserves-nearer-namespace-type.t
pa12/tests/general/200-using-declaration-call.t
pa12/tests/spec/300-block-scope-namespace-alias-qualified-call.t
```

Final broad result is **120/166 passing**, **46 failures**, with all 166
covered. Normalization against the supplied 53-path baseline found **0
current-only** paths and **exactly seven baseline-only paths**, namely the
seven fixed checkpoint paths above. The exact current residual inventory is:

```text
pa12/tests/general/200-builtin-constant-p-propagated-expression.t
pa12/tests/general/200-constexpr-complete-object-cv.t
pa12/tests/general/200-function-pointer-array-deduced-bound.t
pa12/tests/general/200-local-anonymous-union-variable.t
pa12/tests/general/300-array-xvalue-subscript.t
pa12/tests/general/300-block-anonymous-union-injected-members.t
pa12/tests/general/300-block-elaborated-enum-type-use.t
pa12/tests/general/300-builtin-abort-semantics.t
pa12/tests/general/300-builtin-constant-p-call.t
pa12/tests/general/300-decltype-functional-cast.t
pa12/tests/general/300-deref-function-returning-pointer-reference.t
pa12/tests/general/300-deref-string-literal.t
pa12/tests/general/300-elaborated-local-struct-copy-init.t
pa12/tests/general/300-enum-comparisons.t
pa12/tests/general/300-floating-arithmetic-comparisons.t
pa12/tests/general/300-floating-conditional-common-type.t
pa12/tests/general/300-floating-inc-dec.t
pa12/tests/general/300-integral-compound-assign-unscoped-enum.t
pa12/tests/general/300-local-extern-function-declaration.t
pa12/tests/general/300-member-function-pointer-return-pointer-const.t
pa12/tests/general/300-member-function-pointer-type-alias-and-function.t
pa12/tests/general/300-member-pointer-type-alias-and-function.t
pa12/tests/general/300-multidimensional-array-const-reference-binding.t
pa12/tests/general/300-namespace-function-body-later-anonymous-overload.t
pa12/tests/general/300-nullptr-equality.t
pa12/tests/general/300-pointer-bool-conversion.t
pa12/tests/general/300-pointer-nullptr-conditional.t
pa12/tests/general/300-pointer-plus-anonymous-enum.t
pa12/tests/general/300-pointer-subtraction-cv-compatible.t
pa12/tests/general/300-postfix-reference-return-call-inc.t
pa12/tests/general/300-prefix-reference-return-call-inc.t
pa12/tests/general/300-qualified-direct-function-hides-using-directive.t
pa12/tests/general/300-reference-binding-pointee-const-pointer.t
pa12/tests/general/300-reopened-unnamed-namespace-call.t
pa12/tests/general/300-scoped-enum-functional-cast-integral.t
pa12/tests/general/300-simple-assignment-reference-lhs.t
pa12/tests/general/300-static-cast-member-overload-prefers-nontemplate.t
pa12/tests/general/300-static-cast-overloaded-function-template-argument.t
pa12/tests/general/300-subscript-commuted-expression.t
pa12/tests/general/300-subscript-unscoped-enum-index.t
pa12/tests/general/300-switch-scoped-enum-condition.t
pa12/tests/general/300-unnamed-namespace-definition.t
pa12/tests/general/300-unnamed-namespace-qualified-call.t
pa12/tests/general/300-unnamed-namespace-unqualified-call.t
pa12/tests/general/300-unscoped-enum-integral-operators.t
pa12/tests/general/300-zero-arg-functional-cast-alias.t
```

## Active Checkpoint

The approved five-file implementation increment is complete and validated.
Its owner-level changes are:

- PA10 declaration disambiguation for cv-qualified and pointer/reference
  named-type forms, with declaration-follow validation;
- PA11 one-pass block declaration formation, paired source-provenance value
  entries, nearest-common-ancestor using lookup, and direct-initializer target
  recognition;
- PA12 lookup-only statement suppression, typed local-alias facts, function
  provenance in dumps, and target-directed direct initialization.

The checkpoint remains intentionally scoped to block declarations, aliases,
using continuity, lookup precedence, and direct initialization. The 46
residual paths remain owned by unrelated later expression/control families.

## Performance Evidence

The corrected lookup traversal was exercised with two out-of-tree structural
probes. The common-ancestor probe has one `n::f(int)`, one global `f(long)`,
and a block `using namespace n`: two runs both exited 0, produced byte-identical
dumps, and selected `n::f` for `f(0)`. The transitive/cyclic probe has four
namespace-definition nodes, four using edges including an `a <-> b` cycle,
and a block edge to `a`: two runs both exited 0, were byte-identical, and
selected `c::g`. Generation marks prevent cycle retries.

The immutable scaling executable was
`/tmp/pa12-lookup-effective-immutable-final2`, SHA-256
`482f814a4a98626bb66e05a117a35bd106c241195be1887040427a60e8175aa3`.
Equivalent block-nesting inputs used one declaration, one call, one selected
candidate, and only the lexical depth/effective using-edge count varied:

| case | input SHA-256 | H lexical scopes | effective using edges | source lines | selected candidates |
| --- | --- | ---: | ---: | ---: | ---: |
| small | `e2cbb96e747b59ecf24bc1476bfe4dd29902695cec82c261a3d6f2c6cc0b3ba7` | 11 | 8 | 32 | 1 (`target::f`) |
| large | `8094f1fa118e8c3dee78f7d57f0ff293ac57a738eff96640a1745dcf179053de` | 67 | 64 | 200 | 1 (`target::f`) |

Five interleaved rounds of five batches per case timed ten invocations per
sample: 25 samples and 250 successful invocations per case. Median batch
measurements were:

| case | wall (s/10) | user (s/10) | system (s/10) | peak RSS (KiB) |
| --- | ---: | ---: | ---: | ---: |
| small | 0.03 | 0.01 | 0.02 | 7080 |
| large | 0.05 | 0.02 | 0.02 | 7116 |

Separate repeated runs for both inputs exited 0 and produced byte-identical
dumps, with `target::f` selected. The roughly 6.1x increase in H and 8x
increase in effective edges produced bounded near-linear structural growth,
not the prior per-level ancestry/common-ancestor rescan shape: the observed
median batch cost changed from 0.03 s to 0.05 s while peak RSS stayed within
36 KiB.

The direct-initializer probe uses `using FP = int (*)();`, overloads `f()`
with `f(int)`, and initializes `FP p(f)`. It exits 0 and dumps `p` as a
pointer to function returning `int` with an `id-expression` of function
returning `int`, proving target selection of `f()`. The parser probe exits 0
for `const`, `volatile`, pointer, const-pointer, reference, and genuine
function-declaration forms; focused negative controls preserve ambiguous-call
failure behavior.

## checkpoint ledger

| row | evidence / ownership |
| --- | --- |
| turn-start | Clean `994a7000`; PA12 **113/166**, exact **53** failures, all covered; through PA11 **685/685**. |
| implementation | Exactly five changed files: four approved `dev/src` owners plus `pa12/plan.md`; no tests, refs, fixtures, grammar, harnesses, scripts, or generated artifacts. |
| focused | Checkpoint, namespace/using controls, alias cv/pointer/reference controls, function declarations, and negative ambiguity controls: **15/15 PASS**. |
| probes | Direct overloaded-function initialization, common-ancestor overload merge, cyclic/transitive deterministic graph, and parser declaration probe all exited 0; repeated lookup dumps were byte-identical. |
| broad PA12 | Required `make test-pa12`: **120/166 passed**, **46 failed**, all 166 covered; normalized delta **+7 passed / -7 failed**, current-only **0**, baseline-only exactly the **7 fixed checkpoint paths**. |
| through PA11 | Exact required through command: **ALL TESTS PASSED SUCCESSFULLY! (685 / 685)**. |
| file audit | `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src`: passed with the two pre-existing header-division warnings. |
| diff check | `git diff --check`: passed. |
| commit | Existing PA12-focused commit is amended in place with this typed effective-scope performance repair; post-amend review and clean-status checks are recorded in the handoff. |
