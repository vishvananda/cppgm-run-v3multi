# Stage Design

PA5 uses one reusable `PPPreprocessingSession` in `dev/src` per command-line
source.  The session owns the macro environment, conditional/include stacks,
presumed file and line, pragma-once file IDs, counter, and one typed
`PPTokenBuffer` spelling arena.  It consumes phase-3 tokens from the existing
tokenizer, sends directive operands and active text through the shared PA4
macro owner, and hands the resulting typed buffer directly to PA2
post-tokenization.  `dev/preproc.cpp` is only the framing, file-I/O, and
presentation adapter; a fresh session resets all source-local state.

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

The four cluster counts are disjoint and sum to the complete 70 cases.

# Active Checkpoint

Baseline at turn start: `make test-pa5` = 0/70.  All cases currently exit
`EXIT_NOT_IMPLEMENTED` because `dev/preproc.cpp` throws before opening the
output.  The first required milestone is an uncommitted session/adapter
boundary with focused passing coverage for empty/text output, PA4 macro reuse,
invalid-token/error/null/non-directive handling, then pause for Sol review.

Milestone evidence: focused `make -C pa5 check TEST='...'` covering the
boundary and PA4 cases passed 15/15; `make test-report-through-pa4` passed
173/173; the first coherent session boundary passed 67/70.  The typed
provenance/buffering correction then passed the three cross-line cases plus
`610-line-macro`, `660-line-directive`, `200-include`, and `800-pragma-once`
7/7.  Final `make test-pa5` passes 70/70, with PA1–PA4 unchanged at 173/173;
the behavioral clusters above cover the complete set without adding tests or
changing fixtures.

# Performance Evidence

The representative course `600-repeated-argument-expansion` case passes as
part of the 7/7 course run; its checked input is 8,006 lines.  A bounded
cross-line/depth probe (`600-line-macro`, `700-redef-q`, and `800-pragma-once`)
passes 3/3, covering a multiline invocation and recursive include/once state.
The implementation makes one forward token/directive pass per file and only
flushes accumulated active text at semantic boundaries.  Conditional and
include state are O(depth); macro, directive, builtin, and pragma-once lookup
use hash tables with O(1)-average lookup; the persistent PA4 unavailable-set
trie copies a fixed machine-word path rather than a whole set per token.  The
source-line index is built once for tokenizer metadata, and there are no
whole-source rescans or repeated trial expansions.  This is structural
evidence only; no timing claim is made.

# Checkpoint Ledger

- 2026-08-22: inspected PA1–PA4 typed owners, PA5 handout/tests, and
  `spec.md` §§1–2 and §7.  Worktree clean at `c5d18a5d`.
- 2026-08-22: baseline `make test-pa5`: 0/70; no implementation changes yet.
- 2026-08-22: first uncommitted milestone: typed session/adapter implementation
  builds; focused PA5 15/15, through-PA4 173/173, full PA5 67/70.
- 2026-08-22: corrected typed source provenance, physical-line accounting,
  cross-logical-line buffering, and invocation-location builtin propagation;
  focused correction cases 7/7, PA5 plus course 70/70, through-PA4 173/173,
  and file audit passed.
- 2026-08-22: required validation passed: PA5 70/70, through-PA4 173/173,
  through-PA5 243/243, source audit 20 files, bounded probe 3/3, and
  `git diff --check`; commit intent is to record this full coherent checkpoint
  and verify the committed tree is empty.
