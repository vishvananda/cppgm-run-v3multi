# PA9 checkpoint plan

## Stage Design: owner/data flow and spec alignment

`PPPreprocessingSession` owns phase-3 tokens per translation unit;
`posttokenize_cpp_tokens` decodes each literal once into a typed CY86 token
arena. The parser, semantic validator, label table, layout pass, fixups, ELF
writer, and direct x86-64 encoder form one backend ownership chain. Names are
interned, literal bytes are retained in one typed arena, and statements are
laid out and emitted in deterministic linear passes. CY86 registers map to
`rsp/rbp/r12/r13/r14/r15`; 32-bit writes use native zero-extension. Literal
payload size is distinct from ABI alignment: long-double payloads remain 10
bytes and align to 16 bytes, while string arrays align to their element type.
Address capture uses explicit non-CY scratch ownership; typed raw-literal
conversion feeds data, move, move80, and x87 red-zone materialization.

## Failure Map: all 18 turn-start failures grouped by owning missing capability, with baseline 0/18

Baseline: 0/18. The 11 PA9 failures were `100-noop`, `100-ret42`,
`110-hello-world`, `200-duplicator`, `210-reverser`, `220-hexdump`,
`300-binary-calculator`, `400-integer-calculator`, `500-to-float80`,
`501-from-float80`, and `600-float-calculator`; they shared the absent typed
frontend/backend boundary, grammar/semantics, layout/fixups, ELF, and native
emission. The seven course failures were `100-empty-program`, the three
invalid grammar cases, `400-negated-wchar-sign-extension`,
`500-long-double-label-alignment`, and `500-string-literal-element-alignment`;
they additionally exercised empty entry handling, rejection, typed negation,
and literal alignment.

## Active Checkpoint: precise scope, acceptance, uncertainties

Implemented scope is full PA9: typed capture; grammar and descriptor-aligned
semantic validation; label conflict/forward-reference ownership; literal/data
layout; exact local dynamic-branch placeholders; direct integer, transfer,
control, syscall, x87 arithmetic/comparison/conversion, and move80-immediate
encoding; and direct Linux x86-64 ELF writing. Source operands are addressed
by a systematic capture-before-destination-write rule, including two offset
memory sources and destination stores. Acceptance achieved: all 18 PA9 tests,
through-PA8, and the source audit pass. Focused probes also cover float raw
bits, immediate x87 arithmetic, string truncation/copy, and move80 label and
negative widening. Remaining uncertainty is limited to hidden descriptor and
dynamic-target combinations beyond the exercised public/probe cases.

## Performance Evidence: complexity and representative structural/timing/size evidence appropriate to material risks

The implementation performs linear preprocessing/token capture and parsing,
then one deterministic layout pass and one emission pass over statements,
fixups, and emitted bytes; literal conversion, address scratch selection, and
red-zone byte stores are proportional to their payloads. It does not retry
whole programs or render/reparse tokens. On
`pa9/tests/400-integer-calculator.t.1` (7,015 source bytes and 160 source
semicolon statements), three equivalent compiler runs each emitted the same
24,187-byte ELF (20,091 bytes after the 4,096-byte load/header area).
Observed `/usr/bin/time` results were wall `0.01 s` each and maximum RSS
6,380, 6,320, and 6,424 KiB (median RSS 6,380 KiB). These are structural and
resource observations for this representative case, not comparative speed
claims.

## Checkpoint Ledger: milestone status and exact test counts/commands

- Turn start: `make test-pa9` baseline 0/18; git `081d4786` clean.
- First review build: `make -C dev cy86` passed.
- First review focused command passed 7/7:
  `make -C pa9 check TEST='tests/100-noop.t.1 tests/100-ret42.t.1 tests/110-hello-world.t.1 course/pa9/100-empty-program.t.1 course/pa9/300-negative-memory-literal-bad.t.1 course/pa9/300-unparenthesized-label-offset-bad.t.1 course/pa9/300-unparenthesized-negative-immediate-bad.t.1'`.
- First review pre-fix focused command exited 2 with `FAIL (2/6)`:
  `make -C pa9 check TEST='tests/200-duplicator.t.1 tests/210-reverser.t.1 tests/220-hexdump.t.1 course/pa9/400-negated-wchar-sign-extension.t.1 course/pa9/500-long-double-label-alignment.t.1 course/pa9/500-string-literal-element-alignment.t.1'`.
  Before the later fixes, reverser generated status 1 (then 139 after the
  test-register correction) and long-double alignment generated status 6;
  the negated-wchar and string-alignment cases did not report errors.
- Authorized focused correction command passed 6/6:
  `make -C dev cy86 && make -C pa9 check TEST='tests/210-reverser.t.1 tests/400-integer-calculator.t.1 tests/500-to-float80.t.1 tests/501-from-float80.t.1 tests/600-float-calculator.t.1 course/pa9/500-long-double-label-alignment.t.1'`.
- Dynamic register-target probe passed with exit status 0.
- Final PA9 gate: `make test-pa9` passed exactly 18/18.
- Required through-PA8 report passed exactly 433/433:
  `n=9; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`.
- Required audit passed with one existing warning outside this checkpoint:
  `perl scripts/cppgm_file_audit.pl --stage pa9 --paths dev/src` reported the
  `cpp_semantic_core.h` implementation-body warning.
- Before/after PA9 count: 0/18 at turn start, 18/18 after implementation.
- Post-commit correction focused checks: `make -C pa9 check TEST=tests/210-reverser.t.1`,
  `tests/400-integer-calculator.t.1`, `tests/600-float-calculator.t.1`, and
  `course/pa9/500-long-double-label-alignment.t.1` passed 4/4; the two-offset
  `/tmp` probe exited 42 and the typed-literal `/tmp` probe exited 42.
- Final correction gates: `make test-pa9` passed exactly 18/18;
  `n=9; ... make test-report-through-pa$((n - 1))` passed exactly 433/433;
  the required audit passed with the same header warning; `git diff --check`
  passed.
- Final hygiene: generated check/test artifacts were removed; no checked-in
  tests or references were changed.
- Final post-amend correction: address widening now emits explicit zero bytes
  above 64 bits; the strengthened `/tmp` move80-label probe checked both the
  low label address and bytes 8..9 (exit 42), followed by the required gates.
