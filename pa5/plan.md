# Stage Design

PA5 uses one reusable `PPPreprocessingSession` in `dev/src` per command-line
source.  The session owns the macro environment, conditional/include stacks,
presumed file and line, pragma-once file IDs, counter, and one typed
`PPTokenBuffer` spelling arena.  It consumes phase-3 tokens from the existing
tokenizer, sends directive operands and active text through the shared PA4
macro owner, and hands the resulting typed buffer directly to PA2
post-tokenization.  `dev/preproc.cpp` is only the framing, file-I/O, and
presentation adapter; a fresh session resets all source-local state.

Ordinary string-literal operands for `#include` and the optional `#line`
filename are converted by PA2 `posttokenize_cpp_tokens` into `LiteralData`;
the session only removes the typed array's terminating zero at the path
boundary.  A direct quoted include is lexed as `HeaderName` and remains a
header-name text boundary after delimiter removal; macro-produced include
strings arrive as `StringLiteral` and use the same PA2 conversion.  `_Pragma`
uses its own narrow C++11 destringization boundary, replacing only `\\"` and
`\\\\`.

`PPToken` carries a default-safe `PPSourceLocation` containing an interned
presumed-file ID and line.  The tokenizer preserves physical line metadata
even when a block comment contains newlines; the session then annotates each
line's tokens with the current presumed identity, including `#line` changes
and included files.  Active ordinary lines accumulate with `NewLine` tokens
until a directive, include, or file boundary, so macro argument collection
sees ordinary logical newlines as whitespace.  PA4 replacement, substitution,
stringization, and paste output inherit the invocation-head location.  The
typed builtin resolver receives that invocation token and derives `__FILE__`
and `__LINE__` from its location rather than mutable file-frame state.  PA3
controlling expressions use the typed expansion/evaluation seam; no classified
token is rendered and re-tokenized.

# Failure Map

The complete 70-case set is tracked by behavior:

- Session/output/error/null/non-directive boundary (12):
  `100-empty`, `100-nodefs`, `120-invalid`, `150-error`, `150-no-error`,
  `150-null-directive`, `170-nondir1` through `170-nondir6`.
- PA4 macro reuse and expansion/error behavior (38): `150-max`, `200-fnlike`,
  `200-onedef`, `250-badvargs1` through `250-badvargs4`, `250-join`,
  `300-badhash1-1` through `300-badhash1-3`, `300-badhash2-1` through
  `300-badhash2-4`, `300-identifier-missing`, `300-undef-extra`,
  `600-line-macro`, `600-recurse`, `610-line-macro`, `650-recurse`,
  `700-redef-a`, `700-redef-q`, `700-redef2`, `700-redeferr1` through
  `700-redeferr4`, `700-strlit-a`, `700-strlit-a2`, `700-strlit-q`,
  `800-placemarker-a`, `800-placemarker-q`, `850-varargs-a`, `850-varargs-q`,
  `900-recurse`, `910-recurse2`, and course `600-repeated-argument-expansion`.
- Conditional control and typed PA3 semantics (6): `200-if`, course
  `200-conditional-exclusion`, `200-defined-identifier-like-operator`,
  `300-defined-operator-misuse`, `400-predefined-macros`,
  `500-predefined-macros`.
- Include, presumed source state, builtin macro expansion, pragma, and
  multi-source state (14): `200-include`, `400-bad-include`,
  `400-counter-macro`, `400-header-guarded`, `500-alt`, `500-pragma-ignore`,
  `500-tricky-join`, `501-pragma-op-ignore`, `600-pragma-op`,
  `660-line-directive`, `800-pragma-once`, course `300-line-new-line`,
  `400-multiple-source-files`, and course `600-predefined-macro-argument-expansion`.

The four cluster counts are disjoint and sum to the original 70-case PA5
gate.  The architecture audit adds checked course regressions for a cyclic
include and literal/include/pragma boundaries; the focused audit command
below runs the relevant 15 cases.  The high-byte include/`#line` and NUL
inputs are separate course-local `.audit` probes because the provided
reference decoder double-encodes the high-byte escapes outside the checked
handout suite.

Thus the checked-in PA5 inventory is 72 cases (the original 70 plus the cycle
and literal-boundary regressions); the `.audit` probe is not part of that count.

# Final Architecture Audit

The audit starts from clean `3f537c62f0d027a030ff2ec4a578d84d77373ca0`
(`pa5: implement typed preprocessing session`).  The pre-audit checked
evidence is the original PA5 70/70 and through-PA5 243/243 result.  The final
audit adds two checked course regressions, two direct audit probes, and the
owner fixes below.  Final validation completed at 72/72 for PA5, 245/245 through PA5,
5/5 stages, and 20/20 source files in the PA5 file audit.  The single commit
was made with subject `pa5: finalize full-stage architecture audit`;
`git show --check --oneline HEAD` passed and `git status --short` was empty.

# Architecture Review

- Source identity: `Tokenizer::source_line` feeds `TokenBufferStream`, which
  interns arbitrary spellings once and preserves fixed identities in `PPToken`.
  `process_file` assigns the current presumed-file ID and line to each logical
  line.  Replacement, paste, stringization, and builtin output inherit the
  invocation-head location; `__FILE__` and `__LINE__` resolve from that typed
  location.  `PPTokenBuffer` is the canonical PA5-to-PA2 handoff, and only the
  command-line `PostTokenOutput` renders the requested dump.
- Macro facts: `PPMacroSession` owns the ID-keyed definitions, typed parameter
  and replacement tokens, builtin seam, and unavailable-set paint.  Paste is
  the one intentional retokenization boundary; stringization is the required
  textual boundary.  `paint_nodes_` is now reset before and after every public
  expansion, including exceptional exits, because paint never escapes in a
  `PPToken`.
- Conditionals: `expand_control` keeps `defined` and its operand out of macro
  replacement.  PA5 now passes the operand's existing `PPSpellingId` through
  typed posttoken callbacks into `CtrlToken` construction and the evaluator;
  the macro owner no longer renders and relooks up the spelling for every
  occurrence.  One internal evaluator helper owns posttokenize, validation,
  evaluation, and `ParseError` policy; the public string and ID APIs only
  select the callback mode.  The older string callback remains only for PA3's
  standalone compatibility API.
- Includes and state: `FileFrame` owns actual/presumed names, line state, and
  token storage.  Macro-produced ordinary include strings and `#line`
  filenames follow the `PPToken` -> PA2 `LiteralData` -> narrow-char-array-
  bytes trace; direct quoted and angle includes remain textual `HeaderName`
  paths after delimiter removal.  `_Pragma` follows dedicated
  destringization, not ordinary literal decoding.  `PA5FileId` owns
  pragma-once identity, recursive processing merges included tokens without
  `sof`/`eof`, and a fresh session resets macros, counter, conditionals,
  include depth, and once state per command-line source.  The root frame starts
  at depth 0; the first included frame is depth 1; after identity resolution
  and once short-circuit, the owner rejects only an additional non-once read
  when the active depth is already 256.  Fixed `once` and `_Pragma` vocabulary
  IDs are interned with the session vocabulary and compared without rendering;
  only the dedicated destringized pragma text uses textual parsing.  Include
  operands containing NUL are rejected at the filesystem boundary, while
  `#line` presumed-file bytes remain valid non-filesystem state.
- Emission: `posttokenize_cpp_tokens` consumes the typed buffer directly.
  Include frames never emit section markers; `PostTokenOutput` is the only
  required dump renderer.  PA5 emits no object, machine-code, relocation, or
  generated-program metrics; those §7 measures are not applicable at this
  stage.

# Findings and Disposition

- Include boundedness was a confirmed blocker.  A two-header cycle previously
  reached SIGSEGV (status 139).  `PPPreprocessingSession::include_file` now
  resolves the candidate path and `PA5FileId`, honors pragma-once, then
  enforces a 256-frame active include-depth limit immediately before reading
  and recursing.  It restores the depth on exceptions.  This preserves
  sequential guarded/pragma-once reuse while making an unguarded cycle a
  deterministic `EXIT_FAILURE` at the owning include edge.  The course
  regression is `course/pa5/600-include-cycle.t` with two sibling headers.
- The duplicate preprocessing string decoder was a confirmed ownership and
  correctness defect.  Macro-produced ordinary `#include` operands now use
  PA2 `LiteralData`, and `#line` uses its converted second literal; direct
  `HeaderName` operands remain their specified text boundary.  Numeric byte
  escapes therefore remain byte values instead of being UTF-8 encoded a
  second time.  `_Pragma` retains only the specified quote/backslash
  destringization.  The checked course boundary regression is
  `course/pa5/600-literal-boundaries.t`; the high-byte observable include and
  `#line` probe is `600-literal-byte-escape.audit`.
- The conditional public APIs previously duplicated the entire production
  body.  A single internal templated helper now owns the shared policy while
  the string and spelling-ID callbacks remain compatibility modes.
- Fixed vocabulary lookup was a confirmed small ownership leak: direct
  `#pragma once` and `_Pragma` recognition now compare interned spelling IDs;
  the post-destringization `once` word comparison remains the intentional
  pragma text boundary.
- Embedded NUL bytes were a confirmed filesystem-boundary defect.  Include
  operands reject NUL before `PA5GetFileId` or `read_file`; a NUL-bearing
  `#line` presumed filename is retained as non-filesystem state, and any
  derived path is skipped rather than passed to `c_str()`-based filesystem
  calls.
- Retained macro paint was a confirmed scaling defect.  Public expansion
  outputs contain no paint references, so retaining the persistent trie across
  directive-flushed expansions was unjustified.  The canonical macro owner
  now reuses a one-root arena per expansion and clears it on success or error.
- No general render/re-tokenize path was found.  Paste and stringization remain
  the required PA4 textual boundaries; include paths, `#line` filenames,
  diagnostics, and the requested output are the other intentional text
  boundaries.  The typed `defined` ID callback removes the avoidable
  per-occurrence macro-name lookup in the PA5 control path.

# Changes

Implementation changes:

- `dev/src/macro.cpp`: expansion-local paint reset with exception cleanup.
- `dev/src/preproc_session.cpp`: canonical typed ordinary-literal conversion
  for macro-produced include/`#line`, direct header-name handling, dedicated
  `_Pragma` destringization, fixed vocabulary IDs, NUL-safe filesystem
  boundaries, recursion-edge include-depth enforcement, and typed-ID
  conditional lookup.
- `dev/src/posttoken.h` and `dev/src/posttoken.cpp`: optional typed spelling-ID
  callbacks for phase-3 identifier consumers.
- `dev/src/ctrlexpr.h` and `dev/src/ctrlexpr.cpp`: typed-ID conditional
  evaluator seam and one shared evaluator production helper while retaining
  the standalone string callback.
- No new `dev/src/*.cpp` file was added; `dev/frontend_source_sets.mk` is
  unchanged.

Regression and durable record:

- `cppgm.tests/course/pa5/600-include-cycle.t`,
  `pa5-audit-cycle-a.h`, `pa5-audit-cycle-b.h`,
  `600-include-cycle.ref`, and `600-include-cycle.ref.exit_status`.
- `cppgm.tests/course/pa5/600-literal-boundaries.t`,
  `600-literal-boundaries.ref`, `600-literal-boundaries.ref.exit_status`,
  `600-literal-boundaries.ref.stdout`, `pa5-audit-escaped-f.h`,
  `600-literal-byte-escape.audit`, `600-literal-nul-include.audit`, and
  `pa5-audit-byte-é.h`.
- `pa5/plan.md`: this architecture review, findings, measurements, validation,
  and checkpoint ledger.

# Measurement Evidence

The same immutable pre-audit binary copied from HEAD and the post-first-repair
binary were run five times in alternating order on equivalent inputs with
`/usr/bin/time`.  Values below are medians of wall/user/system seconds and
peak RSS in KiB; the later fixed-ID/NUL edits do not alter these measured
macro ownership paths.

| probe | stable input/output structure | pre-audit | post-first-repair |
| --- | --- | ---: | ---: |
| `macro-200-f40` | 442 lines/14,222 bytes; 200 eight-argument `R` calls through `F40`; 1,603 output lines/25,656 bytes | 0.80/0.30/0.47, 397,800 | 0.20/0.14/0.00, 6,968 |
| `macro-1000-f20` | 2,022 lines/68,222 bytes; 1,000 eight-argument `R` calls through `F20`; 8,003 output lines/128,057 bytes | 1.70/0.71/0.96, 794,988 | 0.40/0.36/0.01, 7,940 |

The macro-200 `perf stat` profile corroborates the owner change: task-clock
fell from 804 ms to 148 ms and page faults from 149,345 to 1,550; the
post-first-repair run had five context switches and no CPU migrations.
Outputs from before and after compared byte-for-byte for both macro probes.

An additional post-first-repair-only stress run with 8,082 lines/279,422
bytes, 4,000 eight-argument calls through `F80`, and 32,003 output lines/
512,048 bytes
completed in 6.71 s user-heavy time, 0.04 s system time, and 20,568 KiB RSS
(status 0).  The pre-audit binary exceeded the 20 s timeout at 6,313,732 KiB
RSS on this same stress input; this is recorded as a boundedness signal, not a
before/after timing claim.

The include-depth probe has one root plus 120 headers, produces 123 lines and
2,343 bytes, and completes successfully in about 0.10 s at about 7 MiB RSS.
The two-header cycle has a 49-byte partial output and now fails in 0.10 s at
7,252 KiB RSS after the 256-depth owner limit; the pre-audit binary crashed in
0.27 s with SIGSEGV.  A fresh final-owner probe measured the same depth
chain at 0.00 s/4,312 KiB and the cycle at 0.01 s/4,908 KiB (cycle status 1).
The checked course repeated-argument input is 8,006
lines; its focused post-first-repair run produces 67 lines/1,099 bytes
and passes.  The direct high-byte literal probe emits `line-é.cc` with
`6C696E652DC3A92E636300`, confirming two numeric byte code units plus the
terminating zero.

# Focused Validation

- `make -C dev preproc ctrlexpr posttoken macro CXX=g++ CPPGM_TEST_RUNNER=0`:
  status 0.
- `make -C pa5 ref-test REF_TEST_ROOT=course/pa5 TEST=course/pa5/600-include-cycle.t`:
  status 0; reference fixture records `EXIT_FAILURE`.
- `make -C pa5 ref-test REF_TEST_ROOT=course/pa5 TEST=course/pa5/600-literal-boundaries.t`:
  status 0; reference fixture records `EXIT_SUCCESS`.
- Focused `make -C pa5 check CPPGM_TEST_RUNNER=0 TEST='tests/200-if.t tests/200-include.t tests/400-header-guarded.t tests/500-pragma-ignore.t tests/501-pragma-op-ignore.t tests/600-line-macro.t tests/600-pragma-op.t tests/660-line-directive.t tests/800-pragma-once.t course/pa5/200-defined-identifier-like-operator.t course/pa5/300-defined-operator-misuse.t course/pa5/600-predefined-macro-argument-expansion.t course/pa5/600-repeated-argument-expansion.t course/pa5/600-include-cycle.t course/pa5/600-literal-boundaries.t'`:
  status 0, 15/15.
- Focused PA2 literal cases (`200-string-numeric-escape-code-units`,
  `200-unicode-character-literals`, `300-string-numeric-escape-out-of-range`):
  status 0, 3/3.  Focused PA3 control-expression cases: status 0, 4/4;
  focused PA4 macro ownership cases: status 0, 7/7.
- Direct `600-literal-byte-escape.audit` probe through `./dev/preproc`: status
  0; its escaped include and `#line` output contain the canonical UTF-8 byte
  sequence `C3 A9` and not the duplicate-decoder sequence.  Synthetic depth
  probe: status 0, 123 output
  lines; synthetic cycle probe: status 1 with `maximum include depth reached`;
  PA5 pragma-once and `_Pragma("once")` repeated-include cases are covered by
  the 15/15 command.
- Direct `600-literal-nul-include.audit` probe through `./dev/preproc`: status
  1 with `include path contains NUL`; fixed-ID pragma and include seams pass
  the focused final 7/7 subset.
- `git diff --check`: status 0.
- `perl scripts/cppgm_file_audit.pl --stage pa5 --paths dev/src`: status 0,
  20 files checked.
- `make test-report-through-pa5`: status 0, 245/245 total, PA5 72/72, 5/5
  stages.

The exact intended source, plan, course inputs, and reference-backed fixtures
were staged; no generated `.my`, `.check`, or temporary artifacts were staged.
The single final commit was made, `git show --check --oneline HEAD` passed, and
`git status --short` was empty.

# Remaining Uncertainties

The 256-frame limit is an implementation resource policy rather than a PA5
observable numeric contract; later stages may want a shared diagnostic/budget
policy.  The supplied reference tool double-encodes high-byte numeric escapes
in this new high-byte include/`#line` input, so that standard-correct
regression remains a course-local `.audit` probe rather than a successful
reference fixture; no reference stdout was hand-authored.  PA5 object,
machine-code, relocation, and generated-program metrics remain not applicable
because this stage stops at preprocessing.  The post-commit clean-tree check
was confirmed; no repository-state uncertainty remains.

# Checkpoint Ledger

- 2026-08-22: verified clean HEAD `3f537c62` and read the PA5 handout, plan,
  relevant tests, `TESTING_AND_REFERENCES.md`, and `spec.md` §§1–2, §4, and §7.
- 2026-08-22: baseline probes confirmed unbounded cyclic include failure
  (SIGSEGV) and retained paint growth (397,800 KiB at 200/40; 794,988 KiB at
  1,000/20 median).
- 2026-08-22: applied the include-depth, paint-arena, and typed conditional-ID
  repairs; added the earliest-layer course cycle regression and regenerated its
  reference fixture through the documented ref target.
- 2026-08-22: second review applied canonical PA2 literal ownership,
  dedicated `_Pragma` destringization, recursion-edge depth ordering, and one
  conditional evaluator helper; added the literal/include/pragma fixture and
  direct high-byte `#line` audit probe.  Focused build/check passed 15/15,
  PA2 3/3, PA3 4/4, PA4 7/7, direct depth/cycle/literal probes passed, and
  `git diff --check` passed.
- 2026-08-22: final owner fixes interned `once`/`_Pragma` vocabulary IDs and
  rejected NUL-bearing include operands at the filesystem boundary; the fixed
  owner subset passed 7/7, the NUL probe failed at the owning path as intended,
  file audit passed 20/20, and through-PA5 passed 245/245 (PA5 72/72, 5/5
  stages).  The exact intended 20 files were staged with no generated `.my`,
  `.check`, or temporary artifacts; commit `pa5: finalize full-stage
  architecture audit` was made, `git show --check --oneline HEAD` passed, and
  `git status --short` was empty.
