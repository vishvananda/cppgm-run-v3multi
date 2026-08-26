# PA15 Audit

## Current Checkpoint Review

This checkpoint review covers landed commit
`b7eaf9d868b17cbbf542f3415e7a5e46f07007ba` (`PA15: preserve typed
conditional array glvalues`), parent
`f038141d14cc5c9d10e01964d3a1bdf3a6c5f4ca`, and only the conditional-array
ownership increment. The allowed production files are
`dev/src/pa12_semantic.cpp`, `dev/src/pa15_lowering.cpp`, and
`dev/src/pa15_lowering_flow.cpp`. The affected checked-in case is
`pa15/tests/general/200-nested-conditional-array-decay.t`. Labels/CFG,
classes, templates, and the three unrelated residual surfaces were not
re-audited or repaired.

The supplied full-stage baseline at turn start is `106/109` passing, with
all `109/109` tests covered and exactly these residual failures, as recorded
in `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:

- `100-const-integral-lvalue-overload-category`
- `100-string-hex-escape-code-unit`
- `100-unnamed-parameter-storage`

The affected nested conditional test passes in the focused checks and is not
in the final failure set. The authorized broad gates preserve the baseline:
`make test-pa15` exits `2` at `106/109` with `109/109` covered, the exact
three-name failure proof is in
`/tmp/pa15-audit-gates-20260826/actual-failures.txt`, and the through-PA14
report exits `0` at `1058/1058`. The file audit exits `0` with five existing
`bad-division` warnings; the combined through-PA15 report exits `2` at
`1164/1167` with the same three names.

### Ownership trace

1. PA10/PA11 own the canonical type and expression facts. An array `TypeId`
   retains its element type and bound (including differing bounds), while
   `ExprInfo` retains the typed value category. PA12's
   `expression_object_type` removes a reference wrapper without erasing the
   object shape; `strip_top_cv_type` removes only the permitted top
   qualification and does not turn an array into a pointer.

2. PA12's `semantic_conditional_expression` (at
   `dev/src/pa12_semantic.cpp:1636`) owns the conditional result. For arrays,
   the same-object-type path now preserves an array only when both operands
   have the same non-prvalue category, yielding an lvalue for lvalue/lvalue
   and an xvalue for xvalue/xvalue. Different bounds or different categories
   use the existing pointer common-type path. `conversion_for` and
   `record_builtin_conversion` append typed `ArrayToPointer` facts to the
   relevant local conversion range; reference contexts append typed
   `ReferenceBinding` facts. No source spelling participates in this choice.

3. PA15 consumes those facts. `conditional_address_result` (at
   `dev/src/pa15_lowering.cpp:2595`) checks the semantic category, canonical
   object `TypeId`, and local typed conversion range. It selects address
   lowering for non-prvalue array results and reference bindings, including
   array lvalues and xvalues. `lower_conditional_address` creates one typed
   pointer slot and the condition/arm/join blocks, then lowers each selected
   arm through `lower_address`. A nested conditional therefore follows the
   same typed path recursively.

4. The PA15 conditional case (at
   `dev/src/pa15_lowering_flow.cpp:308`) exposes the selected pointer directly
   when the conditional is prvalue or owns an `ArrayToPointer` conversion. A
   non-prvalue conditional without that conversion remains an lvalue-shaped
   result for the later conversion pass. `apply_conversions` then consumes
   the fact-local range once. Thus a contextual array-to-pointer conversion
   on a conditional does not add a second decay, while an ordinary array
   expression still reaches the normal typed decay path.

5. PA13's LowIR model serializes the typed unary operation as
   `unary decay ptr ...` (`dev/src/lowir_model.cpp:366`). The validator checks
   that the operand/result type is the same and that `decay` has pointer type
   (`dev/src/lowir2cy86_backend.cpp:684-688`). The complete production entry
   remains the single source -> PA10/PA11 -> PA12 -> PA15 typed LowIR path;
   no text rendering, reference binary, host compiler, retry, or alternate
   production pipeline is involved.

### Correctness findings

The landed increment is semantically and architecturally sound under this
bounded audit. No source repair or new durable test was necessary. The
representative matrix exercised the following ownership cases:

| case | expected typed fact/lowering | focused result |
|---|---|---|
| same-bound lvalue arrays | array lvalue fact; conditional address blocks | affected test, subscript, address, and reference cases pass |
| same-bound xvalue arrays | array xvalue fact; selected address remains usable as an array reference | PA12 xvalue fixture and custom conditional/reference probes pass |
| differing bounds | pointer common type with per-arm typed array-to-pointer conversions | affected outer `char[3]`/`char[4]` nesting passes |
| differing value categories | pointer common type; each required arm conversion is applied once | custom lvalue/xvalue mixed probe passes |
| scalar and reference conditionals | existing scalar value/reference materialization ownership remains intact | focused 15-case related matrix and scalar-reference probe pass |
| nested conditionals | inner conditional publishes its own typed address/value fact to the outer consumer | `200-nested-conditional-array-decay` passes |
| contextual array-to-pointer conversion | conditional address is exposed directly; no duplicate materialization/decay | custom pointer call and return paths pass |

The affected `.my` LowIR has one decay in the short-array arm and one decay
for the later array comparison in `main`; the selected same-bound inner
array arms have no extra decay. The custom xvalue conditional has no decay in
the reference-returning selector and one decay at the later subscript use.
The mixed-category pointer result emits one decay in each arm, as required.
The scalar prvalue-to-`const int&` probe creates one conditional value slot
and one reference argument temporary, with no array path involved.

The implementation work is bounded by each fact's child and conversion
ranges plus emitted LowIR. The new decisions are constant work per
conditional and a local conversion scan; there is no body rescan, retry-until
stable loop, source reparsing, text lookup, unbounded ancestry copy, or
timeout/shortcut path. Storage is typed semantic facts, compact conversion
ranges, and the conditional's local LowIR slots/blocks. Arm order and block
names are deterministic.

### Focused evidence

- `make -C pa15 check TEST='tests/general/200-nested-conditional-array-decay.t tests/general/200-conditional-array-decay-subscript.t tests/general/200-lvalue-conditional-address.t tests/general/200-lvalue-conditional-reference-return.t tests/general/200-reinterpret-reference-conditional-materialization.t tests/general/200-global-array-conditional-cast-initializer.t'` -> `PASS (6/6)`.
- The broader related PA15 matrix of 15 checked-in tests, including comma
  lvalue/xvalue, array decay, reference casts, global array initialization,
  scalar reference return, and reference-parameter collision cases ->
  `PASS (15/15)`.
- `make -C pa12 check TEST=tests/general/300-array-xvalue-subscript.t` ->
  `PASS (1/1)`.
- Five `/dev/stdin` array probes covered lvalue contextual pointer use, xvalue
  nested selection, mixed lvalue/xvalue categories, and lvalue/xvalue array
  reference parameters. All compiler exits were `0`; all five outputs passed
  `./dev/lowir2cy86`. The scalar prvalue/reference probe also compiled and
  validated with exit `0`.
- Semantic dumps for the checked-in nested case and the PA12 xvalue case,
  plus the custom xvalue conditional, exited `0` and retained the expected
  array lvalue/xvalue categories. The affected LowIR and every focused
  custom LowIR were accepted by the typed validator.
- `git diff --check` exits `0`; the final post-documentation capture is
  `/tmp/pa15-audit-gates-20260826/diff-check-final.log`. No
  handout test, `.ref` fixture, or generated repository artifact was edited;
  no additional regression test was needed.

### Bounded performance evidence

Fresh ephemeral inputs with 64 and 1,024 repeated nested conditional-array
selectors were compiled three times per size. The exact generated inputs are
`input-64.cpp` and `input-1024.cpp`; the compact generator is
`generate_inputs.sh`, and the expanded recipe trace is `command.log`, all in
`/tmp/pa15-audit-scale-final-20260826-samebound`. Each selector uses three
same-bound `int[4]` lvalue array arms with a nested conditional, exercising
the preserved array-glvalue address path; differing-bound behavior is covered
by the checked-in affected fixture and focused probes. Every LowIR output
passed `lowir2cy86`; the input/output checksum manifest is
`input-output-manifest.sha256` and the complete artifact manifest is
`artifact-manifest.sha256`.

| repeated selectors | input lines / bytes | LowIR lines / bytes | condaddr blocks | address ops | decay ops | wall samples | RSS samples (KiB) |
|---:|---:|---:|---:|---:|---:|---:|---|
| 64 | 258 / 6,168 | 3,080 / 66,208 | 384 | 192 | 0 | `0.01, 0.01, 0.01` s | `7,856, 7,940, 8,108` |
| 1,024 | 4,098 / 106,300 | 49,160 / 1,069,864 | 6,144 | 3,072 | 0 | `0.17, 0.17, 0.17` s | `50,316, 50,072, 50,276` |

The structural counters scale by exactly 16x from 64 to 1,024 selectors, and
all six compilation commands and six validator commands completed under the
60-second timeout. Stable per-size LowIR hashes are recorded three times in
`output-hashes.tsv`: the 64-selector hash is
`0a9eaef30cc63aa80bd919108bc9c37596f630239988cb7282a2038ac8b7ef64`, and the
1,024-selector hash is
`ad4bea5b3a7ce62bb140a545428bd22ba2f2986dd5014fa12a123349ac3f85ac`.
The candidate hash is in `candidate.sha256`
(`506aa46be6476f23b9eb7567253d7d60c0ba348d167e62a365d8f9b86af10796`), and
the validator hash is in `validator.sha256`
(`c4d2af08ed8ca2c357790c174220f2fbbe753ce63524e20301f64456291cbf40`).
These measurements support bounded output and timing invariance at the
listed sizes, not an asymptotic or machine-independent performance claim.

### Broad-gate disposition and bounded uncertainties

The authorized broad gate logs are in `/tmp/pa15-audit-gates-20260826`:
`test-pa15.log` exits `2` at `106/109`, `through-pa14.log` exits `0` at
`1058/1058`, `file-audit.log` exits `0`, and `through-pa15.log` exits `2` at
`1164/1167`. The mechanically compared failure files are
`expected-failures.txt` and `actual-failures.txt`; they are identical and
contain only the three named residuals. `status.tsv` records all command
exits, including `diff-check` exit `0`. The five file-audit warnings are the
existing `bad-division` findings for `dev/src/abi_mangle.h`,
`dev/src/cpp_semantic_core.h`, `dev/src/lowir_model.h`,
`dev/src/pa11_semantic_model.h`, and `dev/src/pa15_lowering.h`; there are no
file-audit errors.

The final full-stage uncertainty is therefore limited to the three explicitly
unrepaired owner surfaces and the bounded nature of the performance sample.
The direct xvalue conditional checks are ephemeral probes rather than a new
checked-in fixture; the existing PA12 xvalue fixture and PA15 reference/LowIR
matrix provide durable adjacent coverage. No handout, test, or `.ref` file
was changed.


## Historical Checkpoint Review — typed label ownership and sparse flow

This bounded review covers `d5e10599edfa983c64746b25373fe3d3ec129b39`
(`PA15: avoid per-function label state clears`), its ownership parent
`a2f33047782dbdc248a382c71cea926a2f3371eb` (`PA15: add typed label CFG
lowering`), and the completed storage/recovery correction at this
checkpoint. The preceding nearest-compound repair was incomplete: it could
omit intervening fallthrough siblings, stop at an inner compound, leave a
live-join target undrained, lose loop/switch continuation, or retain an exited
inner control target. The final correction replaces that cursor with shared
typed structural metadata and a persistent complete control context, and
audits the complete label path. The four unrelated PA15 residual surfaces are
not re-audited or repaired.

The ownership trace is one typed production pipeline. PA10 retains source
spelling on label/goto AST facts. PA12 converts names to `NameId`, registers
one `LabelFact` and dense `LabelId` per function in source order behind a
typed `LabelTableId`, rejects duplicates, resolves each goto, and publishes
the resolved identity in semantic facts. Unresolved names fail before
lowering. PA15 consumes those facts and emits typed `BlockId` control flow;
PA13 owns LowIR serialization and validation. Rendered semantic text is only
an output boundary, never a lookup or reconstruction input.

`collect_label_flow` records one parent `SemanticFactId`, exact child index,
and immutable child identity for every fact in the current function. The
referenced-subtree prepass then marks only paths containing a referenced
label, rejecting stale ranges, cycles, and identity mismatches. A deferred
work item stores only the typed label, label fact, and compound queue
boundary; recovery follows the shared parent/index tables to the body root.
Each compound cursor is a typed identity keyed by the fact at its first
child, and each loop/if/case/default/switch exit is keyed by its typed frame
fact. A `ContinuationIndex` points to one `CompoundContinuation` record with
an entry `BlockId` and an install/complete state. `jump_to_cached_label_continuation`
checks the nearest complete identity before walking ancestry; the enter
operations install each missing identity once, attach a live edge to its
canonical entry, and stop a later path at the existing entry. Consequently
overlapping source tails converge on one typed continuation block rather than
being copied per label. At each compound it resumes after the entered child in
source order, drains new work after every sibling, propagates through every
enclosing compound, and performs a final root/remaining-key drain before
function completion. Nested compounds, intervening fallthrough, live joins,
and final branch-created targets are therefore handled with no unconditional
whole-body rescan/retry per label. Dead sibling subtrees remain omitted.

Recovery restores control context through typed ancestors precomputed during
the same fact walk. Persistent `LoopFlow`, `IfFlow`, and `SwitchContext`
records provide condition/body/iteration/end, branch/join, and
dispatch/arm/end identities after the active stacks have been popped. Each
fact also points to a shared `RecoveryControlNode` head whose typed frame is
either a loop or switch, whose parent is the enclosing control, and whose
cached loop link resolves `continue` in constant time. A deferred label
installs that head without copying an ancestry vector. As the canonical
continuation exits a loop or switch it pops the exact head node before
scanning the enclosing sibling sequence; a later `break`/`continue` therefore
cannot reuse the exited inner target. A direct loop-body entry reaches its
condition/backedge and keeps break/continue targets valid. Case/default
recovery validates the owning switch map, leaves the current block live for
source-ordered fallthrough, and can re-enter an ordinary label skipped after
a terminating case. Every label identity owns one deterministic target block;
forward, backward, nested, and branch-created entries converge on that typed
target.

Flow storage is now typed and sparse. The fact-domain `LoopFlowIndex`,
`IfFlowIndex`, and `SwitchFlowIndex` arrays contain only compact primitive
indices; the corresponding arenas contain records only for loop, if, and
switch facts. `LoopFlow` subsumes the former separate loop-target record, and
the heavyweight `SwitchContext` (arm vector, label map, and sets) is no
longer constructed once per semantic fact: only actual switch facts occupy
the switch arena, while the active stack is bounded by nesting. Compact
fact-domain recovery-control heads and the persistent control arena share
enclosing suffixes and hold no per-label vectors. The indexes and arenas are
initialized once for the translation unit, records are keyed by immutable
semantic fact identity, and no per-function TU-wide flow clear occurs.

Generation isolation is guarded at every scratch read. The seven generation
arrays cover label targets, referenced bits, fact indexes, subtree marks,
lowered labels, waiting recovery, and queued recovery. Each function advances
to a nonzero epoch and clears only its queue/boundary containers. At
`UINT32_MAX`, all seven stamp arrays are filled with zero before epoch `1` is
installed, and the fact-keyed continuation indexes plus their arena are reset
as well, so zero is never active and stale blocks, marks, states, wakeups, or
continuation entries cannot cross a function or wrap. Immutable parent/index
metadata, fact-keyed flow arenas, and typed control nodes are shared safely
because semantic fact identities are translation-unit unique; guarded scratch
reads reject stale function state. Control heads are rebuilt for the active
generation and the control arena is cleared with continuation state on epoch
wrap.

Focused correctness evidence for this completed repair is recorded in
`/tmp/pa15-final-focus.kZsMvM` (`owner.log`, `goto-fixtures.log`,
`adjacent-matrix.log`, and the four `direct-*.log` files). Broad and final
gate logs are in `/tmp/pa15-final-gates.KU0FpN`:

- `make test-pa15` exited `2` because the expected four residual tests fail;
  stage progress is `105/109` with all `109/109` covered and no added or
  replacement failure.
- `make -C dev -B cppgm++ lowir2cy86` exited `0`.
- `cppgm.tests/course/pa15/406-typed-label-resolution-regression.sh` exited
  `0`, with `22/22` labeled/goto facts; its direct LowIR validator exited `0`.
  Assertions cover one-target forward/backward convergence, dead-sibling
  omission, chained and nested-compound fallthrough, a live-join deferred
  branch, direct loop entry/backedge, nested loop break-context replacement,
  switch-to-loop break/continue replacement, and ordinary-label recovery
  through two switch interactions. Duplicate and unresolved probes reject with
  exits `1` and `1`; the generated positive CY86 program compiles and runs
  under the bounded execution check.
- The checked-in goto command passes `2/2`:
  `make -C pa15 check TEST='tests/general/200-goto-case-block-entry-label.t tests/general/200-goto-case-block-label-after-statement.t'`.
  The adjacent for/while/do-while, continue-in-switch, nested-switch, and
  switch-in-if matrix passes `6/6`. Copied LowIR validation, CY86 compilation,
  and the positive execution probe each exit `0`.
- `n=15; ... make test-report-through-pa$((n - 1))` passed through PA14 at
  `1058/1058`, exit `0`. `perl scripts/cppgm_file_audit.pl --stage pa15
  --paths dev/src` passed with the five existing header-division warnings.
  `make test-report-through-pa15` exited `2` only because PA15 retains the
  same four residuals, reporting `1163/1167`.
- `git diff --check` exits `0`. No handout test or `.ref` fixture changed;
  no reference binary, host compiler, or alternate production pipeline was
  used.

The durable 406 assertions check instructions as well as structure: the
`chained_fallthrough` and `nested_fallthrough` paths contain the store,
intervening addition, and later-label edge; `deferred_branch` retains its
conditional and both returns; `loop_entry` contains the condition, increment,
and backedge; `nested_control` proves the later outer break leaves the inner
loop; and `switch_loop_context` proves an inner switch break, outer loop
continue, and outer loop break use three distinct typed targets. The
`switch_ordinary_deferred` case re-enters an ordinary label after a terminating
case and honors switch fallthrough/break. The positive structural counts are
`chained_fallthrough` `8` blocks/`3` goto jumps, `deferred_branch` `6`/`2`,
`nested_fallthrough` `8`/`3`, `loop_entry` `5`/`2`, `nested_control` `8`/`2`,
`switch_loop_context` `12`/`2`, `switch_ordinary_deferred` `12`/`3`,
`switch_deferred` `17`/`5`, and `shared_recovery_tail` `12`/`6`; the last
function has exactly one `label_cont` block for its overlapping recovery
tail.

The measured complexity is backed by the typed continuation invariant. For
one function let `F` be semantic facts, `L` labels, `Q <= L` queued labels,
`K` canonical compound-cursor identities, `E` canonical structural-exit
identities, `C` persistent control-context nodes, `M` ordered switch-arm map
operations, and `G` emitted LowIR. The two typed prepasses cost `O(F)`;
`C + K + E + M = O(F)` because each is keyed by one semantic fact or one
ordered arm. A cursor is keyed by its typed first-child fact and a structural
exit by its typed frame fact; each identity is installed/advanced once, while
a later path performs one typed lookup and jumps to its existing `BlockId`.
Each recovered control head is a shared parent link with an `O(1)` cached
nearest-loop link, and each continuation pop checks/pops exactly one typed
head. Thus recovery is
`O(F + L + C + K + E + (Q + M) log(F + L + 1) + G)`, hence
`O((F + L) log(F + L + 1) + G)`. Storage is
`O(F + L + C + K + E + R + arms + G)`, where `R` counts actual
loop/if/switch records; there is no per-label ancestry or tail copy.
Queue-boundary and switch-map ordering are deterministic. Distinct label
targets retain their own current/termination state while shared source
continuations use one canonical entry. There is no unconditional whole-body
rescan/retry per label, textual lookup, host or reference shortcut, or retry
loop.

Fresh immutable/interleaved candidate-only performance evidence was regenerated
after the shared control-context correction at
`/tmp/pa15-label-perf-control-context-locked.n0tpXf`. The mode `0555`
candidate has SHA-256
`d777451f7246a573c0bd03470ba10e7ce3674c33a7017d6b99cd9426852da1cc`; the
copied validator is
`c4d2af08ed8ca2c357790c174220f2fbbe753ce63524e20301f64456291cbf40`.
Five alternating forward/reverse rounds produced `130` timing rows over
`26` family/size/orientation positions. Timing, semantic-generation, and
validation failure counts are all zero; all `26/26` LowIR validations exit
`0`, and every position has one stable output hash. `artifact-manifest.sha256`,
`inputs.sha256`, `implementation.sha256`, and `binaries.sha256` are verified;
the locked artifact manifest, input manifest, and binary manifest checks exit
`0`. There is no pre/post comparison claim because the implementation changed.
Each timing row used `/usr/bin/time` around `cppgm++ --emit-lowir -O0` with an
explicit output path and a `timeout 60s` guard; the semantic snapshot
used `cppgm++ --emit-semantics` and each validator row used `lowir2cy86 -o`
with explicit output paths, so no output was sent to a file named `-`.

| family | measured sizes | largest structural counters | largest median wall/RSS |
|---|---|---|---|
| nested | depth `8..256`, forward/reverse | `40/40` labels/gotos, `2,057` compound facts, `177` blocks, `362` instructions | `0.01 s / 10,444 KiB` |
| deferred | `d32-l4..d256-l32`, forward/reverse | `264/264` labels/gotos, `2,313` compound facts, `1,297` blocks, `2,858` instructions | `0.06 s / 15,516 KiB` |
| many | `64..2048` functions, forward/reverse | `2,048/2,048` labels/gotos, `2,561` compound facts, `5,633` blocks, `13,825` instructions | `0.20 s / 53,540 KiB` |

The nested/deferred sources are actual nested-compound and deferred-recovery
families, not indentation-only probes; their `structure.tsv` counters expose
depth, labels, facts, blocks, jumps, instructions, and LowIR size. The
deferred family grows from `40` to `264` labels, `177` to `1,297` blocks, and
`394` to `2,858` instructions as recovery size increases; the many family
grows from `64` to `2,048` same-spelling functions with `177` to `5,633`
blocks and identical forward/reverse structure. The many-function family
retains generation isolation. These bounded measurements substantiate
near-linear growth and output invariance at the listed sizes; the exact
canonical-identity bound above is the architecture claim.

Failure-set and gate disposition remain precise. The supplied turn-start
full-stage baseline was `105/109`, all `109/109` covered, with exactly these
four failures; the focused label changes add no failure surface.

| test | turn-start | checkpoint disposition |
|---|---|---|
| `100-const-integral-lvalue-overload-category` | fail | unchanged residual; out of scope |
| `100-string-hex-escape-code-unit` | fail | unchanged residual; out of scope |
| `100-unnamed-parameter-storage` | fail | unchanged residual; out of scope |
| `200-nested-conditional-array-decay` | fail | unchanged residual; out of scope |

All required broad/final gates completed. `make test-pa15` exited `2` because
the expected four residual tests fail; stage progress is `105/109` with all
`109/109` covered and no added or replacement failure. The through-PA14 report
passed `1058/1058` at exit `0`; the PA15 file audit passed with five existing
header-division warnings; and `make test-report-through-pa15` exited `2` only
for the same four PA15 residuals, reporting `1163/1167`. The final repository
scan has no untracked files, including no literal `./-`; the eight reviewed
paths are the complete checkpoint diff. `dev/src/pa15_lowering.cpp` is `2,957`
lines, within the file-audit limit. Remaining risks are bounded stress
representativeness and epoch-wrap safety being a code-level proof rather than
a runtime test over four billion functions; the wrap branch resets every
owned stamp and continuation index before epoch `1`.

## Historical Checkpoint Review — typed discarded and returned expressions

This review covers the landed source checkpoint
`98e75ff9dcf02fe6bd0646330dbc121dfc3c6193`
(`PA15: lower typed discarded and returned expressions`), parent
`2a10382f8abc0ff44ab7712f62c23f530441a63a`, and its final bounded
audit/repair commit. PA11 owns canonical types/constants, PA12 owns typed
conversion and return facts, PA15 owns typed LowIR lowering and declaration
demand, and PA13 owns serialization/validation.

The final `make test-pa15` gate exited 2 at `103/109`, `109/109` covered,
with exactly the six unchanged residuals in the plan. Mechanical comparison
of the sorted names recorded zero added/replacement names and zero missing
names in `/tmp/pa15-final-failure-set-proof.log`. The exact prior gate exited
0 at `1058/1058`; the root through-PA15 gate exited 2 at `1161/1167`, which
is the fully passing prior total plus the 109 PA15 cases and the same six
residuals. Complete logs are `/tmp/pa15-final-checkpoint-test-pa15.log`,
`/tmp/pa15-final-checkpoint-through-pa14.log`, and
`/tmp/pa15-final-checkpoint-through-pa15.log`.
The preserved names are `100-const-integral-lvalue-overload-category`,
`100-string-hex-escape-code-unit`, `100-unnamed-parameter-storage`,
`200-goto-case-block-entry-label`, `200-goto-case-block-label-after-statement`,
and `200-nested-conditional-array-decay`.

The end-to-end ownership trace is coherent. PA12 records `ToVoid`, including
void-to-void casts, and PA15 lowers the discarded source for its side effects
while emitting typed `return void` without an unnecessary scalar load. PA12
rejects a non-void expression in a void return, contextualizes non-void
returns, and represents supported bool/integral/floating/pointer `return {}`
as a typed zero; PA15 preserves the type, cv/category, and literal payload
through LowIR. Comma and discard lowering evaluates the left side once,
preserves a value-producing right lvalue/xvalue when it is consumed, and
avoids materializing a discarded right-hand value in expression statements,
for-init, and iteration paths. The C++11 volatile exception is retained at
the typed boundary: a scalar volatile lvalue glvalue is loaded when discarded,
including through a reference-to-volatile binding; nonvolatile reference
identifiers remain address-only.

The bounded repair closes two late owner gaps found by this audit. A
follow-up volatile audit then narrowed the value-elision shortcut so it cannot
claim a volatile reference is side-effect-free. The
ForInit expression path now calls the common discarded-expression owner. That
owner recursively handles comma expressions and treats a bare discarded
nonvolatile function/reference identifier as a side-effect-free value,
avoiding a storage/value load while retaining assignment/call side effects;
its typed volatile-scalar check routes volatile glvalues through one value
load. Value-context logical
lowering now uses a per-semantic-fact cached proof for literal bool/integer
truth and recursively omits only a provably unreachable RHS; runtime and
unknown paths still lower normally. Condition lowering uses the same recursive
proof, so nested literals cannot leak an unreachable call/address declaration.
The cache fails closed for unknown or malformed facts and avoids repeated
subtree work; there is no textual reconstruction, broad DCE, whole-program
retry, or declaration sweep.

Function declaration demand remains rooted in lowered reachable calls,
function addresses/references, and typed global relocations, with one
deterministic declaration per demanded binding. The focused logical probe
confirms an unreachable `extern` call is not demanded; owner probes 400--404
retain the direct-call/address, typed-global-relocation, reference, and
linkage coverage. The five removed handout tests, adjacent matrix, and one
narrow owner regression passed; no handout test or `.ref` fixture changed.

Focused evidence for this checkpoint is:

- the five residual-owner tests pass `5/5` and the representative adjacent
  matrix passes `7/7`;
- `cppgm.tests/course/pa15/405-typed-discarded-return-regression.sh` passes,
  including typed empty-brace bool/integral/f32/f64/f80/pointer returns,
  void-to-void discard, for-init comma discard, nested/value logical pruning,
  evaluated runtime call/address demand, direct volatile-object and
  reference-to-volatile discard loads, and `lowir2cy86` validation;
- the current `dev/cppgm++` build passes, owner probes 400--404 pass, and the
  five removed inputs plus 405 compile and validate with `lowir2cy86`.

The PA15 file audit exits 0 with five known header-division warnings
(`/tmp/pa15-final-file-audit.log`), and `git diff --check` exits 0. Moving
`lower_logical` to the existing flow owner keeps
`dev/src/pa15_lowering.cpp` at 2945 lines, below the 3000-line limit; no new
source set or parallel lowering implementation was introduced.

Fresh candidate-only performance evidence is retained at
`/tmp/pa15-checkpoint-audit-discarded-final.fRe1n0`: immutable candidate mode
`0555`, SHA-256
`f03860f091c4a646af8937a7e9023e79facd1a6aebb4c885d408d1a81f16d95e`, six
affected-path inputs, five interleaved forward/reverse rounds, 20 compilations
per sample, verified implementation/input/toolchain/LowIR hashes, and zero
compile/validator statuses. Median wall time per invocation is
`0.0025--0.0040 s` and median RSS is `5124--5384 KiB`; these bounded
candidate-only measurements are not comparative or universal claims.

The six unchanged residuals—three `100-*` surfaces, two goto-label cases,
and nested conditional array decay—remain outside this audit and define the
next checkpoint. No unrelated source, handout test, or `.ref` fixture was
changed.

## Historical Checkpoint Review — typed global pointer null/zero initializers

This review is for the landed increment `dea5352e70fc42b3fa5a56bbe2b17682c581777a`
(`PA15 lower typed global pointer null initializers`) and is bounded to the
typed null/zero initializer path plus the two checked-in pointer-array
fixtures. The residual PA15 enum, floating, goto, and unrelated surfaces are
not re-audited here.

The ownership trace is now single and typed:

1. PA10 decodes a literal and retains its decoded bytes on the AST node as the
   source fact. PA12 validates the array type, element count, byte size, and
   terminal `ArrayToPointer` conversion, then takes one downstream snapshot in
   `constant_address_literal_bytes_` and records its range, element type, and
   count in `ConstantAddressFact::Literal`. The PA12 arena is the canonical
   downstream snapshot; PA15 never reads the PA10 payload, semantic kind, or
   conversion range to relocate a literal.
2. `resolve_constant_address` is a transaction around its recursive typed
   implementation. It records the arena tail before resolution, rolls it back
   on false, exception, or an invalid candidate, and accepts only well-formed
   facts. A `SymbolAddend` must name an in-range variable or function binding;
   an `ArrayElement` must retain its typed target/index relation; a `Literal`
   has no binding target or addend and its byte range must match its element
   type. Unsupported literal arithmetic is rejected before it can transform a
   literal into an invalid symbol addend. A transparent cast wrapper may pass
   `ArrayDecay` to the literal child, preserving the valid literal fact.
3. PA15 maps the recorded `ConstantAddressFact` identity to a LowIR symbol.
   For a literal it materializes one deterministic internal backing global
   from the PA12 byte range and caches only the backend identity mapping from
   constant-address fact to `SymbolId`. This is LowIR identity materialization,
   not a second semantic relocation model. Non-literal mappings require the
   validated binding target. No text, fixture name, source pattern, or
   lowering-time semantic reconstruction is used.
4. Null conversions remain PA12 `ConversionFact` ownership. Scalar pointer
   nulls become `INIT_ZERO`; array elements and omitted slots become coalesced
   `ITEM_ZERO` data. Address items use the same typed constant-address mapper,
   so string-backed addresses, repeated literal facts, and zero slots retain
   deterministic identity and order.
5. PA15 accepts a pointer-zero chain only when there is one null conversion at
   the beginning, every following conversion is linked and pointer-valued,
   the suffix is limited to identity, lvalue-to-rvalue,
   pointer-qualification, or pointer-to-void, and the terminal target matches
   the destination after only an outer cv wrapper is removed. Pointer-object cv
   remains in the pointer `TypeId`; pointee cv remains in its child `TypeId`.
   PA12 value conversion discards top-level pointer-object cv, while
   `qualification_convertible` continues to reject pointee qualification
   drops. The `pa11_semantic_core.cpp` change is retained because its callers
   are value-conversion/common-type contexts, not reference identity binding.
6. PA15 loads an lvalue before pointer qualification or pointer-to-void value
   conversion. Runtime `nullptr` is emitted as a typed pointer copy; its
   `NullptrToBool` conversion uses a typed pointer comparison and boolean
   conversion rather than retagging a pointer temporary as a boolean.
   `LowIR Program` remains the only typed production IR model and the
   serializer only renders that model.

The affected work is linear in the typed fact/range sizes, with ordered
identity maps retaining ordinary `O(n log n)` behavior. Zero data is coalesced
in one pass. No broad string-expression lowering, host compiler, reference
shell-out, duplicate production model, or textual downgrade was introduced.

## Final Checkpoint Evidence

- `make -C dev cppgm++` exited `0` after the bounded source repair; its log is
  `/tmp/pa15-enum-followup-build.log`.
- The exact compact 13-test PA15 matrix from `pa15/plan.md` passed `13/13`;
  its log is `/tmp/pa15-enum-followup-focused.log`.
- The durable earliest-owner regression
  `cppgm.tests/course/pa15/402-typed-enum-boundary-regression.sh` exited `0`;
  its log is `/tmp/pa15-enum-followup-402.log`.
- Bounded temporary probes exited `0` for declaration-only and interleaved
  default ownership, enum signed/unsigned boundaries, global unsigned wrap,
  and typed operator/pointer-offset lowering. Fixed-underlying out-of-range
  values, an implicit scoped value above `int`, a 32-bit promoted-width shift,
  a mixed scoped conditional, and a fixed-bool value outside `0`/`1` exited
  nonzero as required. A selected nested conditional chain of depth 16
  preserved the unsigned boundary (`cmp eq u32 4294967295, 4294967295`); its
  source, LowIR, and log are `/tmp/pa15-followup-conditional-chain.cpp`,
  `/tmp/pa15-enum-followup-conditional-chain.lowir`, and
  `/tmp/pa15-enum-followup-conditional-chain.log`.
- `make test-pa15` exited `2` with `79/109` passing, all `109` covered, and
  exactly the unchanged 30-name residual set; its log is
  `/tmp/pa15-enum-followup-full-pa15.log`. The mechanical failure-set proof
  is `/tmp/pa15-enum-followup-failure-set.log`: current and incoming counts
  are both `30`, current-minus-incoming is empty, and incoming-minus-current
  is empty.
- The exact `n=15` prior gate exited `0` with `1058/1058`:
  `n=15; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`; its log is
  `/tmp/pa15-enum-followup-through-pa14.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` exited `0`;
  its log is `/tmp/pa15-enum-followup-file-audit.log`.
  It emitted only the five pre-existing `bad-division` header warnings for
  `abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
  `pa11_semantic_model.h`, and `pa15_lowering.h`.
- `git diff --check` exited `0`; its log is
  `/tmp/pa15-enum-followup-diff-check.log`.
- Fresh post-correction performance evidence uses
  `/tmp/pa15-final-enum-perf.relational-final.ES1ytx/cppgm++-final-immutable`,
  mode `0555`, SHA-256
  `65df5ea3b49bff32cfbf76866001ddc67b89874d18fe7b36bad1704781a3c67e`.
  Five interleaved ascending/descending rounds, proportional selected-chain
  counters, and medians are recorded in that directory's `structure.tsv`,
  `timings.tsv`, and `medians.tsv`.

## Fresh Performance Evidence — selected nested conditional ownership

The immutable candidate is
`/tmp/pa15-final-enum-perf.relational-final.ES1ytx/cppgm++-final-immutable`, mode `0555`,
SHA-256
`65df5ea3b49bff32cfbf76866001ddc67b89874d18fe7b36bad1704781a3c67e`.
The directory contains the candidate hash/mode records, bounded source
inputs, semantic and LowIR outputs, `structure.tsv`, `timings.tsv`, and
`medians.tsv`. Each input retains the bounded enum/default/promotion/operator
coverage and replaces the flat conditional with a selected nested chain of
depth 16, 64, 256, or 512. The `structure.tsv` counters record the selected
chain depth and semantic conditional-node count; five timing rounds alternate
ascending and descending size order.

| depth | input bytes/lines | semantic bytes/lines | LowIR bytes/lines | semantic conditional nodes | LowIR globals/functions | median wall/user/sys s | median max RSS KB |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 16 | 1871/67 | 7885/166 | 4544/167 | 18 | 20/7 | 0.00/0.00/0.00 | 5380 |
| 64 | 4559/211 | 28765/406 | 7424/215 | 66 | 68/7 | 0.00/0.00/0.00 | 6084 |
| 256 | 15935/787 | 250837/1366 | 19412/407 | 258 | 260/7 | 0.01/0.01/0.00 | 8680 |
| 512 | 31295/1555 | 891093/2646 | 35540/663 | 514 | 516/7 | 0.02/0.01/0.01 | 12676 |

These are bounded measurements of the selected nested path, not a universal
performance claim. The depth and conditional-node counters scale with the
generated chain; nested semantic rendering contributes its own depth-shaped
text size. Raw interleaved timings and medians are retained in the artifact
directory.

## Historical Evidence — typed global pointer null/zero initializers

- `make -C dev cppgm++` exited `0` after the final source repair.
- The focused affected-path matrix passed `10/10`; its log is
  `/tmp/pa15-final-focused-affected-matrix.log`.
- The narrow regression
  `cppgm.tests/course/pa15/401-typed-pointer-null-cv-regression.sh` exited
  `0`. It verifies scalar keyword/integer/cast nulls, top-level pointer cv
  with a required lvalue load, typed `nullptr`-to-bool comparison, and
  rejection of a pointee qualification drop.
- The transparent literal-cast probe emitted `__strlit__1` and
  `global @value ... = addr @__strlit__1`; the unsupported literal-arithmetic
  probe was rejected as `PA15 nonconstant global initializer`. Logs are
  `/tmp/pa15-final-literal-wrapper.log` and
  `/tmp/pa15-final-literal-arithmetic.log`.
- `make test-pa15` exited `2` with `70/109` passing and all `109` covered. The
  final failure inventory has exactly the 39 turn-start names: zero names
  were added and zero names were removed. The fresh log is
  `/tmp/pa15-final-full.log`; the sorted inventories used for comparison are
  `/tmp/pa15-turn-start-failures-final-audit.txt` and
  `/tmp/pa15-final-failures-final-audit.txt`.
- The exact `n=15` through-PA14 command exited `0` with `1058/1058`; its log
  is `/tmp/pa15-final-through-pa14.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` exited `0`
  with the five pre-existing header-division warnings. Its log is
  `/tmp/pa15-final-file-audit.log`. `git diff --check` passed.

## Historical Performance Evidence — typed global pointer null/zero initializers

Fresh evidence was measured from the final immutable candidate
`/tmp/pa15-final-perf-final.hvQKB0/cppgm++-final-immutable`, mode `0555`,
SHA-256
`4d9ae4004642bdf402118ef3328efe417a5d8d4427de033de6eba700b8658dd9`.
The candidate, generated sources/outputs, structural counts, raw interleaved
batch timings, and medians are retained in that directory:
`candidate.sha256`, `structure.tsv`, `timings.tsv`, and `medians.tsv`.
There are five interleaved rounds, six family/size positions per round, and
20 repeated compilations per timing sample. Odd rounds use sizes `32, 128,
512`; even rounds reverse them; `mixed` and `repeated` families are
interleaved at each size.

| family | n | LowIR globals | address items | zero items | literal facts | median wall s | median RSS KiB |
|---|---:|---:|---:|---:|---:|---:|---:|
| mixed | 32 | 13 | 12 | 12 | 28 | 0.003500 | 5160 |
| mixed | 128 | 49 | 48 | 48 | 100 | 0.004000 | 5396 |
| mixed | 512 | 193 | 192 | 192 | 388 | 0.006000 | 5928 |
| repeated | 32 | 33 | 32 | 0 | 37 | 0.004000 | 5160 |
| repeated | 128 | 129 | 128 | 0 | 133 | 0.005000 | 5648 |
| repeated | 512 | 513 | 512 | 0 | 517 | 0.010500 | 6896 |

The `mixed` inputs exercise repeated string facts, `nullptr`, integer-zero
pointer initializers, and omitted trailing slots. The `repeated` inputs stress
literal identity/materialization. These are bounded affected-path samples,
not a universal performance claim; the medians show no timeout or unexpected
superlinear behavior at the measured sizes.

Historical evidence is preserved separately and is not used as final proof:
the earlier typed address/value candidate and measurements remain under
`/tmp/pa15-typed-relocation-correction.6EMMbb/context-perf`, and the earlier
pre-repair null-pointer candidate and measurements remain under
`/tmp/pa15-null-pointer-perf.QgTN8n`. Their prior progress and timings are
historical only; the final claims above use the immutable candidate from this
checkpoint.

## Audit Ledger

| status | checkpoint | evidence and disposition |
|---|---|---|
| Historical | PA15 full-stage / checkpointAudit — typed address/value ownership | Amended PA12 relocation ownership with explicit `Value`/`ObjectAddress`/`ArrayDecay` context, rejecting bare pointer/scalar lvalue relocations while preserving object, array, one-past, function, and array-element forms; focused `20/20` plus probes, through-PA14 `1058/1058`, PA15 `68/109` with the exact historical 41 names and all `109` covered, immutable `n=256` performance evidence, file audit pass, and diff-check pass. |
| Historical | PA15 full-stage / checkpointAudit — typed global pointer null/zero initializers | Hardened transactional PA12 literal snapshots and binding invariants; kept PA15 on `ConstantAddressFact` identity/ranges; tightened terminal/destination-safe typed null chains and pointer cv behavior; corrected lvalue loads and runtime `nullptr`-to-bool typing; target matrix `10/10`, narrow regression pass, final PA15 `70/109` with the exact unchanged 39-name set and all `109` covered, through-PA14 `1058/1058`, file audit pass, diff-check pass, and immutable performance evidence. Preserved as historical context. |
| Historical | PA15 typed discarded/returned-expression ownership at `98e75ff9dcf02fe6bd0646330dbc121dfc3c6193` plus final bounded audit repair | Final `make test-pa15` `103/109`, all `109/109` covered, exact six residuals preserved with zero added/replacement or missing names; through-PA14 `1058/1058` exit 0; through-PA15 `1161/1167` with the same six; focused owner matrix `5/5`, adjacent matrix `7/7`, owner regression 405 (including volatile discard and nested logical demand), owner probes 400–404, and affected-path `lowir2cy86` validation pass. PA12 owns typed `ToVoid`, return conversion, cv/volatile and empty-brace facts; PA15 owns discard/return/logical lowering and demand roots. Fresh immutable candidate-only artifact `/tmp/pa15-checkpoint-audit-discarded-final.fRe1n0` is hash-verified; file audit, diff-check, and checkpoint commit complete. Retired as the prior current row. |
| Historical | PA15 full-stage / checkpointAudit — typed label ownership, sparse flow arenas, and generation-guarded canonical structural recovery at `d5e10599edfa983c64746b25373fe3d3ec129b39` plus the completed supervisor-reviewed correction | Focused owner regression exit `0` with `22/22` labeled/goto facts and instruction/structure assertions for intervening and nested fallthrough, live-join deferred recovery, loop entry/backedge, nested loop break-context replacement, switch-to-loop break/continue replacement, ordinary-label switch continuation, and one shared recovery tail; checked-in goto fixtures `2/2`; adjacent matrix `6/6`; direct validation, CY86 compilation/execution, and duplicate/unresolved rejection pass. PA10 spelling -> PA12 `NameId`/dense `LabelId` and function-local `LabelTableId` -> PA15 typed `SemanticFactId`/`BlockId` -> PA13 validation remains the sole production trace. Fact-indexed sparse `LoopFlow`/`IfFlow`/`SwitchContext` arenas replace heavyweight per-fact records; typed parent/index cursors and continuation identities canonicalize overlapping tails; the persistent typed control context uses shared parent links and cached nearest-loop lookup with exact pop-on-unwind; the persistent switch map removes recovered-label arm rescans; final queue drainage, seven guarded stamp arrays, and wrap-reset continuation indexes preserve function isolation and wrap safety. Immutable candidate-only artifact `/tmp/pa15-label-perf-control-context-locked.n0tpXf` has verified per-file/artifact manifests, candidate hash `d777451f7246a573c0bd03470ba10e7ce3674c33a7017d6b99cd9426852da1cc`, `130` timing rows, `26/26` semantic generations, `26/26` validation, and one stable output hash at each position across many, nested, and deferred families. The proven recovery bound is `O((F + L) log(F + L + 1) + G)` with no duplicated ordinary tails and `O(1)` recovered break/continue selection and frame pop. Final `make test-pa15` stage progress is `105/109` with all `109/109` covered and exactly the four preserved residuals; through-PA14 is `1058/1058`, combined through-PA15 is `1163/1167`, and the file audit passes with five existing warnings. |
| Current | PA15 full-stage / checkpointAudit — typed conditional-array glvalues at `b7eaf9d868b17cbbf542f3415e7a5e46f07007ba` | Final `make test-pa15` exit `2`, `106/109` with `109/109` covered and exactly the three named residuals; through-PA14 exit `0` at `1058/1058`; file audit exit `0` with the five listed existing warnings; through-PA15 exit `2` at `1164/1167`; focused checked-in matrix `6/6` and `15/15`, PA12 xvalue fixture `1/1`, custom semantic/LowIR probes and validators exit `0`. Complete logs are in `/tmp/pa15-audit-gates-20260826`, and the reproducible performance artifact, stable output hashes, candidate/validator hashes, and manifests are in `/tmp/pa15-audit-scale-final-20260826-samebound`. Typed PA12 category/type/conversion ownership flows through PA15 address/value lowering to PA13 validation with no source-text reconstruction or duplicate decay. No source repair was found. |
