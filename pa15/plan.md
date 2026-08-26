# PA15 typed statement-CFG and label checkpoint

## Spec alignment and stage design

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
  the same block. Deferred work stores the typed label/fact and a boundary,
  then follows shared parent/index cursors to the function root. Compound
  frames resume at source-order siblings; remembered typed loop/if/switch
  flows restore condition/body/iteration/end, branch/join, and
  dispatch/arm/end continuation. Case/default frames leave the switch body
  live for source-order fallthrough and break; ordinary labels skipped after a
  terminating case use the same typed ancestry recovery. Recovered labels
  install a persistent typed control-context head rather than a copied target
  vector: each loop/switch node links to its enclosing node and caches the
  nearest enclosing loop for `continue`. As continuation exits a loop or
  switch, that exact node is popped, so later siblings resolve `break` and
  `continue` against the enclosing context.
- Flow records are compact and type-sparse: fact-domain
  `LoopFlowIndex`/`IfFlowIndex`/`SwitchFlowIndex` arrays hold primitive arena
  indices, while `LoopFlow`, `IfFlow`, and heavyweight `SwitchContext`
  records are allocated only for their corresponding semantic facts.
  `LoopFlow` includes the former loop-target information. These indexes and
  arenas are initialized once per translation unit; immutable semantic fact
  identity gives deterministic lookup and preserves d5's no-per-function
  TU-wide clear property. Seven nonzero generation arrays guard label and
  recovery scratch state, with a full reset only at 32-bit epoch wrap.
- The two typed prepasses cost `O(F)` for one function with `F` semantic facts.
  Let `L` be labels, `Q <= L` queued labels, `K` canonical compound-cursor
  identities, `E` canonical structural-exit identities, and `M` ordered
  switch-arm map operations. A cursor is keyed by its typed first-child fact;
  a structural exit is keyed by its typed frame fact. Each identity is
  installed and advanced once, while later paths perform one typed lookup and
  jump to its existing `BlockId`. Thus `K + E + M = O(F)` and recovery is
  `O(F + L + K + E + (Q + M) log(F + L + 1) + G)`, or
  `O((F + L) log(F + L + 1) + G)` including emitted LowIR `G`. Storage is
  `O(F + L + K + E + R + arms + G)`, where `R` counts actual loop/if/switch
  records; there is no per-label ancestry or tail copy. Persistent control
  nodes are shared by their typed parent head, and recovered break/continue
  selection plus frame pop are `O(1)`; canonical cursor/exit work is charged
  once. Queue boundary and switch-map ordering are deterministic. There is no
  unconditional whole-body rescan/retry per label, textual lookup,
  reference/host shortcut, or retry loop.
- The architecture-review trace is source -> PA10/PA11 `NameId` and
  `LabelId` facts -> PA12 semantic facts -> PA15 typed `BlockId` LowIR -> PA13
  serialization/validation. LowIR is not reconstructed from text.

## Failure Map

Turn-start baseline at d5 was `105/109` passing with `109/109` covered and
exactly four failures. The final broad gates preserve that exact failure map:
`make test-pa15` reports accepted stage progress `105/109` and all `109/109`
covered, while its expected make status is nonzero because those four residuals
remain.

| test | baseline | final checkpoint result |
|---|---|---|
| `100-const-integral-lvalue-overload-category` | fail | unchanged residual; out of scope |
| `100-string-hex-escape-code-unit` | fail | unchanged residual; out of scope |
| `100-unnamed-parameter-storage` | fail | unchanged residual; out of scope |
| `200-nested-conditional-array-decay` | fail | unchanged residual; out of scope |

## Active Checkpoint

The PA11 storage/model, PA12 semantic registration/resolution, and PA15
statement lowering owners are complete for this bounded checkpoint. The
expanded 406 owner regression passed with `22/22` typed label/goto facts and
direct LowIR validation plus CY86 execution; it asserts one-target
forward/backward convergence, chained and nested-compound fallthrough through
intervening store/add siblings, branch-created deferred targets with both
returns, direct loop entry with condition/backedge, nested loop
break-context replacement,
switch-to-loop break/continue replacement, dead-sibling omission, and deferred
ordinary/switch-label continuation. One shared-tail case has exactly one
canonical `label_cont` block. The positive LowIR structures are
`chained_fallthrough` `8` blocks/`3` goto jumps, `deferred_branch` `6`/`2`,
`nested_fallthrough` `8`/`3`, `loop_entry` `5`/`2`,
`nested_control` `8`/`2`, `switch_loop_context` `12`/`2`,
`switch_ordinary_deferred` `12`/`3`, `switch_deferred` `17`/`5`, and
`shared_recovery_tail` `12`/`6`. The checked-in goto fixtures pass `2/2`,
and the ordinary-goto/switch/loop/nested-compound matrix passes `6/6`. This
bounded checkpoint is complete.

Completed gates:

- The supplied d5 baseline is `105/109`, `109/109` covered, with exactly
  `100-const-integral-lvalue-overload-category`,
  `100-string-hex-escape-code-unit`, `100-unnamed-parameter-storage`, and
  `200-nested-conditional-array-decay` failing; final `make test-pa15` is
  exit `2` solely for these four residuals.
- `n=15; ... make test-report-through-pa$((n - 1))` passes through PA14 at
  `1058/1058`, exit `0`.
- `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` passes with
  five existing header-division warnings; `dev/src/pa15_lowering.cpp` remains
  2,957 lines.
- `make test-report-through-pa15` is exit `2` solely for the same four PA15
  residuals and reports `1163/1167`.
- `git diff --check` exits `0` after the final documentation edit.

## Performance Evidence

Fresh immutable/interleaved candidate-only artifact:
`/tmp/pa15-label-perf-control-context-locked.n0tpXf`. The mode `0555` candidate
SHA-256 is
`d777451f7246a573c0bd03470ba10e7ce3674c33a7017d6b99cd9426852da1cc`; the
copied validator hash is
`c4d2af08ed8ca2c357790c174220f2fbbe753ce63524e20301f64456291cbf40`.
Five alternating forward/reverse rounds produced `130` timing rows over
`26` family/size/orientation positions; timing, semantic-generation, and
validation failure counts are all zero, with `26/26` LowIR validations exit
`0`. Every position has one stable output hash. The artifact, per-file input,
implementation, binary, and locked artifact manifests are hash-verified;
there is no comparative pre/post claim because this implementation changed.

| family | sizes | largest labels/gotos | largest semantic compounds | largest LowIR blocks/instructions | largest median wall/RSS |
|---|---|---:|---:|---:|---:|
| nested | depth `8..256`, forward/reverse | 40/40 | 2,057 | 177/362 | 0.01 s / 10,444 KiB |
| deferred | `d32-l4..d256-l32`, forward/reverse | 264/264 | 2,313 | 1,297/2,858 | 0.06 s / 15,516 KiB |
| many | `64..2048` functions, forward/reverse | 2,048/2,048 | 2,561 | 5,633/13,825 | 0.20 s / 53,540 KiB |

The nested/deferred sources are actual nested-compound and deferred-recovery
inputs, with structural counters for source size, semantic facts, compound
depth/records, labels, jumps, branches, instructions, and LowIR size. The
deferred family grows from `40` to `264` labels, `177` to `1,297` blocks, and
`394` to `2,858` instructions as its recovery size increases; the many family
grows from `64` to `2,048` same-spelling functions with `177` to `5,633`
blocks and identical forward/reverse structure. The many-function family
retains generation isolation. These measurements substantiate bounded
near-linear growth and output invariance at the listed sizes; the exact
canonical-identity bound above is the architecture claim.

## Historical Performance Evidence

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
a historical structural sanity sample: sizes 256/1,024/2,048/4,096 produced
the same number of goto blocks and edges as labels, with LowIR sizes
9,990/39,992/81,976/165,944 bytes. It predates the compact flow-arena
correction and is not used to characterize current storage or timing. The
current evidence is the immutable artifact above; all timings and RSS remain
machine- and startup-sensitive.

## Next Checkpoint

The bounded label checkpoint is complete. The next checkpoint may address the
four residual PA15 surfaces under their owning paths:
`100-const-integral-lvalue-overload-category`,
`100-string-hex-escape-code-unit`, `100-unnamed-parameter-storage`, and
`200-nested-conditional-array-decay`; this label checkpoint does not own any
of them.

## Checkpoint Ledger

| checkpoint | result | durable value |
|---|---|---|
| `959f9481` prior typed discard/return checkpoint | retained the six-test PA15 baseline | preserves earlier typed discard, return, and LowIR ownership work |
| `a2f33047` prior typed statement-CFG/label checkpoint | target `2/2`, owner regression PASS, adjacent `6/6`, PA15 `105/109` with `109/109` covered and four residuals, through-PA14 `1058/1058`, audit PASS | adds function-local typed label identity, one-target recovery, and deterministic compound/switch CFG lowering |
| current canonical label-flow checkpoint at `d5e10599` plus completed bounded structural-continuation/storage repair | owner regression exit `0` with `22/22` typed label/goto facts and direct nested/loop/switch target assertions plus CY86 execution, checked-in goto fixtures `2/2`, adjacent `6/6`, duplicate/unresolved `1/1`, full positive LowIR validation, and one canonical shared recovery tail; immutable candidate-only artifact has `130` timing rows, `26/26` semantic generations, `26/26` validation, stable hashes at all `26` positions, and verified per-file/artifact manifests across many/nested/deferred families; final PA15 `105/109` with all `109/109` covered and exactly four residuals, through-PA14 `1058/1058`, combined through-PA15 `1163/1167`, and file audit pass with five existing warnings | replaces heavyweight per-fact flow records and the former loop-target side table with typed sparse arenas/indexes; replaces per-label ancestry copies with typed shared parent/index cursors and structural-exit identities plus a persistent typed control context for root-to-label ancestry, shared parent links, cached nearest-loop lookup, and exact pop-on-unwind; preserves complete nested compound/loop/switch continuation, dead-sibling omission, final queue drainage, deterministic one-target recovery, and same-name function isolation; exact canonical continuation bound is `O((F + L) log(F + L + 1) + G)` with `O(1)` recovered break/continue selection and no duplicated ordinary tails |
