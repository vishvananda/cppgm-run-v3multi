# Stage Design

`dev/ctrlexpr.cpp` is a thin stream adapter over `dev/src/ctrlexpr.cpp`.
The shared PA1 phase 1--3 tokenizer feeds one shared PA2 typed post-token
conversion owner.  `posttokenize_cpp_source` keeps newline a true no-op for
PA2, while the explicit `posttokenize_cpp_source_by_line` adapter flushes
string sequences and emits typed logical-line boundaries for PA3.  A typed
identifier-origin callback preserves preprocessing identifier provenance,
including alternative word operators, without recovering it from a rendered
dump.  PA3 stores each active line as compact scalar `CtrlToken` records:
operator/kind, decoded `uint64_t` bits and signedness, and only the identifier
flags needed for primary and `defined` semantics.  The parser uses precedence
functions, iterative left-associative chains, and an active-evaluation flag
for short-circuiting and conditional branches.

# Failure Map

Baseline at HEAD `8a0d4488238dcc8e5c01141db80b1f689657e18d`: 0/20 PA3
existing failures, all from `CPPGM_EXIT_NOT_IMPLEMENTED` placeholder output.
Phase 1--3 exceptions return `EXIT_FAILURE`; typed conversion, grammar, and
evaluated arithmetic faults become per-line `error`, with later lines still
processed.  Empty lines emit nothing and successful phase processing ends in
`eof`.

# Active Checkpoint

The complete checked-in PA3 grammar and integral evaluation is implemented:
identifier/keyword zero semantics, `true`/`false`, mock `defined`, all mapped
operators including alternative word operators, usual signed/unsigned
conversions, static conditional result type, short-circuit evaluation,
checked division/modulo/shifts, and modulo-2^64 arithmetic without host
undefined behavior.  PA2's newline-ignoring behavior remains unchanged, and
literal facts are decoded and validated once at callback ingestion rather than
retained in the line vector.

# Performance Evidence

The source and PA1 tokenizer retain O(source bytes) buffers; PA3's additional
live storage is only the compact line token vector, O(max logical-line
tokens), with no per-token owning strings, `LiteralData`, or literal-byte
vectors.  Token conversion and evaluation are O(source bytes + tokens)
overall.  Measured direct runs were:

- 1,000 / 10,000 / 100,000 `1 + 2` lines: 6 KB / 60 KB / 600 KB input,
  0.00 / 0.01 / 0.16 seconds, and 3,736 / 5,256 / 21,008 KB maximum RSS.
- A 200,000-operator flat `1+1+...` line: 400,002 bytes, 0.17 seconds,
  27,832 KB maximum RSS; this exercises the iterative left-associative path.

The scaling is consistent with linear ordinary work and source-sized live
storage.  Recursive calls are bounded by the fixed precedence stack plus
unary, parenthesis, and nested-conditional depth in one logical line; deeply
nested input remains the principal stack-depth risk.

# Checkpoint Ledger

- Baseline recorded: 0/20 passing, 20 failing.
- Focused validation: PA2 string-concatenation checks passed 2/2; PA3
  representative checks passed 6/6; manual provenance/type cases produced
  `1`, `0`, `1`, `-1`, and punctuation in `defined(+)` produced `error`.
- Broad validation: `make test-pa3` passed 20/20; report-through-PA2 passed
  80/80; PA3 audit passed 16/16 files; report-through-PA3 passed 100/100.
- Final diff check passed.  Commit is the remaining handoff action.
