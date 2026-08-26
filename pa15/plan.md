# PA15 typed conditional-array checkpoint

## Stage Design

The production path is source -> PA10/PA11 typed facts -> PA12 semantic
facts -> PA15 typed LowIR.  Conditional meaning stays at the PA12
semantic-fact boundary; PA15 lowers those facts without inspecting source
spelling or emitted text.

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

Turn-start baseline was `105/109` passing with `109/109` covered and exactly
four failures.  Final `make test-pa15` reports `106/109` with `109/109`
covered and only the three unrelated residuals below; its expected make exit
status is `2`.

| test | baseline | final current aggregate |
|---|---|---|
| `100-const-integral-lvalue-overload-category` | fail | residual; unrelated to this checkpoint |
| `100-string-hex-escape-code-unit` | fail | residual; unrelated to this checkpoint |
| `100-unnamed-parameter-storage` | fail | residual; unrelated to this checkpoint |
| `200-nested-conditional-array-decay` | LowIR fail | pass; absent from the full-gate failure list |

## Active Checkpoint

This checkpoint owns the nested conditional-array LowIR failure and is
complete.  The implementation increment is in
`dev/src/pa12_semantic.cpp`, `dev/src/pa15_lowering.cpp`, and
`dev/src/pa15_lowering_flow.cpp`; no tests or `.ref` fixtures were changed.
The full PA15 gate is `106/109` with complete `109/109` coverage.  Through
PA14 is `1058/1058`; the PA15 file audit exits 0 with five existing
header-division warnings.  String parsing, unnamed-parameter storage,
overload-category behavior, labels/CFG, classes, and templates remain outside
this checkpoint.

## Performance Evidence

Generated affected-path sources were kept under `/tmp`.  Each contains one
three-level nested conditional site per generated selector: an outer
differing-bound array conditional and an inner same-bound array conditional.
The normal PA15 runner compiled each with `--emit-lowir -O0`; temporary
self-reference files were used only to invoke the normal LowIR validator, not
as semantic oracles.  Both validator runs passed.

| repeated sites | source size | LowIR size | wall | RSS |
|---:|---:|---:|---:|---:|
| 64 | 772 lines / 16,030 bytes | 4,503 lines / 102,997 bytes | 0.10 s | 9,724 KiB |
| 1,024 | 12,292 lines / 265,522 bytes | 71,703 lines / 1,657,033 bytes | 0.37 s | 72,508 KiB |

The 64-site run was `scripts/compare_results.pl ref my
/tmp/pa15-conditional-scale-64.t` -> `PASS (1/1)`; the 1,024-site run used
the corresponding `...1024.t` path and also passed `1/1`.  The samples show
the expected roughly linear source/LowIR growth and no material nonlinear
signal at these bounded sizes, but do not constitute an asymptotic benchmark.
The justified complexity bound remains
`O(sum of local child/conversion ranges + emitted LowIR)`.

## Checkpoint Ledger

| checkpoint | result | durable value |
|---|---|---|
| `959f9481` | prior typed discard/return checkpoint retained | typed discard, return, and LowIR ownership |
| `a2f33047` -> `d5e10599` -> `f038141d` | prior typed statement-CFG/label sequence completed | typed label identity, shared recovery, deterministic statement CFG, and sparse flow state |
| current typed conditional-array checkpoint | PA15 `106/109`, all `109/109` covered with three named residuals; through-PA14 `1058/1058`; audit and diff checks pass | preserves same-category array glvalues, keeps typed array-to-pointer ownership, and removes spelling-based conditional address selection |
