# PA10 Checkpoint Plan and Evidence

## Stage Design

```text
phase-7 typed tokens/indexes -> PA10ParserSupport typed elaborated classification
    -> PA10Parser canonical class/enum/specifier owner -> deterministic renderer
```

The c16 boundary is owned by `PA10ParserSupport::classify_elaborated_specifier`
and its parser wrapper.  The support result classifies the current class/enum
header as non-elaborated, embedded/declarator-bearing, standalone forward, or
standalone definition, and returns the exact bounded lookahead work that the
parser charges before AST construction.  Existing template-close,
top-level-or, split-RShift, and delimiter indexes remain the typed fact owners.
The renderer is observation-only.  No source reparse, retry/backtracking
parser, duplicate AST path, semantic lookup, host/reference shortcut, or
fixture/reference edit is in scope.

The path matches PA10's structured class/struct/union/enum declarations,
members, class bases and class-key attributes, scoped/underlying-type enums,
structured type-ids in `sizeof`, and embedded definitions followed by
declarators.  Direct forwards/definitions own their semicolon; embedded forms
remain `simple-declaration` children of the canonical decl-specifier path.

## Failure Map

Turn-start required-stage evidence is **159 discovered, 148 passed, 11
failed**.  The exact inactive residual identities are:

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

The three checkpoint-owned elaborated-type identities are outside this
residual set and pass the focused rerun below.  No inactive family is being
widened into.

## Active Checkpoint

Phase A review of landed `c16e04ef82e93bb0c628d2f495cc7132d47dd749`
(`PA10: own elaborated type syntax boundary`, parent `a7c20b87`) is complete
with bounded same-path support corrections.  The classifier now stops at an
immediate enclosing `)`, `]`, `}`, or typed EOF while examining a colon clause,
so malformed/truncated input cannot borrow a later declaration's body or
semicolon.  The support template-follower helper rejects an out-of-range close
before doing `+ 1`; malformed index facts fail closed.  Attribute scans now
initialize and publish `after`/`consumed` on every path, including the failing
token, and the parser charges that published work before throwing.

The parser still constructs the existing canonical AST exactly once: direct
class/enum forwards and definitions are dispatched directly, while body-plus-
declarator forms enter `parse_decl_specifier_seq` and remain simple
declarations.  `sizeof(struct X)` uses `type-id`/`type-specifier-seq`; enum
underlying-type forwards and split-`>>` template names remain on their prior
typed paths.

## Focused Evidence

Fresh Phase A checks on the corrected source:

```text
make -C dev cppgm++                                      exit 0
make -C pa10 check TEST='<three checkpoint-owned tests>' exit 0, PASS (3/3)
make -C pa10 check TEST='<16 sibling/malformed/index tests>'
                                                          exit 0, PASS (16/16)
g++ -std=gnu++11 -Wall -Wextra -O0 -ffunction-sections -fdata-sections -Idev/src /tmp/pa10_support_boundary_harness.cpp dev/src/pa10_parser_support.cpp -Wl,--gc-sections -o /tmp/pa10_support_boundary_harness
/tmp/pa10_support_boundary_harness                         exit 0, PASS
git diff --check                                         exit 0
```

The 16-case matrix covered class-key attributes, scoped/underlying-type
enums, anonymous embedded class/enum definitions, structured type-ids,
class bases, split-RShift/nested-template names, and malformed class/template
boundaries.  Direct production probes for `struct X :`, `enum E :`, and an
elaborated clause followed by an enclosing close all returned exit 1 within a
5-second timeout.  The temporary support harness directly verified sentinel
and out-of-range template closes (`false`, zero follower work), truncated
`__attribute__(((` publication (`after=4`, `consumed=4`, classifier work 4,
`EmbeddedOrDeclarator`), missing-`(` publication (`after=1`, `consumed=2`)
with null output pointers, immediate-close and EOF stopping, deterministic
index reset/reuse, and valid attribute/base/scoped-enum split-RShift cases.
The harness was outside the repository and was not a compiler-output oracle.

## Performance and Bounds Evidence

This Phase A review makes no comparative timing claim.  The bounded equivalent
structural evidence is:

| path | bounded work fact |
| --- | --- |
| `build_indexes` | resets every side vector, performs one token pass, then one reverse delimiter-group pass; missing closes retain the token-count sentinel |
| template/base/underlying clauses | indexed template closes and split-RShift markers jump over nested angle groups; indexed delimiter closes jump over parenthesized/bracketed groups |
| elaborated classifier | advances through only the current header, stops at the immediate body/semicolon/close/EOF boundary, and returns `charged_work` for the wrapper to charge |
| template follower | examines at most the split-RShift pair and one follower token, rejecting an invalid close before indexing |

Representative valid and malformed class/enum/template fixtures exercised
these paths.  There is no text reparse, semantic lookup, retry, or body scan
after the indexed class body is found; parser, recursion, nesting, and global
work limits remain active.

## Required Broad Evidence

Fresh Phase B gates completed after the accounting correction:

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

The PA10 exit-2 residuals are exactly the 11 identities listed above; no new
failure identity or coverage reduction occurred.  The file-audit warning is
the pre-existing `bad-division` warning at
`dev/src/cpp_semantic_core.h:1` for substantial header implementation body.
The clean rebuild completed after the broad run; no implementation source
changed between those validations.

## Next Checkpoint

After Phase B, the next checkpoint is a separately assigned inactive-residual
family audit.  Preserve the exact 11 identities above and do not enter lambda,
general declaration/declarator, qualified-name, trailing-attribute, or other
unrelated PA10 work.

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
| `c16e04ef82e93bb0c628d2f495cc7132d47dd749` current elaborated-type boundary | completed; residuals remain | focused 3/3, sibling/malformed/index 16/16, direct support harness PASS, PA10 148/159 with exact 11 residuals, through-PA9 457/457, file audit exit 0 with one known warning, clean dev rebuild and diff check pass |
