# PA15 typed statement-CFG and label checkpoint

## Stage Design

The production path remains source -> PA10/PA11 typed facts -> PA12 typed
semantic facts -> PA15 typed LowIR -> PA13 serialization and validation.
The PA15 overview/grammar and the checked-in goto fixtures require this
procedural CFG behavior even though the Assignment Boundary list does not
explicitly enumerate `goto`; this plan does not claim that list enumerates
every procedural form.

- PA12 registers labels once per function in source order as typed `LabelId`
  records, rejects duplicate labels during registration, resolves each `goto`
  through the function-local table, and publishes labeled/goto statement
  facts. Unresolved `goto` is a semantic error.
- PA15 uses dense per-function `LabelId` indexing for one target block per
  label. A typed fact prepass marks referenced-label ancestors; recovery walks
  only those compound/switch structural paths, so dead non-label statements
  are not emitted. Normal fallthrough and forward/backward `goto` edges
  converge on the same block.
- The label path is bounded by semantic facts and gotos: PA12 uses the
  existing expected-O(1) flat index, and PA15 uses direct vector indexing plus
  two bounded fact passes. Output order and generated block identities remain
  deterministic. No textual label search, reference binary, host compiler, or
  test-specific output path is used.

## Failure Map

Turn-start baseline: `make test-pa15` was `103/109` passing with `109/109`
covered and exactly six failures. Final gate result is `105/109` with
`109/109` covered and exactly four residuals.

| test | baseline | final checkpoint result |
|---|---|---|
| `100-const-integral-lvalue-overload-category` | fail: PA12 unsupported expression | unchanged residual |
| `100-string-hex-escape-code-unit` | fail: parser expected primary expression | unchanged/deferred residual; broad string literals are listed OOS in the handout, but this checked-in fixture remains gating |
| `100-unnamed-parameter-storage` | fail: unnamed PA11 function definition | unchanged residual |
| `200-goto-case-block-entry-label` | fail: PA12 unsupported statement | fixed; focused PASS |
| `200-goto-case-block-label-after-statement` | fail: PA12 unsupported statement | fixed; focused PASS |
| `200-nested-conditional-array-decay` | fail: LowIR shape mismatch | unchanged residual |

## Active Checkpoint

The PA11 storage/model, PA12 semantic registration/resolution, and PA15
statement lowering owners are complete and commit-ready. The target pair
passed `2/2`; the expanded 406 owner regression passed, including positive
typed semantic handoff, LowIR validation, one-target forward recovery, and
backward convergence, dead-sibling omission, and duplicate/unresolved
rejection. The ordinary-goto/switch/loop/nested-compound matrix passed `6/6`.

Final gates:

- `make test-pa15` exited 2 at `105/109`, `109/109` covered; the exact four
  residuals are `100-const-integral-lvalue-overload-category`,
  `100-string-hex-escape-code-unit`, `100-unnamed-parameter-storage`, and
  `200-nested-conditional-array-decay`.
- `n=15; ... make test-report-through-pa14` exited 0 at `1058/1058`.
- `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` exited 0
  with five existing header-division warnings.
- `git diff --check` exited 0. The two new implementation translation units
  keep all checked source files under the 3,000-line audit limit.

## Performance Evidence

Affected-path artifact: `/tmp/pa15-label-perf-final.GLh7wr`. The copied candidate
compiler is mode `0555` with SHA-256
`6b2ce447133d89160b33063471f15bff64fa16de8a1ad03095a20364bca9fbd3`; its
copied validator hash is
`c4d2af08ed8ca2c357790c174220f2fbbe753ce63524e20301f64456291cbf40`.
Equivalent forward/reverse generated inputs were run in five interleaved
forward/reverse rounds, five samples per orientation and size. The input hash
manifest is `inputs.sha256` (manifest SHA-256
`0dff4d6a064f508a6b1f027e5739d8f2634e7a99d0790eb51be0e966b329e653`);
copied-validator validation was `8/8`.

| labels/gotos | forward median wall/RSS | reverse median wall/RSS | goto blocks/edges | LowIR bytes |
|---:|---:|---:|---:|---:|
| 256 | 0.00 s / 5,728 KiB | 0.00 s / 5,716 KiB | 256 / 256 | 9,990 |
| 1,024 | 0.01 s / 8,748 KiB | 0.01 s / 8,756 KiB | 1,024 / 1,024 | 39,992 |
| 2,048 | 0.03 s / 12,548 KiB | 0.03 s / 12,540 KiB | 2,048 / 2,048 | 81,976 |
| 4,096 | 0.06 s / 20,708 KiB | 0.06 s / 20,724 KiB | 4,096 / 4,096 | 165,944 |

The dense target vector and bounded fact prepass avoid `std::map` lookup per
label. This is bounded affected-path evidence, not a universal or comparative
speed claim; timings and RSS are machine- and startup-sensitive.

## Checkpoint Ledger

| checkpoint | result | durable value |
|---|---|---|
| `959f9481` prior typed discard/return checkpoint | retained the six-test PA15 baseline | preserves earlier typed discard, return, and LowIR ownership work |
| current PA15 typed statement-CFG/label checkpoint | target `2/2`, owner regression PASS, adjacent `6/6`, final PA15 `105/109` with `109/109` covered and four residuals, through-PA14 `1058/1058`, audit PASS, diff-check PASS | adds function-local typed label identity, one-target recovery, and deterministic compound/switch CFG lowering |
