# PA15 Audit

## Current Checkpoint Review

This bounded review covers the amended typed address/value checkpoint, whose
parent is `c96b9527d4d589b1d57fd36fa38482d7449efca3`. It audits the complete affected
ownership path: PA11 declarations, types, bindings, linkage, storage, and
scope; PA12 typed conversions, categories, constant values, and relocations;
PA15 indexes, global declarations and definitions, local/global addresses,
references as referent addresses, arrays, decay, subscripts, projections,
pointer scaling, address/value conversion, assignment, comma, conditional,
single-evaluation mutation, typed LowIR, and source-set wiring. The residual
41-feature failure surface is not re-audited here.

The traced path is continuous and typed:

- PA11 owns canonical `BindingId`, `TypeId`, `ScopeId`, declaration,
  storage-duration, linkage, function, and scope facts. Value lookup remains
  preferred over type lookup for `sizeof` shadowing, and namespace static
  linkage/storage remains on the binding owner.
- PA12 owns `SemanticFactId` expression identity, selected bindings and calls,
  value categories, contiguous conversion ranges, typed `sizeof` values, and
  the namespace initializer boundary. `record_constant_expression_value`
  evaluates each typed integral root once. `ConstantAddressFact` is a sparse
  typed arena record keyed from the owning semantic fact and contains explicit
  `evaluated`/`valid` state, the target `BindingId`, byte addend, relocation
  kind, element/index `TypeId`s, index `SemanticFactId`, and typed index value.
  `resolve_constant_address` carries typed `Value`, `ObjectAddress`, and
  `ArrayDecay` contexts: a variable IdExpression is relocatable only for its
  recorded array decay or an explicit address-of operand; function identity
  remains a function relocation.
- PA15 now only consumes that result. It maps the target `BindingId` to the
  already-indexed LowIR `SymbolId`, emits direct symbol-plus-addend forms, or
  consumes the recorded `ArrayElement` projection to build the checked-in
  runtime initializer. The former recursive `constant_address` and second
  `find_address_subscript` traversal are removed. PA15 does not inspect
  expression syntax or retry constant evaluation.
- PA15 builds binding/declaration, function-scope, global-symbol, and
  local-slot indexes once. `LoweredValue` preserves semantic and physical
  types through address/value conversion, references, array decay/subscript,
  pointer scaling, assignment/comma/conditional categories, and
  compound/prefix/postfix LHS evaluation.
- The shared typed LowIR `Program` remains the backend owner. `IK_INDEX`
  carries typed element and projection facts; `lowir_model.cpp` serializes
  the model at the output boundary. `frontend_source_sets.mk` wires the split
  PA12 and PA15 sources. ABI and generated names are presentation sidecars,
  not semantic lookup keys.

The audit found two related correctness defects in the landed increment: PA15
was reconstructing address semantics recursively and then traversing again to
rediscover a subscript projection, and the new PA12 resolver initially treated
every variable IdExpression as its storage address. The bounded repair moves
the decision to the PA12 owner, records it once, carries the typed address
context through the expression facts, preserves both direct relocations and
the array-element runtime form, and leaves PA15 with only typed identity
mapping. No unrelated PA15 feature was changed. The resulting address walk
and typed indexing are linear in the affected facts, with the existing
deterministic ordered indexes retaining their `O(n log n)` bound.

## Focused and Final Evidence

Fresh focused validation:

- `make -C dev cppgm++` exited `0`.
- The implicated 20-test PA15 matrix passed `20/20`, including direct object
  addresses, one-past array pointers, array-element projections, pointer
  scaling, references, categories, and single-evaluation mutation.
- Temporary typed probes in
  `/tmp/pa15-typed-relocation-correction.6EMMbb/probes` showed `&object`
  emits `addr @value`, array decay and one-past emit `addr @data` and
  `addr @data + 16`, `&array[1]` retains one
  `index i32 [projection=array_element]` in `function @__cppgm_init`, and
  direct and explicit function pointers both emit `addr @function`.
- The bare-pointer probe `int *copy = pointer` exited nonzero with
  `PA15 nonconstant global initializer` and emitted no `addr @pointer`.
- `200-global-array-element-address-initializer` emitted one
  `index i32 [projection=array_element]` and one `function @__cppgm_init`.
- `200-global-array-one-past-end-pointer` emitted `addr @data + 800`.
- `200-global-object-address-initializer` emitted `addr @value`.
- The compound-assignment probe contained exactly one `call ptr @lhs()` and
  one `addr @value`.

Final gates:

- The exact `n=15` through-PA14 gate passed `1058/1058`.
- `make test-pa15` exited `2` with `68/109` passing, all `109` covered, and
  exactly the same 41 named failures recorded in `pa15/plan.md`. The fresh
  complete log is
  `/tmp/pa15-typed-relocation-correction.6EMMbb/full-pa15-context-final.log`.
- The exact through-PA14 output is retained at
  `/tmp/pa15-typed-relocation-correction.6EMMbb/through-pa14-context-final.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` exited `0`
  with five nonfatal header-division warnings for
  `abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
  `pa11_semantic_model.h`, and `pa15_lowering.h`; the complete output is in
  `/tmp/pa15-typed-relocation-correction.6EMMbb/file-audit-context-final.log`.
- `git diff --check` passed.

Fresh performance evidence uses the immutable corrected executable
`/tmp/pa15-typed-relocation-correction.6EMMbb/context-perf/cppgm++-corrected`,
mode `0555`, size `1,924,688` bytes, SHA-256
`69b7221e16dffec0e266c91b651cfcab6851fcd8678906941d5845882f4cfe77`.
The raw interleaved samples are in
`/tmp/pa15-typed-relocation-correction.6EMMbb/context-perf/timings.tsv`;
structural counts are in `structure.tsv`, and medians are in `medians.tsv` in
the same directory. There are seven samples per family and size, each sample
performs 20 repeated compilations.
Odd rounds use `32, 64, 128, 256`; even rounds use
`256, 128, 64, 32`; each size runs long-expression then many-global.

The exact structural records are:

| family | n | input bytes/lines | LowIR lines | semantic lines | LowIR globals | binary facts |
|---|---:|---:|---:|---:|---:|---:|
| long-expression | 32 | 131/3 | 9 | 75 | 2 | 32 |
| long-expression | 64 | 195/3 | 9 | 139 | 2 | 64 |
| long-expression | 128 | 324/3 | 9 | 267 | 2 | 128 |
| long-expression | 256 | 580/3 | 9 | 523 | 2 | 256 |
| many-global | 32 | 1403/65 | 69 | 168 | 64 | 0 |
| many-global | 64 | 2811/129 | 133 | 328 | 128 | 0 |
| many-global | 128 | 5711/257 | 261 | 648 | 256 | 0 |
| many-global | 256 | 11727/513 | 517 | 1288 | 512 | 0 |

Per-compilation medians are wall/user/system seconds and peak RSS KiB:

| family | n | wall | user | system | RSS KiB |
|---|---:|---:|---:|---:|---:|
| long-expression | 32 | 0.003500 | 0.001500 | 0.002000 | 5152 |
| long-expression | 64 | 0.003500 | 0.001500 | 0.002000 | 5128 |
| long-expression | 128 | 0.004000 | 0.001500 | 0.002000 | 5388 |
| long-expression | 256 | 0.004500 | 0.002000 | 0.002500 | 5676 |
| many-global | 32 | 0.004500 | 0.002000 | 0.002500 | 5672 |
| many-global | 64 | 0.006000 | 0.003500 | 0.002500 | 6164 |
| many-global | 128 | 0.010000 | 0.005500 | 0.004000 | 6892 |
| many-global | 256 | 0.016500 | 0.010500 | 0.006000 | 8604 |

The long-expression family has `n` typed binary facts and a constant-size
serialized relocation; the many-global family has `2n` global definitions and
`2n` source global objects/pointers. These structural counts corroborate the
bounded ownership work and deterministic output shape. Historical evidence
preserved from the prior checkpoint is the `21/109` to `68/109` progress; the
current focused, full-stage, through-PA14, and file-audit results above are
fresh.

## Audit Ledger

| Checkpoint | Evidence and disposition |
|---|---|
| PA15 full-stage / checkpointAudit — typed address/value ownership | Amended the checkpoint so PA12 records one typed `ConstantAddressFact` per namespace initializer, carries `Value`/`ObjectAddress`/`ArrayDecay` context, rejects bare scalar/pointer IdExpression relocations, and preserves direct, one-past, function, and array-element forms; PA15 consumes only typed identity, addend, and projection fields. Focused validation passed `20/20` plus the four probes; through-PA14 passed `1058/1058`; PA15 retained `68/109`, the same 41 named failures, and all `109` covered; fresh immutable `n=256` performance evidence is in `context-perf`; file audit and diff check passed. |
