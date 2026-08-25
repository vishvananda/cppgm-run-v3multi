# PA13 final architecture-audit plan

## Scope and final status

- Review starts from HEAD `3521adf33136ddf5ca477170eda4caa54c299632` (`PA13: implement LowIR to CY86 adapter`) with the inherited typed-ownership repair and this architecture split intentionally uncommitted and unstaged.
- The production boundary is `dev/src/lowir2cy86_backend.h::compile`; `lowir_text_adapter.cpp` preserves command-line LowIR file order and the backend emits deterministic PA9 CY86 text.
- PA13 adds 96 stages/tests and the required through total is 947 tests across 13 stages. Tests, references, grammar, harnesses, and scripts were not changed.
- This final audit follows the supervisor-directed compatibility correction. The required file audit and root through-PA13 report have both passed; staging and commit are the remaining repository-history actions.

## Final implementation shape under review

`argv -> lowir_text_adapter.cpp (ordered file loading, lexer/parser, canonical vocabulary + presentation IDs) -> one typed lowir_model::Program -> Validator assigns semantic IDs/owners and checks legality -> typed CY86 emitter`.

The text adapter owns textual canonicalization only: types become `LowType` payloads, fixed vocabularies become enums, source text is interned into one cold `Program::presentation` table, and file concatenation remains ordered. The Validator assigns `SymbolId`, `ValueId`, `SlotId`, and `BlockId`, creates direct owner records, and resolves each use once. Emission consumes those owners and the presentation table for boundary rendering; it does not rebuild whole-program name maps or reconstruct result types from instruction kinds.

## Shipped repairs

- Removed `LowType::spelling` and all per-occurrence owning name/literal/debug strings from the canonical model. `Operand`, destinations, parameters, functions, slots, blocks, globals, aliases, metadata, and debug locations carry compact `SpellingId` values into one Program-owned table.
- Added canonical `Program::ValueRecord` owner records and `Instruction::result_type`; layout/emission read `Validator::value_type`. Slot and block side indexes now hold direct owners only, not duplicate type/name facts. Symbol indexes retain direct top-level owners and cached presentation label IDs, not duplicate semantic type/name strings.
- Moved LowType methods/operators into model-owned `dev/src/lowir_model.cpp` and added it to the lowir2cy86 source set.
- Split the lexer, parser, type/operator factories, metadata decoding, and ordered file adapter into `dev/src/lowir_text_adapter.cpp`; added that translation unit to `FRONTEND_OBJ_BASENAMES_lowir2cy86`. The CY86 backend is now responsible for validation, layout, and emission rather than textual parsing.
- Replaced node-based `FunctionLayout` value/slot maps with dense vectors indexed by each function's exact `[value_begin, value_begin + value_count)` and `[slot_begin, slot_begin + slot_count)` ranges. Allocation is one shared frame-allocation body.
- Removed the per-symbol global declaration/definition metadata rescan; TLS metadata is resolved through direct symbol owners in one indexed symbol traversal.
- Restored the baseline PA13 CY86 serialization policy through typed owners: object-valued temporary `load`/`index` uses the historical value-load path; direct and indirect calls preserve source-order argument preparation, baseline hidden-result preparation, indirect-callee placement, and baseline stack materialization. The compatibility policy is general and contains no fixture-name or shape branch.
- Tightened typed checks for headers, values, storage, operators, conversions, atomics, calls, and returns. The ownership-correct scheduling/addressing experiment is not part of the shipped emitter.

## Focused validation status

- `make -C dev lowir2cy86`: passed with `-Wall -O3` and no warnings after the split.
- All eight focused checked-in cases (the three formerly failing cases plus five adjacent call/object cases) passed exact comparison; all compiler exit-status sidecars matched. `make test-pa13` also passed `96/96`.
- Generated validator probes rejected duplicate call parameters, illegal pointer metadata, function-symbol storage, copy type mismatch, wrong fpext direction, and atomic operand mismatch.
- Rejected-experiment CY86 evidence (not shipped): direct six-argument/two-stack-argument call exited 21; indirect six-argument call exited 21; indirect f80 return comparison exited 1; indirect object return value exited 37; direct object argument exited 10; direct f80 return comparison exited 1; call-boundary metadata exited 5.
- `git diff --check`: passed. `perl scripts/cppgm_file_audit.pl --stage pa13 --paths dev/src` passed with three pre-existing header-division warnings (`cpp_semantic_core.h`, `lowir_model.h`, and `pa11_semantic_model.h`); the fatal backend line-count finding is gone. Current source counts are backend 2,219 lines, text adapter 1,161, model implementation 57, and model header 567.
- Bounded static counters: emitter `std::map`, `.spelling`, `instruction.op`, `.text`, symbol maps, and function-declaration maps are all 0; FunctionLayout `std::map` and `definition_type` are 0; the role pass has 1 symbol-index loop and 0 nested global/global-declaration references; the canonical model has no `LowType::spelling` field and its only record-table `std::string` storage is `Program::presentation`.

## Performance evidence and limits

The post-split final candidate was copied to immutable executable `/tmp/pa13-lowir2cy86-postsplit.1hQjXP/lowir2cy86` (SHA-256 `e80802e84c142fdee3c106d8661bd31b7c9fd29a3636623b2bc173fc19e3083e`). Equivalent generated LowIR inputs and all outputs remain outside the repository in `/tmp/pa13-lowir2cy86-postsplit.1hQjXP`. Runs were interleaved A/B/A/B/A/B; each output hash was identical across its three repetitions.

| scale | globals | functions | blocks | instructions | value records | slot records | median wall/user/sys/RSS | output bytes | output SHA-256 |
|---|---:|---:|---:|---:|---:|---:|---|---:|---|
| A | 256 | 17 | 17 | 2,082 | 2,065 | 0 | 0.02/0.01/0.00 s, 6,520 KB | 172,976 | `6c033d3373e7b59f3d674fcf8f12873426c427079468c3b0fb1a088547ee1eff` |
| B | 512 | 33 | 33 | 4,162 | 4,129 | 0 | 0.03/0.02/0.01 s, 8,768 KB | 345,915 | `3593898500d9aa93b5045ea5bf19e6284f80704aae555d5f73457f6c8a35d96c` |

The role path has one symbol-index traversal and no nested global scan; layout has one instruction traversal and dense `[value_begin, value_begin + value_count)`/`[slot_begin, slot_begin + slot_count)` vectors. The timers are near hundredths of a second, so structural counts and repeated hashes are stronger than the wall-time ratio; this is not a fine-grained constant-factor claim.

## Rejected backend-scheduling experiment

The prior checkpoint tested an ownership-correct alternative: object temporary addresses used `isub64` frame addresses; f80 calls prepared the argument before the hidden result pointer; and stack arguments were materialized into disjoint slots before high-to-low register preparation. CY86 tracing confirmed why that experiment was semantically attractive: `move` overwrites its destination and `call` does not preserve argument registers. The old sequences can produce runtime exit 139, while the experiment produced exits 10, 1, and 5 on the three affected programs.

That experiment is explicitly rejected for this PA13 adapter. Checked successful CY86 text is the authoritative public serialization contract, and §7 requires representation-only changes to preserve it; the experiment changed serialized bytes and failed the hard exact-output gate. The final emitter therefore mechanically retains the pre-refactor general scheduling/addressing policy through typed IDs, with no dormant alternate scheduler and no fixture-specific branch. The runtime limitation of the checked references is recorded as a future contract-revision consideration, but it is not a blocker under the final PA13 serialization contract.

## Final ledger

- Baseline and clean starting state verified at the requested source commit.
- Current uncommitted paths: `dev/frontend_source_sets.mk`, `dev/src/lowir2cy86_backend.cpp`, `dev/src/lowir_model.cpp`, `dev/src/lowir_model.h`, `dev/src/lowir_text_adapter.cpp`, `pa13/plan.md`, and `pa13/audit.md`.
- Required file audit: passed with the three documented nonfatal header-division warnings.
- Required root gate: `make test-report-through-pa13` passed `947/947` tests across `13/13` stages.
- The final commit is intentionally not recorded in tracked documentation; repository history and clean-worktree proof are reported separately.
