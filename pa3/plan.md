# PA3 final architecture audit

This record covers the repair from clean HEAD
`5f662476028b2da0b896c20cf411133fd0d75f87` and the final PA3 validation.

## Spec alignment

PA3 remains one adapter around the production frontend path.  `run_ctrlexpr`
reads source, `posttokenize_cpp_source_by_line` preserves logical-new-line
events, and the shared PA1 tokenizer feeds the shared PA2 typed post-token
conversion.  PA2's ordinary `posttokenize_cpp_source` newline-ignoring entry
point is unchanged.

The evaluator implements the PA3 grammar, usual precedence and associativity,
alternative word operators, identifier/keyword zero semantics, `true`/`false`,
mock `defined`, signed and unsigned arithmetic, static conditional result
signedness, short-circuit evaluation, checked arithmetic faults, per-line
error isolation, and typed EOF output.  §7 measurement applies here only to
the frontend-only `ctrlexpr` executable: there is no generated-code or backend
quality artifact to measure, so object hashes, sections, instructions,
relocations, and similar metrics are not applicable.

## Architecture and typed ownership review

- Source boundary: `run_ctrlexpr` -> `posttokenize_cpp_source_by_line` ->
  shared PA1 `tokenize_cpp_source`.  The line-aware adapter flushes pending
  strings and resets line state at logical new-line, then emits typed EOF.
- Identifier origin: PA1 classifies alternative operator spellings and calls
  `IPPTokenStream::emit_identifier_as_preprocessing_op_or_punc`; PA2's
  `PostTokenStream` calls `emit_simple_identifier` after fixed-vocabulary
  lookup.  `LineOutput` retains only typed `SimpleTokenType` and the
  identifier-origin, `defined`, and parity facts needed by PA3.  Keywords and
  alternative words remain identifier-or-keyword primaries where the grammar
  permits them; punctuation-origin tokens do not.
- Literal path: PA2 creates `LiteralData` once; `LineOutput` validates a
  non-array integral scalar and decodes its bytes once into `CtrlToken` bits
  and signedness.  Arena leaves and `Value` retain typed facts; only the
  final value is rendered as decimal text.
- Logical lines: conversion and parse/evaluation state is cleared per line;
  conversion, parse, and evaluation faults emit `error` and later lines
  continue; EOF emits `eof`.  Phase 1--3 failures still return process
  failure.

## Finding and repair

At the clean baseline, flat binary chains were iterative but controlling
expression and parenthesized-primary parsing still recursed.  Valid depth
50,000 parentheses and right-nested conditionals crashed with SIGSEGV/139.

The recursive ownership path is replaced by a shunting-style parser with typed
`ParseOperator` and value-index stacks.  Reductions build a compact `ExprNode`
arena containing enums, scalar literal bits/flags, and node indices only.  An
explicit `EvaluationFrame` stack performs iterative post-order evaluation.
Active-value flags suppress inactive logical and conditional faults while
conditional branches still contribute their static common signedness.  There
is no nesting cap, error downgrade for valid input, textual literal recovery,
per-node owning string, reference/host/compiler call, or compiler shell-out.

## Performance and retained storage

Immutable executables were built outside the worktree from the clean baseline
and the candidate source with `make -C dev -s ctrlexpr CPPGM_TEST_RUNNER=0`:

| executable | SHA-256 |
| --- | --- |
| clean baseline | `9c57be6238ae93bc7021ef47fba888a330fa2092df918c782858ac6744f699c8` |
| candidate | `9bf4edec9ff2365ab2b983493ed814ae7f816468d21000f24daa4f0202007cf8` |

The flat input was 400,002 bytes, SHA-256
`89cef11ff9a2c5d50613f98a53a3060117dee9f8b229fe3973664ee223eefc26`, with
200,000 `+` operators and 400,001 typed tokens.  Each executable ran five
times in alternating baseline/candidate order under `/usr/bin/time -f
'%e %U %S %M %x'`; the table reports medians of wall, user, system, peak RSS
KB, and exit status:

| executable | wall s | user s | sys s | peak RSS KB | exit |
| --- | ---: | ---: | ---: | ---: | ---: |
| clean baseline | 0.16 | 0.13 | 0.02 | 27,916 | 0 |
| candidate | 0.20 | 0.15 | 0.04 | 53,016 | 0 |

The candidate cost is therefore measurable: about 0.04 seconds wall and
25,100 KB peak RSS on this flat case.  It is the expected cost of retaining
typed AST nodes and an explicit evaluation frontier needed to remove process
stack dependence; it is not claimed to be free.

Five interleaved candidate rounds (case order reversed on alternating rounds)
gave these medians.  Input bytes include the final newline; token counts
exclude the logical newline.

| case | structure | bytes | tokens | wall s | user s | sys s | peak RSS KB |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| flat100k | 100,000 `+` operators | 200,002 | 200,001 | 0.10 | 0.07 | 0.02 | 28,192 |
| flat200k | 200,000 `+` operators | 400,002 | 400,001 | 0.20 | 0.14 | 0.05 | 53,004 |
| paren10k | 10,000 nested pairs | 20,002 | 20,001 | 0.01 | 0.00 | 0.00 | 5,324 |
| paren50k | 50,000 nested pairs | 100,002 | 100,001 | 0.03 | 0.02 | 0.00 | 12,240 |
| cond10k | 10,000 right-nested `?:` levels | 40,002 | 40,001 | 0.02 | 0.01 | 0.00 | 8,180 |
| cond50k | 50,000 right-nested `?:` levels | 200,002 | 200,001 | 0.10 | 0.07 | 0.02 | 26,932 |

The scaling is consistent with O(source bytes + tokens) work and O(tokens)
live storage: the tokenizer/line conversion, compact `CtrlToken` vector,
node arena, parser stacks, and evaluation frames are each bounded by the
logical line.  The nested rows exercise explicit ownership rather than
process-stack growth.

The debug-layout probe reported `sizeof(CtrlToken)=24`, `ExprNode=48`,
`ParseOperator=32`, `EvaluationFrame=48`, and `Value=16`.  For the flat
200,000-operator line, the current parser completed with:

- 400,001 `ExprNode`s, capacity 400,001: 19,200,048 byte-equivalent;
- parser operator stack size/capacity 0/1: 32 byte-equivalent;
- parser value-index stack size/capacity 1/2: 16 byte-equivalent;
- 400,001 `CtrlToken`s: 9,600,024 byte-equivalent;
- evaluation frame warm capacity `nodes.size()/2 + 2` = 200,002:
  9,600,096 byte-equivalent; the left-deep flat tree reaches at most 200,001
  live frames and a two-value result frontier.

The unconditional full-size reserves were inspected and removed for operator,
value-index, and result vectors.  The arena reserves only a token scan's
node-producing estimate (parentheses and colons do not reserve nodes); frames
get the half-node warm reserve above and other vectors grow with live state.
For comparison, the pre-adjustment probe retained full capacities for the
400,001-element operator, value, frame, and result vectors: approximately
12,800,032 + 3,200,008 + 19,200,048 + 6,400,016 byte-equivalent in addition
to the arena.  The adjustment removes that unnecessary retained capacity.
RSS remains near 53 MB because the required token/arena/frame traversal
touches its pages; virtual capacity reduction is nevertheless real and the
records remain compact, typed, and O(tokens).

## Baseline regression proof

`cppgm.tests/course/pa3/150-deep-nesting.t` contains 20,000 nested
parentheses and 20,000 right-nested conditionals (120,004 bytes).  Against the
immutable clean baseline in the same environment, the run terminated with
shell status 139 and signal 11 (`SIGSEGV`), after 0.24 seconds and 16,140 KB
peak RSS.  The immutable candidate returned shell status 0 after 0.05 seconds
and emitted `1`, `1`, `eof`.  The fixture and its two sidecars were generated
only with:

`make -C pa3 ref-test TEST='../cppgm.tests/course/pa3/150-deep-nesting.t'`

The 50,000-depth parenthesis and conditional candidate runs also exited 0;
the separate 50,000-depth malformed-parenthesis/conditional run emitted
`error`, `error`, `7`, `eof`, proving line isolation without a valid-input cap.

## Validation

- Focused semantic matrix: 43 expected output lines passed, covering nested
  true/false conditional association, every binary-precedence boundary,
  parentheses/unary, signed and unsigned literals, alternative-word versus
  identifier-origin behavior, `defined`, inactive and active arithmetic
  faults, common conditional signedness, malformed input, and recovery.
- Focused repository checks passed 1/1 each for `tests/100-primary.t`,
  `tests/120-defined.t`, `tests/200-ops.t`, `tests/250-eval-order.t`, and
  the new public deep-nesting test.
- Standalone gates: PA1 report-through `54/54`; PA2 report-through `80/80`;
  PA3 file audit `16 files checked`; required report-through-PA3
  `101/101` (PA3 contributes 21 tests, including the new regression).
- `git diff --check` passed after the final plan consolidation.  Generated
  code quality metrics are intentionally out of scope as recorded above.

## Final checkpoint ledger

- Baseline crash ownership and candidate repair proof complete.
- Typed arena, explicit operator/evaluation stacks, reserve/layout review,
  and O(tokens) storage accounting complete.
- Public deep-nesting regression and documented reference sidecars complete.
- Focused matrix, immutable measurements, file audit, and all standalone
  through-stage reports complete.
- Complete intended diff reviewed; this consolidated audit/repair change is
  recorded as the single worker commit containing this plan.  The final
  repository HEAD and clean-status result are reported with the handoff.
