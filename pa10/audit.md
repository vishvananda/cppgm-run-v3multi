# Current Checkpoint Review

This is the bounded audit of landed checkpoint
`08c38115a64397ae7170a53a81b74a1c36e0a9fb`, `PA10: route structured names and
special members`, whose parent is `b9b58b9c`.  The reviewed landed increment
is limited to `dev/frontend_source_sets.mk`, `dev/src/pa10_ast.{h,cpp}`,
`dev/src/pa10_parser_support.{h,cpp}`, and `dev/src/pa10_renderer.cpp`.
The final bounded repair remains within `pa10_ast.cpp` and
`pa10_renderer.cpp`; no production owner, grammar, harness, or unrelated
residual family was widened.

## Contract and specification alignment

The production path remains one forward flow:

```text
phase-3 buffer -> posttoken facts -> typed PA10Token -> PA10Parser
    -> PA10Ast typed names/declarators and cold sidecars -> renderer
```

This follows root `spec.md` Purpose and §§1-4 and §7.  The support translation
unit owns posttoken collection, contextual classification, indexed template
and delimiter facts, attribute balancing, and bounded special-member
lookahead.  It does not own a second parser or AST.  `PA10Parser` remains the
owner of canonical syntax nodes; the renderer is the sole requested text
boundary and does not reparse its output.

The collector preserves producer `PPSpellingId` identity, retains source text
only for cold presentation, classifies `override`, `final`, and GNU attribute
introducers once, and splits producer `OP_RSHIFT` into two logical close
pieces.  `build_indexes` is a monotonic pass over the token vector.  Its angle
close and delimiter tables are indexed by absolute token position, and every
parser-side scan has bounds checks or the existing parser failure boundary.

`parse_name` is now the shared route for identifier/template components and
operator, conversion, literal-operator, destructor, and qualified finals.
`name_node` copies typed unqualified-id facts while template arguments,
decltype roots, conversion children, and operator presentation remain in
validated cold ranges.  Special-member routing uses indexed, charged
lookahead, then the normal declarator path consumes the name once.  Explicit
instantiation uses that declaration path for functions and the class-forward
path for class targets.

## Findings and bounded repair

The landed path passed its owned checkpoint identities, but focused boundary
probes found four omissions in the same ownership path:

- A destructor parsed as `~decltype(x)` was stored in a semantic sidecar but
  the renderer emitted only `~`.  Destructor template-ids likewise had no
  structured final-name owner.  The repair stores a template or decltype final
  as exactly one semantic child; an ordinary `~Identifier` keeps its
  `unqualified_id_spelling`.  The tilde remains the presentation token, and
  one fail-closed renderer helper validates and emits all three forms.
- A leading-global final such as `int ::operator+(int)` reached `parse_name`
  after consuming `::`, where the old loop still required an identifier.  The
  unqualified-id route now applies after a global prefix too, and renderer
  dispatch preserves `global_name` when a name has no component prefix.
- The landed explicit-instantiation route always called
  `parse_decl_or_function`, and class-forward detection did not recognize a
  template-id.  Class targets now route through `parse_class_declaration`,
  whose forward node owns structured template components and renders
  `Holder<int>` without flattening it.
- The support recognizer accepted alternating member-function-specifier and
  attribute batches, while the consumer previously admitted only one batch
  before and one after the specifiers.  Replaying the valid
  `inline __attribute__((always_inline)) virtual
  __attribute__((visibility("hidden"))) ~C();` probe against that consumer
  failed with `expected declarator-id at token 10` (exit 1).  The typed
  `parse_member_specifiers` path now consumes balanced attributes before the
  first specifier and after every specifier, retaining the existing
  attribute-skipping dump convention; the repaired probe exits 0 and emits
  both `inline` and `virtual`.

The forward nested-template constructor remains the only failure in the
21-case owned command.  Its current failure is at the constructor-body
`typedef typename alloc<Y>::type alloc_t;` (`KW_TYPENAME` is not admitted by
the existing non-type-context declaration-specifier route).  That behavior is
outside this coherent increment and was not widened.

The repair adds no hot-record fields, no parser retry loop, and no production
textual downgrade.  Current source/layout characterization is 2,950 lines
for `pa10_ast.cpp`, 552 for `pa10_parser_support.cpp`, and 849 for
`pa10_renderer.cpp`; `sizeof(PA10Token)=240`, `sizeof(PA10AstNode)=232`, and
`sizeof(PA10Ast)=376` in the audit build.  The parser continues to charge the
indexed pass, token consumption, balanced-attribute consumption, recursion,
nesting, and bounded lookahead against its existing limits.

## Focused evidence

Observed results after the final repair and broad validation:

```text
make -C dev cppgm++ CXX=g++                                      exit 0
g++ -std=gnu++11 -Wall -Wextra -Werror ... pa10_ast.cpp          exit 0
g++ -std=gnu++11 -Wall -Wextra -Werror ... pa10_parser_support.cpp exit 0
g++ -std=gnu++11 -Wall -Wextra -Werror ... pa10_renderer.cpp     exit 0
make -C pa10 check [owned cluster plus 9 green siblings]        exit 2; 20 / 21
make -C pa10 check [15 RShift/template/attribute siblings]      exit 0; 15 / 15
make -C pa10 check [new course boundary fixture]                exit 0; 1 / 1
valid interleaved attribute probe                               exit 0
four mismatched/truncated /tmp probes                           exit 0; 4 / 4 rejected
warning-clean compile of three affected translation units       exit 0 each
final invalid sidecar-range renderer probe                      exit 0
make test-pa10                                                   exit 2; 136 / 158
make test-report-through-pa9                                     exit 0; 457 / 457
perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src    exit 0; 1 warning
git diff --check                                                  exit 0
```

The local PA10 directory still contains exactly 157 `.t` files.  The
turn-start `last-test.log` baseline was 157 discovered, 135 passed, and 22
failed.  The final local portion is still 157 discovered, 135 passed, and the
same 22 failed.  The exact sorted identity comparison has zero turn-start-only
and zero final-only identities.  The root PA10 report totals 158 because the
one new course fixture is included there; it passes separately as 1/1 and does
not compensate for any local failure.  No local test was removed, renamed, or
replaced.

The file audit's single warning is the pre-existing
`dev/src/cpp_semantic_core.h:1` `bad-division` warning; no new warning was
observed.

The current executable hash is
`2f8dd71adb596507f74a438c8056b1e946de9c65acbaea947d751c4ad12aeff9`.
Using that immutable executable, 128 repeated invocations of each
representative valid fixture measured:

| input | elapsed | user | sys | peak RSS | exit |
| --- | ---: | ---: | ---: | ---: | ---: |
| qualified conversion/template name | 0.33 s | 0.13 s | 0.19 s | 7664 KB | 0 |
| nested GNU/standard attribute member path | 0.32 s | 0.13 s | 0.19 s | 7872 KB | 0 |
| reduced structured-name boundary fixture | 0.34 s | 0.14 s | 0.19 s | 7428 KB | 0 |

These are repeated single-executable characterization runs, dominated by
process launch; they are not a comparative performance claim.  The structural
bound is one indexed token pass plus monotonic parsing, with no name
reparse/backtracking or renderer round trip.

## Historical evidence

The following material belongs to earlier checkpoints and is retained as
history, not as current source or validation:

- `b9b58b9c` audited the unified declarator/member boundary and its private
  `pa10_declarator_shape` helper.  Its handoff was 157 discovered, 123 passed,
  and 34 failures, with through-PA9 457/457 and the pre-existing
  `dev/src/cpp_semantic_core.h:1` file-audit warning.  That audit established
  the nearest-derived-operator function boundary, one-shot member-pointer
  qualification facts, and the existing work/nesting/recursion limits.
- That audit also recorded handout/fixture extensions not represented by
  named productions in `pa10.gram`: linkage specifications, qualified
  member-pointer operators, and dynamic throw specifications.  Neither this
  checkpoint nor this audit edits the grammar, existing fixtures, harness, or
  existing references; the new reduced course fixture is the sole added test
  material.
- `a2b82dcb` was the earlier typed template-id/qualified-name checkpoint.  Its
  historical result was 106/157 with 51 failures before later repairs; its
  rejected RShiftPiece2 experiment, template-index characterization, and
  prior storage measurements remain historical only.

## Audit ledger

| checkpoint | review result | owner action | validation state |
| --- | --- | --- | --- |
| `27623d646279d867e58039af60a1cc52e09e090e` declarator/member boundary | historical nearest-derived-operator and member-pointer audit | keep the private shape helper and unified declarator owner | historical focused 23/23; PA10 123/157; through-PA9 457/457 |
| `08c38115a64397ae7170a53a81b74a1c36e0a9fb` structured names and special members | bounded audit complete; four ownership-path gaps repaired without widening residual families | retain one typed name/special-member path, validated sidecars, indexed facts, and class/function explicit-instantiation routing | local 135/157 with the exact unchanged 22 residuals; course 1/1; through-PA9 457/457; file audit exit 0 with one pre-existing warning |

## Next checkpoint

The next checkpoint is a supervisor-selected residual-family audit.  Keep the
forward nested-template constructor separate unless evidence shows that its
`typename` failure belongs to this structured-name or special-member ownership
path; the current failure is in the constructor-body declaration-specifier
route and remains outside this increment.
