# Current Checkpoint Review

This is the bounded audit of landed checkpoint
`a2b82dcb56245406f695c271a44ca55ca82f3949` and its final repair in the same
template-id, qualified-name, angle-token, decltype-prefix, and exact-renderer
ownership path. It does not attempt the remaining PA10 grammar families.

## Contract and specification alignment

The production path remains one forward flow:

```text
PPTokenBuffer -> posttoken facts -> PA10Token -> PA10 parser -> PA10Ast -> dump renderer
```

Identifiers retain producer `PPSpellingId` identity through the posttoken
collector and AST sidecars. The renderer is the requested text boundary; it
does not reparse joined names, invoke a host compiler or reference binary, or
recognize test names. `pa10_renderer.cpp` is already in
`dev/frontend_source_sets.mk`; this audit added no source file.

The reviewed productions are the qualified-id and nested-name-specifier,
simple-template-id and template-argument, template-disambiguator, and
decltype paths in `pa10/pa10.gram`, plus base and mem-initializer consumers.
The operator decision follows `pa10/pa10.gram:474-488,706-709`: relational
operators include `OP_GT`, while right shift is exactly
`ST_RSHIFT_1 ST_RSHIFT_2`; close-angle-bracket accepts either logical piece.
This is also the PA6 handout rule at `pa6/README.md:124-143`. The design
therefore aligns with root `spec.md` §§1-4 and §7: one production path, typed
fact continuity, canonical component/range ownership, bounded ordinary work,
compact hot records with cold presentation, and characterization without an
unverified performance comparison.

## Ownership trace

- The producer emits identifiers with `PPSpellingId`; `PA10Ast` snapshots the
  producer spelling table before the preprocessing session ends. A qualified
  name stores components, disambiguator state, and template-argument ranges,
  not a joined string. Fixed tokens remain `SimpleTokenType`; decoded literal
  data remains typed, with source spelling only as cold presentation.
- The posttoken collector splits one producer `OP_RSHIFT` into typed
  `RShiftPiece1` and `RShiftPiece2`, each presented as `OP_GT`. The parser uses
  one piece per nested close angle and recognizes a real shift only when the
  two pieces are adjacent. A lone `RShiftPiece2` is not relational `>` and it
  is not combined with a following ordinary `OP_GT`.
- `build_template_indexes()` makes one charged pass over the logical token
  stream. Each `(`, `[`, or `{` pushes a delimiter frame and a fresh angle
  stack; only a matching close pops that frame. Both RShift pieces are
  processed independently for close-index construction, so nested closes do
  not revisit or pair a piece incorrectly. Candidate lookup is indexed and
  the follow check is bounded lookahead.
- `PA10NameComponent` owns producer identity and a `(begin,count)` range into
  typed `PA10TemplateArgument` sidecars. Arguments preserve `TypeId`,
  `Expression`, or `Unresolved` kind. Direct arguments are moved into the
  arena only after nested parsing completes, and nodes retain indices rather
  than addresses, so vector growth does not invalidate an owner.
- A decltype root is one typed `DecltypeSpecifier` in the name-prefix
  sidecar. There is no standalone-decltype boolean in either `PA10Name` or
  `PA10AstNode`. The structural invariant is: a prefix with no name
  components is standalone; a prefix with one or more components is a
  qualified decltype root. `append_name()` emits `::` only in the latter
  case. The parser's `allow_standalone_decltype` parameter controls grammar
  admission but is not persisted as a second fact, so trailing-return copies
  copy only canonical prefix ranges and components.
- The same name path is used by namespace aliases, using declarations,
  declaration/type specifiers, declarators, member access, base names,
  constructor mem-initializer ids, and trailing-return presentation. Template
  disambiguators and conversion-function type-ids remain typed sidecar facts.

## Correctness, boundedness, and repair disposition

The audit found two in-scope repairs:

1. Base-specifier and constructor mem-initializer entry predicates now admit
   standalone `decltype(...)` names. Their nodes use the structural invariant
   above, so standalone output has no manufactured trailing `::`.
2. The renderer validates name-prefix, template-argument,
   operator-presentation, and semantic-child ranges before every inline or
   full-node traversal, and retains the shared recursion limit.

The provisional RShiftPiece2 repair was explicitly rejected after checking the
authoritative grammar and handout. Its acceptance of a leftover second piece
as relational `>` and its cross-boundary `RShiftPiece2 + OP_GT` shift were
reverted. The remaining angle behavior is limited to the checked logical
piece/close-angle rules and the exact adjacent two-piece shift production.
Synthetic probes that depended on the rejected transitions are not evidence.

Parser consumption is monotonic. The existing work budget is
`96 * token_count + 2048`, with nesting and recursion guards; index construction
charges each token and does not retry or backtrack. Angle and non-angle depth
updates have paired successful paths, and RAII recursion guards unwind on
parse exceptions. A malformed delimiter probe exits 1, and 1100 nested
parentheses stop at the existing recursion limit rather than exhausting the
stack.

The current source sizes are 2741 lines (`pa10_ast.cpp`), 346
(`pa10_ast.h`), and 700 (`pa10_renderer.cpp`), within the PA10 file-audit
limits. No shortcut, duplicate parser/model, render-and-reparse path, or
test-name recognition was found.

## Historical evidence retained from the landed checkpoint

The following measurements are explicitly pre-repair evidence from the landed
`a2b82dcb` checkpoint, not measurements of the final executable:

- The parent `43703613` record was 77/157. The landed handoff record was
  105/157 with 52 failures, 28 removed identities, and zero new identities.
- The focused owner set was 11/11; the warning-clean build passed; through-PA9
  was 457/457; and the file audit exited 0 with the sole warning
  `dev/src/cpp_semantic_core.h:1` (`bad-division`, substantial header body).
- Bounded landed probes measured: 140 template arguments at
  `0.00s/4340 KB`; triple adjacent closes plus a separate `x >> 1` at
  `0.00s/4120 KB`; sibling `()`, `[]`, and `{}` scopes at `0.00s/4352 KB`;
  relational chains of 32/128/256 at `0.00s/4372 KB`, `0.00s/4812 KB`, and
  `0.02s/5816 KB`; and the 512-pair case at the existing renderer depth guard,
  `0.02s/7980 KB`.
- Earlier retained storage characterization (also not current layout) was:

  | declarations | source bytes | PP tokens | AST nodes | child edges/capacity | literal nodes/bytes/capacity | producer bytes |
  | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
  | 32 | 438 | 289 | 289 | 288/288 | 32/128/128 | 295 |
  | 128 | 1810 | 1153 | 1153 | 1152/1152 | 128/512/512 | 611 |
  | 512 | 7570 | 4609 | 4609 | 4608/4608 | 512/2048/2048 | 2147 |

  The same earlier record measured `sizeof(PA10Ast)=312`,
  `sizeof(PA10AstNode)=216`, presentation capacity 3, and full-driver RSS
  samples of 4116/4848/6736 KB at 32/128/512 declarations. Those values are
  preserved for history only; removing the duplicate decltype flag means they
  are not a current layout claim.

## Final validation and performance evidence

- The warning-clean PA10 build with `-Wextra -Werror` exited 0.
- `make test-pa10` discovered all 157 tests and exited 2 with 106 passing and
  51 failing. Compared with the preserved turn-start set of 52, exactly
  `general/200-decltype-base-and-mem-initializer.t` was removed and no
  identity was added.
- The required through-PA9 command exited 0 with `457 / 457` passing.
- `perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src` exited 0:
  one warning, the pre-existing `dev/src/cpp_semantic_core.h:1` bad-division
  warning.
- `git diff --check` exited 0. Current malformed-input and depth probes exit
  1 with the expected parser diagnostics.

The current executable is `dev/cppgm++`, SHA-256
`6cf44a43a293399ec829c6423526516cb502ec3b198b74c30209589922d665df`. These
are single bounded characterization runs, not a §7 median/interleaved
comparison:

| probe | result | wall | peak RSS |
| --- | --- | ---: | ---: |
| 140 template arguments | success | 0.00 s | 4328 KB |
| nested `foo<bar<int>>` closes | success | 0.00 s | 4076 KB |
| real `(x >> 8)` shift | success | 0.00 s | 4148 KB |
| relational pairs 32 / 128 / 256 | success | 0.00 / 0.00 / 0.00 s | 4124 / 4124 / 4308 KB |
| 1100 parenthesized expressions | bounded failure at recursion guard | 0.01 s | 8812 KB |

The residual failures are unrelated PA10 families, including operator names,
attributes, lambdas, declarators, explicit instantiation, and template
parameters. The indexed candidate heuristic and cold-dump storage should be
re-measured when a later hot consumer uses this AST; no such performance claim
is made here.

## Audit ledger

| checkpoint | review result | owner action | validation state |
| --- | --- | --- | --- |
| `a2b82dcb56245406f695c271a44ca55ca82f3949` template-id / qualified-name checkpoint | bounded ownership audit complete; standalone decltype is structural, authoritative RShift behavior is preserved, and renderer sidecar ranges are checked | retain typed producer/name/argument ownership and defer unrelated PA10 families | final 106/157, 51 residuals, one removed identity and zero added identities; through-PA9 457/457; file audit exit 0 with one pre-existing warning |

## Next implementation checkpoint

Select one remaining PA10 grammar family under supervisor direction, add only
its public-owner repair in `dev/`, and rerun the same PA10 identity gate before
expanding scope. The template-id/qualified-name ownership path audited here is
closed for this checkpoint.
