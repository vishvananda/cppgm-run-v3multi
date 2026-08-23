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
- Post-change valid-unary characterization, each one run of the final
  executable (not a comparative speed claim):

  | unary operators | result | elapsed | peak RSS |
  | ---: | :--- | ---: | ---: |
  | 1,000 | `OK`, exit 0 | 0.00s | 4,636 KiB |
  | 16,000 | `OK`, exit 0 | 0.02s | 11,296 KiB |
  | 128,000 | `OK`, exit 0 | 0.15s | 63,412 KiB |
  | 256,000 | `OK`, exit 0 | 0.30s | 123,072 KiB |

  Time and retained token storage scale linearly over this sample; the
  iterative prefix path avoids the pre-repair stack fault at the large sizes.
- Deep malformed-template fixture: `BAD`, tool exit 0, work-bound reason,
  `elapsed=0.04s`, `rss_kb=5104`; the bounded hostile path did not crash or
  time out.
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
- Final audit milestone, 2026-08-23, identified by this content/date: typed
  grammar repair, parser ownership split, charged movement/work-budget audit,
  iterative cast/unary prefix parsing, state-discipline repair, and the one
  focused regression with documented reference provenance.
- Final checks recorded for this tree: zero-warning PA6 file audit (26 files),
  `293 / 293` through-stage tests, and clean `git diff --check`.
- Commit-ready checkpoint: the scoped PA6 sources, plan, fixture, and three
  generated reference sidecars are the complete intended change set; no final
  commit hash is recorded here because this plan is part of that commit.
