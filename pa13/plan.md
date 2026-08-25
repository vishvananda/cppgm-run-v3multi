# PA13 checkpoint

## Stage Design

Owner: PA13 implementation worker. `argv -> files in command-line order -> lexer/parser -> typed lowir_model::Program -> validator -> CY86 emitter -> output file`. The driver also maps the wrapped and standalone batch-stdin protocols to the same compile boundary. The parser preserves top-level/function/block/instruction order; validator and emitter use indexed symbol/function/declaration maps and per-function locations.

## Failure Map

Turn-start baseline: provider progress reported 0/114; the checked-in last primary log reported 0/96, all `EXIT_NOT_IMPLEMENTED`. Ownership was the `dev/lowir2cy86.cpp` scaffold. Current families are all owned by the five-file boundary: lexer/parser and multi-file/batch driver; structural/type/metadata/rejection validation; scalar, pointer, f32/f64/f80, global/structured-data, address/index/load/store, bulk, atomic, control-flow, conversion, direct/indirect-call and direct-object ABI emission; and EH/runtime emission. Complete fixture outcome: no remaining PA13 failures.

## Active Checkpoint

Implemented in `dev/lowir2cy86.cpp`, `dev/src/lowir2cy86_backend.{h,cpp}`, `dev/frontend_source_sets.mk`, and this plan. It parses the complete checked-in PA13 surface, validates the structural/type/metadata contract, and deterministically emits exact PA9 CY86 including startup/init/fini, globals, object ABI, f32/f64/f80 operations and conversions, atomics, control flow, and the three EH/runtime fixtures. Tests and references were not changed. Stable boundary: `lowir2cy86_backend.h::compile`. Exit criterion: PA13, prior-through-PA12, audit, diff-check, and clean post-commit status.

## Performance Evidence

Lexing/parsing and validation are single traversals with indexed `std::map`/`std::set` lookups; emitter symbol kind, function, and declaration maps are built once, so per-instruction resolution is logarithmic rather than a whole-program scan. A generated non-grading stdin workload succeeded at 256 globals/2,048 instructions in 0.02 s elapsed, 0.01 s user, 8,300 KB RSS, 174,764-byte output; 512 globals/4,096 instructions in 0.04 s elapsed, 0.03 s user, 12,812 KB RSS, 352,172-byte output.

## Checkpoint Ledger

- Start: clean at `5f05fce1 PA12: finalize architecture audit`.
- Focused milestone matrix: 19/19.
- `make test-pa13`: `===== ALL TESTS PASSED SUCCESSFULLY! (96 / 96) =====`.
- `n=13; ... make test-report-through-pa12`: `===== ALL TESTS PASSED SUCCESSFULLY! (851 / 851) =====`.
- `perl scripts/cppgm_file_audit.pl --stage pa13 --paths dev/src`: passed; two pre-existing header warnings, no fatal issues.
- Batch probe: valid three-field request `EXIT_SUCCESS`; valid wrapped five-field request with a rejection `EXIT_FAILURE`; driver exited 0 and emitted the expected diagnostic.
- `git diff --check`: passed.
- Commit: created by the following command; final id is reported after commit.
