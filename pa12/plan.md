# PA12 block-scope declaration and lookup-continuity checkpoint

## Stage Design

The production route remains one typed owner/data flow:

`PPPreprocessingSession -> PPTokenBuffer -> PA10Ast -> PA11SemanticModel -> PA12 typed facts -> cold deterministic dump`

PA11 owns canonical `TypeId`, `ScopeId`, `BindingId`, `NamePath`, and lookup.
`process_compound_statement` forms every supported block declaration exactly
once in source order: `AliasDeclaration`, `NamespaceAliasDefinition`,
`UsingDirective`, `UsingDeclaration`, and `SimpleDeclaration`. PA12 consumes
the resulting facts; lookup-only declarations have no statement line, while a
local alias is a typed `TypeAlias` fact and `type-alias` dump line. Declaration
nodes reached through an implicit unbraced substatement or label/case edge are
formed by `prepare_pa12_statement` in that edge's canonical scope. The
compound preparation loop skips its direct declaration children because the
PA11 source-order pass already formed them.

The bounded audit repair also carries the stored scope depth into PA12's
internal unbraced-control scopes, and routes top-level aliases through the same
cached `TypeAlias` fact used by block aliases before cold rendering.

Using-declaration entries are typed `(BindingId, ScopeId)` pairs. The binding
is the canonical source declaration and the scope is its source provenance;
the importing scope gets only a lookup entry and a cold dump view, never a
second semantic declaration or a rendered-name key. Same-scope using
declarations merge only function bindings into one overload set; exact
`(BindingId, origin ScopeId)` pairs and their per-scope dump views are
deduplicated, while non-function collisions remain errors.

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
using directive, where H is its scope depth; same-name using-declaration merge
work is bounded by the existing and incoming candidate entries. Each query
costs one O(H) lexical mark pass plus scans of effective entries at visited
levels and reachable lookup graph/candidate work. It does not rescan the start
ancestry per level or edge. There is no whole-arena scan, retry loop,
rendered-name reparsing, or hash-order output.

## Failure Map

The reviewed checkpoint starts from landed commit `61b60cb1` (`pa12: preserve
block lookup provenance`) and its supplied **120/166 passing**, exact **46
failure** baseline, all 166 covered. The final broad result remains **120/166**
with the earlier through-PA11 gate at **685/685**.

| partition | total | passed | failed |
| --- | ---: | ---: | ---: |
| `tests/spec/100-*.t` | 12 | 12 | 0 |
| `tests/general/100-*.t` | 25 | 25 | 0 |
| `tests/spec/200-*.t` | 9 | 9 | 0 |
| `tests/general/200-*.t` | 33 | 29 | 4 |
| `tests/spec/300-*.t` | 10 | 10 | 0 |
| `tests/general/300-*.t` | 77 | 35 | 42 |
| **all PA12 final broad** | **166** | **120** | **46** |

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

The final broad run and the supplied primary log have the same exact residual
inventory: **46 failures**, all 166 covered, with zero current-only and zero
baseline-only paths. The residual inventory is:

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

The landed five-file implementation increment is complete at `61b60cb1` and
this audit found two additional bounded repairs in its approved PA12 owner.
The owner-level changes are:

- PA10 declaration disambiguation for cv-qualified and pointer/reference
  named-type forms, with declaration-follow validation;
- PA11 one-pass block declaration formation, paired source-provenance value
  entries, nearest-common-ancestor using lookup, and direct-initializer target
  recognition, including typed same-scope function-using overload merging and
  pair/dump-view deduplication;
- PA12 lookup-only statement suppression, typed local-alias facts, function
  provenance in dumps, and target-directed direct initialization.
- PA12 internal unbraced-control scope depth propagation for common-ancestor
  lookup;
- PA12 direct unbraced/label/case declaration preparation without reprocessing
  direct compound children;
- cached top-level `TypeAlias` facts for cold PA12 rendering, eliminating the
  dump-time type reconstruction path.

The checkpoint remains intentionally scoped to block declarations, aliases,
using continuity, lookup precedence, and direct initialization. The final
46-path residual set remains owned by unrelated later expression/control
families. This bounded checkpoint audit is complete; a future residual-family
pass is a separate checkpoint.

## Performance Evidence

Fresh out-of-tree probes exercised direct unbraced selection/iteration
declarations, nested implicit scopes, label/case preparation, direct
overloaded-function initialization, same-scope using-function merge and
repeat-import deduplication, common-ancestor overload merge, and an `a <-> b`
transitive/cyclic using graph. Valid direct, case, iteration, and nested-scope
probes exited 0. Invalid targets and post-substatement name uses exited 1;
the valid labeled-alias probe reaches the pre-existing unsupported labeled
semantic statement path, while an invalid labeled target is rejected during
declaration preparation.

The parser evidence is concrete: `PA10Parser::parse_statement` checks
`declaration_start()` and returns `parse_declaration()` for direct substatements
(`dev/src/pa10_ast.cpp:2360-2361`), and `--emit-ast` emitted the expected
`using-directive`, `using-declaration`, and `alias-declaration` branch/case
children. No grammar or parser change was needed.

The refreshed immutable scaling executable was copied from `dev/cppgm++` to
`/tmp/pa12-checkpoint-audit-immutable-61b60cb1-direct-using`, SHA-256
`2fc6d0b184ed8a5a6aea86224607b9ef4ad3b04f93975040e3aab0b2156a680b`.
Equivalent inputs used one declaration, one call, one selected candidate, and
only nested lexical depth/effective using-edge count varied:

| case | input SHA-256 | H lexical scopes | effective using edges | source lines | selected candidates |
| --- | --- | ---: | ---: | ---: | ---: |
| small | `7e3ee5825d46edb515ac5b964390f490501200b4c2a697817630fbbcf1c1b2f1` | 11 | 8 | 23 | 1 (`target::f`) |
| large | `2a0f6e8fa07a47af133d2f09bba986ec74537d6e6abaaa48dc1f4d9a3eb002ab` | 38 | 35 | 77 | 1 (`target::f`) |

Five interleaved rounds timed ten invocations per sample: 5 batches and 50
successful invocations per case. Refreshed median batch measurements were:

| case | wall (s/10) | user (s/10) | system (s/10) | peak RSS (KiB) |
| --- | ---: | ---: | ---: | ---: |
| small | 1.06 | 0.03 | 0.06 | 7206 |
| large | 1.06 | 0.03 | 0.06 | 7194 |

Separate repeated runs for both inputs exited 0 and produced one byte-identical
dump hash per case, with `target::f` selected. The roughly 3.5x increase in H
and 4.4x increase in effective edges remained within the coarse refreshed
1.06-second batch wall median and nearly unchanged peak RSS. This supports the
documented bounded lexical-mark/effective-entry and reachable-graph work; it
is not a claim about the full PA12 suite or a fine-grained scaling coefficient.

The direct-target input hash is
`60ff14d8c359511d497a90129726a23afd0d6193bed743ecea7209582f6a88a4` and its
repeated output hash is
`bff28ef00c661139964e5c478c46a6661aa2229cb81cc4a4d25273dbce828b6b`. The
cyclic input hash is
`bb50766e3aff8dd32140df018a26c75c0454bbb76a5676abc07524aaa3ad1850` and its
repeated output hash is
`8f7bbacd28fed9d4bcc70f79db2ae5cae80f273cebb68c862b9126db467746d7`.
The focused parser/declaration controls cover `const`, `volatile`, pointer,
const-pointer, reference, and genuine function-declaration forms.

## checkpoint ledger

| row | evidence / ownership |
| --- | --- |
| turn-start | Clean `61b60cb1`; supplied PA12 **120/166**, exact **46** failures, all covered; through PA11 **685/685**. |
| implementation | Landed five-file increment audited; unbraced/label/case declaration preparation and typed same-scope function-using merge/dedup repairs are included in the final four-file checkpoint-audit change; no tests, refs, fixtures, grammar, harnesses, scripts, or generated repository artifacts changed. |
| focused | Exact checkpoint paths **7/7 PASS**; expanded ownership/lookup controls **14/14 PASS**; the one additional qualified-`decltype` control remains its supplied residual failure. |
| probes | Valid direct selection/iteration, alias, case, nested-scope, overload-merge, and repeat-import probes exited 0; invalid target/collision/non-leakage probes exited 1; merge selected both `left::f` and `right::f`, and repeat `--emit-types` showed one block `f` view. |
| performance | Refreshed immutable executable/input hashes, five interleaved 10-run batches, 50/50 successful invocations per case, medians, RSS, selected candidates, and one output hash per case are recorded above. |
| broad PA12 | Exact `make test-pa12` exited 2 with **120/166** passed and **46** failures; all 166 covered; normalized against the supplied log as **46 baseline / 46 current / 0 current-only / 0 baseline-only**; none of the seven checkpoint paths regressed. |
| through PA11 / file audit | Exact through-PA11 command passed **685/685**. Exact file audit passed with the two known warnings: `cpp_semantic_core.h:1` and `pa11_semantic_model.h:1` substantial header implementation bodies. |
| diff / paths / commit | `git diff --check` passed; diff from `61b60cb1` contains exactly the four approved paths and no tests, refs, fixtures, grammar, harnesses, scripts, or generated artifacts; final checkpoint-audit commit records this row. |
