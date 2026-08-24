# Current Checkpoint Review

This is the bounded Phase A audit of landed increment
`c16e04ef82e93bb0c628d2f495cc7132d47dd749` (`PA10: own elaborated type syntax
boundary`), whose parent is `a7c20b87`.  The review covers the exact parent
diff, `pa10/README.md`, the relevant class/enum/simple-declaration/
decl-specifier/type-id productions in `pa10/pa10.gram`, root `spec.md` §§1, 2,
4, and 7, and the owned parser-support/AST path.  No supplied test, reference,
status fixture, grammar, harness, renderer, or inactive residual source was
changed.

## Contract and ownership trace

The representative path is one typed forward flow:

```text
phase-7 typed tokens
    -> template-close/top-level-or/RShift/delimiter indexes
    -> PA10ParserSupport elaborated classification + charged_work
    -> PA10Parser dispatch and canonical decl-specifier/type-id AST path
    -> existing deterministic renderer (observation only)
```

PA10 is a syntax AST assignment: class/struct/union/enum declarations,
class members, class bases, class-key attributes, scoped and underlying-type
enums, and structured type-ids in `sizeof` must remain structured without
semantic lookup or source reparsing.  `classify_elaborated_specifier` owns
only the current header and its immediate delimiter decision.  It uses typed
template and delimiter facts, jumps over nested angle/parenthesis/bracket
groups, and returns the bounded work that `PA10Parser` charges before it
constructs nodes.

At the parser boundary, standalone forwards and definitions are selected by
the outer dispatch and their canonical parser owns the declaration semicolon.
An elaborated definition followed by a declarator is classified as embedded;
`parse_decl_specifier_seq` consumes the class/enum specifier with
`in_decl_specifier=true`, and `parse_simple_declaration` owns the trailing
semicolon.  This preserves the exact AST shape for anonymous/named embedded
definitions, class-member enum declarators, friend type declarations, and
`sizeof` elaborated type-ids.  The cold renderer was not edited because the
focused AST paths remain valid and deterministic.

## Findings and bounded correction

The c16 ownership split is sound for the active syntax boundary, but review
found three concrete fail-closed/accounting gaps in the support path:

1. A colon-clause lookahead treated every later token as part of the current
   header.  On malformed/truncated input it could pass an immediate enclosing
   close and observe a later class body or semicolon.  The classifier now
   returns `EmbeddedOrDeclarator` at `)`, `]`, `}`, or typed EOF, while retaining
   indexed jumps for valid nested delimiters and template groups.
2. `template_follow_is_valid` formed `absolute_close + 1` before rejecting an
   invalid close.  It now rejects the token-count sentinel and other
   out-of-range closes before arithmetic and reports the bounded result.
3. `skip_balanced_delimiters` and `skip_attribute_specifiers` could return
   false without publishing the scan cursor/count, and the parser threw before
   charging that count.  Both helpers now initialize/publish `after` and
   `consumed` on success and failure; `consumed` counts examined token
   positions, including a present failing token.  The parser charges the
   published count before calling `fail()`.

Malformed attribute lookahead also reports the known elaborated token as an
embedded/fail-closed context rather than the unrelated `NonElaborated` default.
These corrections are confined to the owned support/parser path; they do not
add a parser, AST, semantic model, or renderer path.

All side indexes are reset/sized by `build_indexes` on each invocation.  The
template-close and delimiter helpers check their vector bounds, split-RShift
access is marker-guarded, and missing indexed closes use the token-count
sentinel.  The index pass and reverse group pass return counted work, and the
parser charges that returned work against its global limit.  The classifier's
linear cursor is input-bounded and now stops at its immediate owner on
malformed input; no unbounded body scan, text reparse, retry, backtracking,
duplicate AST construction, semantic lookup, or host/reference shortcut was
introduced.

## Phase A focused evidence

Fresh checks on the corrected source:

```text
make -C dev cppgm++
```

Exit 0.

```text
make -C pa10 check TEST='tests/general/200-elaborated-enum-member-declarators.t tests/general/200-friend-type-declaration.t tests/general/200-sizeof-elaborated-class-type-id.t'
```

Exit 0; `pa10 check: PASS (3/3)` with exact checked-in AST/status
comparisons.

The representative sibling/malformed/index command covered 16 tests:

```text
100-class-alignas-after-class-key
100-scoped-enum-underlying-type
100-structured-type-id
100-typedef-anonymous-enum
100-typedef-anonymous-union
100-typedef-struct-union
100-rshift-piece-normalization
200-dependent-template-keyword-nested-angle
200-nested-qualified-template-id-template-args
200-decltype-base-and-mem-initializer
200-qualified-base-and-ctor-init
spec/100-enum
spec/200-class-bases-and-ctor-init
spec/300-type-id-expression-contexts
200-class-definition-missing-semicolon-bad
200-malformed-template-parameter-clause
```

The command exited 0 with `pa10 check: PASS (16/16)`.  Direct production
probes for `struct X :`, `enum E :`, and an elaborated clause followed by an
enclosing close each returned exit 1 within a 5-second timeout.

The direct temporary support harness was compiled outside the repository with:

```text
g++ -std=gnu++11 -Wall -Wextra -O0 -ffunction-sections -fdata-sections -Idev/src /tmp/pa10_support_boundary_harness.cpp dev/src/pa10_parser_support.cpp -Wl,--gc-sections -o /tmp/pa10_support_boundary_harness
/tmp/pa10_support_boundary_harness
```

It exited 0 with `pa10 support boundary harness: PASS`.
It asserted sentinel/out-of-range template closes return false with zero
follower work; truncated `__attribute__((((` publishes `after=4`,
`consumed=4`, and classifier work 4 with `EmbeddedOrDeclarator`; colon clauses
stop at an immediate close and at EOF without borrowing a later body; a
missing attribute `(` publishes `after=1`, `consumed=2` and tolerates null
output pointers; index reset/reuse is deterministic; and valid attribute, base,
underlying-type, and split-RShift cases retain their expected contexts.  This
harness directly checks support outputs; the parser charge-before-fail ordering
is evidenced by the reviewed source sequence and the malformed production
probes, not by an output-oracle comparison.  No reference binary or generated
fixture was used.

## Structural performance evidence

No comparative timing claim is made in Phase A.  The equivalent structural
evidence for the material bounded-lookahead claim is:

| representative fact | observed source bound |
| --- | --- |
| index setup | one forward token pass plus one reverse pass over delimiter opens; every output vector is reset and missing facts are sentinels |
| nested templates/RShift | indexed close lookup and the existing nested-close marker jump over each matched angle group without text scanning |
| class bases/enum underlying types | classifier advances only through the current header, jumps matched `()`/`[]` and template groups, and stops at body, semicolon, close, or EOF |
| template follower | at most the split-RShift pair and one follower token are examined, with invalid closes rejected first |

The 3/3 and 16/16 focused matrix exercises these valid and malformed
representatives.  The work returned by support is charged by the parser, and
the existing global, recursion, delimiter, angle, and renderer limits remain
in force.  This is a structural bound, not a timing comparison.

## Required broad evidence and residual map

The turn-start required-stage baseline was 159 discovered, 148 passed, and 11
failed.  The exact identities remain unchanged and inactive:

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

Fresh post-review Phase B execution completed after the accounting correction:

```text
make test-pa10                                             exit 2
159 discovered, 148 passed, 11 failed; exact residual set unchanged
n=10; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
                                                          exit 0, 457/457
perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src
                                                          exit 0, one known warning
git diff --check
                                                          exit 0
make -C dev clean; make -C dev cppgm++
                                                          exit 0
```

The PA10 failures are exactly the 11 inactive identities listed above, with
no coverage reduction or identity change.  The file-audit warning is the
known pre-existing `bad-division` warning at
`dev/src/cpp_semantic_core.h:1` for substantial header implementation body.
The clean dev rebuild completed after the broad run; no implementation source
changed between those validations.

## Risks and next checkpoint

The focused and broad review found no remaining correctness gap in the owned
elaborated-type path.  The 11 residual identities are explicitly out of scope
and must not increase or change identity.  The next checkpoint is a separately
assigned residual-family audit and must not enter lambda, general
declaration/declarator, qualified-name, trailing-attribute, or unrelated PA10
surfaces.

## Historical checkpoint ledger

The historical rows are retained below; the final row is the single current
c16 audit row.

| checkpoint | review result | owner action | validation state |
| --- | --- | --- | --- |
| `a2b82dcb` template/angle ownership | historical | retain typed template components and bounded close ownership | historical 106/157 baseline |
| `27623d64` declarator/member boundary | historical | retain unified declarator/member path and bounded shape | historical focused evidence |
| `b9b58b9c` declarator audit | historical | retain nearest-derived-operator and member-pointer bounds | historical 123/157; through-PA9 457/457 |
| `08c38115` structured names/special members | historical | retain one typed name/special-member path and validated sidecars | historical local 135/157; course 1/1 |
| `25f784873f2a852fd825316b2188d9f157f8eae5` typed postfix checkpoint | historical (audited and committed) | use one exact cast-keyword predicate, initialize indexes, route all RShift consumers through the marker, validate synthetic renderer nodes, and retain the 3000-line source bound | historical fresh 142/159 with exact original 17 failures; through-PA9 457/457; file audit exit 0 with one pre-existing warning |
| `d24f8e1689130b0449e19654ffd9e9f3dfc3b853` structured new expressions | historical (prior checkpoint audited) | retain the typed indexed abstract-group fact, distinguish definite from identifier-led nested parameter clauses under pointer/member-pointer spines, route complete abstract-declarator consumption through the canonical parser, validate inline initializer sidecars, and preserve global/placement/pack ownership | historical fresh 159/145 with exactly the original 14 residuals; through-PA9 457/457; file audit exit 0 with one pre-existing warning; focused 55/55 + exact refs 4/4 + warning/index/renderer harnesses; immutable performance characterization recorded in that prior audit |
| `c16e04ef82e93bb0c628d2f495cc7132d47dd749` current elaborated-type boundary | completed; residuals remain | publish/charge malformed attribute scan work, stop colon lookahead at immediate owners, reject invalid template closes, preserve one canonical class/enum AST path and semicolon owner | focused 3/3 + 16/16; direct support harness PASS; PA10 148/159 with exact 11 residuals; through-PA9 457/457; file audit exit 0 with one known warning; clean rebuild and diff check pass |
