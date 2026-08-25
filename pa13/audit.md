# PA13 final architecture audit

## Review boundary and status

This final audit reviews the inherited repair from HEAD `3521adf33136ddf5ca477170eda4caa54c299632` after supervisor-directed compatibility correction. The PA13 increment is 96 stages/tests and the required through total is 947 across 13 stages. No tests, `.ref` files, grammar, harnesses, or scripts were changed.

Changed paths are exactly:

- `dev/frontend_source_sets.mk`
- `dev/src/lowir2cy86_backend.cpp`
- `dev/src/lowir_model.cpp`
- `dev/src/lowir_model.h`
- `dev/src/lowir_text_adapter.cpp`
- `pa13/plan.md`
- `pa13/audit.md`

## Spec and ownership review

`lowir_text_adapter.cpp` owns ordered file loading, lexing, textual type/operator/metadata canonicalization, and interning of source, literal, external, and debug spellings into `Program::presentation`. It does not assign semantic IDs. The Validator assigns `SymbolId`, `ValueId`, `SlotId`, and `BlockId`, creates owner records, resolves uses once, validates declared types and call headers, and selects runtime identities. The emitter receives typed owners and uses the presentation table only for required output spellings. `FRONTEND_OBJ_BASENAMES_lowir2cy86` wires the adapter, model, and backend as separate translation units.

`LowType` contains only kind and typed payload fields. `Operand`, instruction destinations, parameters, functions, slots, blocks, globals, aliases, metadata, and debug locations contain IDs rather than owning strings. `Program::ValueRecord` points to the canonical parameter/instruction result owner; `Instruction::result_type` is assigned once by validation. Function-local value and slot ranges are dense and layout lookup is vector indexing, not `std::map`. Slot/block indexes point to their model owners; symbol indexes point directly to global/function owners and metadata and cache only output-label IDs.

LowType methods/operators live in model-owned `dev/src/lowir_model.cpp`, which is explicitly included in the lowir2cy86 source set. Another adapter can compile/link the model boundary without taking the CY86 backend implementation.

## Representative fact traces

- Scalar/object type: parser `make_type` -> `LowType::Kind` plus integer/float/object payload -> Validator declared-type checks -> `FunctionLayout::allocate_frame` uses canonical storage size/alignment -> CY86 width, frame, and data emission.
- Local/global/function/block identity: parser `SpellingId` -> Validator resolves once to `ValueId`/`SlotId`/`SymbolId`/`BlockId` -> use/target validation -> dense frame location, symbol label, or block label emission. No emitter name re-resolution occurs.
- Unary/binary/cmp/convert: parser enum -> Validator legality and operand-owner checks -> emitter enum switch/opcode rendering. Comparison result ownership is canonical `i64`; conversion direction is checked, including `fpext`/`fptrunc`.
- Direct/indirect call: typed direct callee or explicit typed indirect signature -> `validate_function_header`, arity, argument/result checks -> non-copying call view -> baseline hidden object/f80 preparation, source-order argument lowering, baseline stack materialization, and direct label or indirect `[sp]` call.
- Role/init/fini/TLS/object metadata: parser enum/IDs -> direct symbol metadata owner -> one role/TLS validation traversal and `tls_for_id` resolution -> typed startup selection and CY86 startup/global behavior. Object/linkage/binding/debug fields with no PA13 CY86 representation remain boundary metadata and are not used as semantic lookup paths.
- EH target/value and atomic order: parser target/order enums -> Validator block/symbol/value and order checks -> deterministic EH labels/values and ordinary-memory/fence lowering. Function symbols are rejected as memory storage operands.

## Findings and repairs

1. Per-occurrence strings and `LowType::spelling` were semantic-owner-shaped hot data. Replaced them with compact `SpellingId` references into one cold Program table.
2. Value types were separately reconstructed in validation/layout. Replaced the type side table and instruction-kind `definition_type` switch with owner-backed `Program::ValueRecord` and `Instruction::result_type`; emission and layout read `Validator::value_type`.
3. Dense generated IDs used node-based `std::map` locations. Replaced them with exact function-local ranges and vectors; the shared frame allocation body is used by both value and slot locations.
4. Symbols/slots/blocks no longer duplicate owner strings/types in side indexes. Index entries retain direct owners and IDs only.
5. `validate_roles_and_tls` no longer rescans all global declarations/definitions per symbol; it traverses the symbol index once and follows direct metadata owners.
6. Calls retain the pre-refactor PA13 serialization policy after typed-owner conversion: source-order arguments, baseline hidden-result setup, indirect-callee placement, and baseline stack writes. The alternate ownership-correct scheduler/address policy is recorded as rejected below and is not dormant in the emitter.
7. Structural/type validation now covers explicit call-header legality, declared operand types, storage-kind restrictions, atomic operands, conversion direction, return types, and object-copy shape.
8. The former parser body was removed from the 3,362-line backend and placed in the shared text adapter. The resulting backend is 2,219 lines and the adapter is 1,161 lines; this is a responsibility split, not a file-audit evasion.

## Focused evidence

- Affected build: `make -C dev lowir2cy86` passed with `-Wall -O3`; no compiler warnings were emitted.
- All eight focused exact cases passed: the three formerly failing tests (`100-small-direct-object-argument`, `200-call-boundary-metadata`, `200-f80-direct-call`) plus five adjacent call/object tests. `make test-pa13` passed `96/96`; compiler exit-status sidecars matched.
- Required final gates passed freshly from the repository root: `perl scripts/cppgm_file_audit.pl --stage pa13 --paths dev/src` passed with the three documented warnings, and `make test-report-through-pa13` passed `947/947` across `13/13` stages.
- Rejection probes passed: duplicate call parameters; pointer metadata on an integer parameter; store to a function symbol; copy type mismatch; wrong `fpext` direction; atomic add value mismatch.
- Rejected-experiment CY86 probes through `dev/cy86`: direct six-argument call with two stack arguments returned 21; indirect six-argument call returned 21; indirect f80 return comparison returned 1; indirect object return returned 37; direct object argument returned 10; direct f80 return comparison returned 1; explicit indirect-result boundary returned 5.
- `git diff --check` passed. `perl scripts/cppgm_file_audit.pl --stage pa13 --paths dev/src` passed with three nonfatal header-division warnings: `cpp_semantic_core.h`, `lowir_model.h`, and `pa11_semantic_model.h`. The fatal >3,000-line backend finding is removed. Current source counts are backend 2,219 lines, text adapter 1,161, model implementation 57, and model header 567. The model header remains a defensible warning because it contains canonical typed declarations/inline value semantics while nontrivial LowType methods/operators are in `lowir_model.cpp`.
- Static counters: emitter `std::map`, `.spelling`, `instruction.op`, `.text`, symbol-map, and function-declaration-map references are each 0; FunctionLayout `std::map` and `definition_type` are 0; `validate_roles_and_tls` has one symbol loop and zero `program_.globals`/`global_declarations` references; the model has no `LowType::spelling` field and only the centralized `Program::presentation` string table.

## Rejected backend-scheduling experiment and scope

The prior checkpoint tested an ownership-correct alternative: object temporary addresses used `isub64` frame addresses; f80 calls prepared the argument before the hidden result pointer; and stack arguments were materialized into disjoint slots before high-to-low register preparation. Under PA9 CY86, `move` overwrites its destination and `call` does not save argument registers, so the old sequences can execute with exit 139 while the experiment produced exits 10, 1, and 5.

That alternative is rejected for this PA13 adapter because checked successful CY86 text is the authoritative public serialization contract and the exact-output gate is hard. The final emitter mechanically retains the pre-refactor general policy through typed IDs: no fixture-name branch, no reference edit, and no dormant alternate scheduler. The checked-reference runtime limitation is durable evidence for a later contract revision, not a remaining PA13 release blocker once serialization passes.

The rejected experiment's broader execution probes returned 21 for direct six-argument/two-stack and indirect six-argument calls, 1 for indirect f80 comparison and direct f80 comparison, 37 for indirect object return, 10 for direct object argument, and 5 for the metadata boundary. These results remain experiment evidence only; final focused conformance is the eight exact cases plus `96/96` for `make test-pa13`.

The PA13 adapter remains intentionally out of scope for C++ lowering, optimization, native/object emission, linking, debugger emission, host ABI policy, and object-file alias/TLS runtime materialization. Those metadata facts are parsed, owned, and validated where required but have no CY86 output representation here.

The post-split final candidate was copied to immutable executable `/tmp/pa13-lowir2cy86-postsplit.1hQjXP/lowir2cy86` (SHA-256 `e80802e84c142fdee3c106d8661bd31b7c9fd29a3636623b2bc173fc19e3083e`). Equivalent generated inputs and outputs remain outside the repository in `/tmp/pa13-lowir2cy86-postsplit.1hQjXP`, with interleaved A/B/A/B/A/B runs. Scale A was 256 globals, 17 functions, 17 blocks, 2,082 instructions, 2,065 value records, and 0 slots; its median wall/user/sys/max-RSS was 0.02/0.01/0.00 s/6,520 KB, output 172,976 bytes with hash `6c033d3373e7b59f3d674fcf8f12873426c427079468c3b0fb1a088547ee1eff`. Scale B was 512 globals, 33 functions, 33 blocks, 4,162 instructions, 4,129 value records, and 0 slots; its median was 0.03/0.02/0.01 s/8,768 KB, output 345,915 bytes with hash `3593898500d9aa93b5045ea5bf19e6284f80704aae555d5f73457f6c8a35d96c`. Each scale repeated the same hash three times. Timers are near hundredths of a second, so structural counts and repeated hashes outweigh the timing ratio; this is not a fine-grained constant-factor claim.

## Final ledger

- Baseline HEAD and inherited dirty paths verified; no unrelated paths were added.
- First coherent repair diff: reviewed and extended without staging or history mutation.
- Final candidate: baseline-compatible emitter restoration, source split, build, all PA13 tests, refreshed post-split measurement, required gates, file audit, static ownership review, and diff check complete.
- No blocker remains under the authoritative PA13 serialization contract; the three file-audit header warnings are nonfatal and documented.
- The final commit is intentionally not recorded in tracked documentation; repository history and clean-worktree proof are reported separately.
