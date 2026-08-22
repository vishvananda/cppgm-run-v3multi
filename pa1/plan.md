# PA1 checkpoint plan

## Stage Design

`dev/pptoken.cpp` is the CLI/debug-stream adapter.  `dev/src/pp_tokenizer.cpp`
owns one forward pipeline: strict physical UTF-8 decoding, BOM handling,
trigraphs, UCNs, line splicing, implied final LF, and a single linear
preprocessing-token scan.  Token data is kept as decoded code points until the
existing `DebugPPTokenStream` boundary.  Raw literals use the logical prefix
and opening-quote mapping, then retain physical spelling only from the opening
quote through the closing quote; this permits phase-2 splices in raw prefixes
while preserving raw reversal.  UCN conversion and line-splice contraction
compact in place, so one logical `Unit` vector remains; reordered fields keep
size-t offsets without imposing a 4 GiB limit.  Fixed token categories use
`TokenKind`, and lookahead is bounded except for the required literal/header
spans, which are consumed once.

## Failure Map

Turn-start execution failure: 53/53 tests (30 PA-local + 23 course) failed at
the `NotImplementedException` EOF stub, so those broad behavioral families had
no independently observable counts: basic tokens, phase ordering/UTF-8,
comments, literals, pp-numbers, header context, malformed input, and maximal
munch.  The first implementation cleared all 53.  Architecture review then
isolated one raw-prefix family defect (two direct probes: direct and trigraph
backslash-newline before the opening quote); the correction clears both and
adds one course regression.  Current failure count is 0/54.

## Active Checkpoint

The correction checkpoint retains the complete PA1 phase-1-through-3 boundary:
strict UTF-8/BOM, trigraph and UCN ordering, line splices/final newline,
comments-as-whitespace, identifiers with Annex E ranges, pp-numbers,
punctuators including `<::`, contextual header names, character/string/raw
literals with suffixes and escape validation, non-whitespace fallback, and
required malformed-input failures.  The character-literal and header-name
sequences are nonempty; string/raw bodies retain their specified optional
emptiness.  Raw prefixes are recognized only from post-phase logical units,
including direct/trigraph-produced splices and an encoded prefix; normal
literal prefixes are externally validated while c-/s-char and raw reversal
exceptions remain intact.  Wrapped `--batch-stdin` behavior continues to be
provided by `test_runner.cpp`.

## Performance Evidence

The design uses a decoded source vector, two linear phase passes, and one
tokenization pass.  UCN and splice contractions are read/write compactions;
raw closing-delimiter checks compare at most 16 delimiter code points, so
ordinary work is O(n) with bounded lookahead and no whole-input rescan per
token.  Reordering `Unit` fields reduces the x86_64 object from 32 to 24 bytes.
The checked-in 4,134-byte `pa1/tests/900-real-world.t` completed in 0.00s
elapsed/user/system with 3,876 KB peak RSS.  A temporary repeated-`int x;`
workload (1,048,572 and 4,194,302 bytes; output discarded; one warm run per
size; five interleaved runs) measured these medians before and after the
correction:

| input | elapsed | user | system | peak RSS |
| --- | ---: | ---: | ---: | ---: |
| 1 MiB before | 1.26s | 0.68s | 0.57s | 75,276 KB |
| 1 MiB after | 1.24s | 0.67s | 0.57s | 58,880 KB |
| 4 MiB before | 5.11s | 2.71s | 2.43s | 290,400 KB |
| 4 MiB after | 5.03s | 2.77s | 2.25s | 224,844 KB |

The approximately fourfold time growth for fourfold input and the lower peak
RSS are scaling sanity claims on this host, not universal benchmarks; generated
inputs remained outside the repository under `/tmp` and were not committed.

## Checkpoint Ledger

- Baseline: 0/53 PA1 tests passing (30 local + 23 course; EOF stub).
- Focused milestone: 8/8 mixed PA1 checks passed.
- Raw-prefix regression: `200-raw-prefix-splices.t`, with references generated
  by `make -C pa1 ref-test TEST=../cppgm.tests/course/pa1/200-raw-prefix-splices.t`;
  focused result PASS, 1/1; no existing fixture/reference changed.
- `make test-pa1`: PASS, 54/54.
- `make test-report-through-pa1`: PASS, 54/54.
- Explicit prior-through command for `n=1`: PASS, 0/0.
- `perl scripts/cppgm_file_audit.pl --stage pa1 --paths dev/src`: PASS,
  12 files checked.
- `git diff --check`: PASS.
- Performance evidence: recorded above for the 900-real-world case and the
  before/after interleaved 1/4 MiB temporary scale samples.
- Implementation checkpoint commit: `d62d4144` (`PA1: implement phase 1-3
  preprocessing tokenizer`).
- Ledger follow-up commit: `bc8a31bb` (`PA1: record validation ledger`).
- Correction commit: `85bd3a67` (`PA1: fix raw prefix spans and compact phases`).
- This ledger follow-up records the correction hash; its own hash is reported
  in the final handoff because a commit cannot embed its own ID.
- Final `git status --short`: verified clean after the ledger follow-up commit.
