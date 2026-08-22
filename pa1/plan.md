# PA1 checkpoint plan

## Stage Design

`dev/pptoken.cpp` is the CLI/debug-stream adapter.  `dev/src/pp_tokenizer.cpp`
owns one forward pipeline: strict physical UTF-8 decoding, BOM handling,
trigraphs, UCNs, line splicing, implied final LF, and a single linear
preprocessing-token scan.  Token data is kept as decoded code points until the
existing `DebugPPTokenStream` boundary; raw literals retain their physical
spelling by source-span mapping.  Fixed token categories use `TokenKind`, and
lookahead is bounded except for the required literal/header spans, which are
consumed once.

## Failure Map

Turn-start baseline: 0/53 passing, 53 failing.  All failures were caused by
`dev/pptoken.cpp` throwing `NotImplementedException` at EOF, so no finer
behavioral failure counts were observable yet.  The 53 cases are 30 PA-local
tests and 23 course tests, covering basic tokens, phase ordering/UTF-8,
comments, literals, pp-numbers, header context, malformed input, and maximal
munch.  The reviewed corrections also explicitly cover C++11 literal prefix
grammar, nonempty character/header productions, complete consumed-unit
validation, and typed token-state tracking.

## Active Checkpoint

Implement the complete PA1 phase-1-through-3 boundary in the shared tokenizer:
strict UTF-8/BOM, trigraph and UCN ordering, line splices/final newline,
comments-as-whitespace, identifiers with Annex E ranges, pp-numbers,
punctuators including `<::`, contextual header names, character/string/raw
literals with suffixes and escape validation, non-whitespace fallback, and
required malformed-input failures.  The character-literal and header-name
sequences are nonempty; string/raw bodies retain their specified optional
emptiness.  Wrapped `--batch-stdin` behavior continues to be provided by
`test_runner.cpp`.

## Performance Evidence

The design uses a decoded source vector, two linear phase passes, and one
tokenization pass.  Raw closing-delimiter checks compare at most 16 delimiter
code points, so ordinary work is O(n) with bounded lookahead and no whole-input
rescan per token.  The checked-in 4,134-byte `pa1/tests/900-real-world.t`
completed in 0.00s elapsed/user/system with 3,876 KB peak RSS in the initial
sanity sample.  A temporary repeated-`int x;` workload (output discarded,
one warm run per size, then five interleaved runs) measured these medians:

| input | elapsed | user | system | peak RSS |
| --- | ---: | ---: | ---: | ---: |
| 1 MiB | 1.26s | 0.68s | 0.57s | 75,276 KB |
| 4 MiB | 5.11s | 2.71s | 2.43s | 290,400 KB |

The approximately fourfold time growth for fourfold input is a scaling
sanity claim on this host, not a universal benchmark; generated inputs were
temporary and removed.

## Checkpoint Ledger

- Baseline: 0/53 PA1 tests passing.
- Focused milestone: 8/8 mixed PA1 checks passed.
- `make test-pa1`: PASS, 53/53.
- `make test-report-through-pa1`: PASS, 53/53.
- Explicit prior-through command for `n=1`: PASS, 0/0.
- `perl scripts/cppgm_file_audit.pl --stage pa1 --paths dev/src`: PASS,
  12 files checked.
- `git diff --check`: PASS.
- Performance evidence: recorded above for the 900-real-world case and
  interleaved 1/4 MiB temporary scale samples.
- Implementation commit: pending; this ledger will be amended in the
  follow-up ledger commit with the exact hash.
- Final `git status --short`: pending final commit verification.
