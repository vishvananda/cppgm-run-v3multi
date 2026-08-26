# PA15 typed conditional-array checkpoint

## Stage Design

The production path is source -> PA10/PA11 typed facts -> PA12 semantic
facts -> PA15 typed LowIR -> PA13 serialization/validation.  Conditional
meaning stays at the PA12 semantic-fact boundary; PA15 lowers those facts
without inspecting source spelling or emitted text.  This checkpoint audits
landed `b7eaf9d868b17cbbf542f3415e7a5e46f07007ba`, parent
`f038141d14cc5c9d10e01964d3a1bdf3a6c5f4ca`.

- PA12 preserves an array result when the operands have the same object type
  (under the existing top-level-cv common-type rule) and the same non-prvalue
  value category.  This covers same-bound lvalue and xvalue arrays.  When
  bounds or value categories differ, the existing pointer common-type path
  records the required typed `ArrayToPointer` conversions.
- PA15 selects conditional address lowering from the result fact's category
  and object type, or its typed `ReferenceBinding` conversion range.  A
  conditional carrying an `ArrayToPointer` fact is exposed as its selected
  address, avoiding duplicate decay; ordinary array expressions retain their
  normal typed decay.
- The bound is `O(sum of local child/conversion ranges + emitted LowIR)`:
  constant work per conditional plus fact-local child/conversion scans.  There
  are no body rescans, textual reparsing, or heuristic child reconstruction.

## Failure Map

The supplied turn-start baseline and authorized final full-stage result are
both `106/109` passing with `109/109` covered and exactly the three unrelated
failures below.  Mechanical comparison of the final PA15 failure names found
no added or replacement failure.  `make test-pa15` exits `2` for these three
residuals, while the affected nested conditional remains absent from the
failure set.

| test | supplied baseline | checkpoint disposition |
|---|---|---|
| `100-const-integral-lvalue-overload-category` | fail | residual; unrelated to this checkpoint |
| `100-string-hex-escape-code-unit` | fail | residual; unrelated to this checkpoint |
| `100-unnamed-parameter-storage` | fail | residual; unrelated to this checkpoint |
| `200-nested-conditional-array-decay` | pass; absent from failure set | focused `PASS`; landed conditional-array increment confirmed |

## Active Checkpoint

This checkpoint audits the nested conditional-array ownership path.  No
bounded source repair was necessary: the committed implementation in
`dev/src/pa12_semantic.cpp`, `dev/src/pa15_lowering.cpp`, and
`dev/src/pa15_lowering_flow.cpp` remains unchanged, and no tests or `.ref`
fixtures were changed.  The final gate logs are in
`/tmp/pa15-audit-gates-20260826`: PA15 `106/109`, through-PA14 `1058/1058`,
file audit exit `0`, combined through-PA15 `1164/1167`, and final
`git diff --check` exit `0` (`diff-check-final.log`).  String parsing,
unnamed-parameter storage, overload-category behavior, labels/CFG, classes,
and templates remain outside this checkpoint.

## Performance Evidence

Fresh exact inputs and a compact generator are preserved in
`/tmp/pa15-audit-scale-final-20260826-samebound` beside the expanded
`command.log`. Each selector uses three same-bound `int[4]` lvalue array arms
with a nested conditional; differing-bound behavior is covered by the
checked-in affected fixture and focused probes. The normal PA15 runner
compiled each input three times with `--emit-lowir -O0` under `timeout 60s`;
all six outputs passed the normal `lowir2cy86` validator. This is a bounded
affected-path sample, not an asymptotic benchmark.

| repeated selectors | input lines / bytes | LowIR lines / bytes | `condaddr` blocks | address ops | decay ops | wall samples | RSS samples (KiB) |
|---:|---:|---:|---:|---:|---:|---|---|
| 64 | 258 / 6,168 | 3,080 / 66,208 | 384 | 192 | 0 | `0.01, 0.01, 0.01` s | `7,856, 7,940, 8,108` |
| 1,024 | 4,098 / 106,300 | 49,160 / 1,069,864 | 6,144 | 3,072 | 0 | `0.17, 0.17, 0.17` s | `50,316, 50,072, 50,276` |

The structural counters scale by exactly 16x from 64 to 1,024 selectors, with
stable per-size LowIR hashes recorded three times in `output-hashes.tsv`:
`0a9eaef30cc63aa80bd919108bc9c37596f630239988cb7282a2038ac8b7ef64` for 64
and `ad4bea5b3a7ce62bb140a545428bd22ba2f2986dd5014fa12a123349ac3f85ac` for
1,024.  Candidate and validator hashes are in `candidate.sha256` and
`validator.sha256`; `input-output-manifest.sha256` and
`artifact-manifest.sha256` cover the recipe, inputs, outputs, and validation
artifacts.  The justified complexity bound remains
`O(sum of local child/conversion ranges + emitted LowIR)`; the local scans do
not rescan source bodies or retry lowering.

## Focused Evidence

- The six direct affected-path and adjacent PA15 tests pass `6/6`.
- The broader related PA15 matrix passes `15/15`; the PA12 checked-in array
  xvalue fixture passes `1/1`.
- Ephemeral `/dev/stdin` probes for lvalue contextual use, xvalue nesting,
  mixed lvalue/xvalue categories, array lvalue/xvalue reference parameters,
  and scalar prvalue reference materialization compile and validate with
  exit `0`.
- Semantic dumps retain array lvalue/xvalue categories, and the affected
  LowIR contains no duplicate inner-array decay. No durable regression test
  was needed; handouts and fixtures remain immutable.

## Next Checkpoint

The next bounded work is the three remaining owner surfaces, each kept
separate from this conditional-array checkpoint:

- `100-const-integral-lvalue-overload-category`
- `100-string-hex-escape-code-unit`
- `100-unnamed-parameter-storage`

The final gate logs and performance artifact for this checkpoint are retained
in `/tmp/pa15-audit-gates-20260826` and
`/tmp/pa15-audit-scale-final-20260826-samebound`; no broad-validation work
remains for this checkpoint.

## Checkpoint Ledger

| checkpoint | result | durable value |
|---|---|---|
| `959f9481` | prior typed discard/return checkpoint retained | typed discard, return, and LowIR ownership |
| `a2f33047` -> `d5e10599` -> `f038141d` | prior typed statement-CFG/label sequence completed | typed label identity, shared recovery, deterministic statement CFG, and sparse flow state |
| current typed conditional-array checkpoint at `b7eaf9d8` | final `make test-pa15` exit `2`, `106/109` with `109/109` covered and exactly the three named residuals; through-PA14 exit `0` at `1058/1058`; file audit exit `0` with five existing warnings; through-PA15 exit `2` at `1164/1167`; focused matrices and validators pass; logs in `/tmp/pa15-audit-gates-20260826` and performance manifest in `/tmp/pa15-audit-scale-final-20260826-samebound` | preserves same-category array glvalues, keeps typed array-to-pointer ownership, and removes spelling-based conditional address selection |
