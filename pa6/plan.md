# PA6 full-stage architecture audit

## Contract and ownership

PA6 follows the typed single pipeline required by spec sections 1 and 2:

`recog` source -> `PPPreprocessingSession` -> canonical `PPTokenBuffer` ->
one `PA6TokenCollector` -> canonical `vector<PA6Token>` -> `PA6Parser`.

The preprocessor owns spelling-table identities and typed PP facts.  The
collector owns the PA6 boundary facts: fixed enum vocabulary, one-time mock
name-category classification, literal coarsening plus `ST_EMPTYSTR`/`ST_ZERO`,
`OP_RSHIFT` splitting, and `ST_EOF`.  The parser consumes only those typed
facts; it does not render, re-tokenize, or shell out to any reference, prior
solution, host compiler, assembler, or linker.

Representative ownership traces:

- `TC1<...>`: identifier spelling is classified once by the collector;
  `PA6Parser::parse_type_name`/`parse_simple_template_id` owns the committed
  angle state and consumes the typed close-angle stream.
- `int const Yx` and `const C&`: `parse_type_specifier` publishes an explicit
  cv/non-cv result to `parse_decl_specifier_seq`; the shared parameter path then
  owns `parse_abstract_declarator`, rather than guessing from the next token.
- Every parser advance, including balanced-token leaves and formerly direct
  `position_` increments, goes through a charged consume helper.  Marks restore
  position and angle/non-angle state for failed alternatives.

## Findings and coherent repair milestone

The audit reduced the two independently observed correctness blockers:

1. Parameter and exception declarations now try the shared abstract-declarator
   grammar after a named declarator fails.  This accepts unnamed `int*`,
   `const C&`, `int[]`, and catch `int*` forms without spelling-specific
   branches.
2. Decl-specifier sequencing now uses the consumed type-specifier’s explicit
   classification.  A category-bearing identifier after a prior non-cv type
   is left for the declarator, while cv qualifiers do not set that boundary.

The architecture audit also repaired the material structural findings:

- Parser state/work helpers moved from `pa6_parser.h` into the new
  `pa6_parser.cpp`; `FRONTEND_OBJ_BASENAMES_recog` owns that source exactly
  once.  The header is now declarations/state only and the file audit has no
  bad-division warning.
- The work limit remains `max(10000, 512 * token_count)` with overflow-safe
  sizing.  Nesting and all token movement are bounded/charged, including
  balanced scans; exhaustion becomes a deterministic per-source `BAD` reason.
- Partial helpers and alternative paths repaired to restore their entry mark;
  this includes expression operators, delimiters, jumps, namespace/member
  lists, template arguments, abstract declarators, and handler paths.
- Contiguous cast/unary prefixes now parse iteratively before the shared unary
  base, preserving the grammar and mark ownership without recursive stack
  growth on long valid unary chains.
- Charged `override`/`final` suffix loops now propagate a failed consume, so
  work exhaustion cannot leave a non-advancing loop.

## Measurement and conformance evidence

The baseline had the durable 47-test PA6 inventory and 292/292 through-stage
result.  This audit adds the compact course regression
`125-abstract-parameter-and-cv-declarator.t`; its reference sidecars were
generated only by the documented `ref-test` target.

Focused evidence:

- `make -C dev recog -j2`: compile/link pass.
- `make -C pa6 check TEST='course/pa6/125-abstract-parameter-and-cv-declarator.t tests/250-decl.t tests/260-declarator.t tests/400-exceptions.t course/pa6/500-deep-template-argument-failure-bad.t course/pa6/500-operator-template-angle-boundary.t'`: 6/6 pass.
- Representative valid operator fixture: `OK`, tool exit 0,
  `elapsed=0.00s`, `rss_kb=4076`.
- Immutable executable evidence: at HEAD
  `e23bf725aefdc0dee6624ea7c5998fd0a7461ae5`,
  `make -B -C dev recog -j2` passed and
  `sha256sum dev/recog` was
  `6ae938703ce47cdcb9939de73e8faf9792ffe2f4f945725ec4f60c1dc0a9c1b8`.
  The hash was identical before and after all measurements; no rebuild was
  performed between samples.
- Final valid-unary characterization used temporary inputs of the exact form
  `int main() { return !...!0; }`, with N adjacent `!` operators and no
  inter-operator whitespace.  Each sample used:
  `/usr/bin/time -f 'n=N elapsed=%e rss_kb=%M exit=%x' ./dev/recog -o OUT INPUT`.
  These are single-run characterization results, not a comparative speed
  claim:

  | unary operators | result | elapsed | peak RSS |
  | ---: | :--- | ---: | ---: |
  | 1,000 | `OK`, exit 0 | 0.00s | 4,560 KiB |
  | 16,000 | `OK`, exit 0 | 0.01s | 7,860 KiB |
  | 128,000 | `OK`, exit 0 | 0.10s | 36,528 KiB |
  | 256,000 | `OK`, exit 0 | 0.19s | 69,368 KiB |

  Time and retained token storage scale linearly over this sample; the
  iterative prefix path accepts the large inputs without stack growth.
- The previously recorded `4,636/11,296/63,412/123,072 KiB` table is
  superseded/rejected intermediate evidence: it was recorded without an
  immutable executable identity and with a different temporary input shape;
  it is not a final performance result.
- Deep malformed-template fixture: `BAD`, tool exit 0, work-bound reason,
  `elapsed=0.03s`, `rss_kb=5144`; the same immutable executable remained
  unchanged before and after the probe, and the bounded hostile path did not
  crash or time out.
- PA6 terminates at syntax status and emits no object, generated program,
  relocation, or optimizer IR; the section-7 generated-code measures therefore
  have no PA6 consumer.  The parser's monotonic `work_` counter, explicit
  nesting caps, and bounded hostile probe provide the stage-local structural
  corroboration.
- `perl scripts/cppgm_file_audit.pl --stage pa6 --paths dev/src`: pass,
  26 files, zero warnings.
- `make test-report-through-pa6`: all tests passed, `293 / 293`.
- `git diff --check`: pass.

## Checkpoint ledger

- Baseline: HEAD `2f150101c53e1f3ef40095eaa2e1b81c994037fd`, clean, prior PA6
  implementation and 292/292 through-stage evidence.
- Implementation/audit commit: `e23bf725aefdc0dee6624ea7c5998fd0a7461ae5`,
  containing the typed grammar repair, parser ownership split, charged
  movement/work-budget audit, iterative cast/unary prefix parsing,
  state-discipline repair, and focused regression with documented reference
  provenance.
- Evidence-correction milestone, 2026-08-23, identified by this content/date:
  immutable executable identity plus corrected tight-unary and hostile
  measurements; this is a documentation-only follow-up to the implementation
  commit.
- Final checks recorded for this tree: zero-warning PA6 file audit (26 files),
  `293 / 293` through-stage tests, and clean `git diff --check`.
- Commit-ready evidence checkpoint: only `pa6/plan.md` changes in this
  follow-up; its own follow-up commit hash is intentionally identified by the
  content/date above because this plan is part of that commit.
