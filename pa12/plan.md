# PA12 namespace scope/lookup checkpoint

## Stage Design

`PA11SemanticModel` remains the sole semantic owner. PA10 syntax is reduced to
typed `NameId`, `TypeId`, `ScopeId`, `BindingId`, `ValueRef`, and `SourcePoint`
facts; PA12 records resolved expression facts; the cold renderer prints
syntax occurrences and canonical declaration qualification. Each unnamed
namespace has one stable `ScopeId` per enclosing namespace and is reopened
through that identity. Namespace-owned type declaration points and inline
marker points are sparse sidecars; lookup collects namespace candidates by
canonical `ScopeId` and type candidates by stable originating declaration
`BindingId` for ambiguity, deduplication, and direct-hiding decisions, returning
`TypeId` only at the consumer boundary.

## Failure Map

The audited six-test family is:

- `pa12/tests/general/300-namespace-function-body-later-anonymous-overload.t`
- `pa12/tests/general/300-qualified-direct-function-hides-using-directive.t`
- `pa12/tests/general/300-reopened-unnamed-namespace-call.t`
- `pa12/tests/general/300-unnamed-namespace-definition.t`
- `pa12/tests/general/300-unnamed-namespace-qualified-call.t`
- `pa12/tests/general/300-unnamed-namespace-unqualified-call.t`

The exact turn-start and final residual set is `155/166` passing, exactly `11`
failures, with `166/166` paths covered:

- `pa12/tests/general/300-decltype-functional-cast.t`
- `pa12/tests/general/300-local-extern-function-declaration.t`
- `pa12/tests/general/300-member-function-pointer-return-pointer-const.t`
- `pa12/tests/general/300-member-function-pointer-type-alias-and-function.t`
- `pa12/tests/general/300-member-pointer-type-alias-and-function.t`
- `pa12/tests/general/300-multidimensional-array-const-reference-binding.t`
- `pa12/tests/general/300-reference-binding-pointee-const-pointer.t`
- `pa12/tests/general/300-scoped-enum-functional-cast-integral.t`
- `pa12/tests/general/300-static-cast-member-overload-prefers-nontemplate.t`
- `pa12/tests/general/300-static-cast-overloaded-function-template-argument.t`
- `pa12/tests/general/300-zero-arg-functional-cast-alias.t`

The six audited namespace paths are not residual failures. Final normalization
compared the fresh failure set with the supplied set: `0` current-only and `0`
supplied-only paths.

## Active Checkpoint

- `unnamed_namespace_index_` maps each enclosing `ScopeId` to one unnamed
  namespace. Its implicit visibility is one typed using relation on the
  enclosing namespace and effective ancestor; reopened definitions reuse the
  scope and do not duplicate the edge.
- Namespace scopes, value entries, explicit/effective using relations, and
  function-definition ownership retain typed source points. Lookup filters
  later namespace scopes, values, relations, aliases, and namespace-owned types
  at the function definition point. Qualified and global paths use the same
  point filter.
- Direct lookup is considered before using-directive lookup. Inline namespace
  children are traversed through the existing generation-marked lookup frames;
  repeated edges and cycles are deduplicated by typed scope marks. Candidate
  collection rejects distinct namespace targets by `ScopeId` and distinct type
  declarations by originating `BindingId`, while preserving repeated
  nominations of the same canonical target/declaration.
- Namespace aliases retain their canonical `NameId -> ScopeId` map plus a
  sparse `ScopeId`-keyed declaration-point sidecar. This repairs late-alias
  leakage without enlarging `Scope`, `Binding`, or `NamedRecord`.
- Namespace-owned types retain a sparse `ScopeId`-keyed
  `TypeDeclarationRelation` list for typedefs, aliases, class/enum records,
  and type using-declarations; each relation carries its originating binding
  identity for lookup ambiguity. Optional declaration identity travels through
  qualified/unqualified `lookup_type_path` into type-using formation. Reopened
  inline markers retain sparse marker points, preventing later inline exposure
  to earlier bodies.
- Call-shaped `decltype` uses ordinary typed lookup and accepts one matching
  function by arity, rejecting no-match and same-arity ambiguity. Indirect
  function types receive an arity check; no broad conversion semantics are
  added.

## Spec Alignment

Sections 1-3 of `spec.md` are respected by the one forward PA10 -> PA11 ->
PA12 fact pipeline, one semantic owner, stable typed identities, and cold-only
rendering. Section 4 is handled with typed namespace/type/inline declaration
points, sparse sidecars, bounded candidate scans, generation-marked work, and
no whole-program retry or broad invalidation. Section 7 is addressed with
deterministic vector traversal, unchanged `Binding`/`NamedRecord`/`Scope`
layout, a 3000-line lookup core at the file-audit limit, and no production
name rendering or reparsing in lookup.

## Focused Evidence

| evidence | result |
|---|---|
| `make -C pa12 -j2` | exit `0` |
| six checkpoint tests plus 13 nearby PA12 namespace/using/alias/conflict controls | `19/19` |
| 12 nearby PA11 namespace/using/alias/reopen controls, including the graph-exposed regression control | `12/12` |
| nine late namespace-owned type probes, including qualified/global and using-declaration paths | all exit `1` as required |
| distinct same-`TypeId` type declarations, same-origin repeated/transitive/graph-exposed type using-declarations, direct hiding, and inline-sibling probes | expected exits: distinct declarations ambiguous `1`; same-origin dedup/direct hiding `0`; distinct inline siblings ambiguous `1` |
| reopened inline marker before/after function body | expected exits `1/0` |
| using-directive cycle and repeated-edge probes | expected exits `0/0` |
| call-shaped `decltype` ambiguity, direct arity, and indirect arity probes | expected exits `1/1/1` |
| cv-redeclaration, conflicting return, duplicate definition probes | expected exits `0/1/1` |
| two runs of four checked-in namespace shapes | `cmp` exit `0` for each; one stable hash per shape |
| `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src` | exit `0`; exactly two existing header warnings |

The retained deterministic hashes are `b188ba30a9e560d9d28c472b05dfeea38c849f33237ae9a8de12a8882067f293` (later anonymous overload), `51b3d86bc87e2e3d69bad287c38c23d89f0f90cc1dd0e089ac9e975c3ed2d968` (reopened unnamed namespace), `e96947e6b28aea4012231d6869db1cd40709749cb694cb42838081ee309e34a9` (unnamed qualified call), and `bb94887ce1da0d9b5cf4eea0c406e24618f3276b6bc326282e56916c5ee6b3e` (qualified direct hide).

The retained parent-versus-checkpoint layout evidence was: `Binding 80 ->
80`, `NamedRecord 120 -> 120`, `Scope 440 -> 440`, `ValueEntry 16 -> 24`,
`UsingDirectiveRelation` absent -> `16`, and `EffectiveUsingDirective 16 ->
24`. The current repair adds only sparse alias/type relation lists and inline
marker points; their measured values are listed above.

The current layout probe reports `Binding 80`, `NamedRecord 120`, `Scope 440`,
`ValueEntry 24`, `UsingDirectiveRelation 16`, `EffectiveUsingDirective 24`,
`NamespaceAliasRelation 16`, `NamespaceAliasList 24`,
`TypeDeclarationRelation 24`, `TypeDeclarationList 24`, and `SourcePoint 8`
bytes. No timing, RSS, scaling, or full-suite performance claim is made from
these focused structural measurements.

## Checkpoint Ledger

| checkpoint | exact evidence | result |
|---|---|---|
| Turn-start gate | supplied starting result and final rerun of `make test-pa12`: `155/166`, 11 failures, `166/166` covered | rerun; exact residual set preserved |
| Landed namespace/type audit and repair at `1a150235` | build, focused PA12 `19/19`, PA11 `12/12`, declaration-point/declared-identity ambiguity probes, deterministic output, layout, exact broad residual normalization, through-PA11, and file audit above | completed in the amended existing checkpoint commit; exact residual set preserved; five approved paths and a clean tree verified |

## Next Checkpoint

This checkpoint is complete in the amended existing commit; all required gates
and the exact five-path audit passed, and `git status --short` is empty. The
next checkpoint is a separately authorized residual family: the exact eleven
paths listed in the Failure Map (decltype functional-cast, local-extern,
member-pointer, reference-binding, scoped-enum-cast, and static-cast families).
