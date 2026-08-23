# Current Checkpoint Review

This is the bounded checkpoint audit of landed increment
`d24f8e1689130b0449e19654ffd9e9f3dfc3b853`, `PA10: parse structured new
expressions`, whose parent is `a35dfc17`.  The review covers that exact
parent diff, the affected parser-support/AST/renderer sources, `pa10/README.md`,
the new-expression/type-id/placement/abstract-declarator/direct-
abstract-declarator/initializer grammar pages, and root `spec.md` §§1-4 and
§7.  No test, reference, supplied harness, or residual-family source was
changed.

## Contract and specification alignment

The audited path is one typed forward path:

```text
phase-3 source buffer -> typed posttokens and indexed facts
    -> PA10Parser new-expression selection and canonical productions
    -> typed GlobalScope/NewPlacement/NewExpression/TypeId/Initializer AST
    -> cold deterministic renderer
```

The PA10 grammar requires `new-expression` to own optional global scope,
optional placement, a type-id, and an optional new-initializer.  A type-id may
carry the complete abstract-declarator/direct-abstract-declarator production,
including pointer/reference/member-pointer operators, nested parenthesized
declarators, array suffixes, parameter clauses, and function suffixes.  The
initializer path remains separate, including `new T(*p)`, `new T((x))`, and
`new T(int())`.

`delimiter_close_index_` remains the typed owner of balanced parenthesis and
bracket boundaries.  The bounded correction adds the one-byte-per-token
`new_abstract_declarator_group_` fact.  `build_indexes` clears and fills it
once from those indexed boundaries, with the existing template-close and
RShift facts.  Its four values mean `none`, abstract shape, parameter clause,
or a nested parameter-only shape that is admitted only by the explicit
parenthesized type-id spelling; the last distinction preserves initializer
ambiguities such as `new T((x))`.  It does not build AST, parse expressions,
or consume a second production.  The canonical parser consumes the selected
abstract-declarator production once, including the full suffix/function-
suffix path.

The AST and renderer retain explicit `GlobalScope`, `NewPlacement`,
`NewExpression`, and `PackExpansionExpression` nodes.  Renderer validators
continue to enforce child order, count, token identity, and sidecar bounds.

## Ownership-path findings and bounded repairs

1. The d24 parenthesized classifier recognized only exact pointer/reference
   groups and one exact nested pointer group.  That left grammar-valid forms
   such as `new (int (*[3])())`, nested direct abstract declarators, and
   qualified/member-pointer forms outside the required path.  The correction
   replaces that narrow decision with an indexed group fact.  It recognizes
   pointer/reference operators, cv qualifiers, array/function suffix starts,
   nested group facts, and qualified member-pointer spines using the existing
   template-close/RShift indexes.  It deliberately keeps the ambiguous
   nested-parameter marker out of unparenthesized new type-id selection,
   preserving identifier-led and parenthesized initializer siblings.

2. The correction is not the removed recursive `new_abstract_declarator_at`
   support parser.  It is a precomputed typed ambiguity fact, built once for
   the token buffer.  It performs no source-text rescan, retry, backtracking,
   per-new-expression walk, or AST construction.  Once selected, the existing
   canonical `parse_abstract_declarator` owns nested, member-pointer,
   array/function-suffix, parameter-clause, and function-suffix consumption.

3. The renderer repair is retained because inline new-expression presentation
   manually unwraps the initializer instead of dispatching it through the
   normal child renderer.  The guard validates the initializer sidecar range
   before `children.front()`, then validates the selected
   `ParenInitializer`/`BracedInitList` syntax sidecar before inline child
   emission.  The malformed invariant harness supplies an invalid syntax
   name-prefix range and gets the renderer exception at this boundary; this
   is a real unchecked-sidecar boundary, not merely an earlier copy of a
   later rejection.  Valid output is unchanged.

4. All index vectors are sized/cleared on every `build_indexes` call, use the
   token-count sentinel for missing closes, and fail closed for malformed
   template/delimiter facts.  The reverse group pass classifies each delimiter
   once; nested delimiters terminate the owning leading-spine scan, and
   template/member-pointer steps use O(1) indexed closes.  The returned
   counted work includes the ordinary token pass and every fact predicate or
   scan step, and the parser charges that exact return value.  Parser work,
   recursion, angle, non-angle, and renderer nesting limits remain active.

5. No implementation work entered the 14 preserved residual identities,
   lambda/general-declaration/qualified-name fixes, or unrelated PA10
   surfaces.  No fixture or reference coverage was reduced.

## Focused evidence

The final focused checks on the corrected source state were:

```text
make -C dev cppgm++ CXX=g++                                  exit 0
four direct .t/.ref AST comparisons                          4/4 exact
new-expression positive/sibling/negative/malformed matrix   32/32 expected statuses
renderer malformed-sidecar invariant harness                 exit 0
build_indexes reset/reuse/index harness                      exit 0
g++ -Wall -Wextra -Werror syntax checks                      3/3 pass
```

The exact fixture comparisons were:

```text
pa10/tests/general/200-parenthesized-new-type-vs-placement.t
pa10/tests/general/200-placement-new-identifier-led-initializer.t
pa10/tests/general/200-placement-new-pack-init.t
pa10/tests/general/100-new-delete-traits.t
```

The 32-case matrix covered initializer siblings, global and placement new,
direct/nested pointer/reference forms, array/function suffix structure,
qualified/member-pointer and nested-template forms, and truncated/malformed
groups.  It included `new int(*p)`, `new int((x))`, `new (int())`,
`new (int (*[3])())`, `new (int (A<T>::*)())`, and global member-pointer
forms.

## Required broad evidence and residual map

Fresh required-stage execution gave:

```text
make test-pa10                                             exit 2
159 discovered, 145 passed, 14 failed
```

The failures are exactly the original residual identities, with no new
failure and no coverage reduction:

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

The exact through-stage command

```text
n=10; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
```

exited 0 with `457 / 457` through-PA9.  The required file audit

```text
perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src
```

exited 0: all files passed, with one pre-existing warning at
`dev/src/cpp_semantic_core.h:1` (`bad-division`, implementation body in a
header).  No new file-audit warning was introduced.  `git diff --check` also
passes on the final diff.

## Performance and structural bounds

The prior first-milestone 20-run characterization of
`200-placement-new-pack-init.t` and
`200-parenthesized-new-type-vs-placement.t` is retained as historical
d24-only evidence.  It applies to the earlier immutable executable and is not
used as a comparative claim for this corrected source.

For the corrected final executable, an immutable copy was hashed before and
after the repeated runs with the same result:

```text
SHA-256 bfc4058782989d23df54a173a9d7321facba3592c7176602dbd83759d9afa8c7
```

Twenty repeated invocations per equivalent input, timed as one aggregate
loop on that immutable executable, produced this characterization:

| input | runs | elapsed | user | sys | peak RSS |
| --- | ---: | ---: | ---: | ---: | ---: |
| `200-placement-new-pack-init.t` | 20 | 0.06 s | 0.03 s | 0.02 s | 4364 KB |
| `200-parenthesized-new-type-vs-placement.t` | 20 | 0.05 s | 0.02 s | 0.03 s | 4368 KB |

These are process-launch-dominated characterization measurements, not a
comparative performance claim.  Structurally, the new fact is one byte per
token, is reset and produced in the one global index-building phase, and is
consulted by the parser rather than recomputed for each new-expression.  The
current-source reset/reuse harness returned identical counts on both builds;
shape inputs grew from 6 tokens/43 work units to 641/4996, and member-pointer
inputs from 10/92 to 1153/11268.  The delimiter-owned spine argument and
these bounded counters support amortized linear construction; every counted
fact unit is charged to the global work limit.  There is no text retry or
per-new-expression rescan.  The measurements do not claim more than these
observations establish.

Final affected-source line counts are:

```text
dev/src/pa10_ast.cpp            2999 lines
dev/src/pa10_parser_support.cpp  889 lines
dev/src/pa10_parser_support.h     41 lines
dev/src/pa10_renderer.cpp       1017 lines
```

The broad audit, rather than these counts alone, is the source-shape exit
criterion.

## Risks and next checkpoint

The selected path now has no known relevant abstract-declarator correctness
gap in the audited grammar family.  The remaining uncertainty is limited to
the PA10 mock-name ambiguity policy outside this new-expression ownership
trace; those decisions remain governed by their existing boundaries and the
residual set above.  The file-audit warning is pre-existing and unrelated.
The next checkpoint is a separately assigned residual-family audit; it must
not enter the 14 identities listed above or unrelated PA10 surfaces.

## Historical prior-checkpoint evidence

These records are retained as history, not current claims:

- `a2b82dcb` established typed template components/sidecars and bounded close
  ownership; historical baseline 106/157.
- `27623d64` unified the declarator/member boundary and bounded declarator
  shape.
- `b9b58b9c` audited that boundary at 123/157 and retained through-PA9
  457/457 plus the pre-existing file-audit warning.
- `08c38115` routed structured names and special members and removed 12 prior
  residuals.
- `017eb658` was the prior clean turn-start at 158/136 with 22 failures.
- `25f784873f2a852fd825316b2188d9f157f8eae5` is the prior committed typed
  postfix checkpoint with its historical focused/broad evidence.

## Audit ledger

| checkpoint | review result | owner action | validation state |
| --- | --- | --- | --- |
| `a2b82dcb` template/angle ownership | historical | retain typed template components and bounded close ownership | historical 106/157 baseline |
| `27623d64` declarator/member boundary | historical | retain unified declarator/member path and bounded shape | historical focused evidence |
| `b9b58b9c` declarator audit | historical | retain nearest-derived-operator and member-pointer bounds | historical 123/157; through-PA9 457/457 |
| `08c38115` structured names/special members | historical | retain one typed name/special-member path and validated sidecars | historical local 135/157; course 1/1 |
| `25f784873f2a852fd825316b2188d9f157f8eae5` typed postfix checkpoint | audited and committed | use one exact cast-keyword predicate, initialize indexes, route all RShift consumers through the marker, validate synthetic renderer nodes, and retain the 3000-line source bound | fresh 142/159 with exact original 17 failures; fresh through-PA9 457/457; file audit exit 0 with one pre-existing warning; focused 14/14 + 12/12 + regression 1/1; immutable performance characterization |
| `d24f8e1689130b0449e19654ffd9e9f3dfc3b853` structured new expressions | checkpoint audited; final gates pass | retain the typed indexed abstract-group fact, route complete abstract-declarator consumption through the canonical parser, validate inline initializer sidecars, and preserve global/placement/pack ownership | fresh 159/145 with exactly the original 14 residuals; through-PA9 457/457; file audit exit 0 with one pre-existing warning; focused 32/32 + exact refs 4/4 + warning/index/renderer harnesses; immutable final SHA-256 `bfc4058782989d23df54a173a9d7321facba3592c7176602dbd83759d9afa8c7`, aggregate 20-run characterization |
