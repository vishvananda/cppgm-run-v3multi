## Stage Design

- Baseline at `7062fca1`: the tree was clean and PA2 was 0/26, with all 26
  failures returning `CPPGM_EXIT_NOT_IMPLEMENTED`.
- `posttokenize_cpp_source` in `dev/src/posttoken.cpp` owns the PA1 callback
  adapter and PA2 stream state.  It consumes each preprocessing-token once,
  classifies it into typed enums/records, and sends typed facts to the output
  boundary in `posttoken.h`; `dev/posttoken.cpp` only renders those facts.
- The stream ignores whitespace/newlines, maps the complete simple-token
  vocabulary, rejects hashes/header-names/non-whitespace characters, and
  keeps one pending maximal string sequence.  Numeric, character, and string
  decoders retain source spellings while storing decoded values/bytes in
  `LiteralData` and `UserDefinedLiteralData`.
- PA2 grammar and ABI decisions are explicit: integer suffix candidates and
  32/64-bit ranges, decimal-float forms through the PA2 compatibility
  decoders, Unicode/range character rules, and one-pass string encoding with
  ordinary/u8/u/U/L conflict and UDL-suffix checks.

## Failure Map

- PA1 phase 1--3 errors still escape through the CLI as `EXIT_FAILURE`.
  Conversion errors become one `invalid` fact and processing continues.
- Empty character literals are now emitted by the reusable tokenizer so PA2
  can classify `''` as an invalid preprocessing-token; unterminated and
  otherwise phase-invalid source still raises a phase error.
- Invalid numeric suffix/core/range combinations are rejected before value
  emission.  Valid integer/floating UDLs retain their textual prefix and do
  not require ordinary-literal ABI range selection.
- Character escapes require one valid Unicode code point and enforce the
  course ABI's `char16_t` single-unit rule.  String numeric escapes remain
  numeric code units, with width checks after concatenation encoding is
  selected.
- Maximal string sequences are invalid as one source when encoding prefixes,
  suffixes, escapes, or code-unit ranges conflict.  The `operator""sv`
  preprocessing-token boundary is handled as the required empty string plus
  identifier presentation.

## Active Checkpoint

- Final PA2 checkpoint is validated and is being finalized as one coherent
  commit.  `pp_tokenizer` is linked into `posttoken`, and the PA2 model and
  CLI adapter are both in `dev/`/`dev/src/`.
- `make test-pa2` passes 26/26; the exact prior-through command passes PA1
  54/54; `make test-report-through-pa2` passes 80/80; and the PA2 source audit
  checks 14 files successfully.
- No tests or reference fixtures were edited.  The plan and six intended
  source/plan files are the complete checkpoint scope.

## Performance Evidence

- `pa2/tests/700-hard-string-concat.t`: 429,984 input bytes, 14,400 lines;
  output is 1,324,500 bytes.  The direct focused run matched the checked-in
  reference in 0.08 seconds wall, 0.07 user, 0.01 system, with 16,052 KB
  maximum RSS on this host.
- Each token is decoded once.  String parts store typed code-point or
  numeric-code-unit elements; final encoding appends directly to one byte
  vector.  Pending source joining reserves the total source size, so ordinary
  processing is O(source bytes + token bytes + emitted code units), with
  memory proportional to the pending sequence and emitted value.
- Simple-token lookup uses one immutable function-local hash index built from
  the canonical entry table.  Its ordinary cost is O(spelling length)
  average, with no per-token allocation or vocabulary iteration; output is
  independent of hash iteration order.
- No reference executable, host compiler, rendered internal output, or
  accumulated-string rescanning is used by the implementation.

## Checkpoint Ledger

- `7062fca1` — clean-tree PA2 baseline: 26/26 failures, all
  `CPPGM_EXIT_NOT_IMPLEMENTED`; prior PA1 passes and file audit were green.
- Final PA2 checkpoint — typed PA2 model/stream, thin renderer, source-set
  link, bounded simple lookup, and narrow empty-character tokenizer boundary
  adjustment validated at 26/26 PA2 and 80/80 through PA2.  This plan is part
  of the single coherent checkpoint commit; its VCS hash is deliberately
  reported by the handoff rather than guessed before commit.
