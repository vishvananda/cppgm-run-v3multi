# PA10 Checkpoint Plan and Evidence

## Current spec alignment

The audited production path is:

```text
phase-3 buffer -> posttoken facts -> typed PA10Token -> PA10Parser
    -> PA10Ast typed names/declarators and cold sidecars -> renderer
```

This is the single PA10 parser/model/renderer path required by root
`spec.md` Purpose and §§1-4 and §7.  `dev/src/pa10_parser_support.{h,cpp}`
owns typed posttoken collection, contextual classification, indexed
template/delimiter facts, balanced attributes, and charged special-member
lookahead.  The parser remains the canonical AST owner.  Qualified names are
component records; template arguments, decltype roots, conversion children,
operator presentation, and destructor template/decltype finals use bounded
sidecar ranges.  The renderer validates those ranges and performs the only
requested text rendering.

The audit repair for landed commit `08c38115a64397ae7170a53a81b74a1c36e0a9fb`
keeps that ownership path intact while adding: canonical destructor finals for
`~decltype(...)` and `~C<T>`, global-prefix admission for qualified
unqualified-ids, class-target routing/structured forward names for
`extern template struct C<int>;`, and alternating attribute/specifier
consumption for special members.  No existing grammar, harness, fixture, or
reference was edited; the only new test material is the reduced course
fixture and its matching expected output.  No parallel parser, retry loop, or
host/reference/compiler production call was added.

## Exact baseline and failure map

Turn-start evidence from `last-test.log`: **157 discovered, 135 passed, 22
failed**.  The exact residual identities were:

```text
pa10/tests/general/200-builtin-function-style-cast-expression.t
pa10/tests/general/200-builtin-function-style-cast-member-body.t
pa10/tests/general/200-conditional-simple-type-shift-return.t
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
pa10/tests/general/200-template-conditional-simple-type-shift-return.t
pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t
pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t
pa10/tests/general/200-trailing-parameter-vendor-attribute.t
pa10/tests/general/200-typeid-postfix-member-suffix.t
```

The local PA10 directory still discovers exactly 157 `.t` files.  The final
root PA10 report is **136/158**: the 157-test local portion is **135/157**, and
the one new course fixture is **1/1**.  The course pass is additional evidence,
not compensation for a local failure.  No local test was removed, renamed, or
replaced.

The exact sorted final identity comparison against the turn-start
`last-test.log` has 22 identities in both sets, with zero turn-start-only and
zero final-only identities.  Thus there is no local coverage loss and no new
failing identity.  Through-PA9 is current reused-green evidence at **457/457**.
The current file audit exits 0 with only the pre-existing
`dev/src/cpp_semantic_core.h:1` `bad-division` warning.

## Focused evidence and performance characterization

Observed focused commands/results:

```text
make -C dev cppgm++ CXX=g++                                      exit 0
g++ -std=gnu++11 -Wall -Wextra -Werror ... pa10_ast.cpp          exit 0
g++ -std=gnu++11 -Wall -Wextra -Werror ... pa10_parser_support.cpp exit 0
g++ -std=gnu++11 -Wall -Wextra -Werror ... pa10_renderer.cpp     exit 0
make -C pa10 check [owned cluster plus 9 green siblings]        exit 2; 20 / 21
make -C pa10 check [15 RShift/template/attribute siblings]      exit 0; 15 / 15
make -C pa10 check [course/pa10 boundary fixture]               exit 0; 1 / 1
valid interleaved attribute probe                               exit 0
four mismatched/truncated /tmp probes                           exit 0; 4 / 4 rejected
invalid sidecar-range renderer probe                            exit 0
make test-pa10                                                   exit 2; 136 / 158
make test-report-through-pa9                                     exit 0; 457 / 457
perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src    exit 0; 1 warning
git diff --check                                                  exit 0
```

The focused cluster includes representative identifier/template-id,
qualified conversion/operator, literal operator, constructor/destructor,
postfix member, attributes, member specifiers, explicit instantiation,
member-pointer, nested declarator, and RShift cases.  The valid `/tmp` probes
also cover nested template RShift closes, template destructor definitions,
leading-global operator names, class explicit instantiation, and decltype
postfix destructor names.  Mismatched GNU/standard attributes and truncated
template/attribute inputs exit with failure rather than reading past a range.

The final executable is
`2f8dd71adb596507f74a438c8056b1e946de9c65acbaea947d751c4ad12aeff9`.  With
that immutable executable, 128 repeated invocations of each valid fixture
measured:

| input | elapsed | user | sys | peak RSS | exit |
| --- | ---: | ---: | ---: | ---: | ---: |
| qualified conversion/template name | 0.33 s | 0.13 s | 0.19 s | 7664 KB | 0 |
| nested GNU/standard attribute member path | 0.32 s | 0.13 s | 0.19 s | 7872 KB | 0 |
| reduced structured-name boundary fixture | 0.34 s | 0.14 s | 0.19 s | 7428 KB | 0 |

These are repeated single-executable characterization runs, dominated by
process launch, and are not a comparative performance claim.  Structurally,
the index builder is one monotonic pass; parser consumption and balanced
attribute skipping are monotonic; special-member lookahead uses O(1) indexed
close lookup; and no qualified name or conversion type-id is reparsed.  The
current audit build reports 2,950 lines in `pa10_ast.cpp`, 552 in
`pa10_parser_support.cpp`, and 849 in `pa10_renderer.cpp`, with
`sizeof(PA10Token)=240`, `sizeof(PA10AstNode)=232`, and `sizeof(PA10Ast)=376`.

## Final checkpoint and risks

The forward residual fails at the constructor-body
`typedef typename alloc<Y>::type alloc_t;` because the existing non-type-context
declaration-specifier route does not admit `KW_TYPENAME`; it remains outside
this increment.  Other residual families remain out of scope: casts,
condition/declaration ambiguity, enum/friend/local declaration boundaries,
lambda capture, placement new, enumerator calls, and typeid/new suffixes.

Remaining risks are ordinary PA10 syntax corners outside the focused clusters
and whether a future investigation proves the forward `KW_TYPENAME` residual
belongs to this ownership path.  Range validation and malformed-boundary
probes are clean, and the repair adds no hot-record fields or duplicate parser
owner.

## Historical evidence

The parent `b9b58b9c` handoff was 157 discovered, 123 passed, and 34 failed;
its declarator/member audit established the nearest-derived-operator function
boundary, one-shot qualified member-pointer facts, and the existing bounded
work/nesting/recursion controls.  It also recorded handout/ref extensions for
linkage specifications, qualified member-pointer operators, and dynamic throw
specifications that were not added to `pa10.gram`.  The earlier `a2b82dcb`
typed template-id checkpoint recorded 106/157 with 51 failures before later
repairs, plus the rejected RShiftPiece2 experiment and prior storage
characterization.  These values are historical and are not current baseline
claims.

## Next checkpoint

The next checkpoint is a supervisor-selected residual-family audit.  Do not
widen into the 22 residual families; select the forward constructor only if
its `KW_TYPENAME` failure is shown to be caused by this ownership path.

## Checkpoint ledger

| checkpoint | status | evidence / intent |
| --- | --- | --- |
| `a2b82dcb` template/angle ownership | landed historical | typed template components/sidecars and bounded close ownership |
| `27623d64` declarator/member-declaration boundary | landed historical | unified declarator/member path and private nearest-derived-operator helper |
| `b9b58b9c` declarator-boundary audit | landed historical baseline | 157 discovered, 123/157 pass, exact 34 failures; through-PA9 457/457; one pre-existing audit warning |
| `08c38115a64397ae7170a53a81b74a1c36e0a9fb` structured names/special members | audit complete | local 135/157 with the exact unchanged 22 residuals; course 1/1; through-PA9 457/457; file audit exit 0 with one pre-existing warning |
