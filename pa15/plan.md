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
- PA15 uses dense `LabelId` indexing for one target block per label. The typed
  fact prepass marks referenced-label ancestors; recovery walks only those
  compound/switch structural paths, so dead non-label statements are not
  emitted. Normal fallthrough and forward/backward `goto` edges converge on
  the same block.
- PA15 sizes its label/fact value and generation arrays once for the
  translation unit. Each function advances a guarded epoch, so normal setup
  is O(1), each relevant fact is touched by the two bounded passes O(1) times,
  and typed label lookup is expected O(1); there is no F-times-TU clear. Thus
  total setup/traversal is O(TU label/fact storage + total relevant facts +
  gotos), with one defensive full stamp reset only on 32-bit epoch wrap.
  Output order and generated block identities remain deterministic. No
  textual label search, reference binary, host compiler, or test-specific
  output path is used.

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
statement lowering owners are complete and commit-ready. The corrective
generation-stamped setup retains the target pair at `2/2`; the expanded 406
owner regression passed, including positive typed semantic handoff, LowIR
validation, one-target forward recovery, and backward convergence,
dead-sibling omission, and duplicate/unresolved rejection. The
ordinary-goto/switch/loop/nested-compound matrix passed `6/6`.

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

Many-function affected-path artifact: `/tmp/pa15-label-many-functions.2kvXCG`.
The copied candidate compiler is mode `0555` with SHA-256
`d15163a4c0435daa88d2ef145669f9166f7be18ff8ad1d67c85ba522f649285e`;
its copied validator hash is
`c4d2af08ed8ca2c357790c174220f2fbbe753ce63524e20301f64456291cbf40`.
Each generated input has one label and one goto in every function. Forward
and reverse definition orders were run in five interleaved rounds, with five
samples per orientation and size. The input hash manifest is `inputs.sha256`
(manifest SHA-256
`bee2fb4d171f8be11e9ffb1da8edc82417a5c04eb8fad385ec43b5de8760121e`);
copied-validator validation was `8/8`.

| functions | forward median wall/RSS | reverse median wall/RSS | goto blocks/edges | LowIR bytes |
|---:|---:|---:|---:|---:|
| 64 | 0.00 s / 5,620 KiB | 0.00 s / 5,636 KiB | 64 / 64 | 8,369 |
| 256 | 0.01 s / 7,636 KiB | 0.01 s / 7,624 KiB | 256 / 256 | 33,449 |
| 1,024 | 0.05 s / 16,068 KiB | 0.05 s / 16,068 KiB | 1,024 / 1,024 | 134,105 |
| 2,048 | 0.10 s / 28,496 KiB | 0.10 s / 28,244 KiB | 2,048 / 2,048 | 270,297 |

The prior single-function artifact `/tmp/pa15-label-perf-final.GLh7wr` remains
a compact structural sanity sample: sizes 256/1,024/2,048/4,096 produced
the same number of goto blocks and edges as labels, with LowIR sizes
9,990/39,992/81,976/165,944 bytes. The dense target vector and generation
stamps avoid `std::map` lookup and global-width per-function clearing. This is
bounded affected-path evidence, not a universal or comparative speed claim;
timings and RSS are machine- and startup-sensitive.

## Checkpoint Ledger

| checkpoint | result | durable value |
|---|---|---|
| `959f9481` prior typed discard/return checkpoint | retained the six-test PA15 baseline | preserves earlier typed discard, return, and LowIR ownership work |
| `a2f33047` prior typed statement-CFG/label checkpoint | target `2/2`, owner regression PASS, adjacent `6/6`, PA15 `105/109` with `109/109` covered and four residuals, through-PA14 `1058/1058`, audit PASS | adds function-local typed label identity, one-target recovery, and deterministic compound/switch CFG lowering |
| current corrective label-flow checkpoint | target `2/2`, owner regression PASS, adjacent `6/6`, PA15 `105/109` with `109/109` covered and four residuals, through-PA14 `1058/1058`, audit PASS, diff-check PASS | replaces per-function TU-wide initialization with generation-stamped typed state and many-function evidence |
