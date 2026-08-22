## Stage Design

`PA6Recognizer` owns the stage boundary: `PPPreprocessingSession` produces the
typed `PPTokenBuffer`, `posttokenize_cpp_tokens` feeds one
`PA6TokenCollector`, and `pa6_internal::PA6Parser` consumes its canonical
typed stream.  Fixed tokens use enums; identifiers retain only mock
class/template/typedef/enum/namespace category bits; literals retain only
grammar kind and the two PA6 special kinds.  The collector splits logical
`OP_RSHIFT` into `ST_RSHIFT_1`/`ST_RSHIFT_2` and appends `ST_EOF`; it never
renders or re-tokenizes.

The parser is bounded predictive recursive descent with precedence parsing for
expressions, explicit marks for alternatives, committed template angles, and
grammar coverage for translation units, declarations/declarators, statements,
expressions, templates, names, and mock lookup categories.  `recog` builds
from `pp_tokenizer posttoken ctrlexpr macro preproc_session pa6_recognizer
pa6_parser_declarations pa6_parser_expressions`.  This follows the PA6
grammar and spec sections 1 (typed single pipeline), 2 (typed fact
continuity), 4 (bounded storage/work), and 7 (performance/conformance).

## Failure Map

- Baseline was 0/47: the `DoRecog` stub threw `NotImplementedException`, so
  all 47 checked cases produced `EXIT_NOT_IMPLEMENTED` instead of the recap;
  test coverage was unchanged.
- The implemented recognizer handles the required BAD cases: invalid token,
  bare label, empty `case`, handler-less `try`, malformed deep template,
  committed incomplete template name, and both closing-angle cases.
- Review corrections made conditional third operands mandatory, implemented
  both `new-declarator` alternatives, made throw operands FIRST/FOLLOW-aware,
  and implemented pointer-prefixed abstract-pack declarators with ordinary
  suffixes.  No tests or reference fixtures were modified.

## Active Checkpoint

`recog` reports one `OK`/`BAD` line per source, keeps translation,
preprocessing, token, and syntax failures at tool exit success, and reserves
nonzero exits for usage/output failures.  Focused non-repository probes passed:
valid `new C*`, valid conditional, throw without an operand before `;` and
`,`; and `using X = int *...;`; missing conditional third operand and malformed
`throw +` were BAD.

Post-split validation: `make test-pa6` passed 47/47.  This checkpoint is
committed.  Generated focused `.check`, `.check.exit_status`, and
`.check.stdout` files were removed before commit; checked fixtures and `.ref`
files remain unchanged.  The post-commit working tree was observed clean.

Remaining uncertainties are ordinary non-fixture C++ ambiguity breadth and
future-stage consumers of the public PA6 token model.  The file audit reports
one warning because `pa6_parser.h` contains substantial inline implementation
body; it is non-fatal and the stage audit passes.

## Performance Evidence

Recognizer work is bounded by `max(10000, 512 * token_count)` ticks and angle
and non-angle nesting is capped at 1024.  After the final split/build, the
checked deeply malformed template returned the expected BAD recap with tool
exit 0; `/usr/bin/time` measured `elapsed=0.01s`, `rss_kb=4880`, `exit=0`, and
the recognizer reported its work bound exceeded.

## Checkpoint Ledger

- 2026-08-22 — HEAD `2354eec9`: inherited PA6 stub baseline, 0/47; all
  failures were `EXIT_NOT_IMPLEMENTED`.
- 2026-08-22 — final committed PA6 increment: typed PA5 flow, bounded
  recognizer, parser split, review corrections, recap policy, and source-set
  integration complete.
- Gates: `make test-pa6` = 47/47; `n=6 ... make test-report-through-pa5` =
  245/245; `perl scripts/cppgm_file_audit.pl --stage pa6 --paths dev/src` =
  pass with 1 warning; `make test-report-through-pa6` = 292/292.
- Ledger status: committed checkpoint; post-commit `git status --short` was
  observed empty.
