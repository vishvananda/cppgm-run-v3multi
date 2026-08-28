# PA16 checkpoint audit

## Current Checkpoint Review

This review covers landed commit `dea01c52089fe78b8d23cce0b72ecbe8686ddb26`
(`PA16: lower typed recursive aggregate initialization`) relative to parent
`36b93869`, plus the bounded checkpoint-audit repairs and course control 415.
The ownership boundary is C++11 aggregate initialization for the supported
object subset: nested arrays and records, brace elision, omitted tails,
default member initializers, scalar/class/reference members, bit-fields,
string-literal pointer members, value initialization, and namespace/static
aggregate storage. Unions, aggregate bases, copy/move or by-value transfer,
templates, virtual/multiple inheritance, and unrelated residual stage surfaces
remain outside this checkpoint. No handout test, fixture, `.ref` file,
comparator, harness, or source-set list changed.

### Contract and ownership

The implementation follows spec.md §§1, 2, 4, 5, and 7: one shared typed
pipeline, one canonical owner for each fact, demand-driven bounded work, no
source-text reconstruction, and deterministic structural evidence. The
representative ownership trace is:

`PA10 BracedInitList + typed destination`
  -> `PA11/PA12 canonical TypeId, BindingId, SemanticFactId`
  -> declaration-ordered `RecordLayout::members` and sparse
     `AggregateElementFact` ranges
  -> PA12 appertainment, brace elision, DMI/omitted-tail/reference decisions,
     constructor actions, and typed parameter resolution
  -> PA15 independent global-root/runtime demand visitation
  -> one source/declaration-ordered `PendingGlobalAction` stream
  -> typed constant data or `SR_INIT` lazy aggregate-root/path lowering
  -> checked owner/type/range/layout offsets
  -> direct scalar/reference/bit-field stores or a demanded constructor helper
  -> LowIR and backend.

`RecordLayout::members` is the sole declaration-order/index owner in the
changed PA12 paths. It excludes static members and anonymous/zero-width
bit-field layout events while retaining named bit-fields in declaration order.
Sparse aggregate ranges carry only present elements; omitted scalar runs are
zero-filled without a bound-sized semantic arena. PA12 copies arena values
before initializer work that may append bindings or types.

For `Pair rows[2] = {{1, 2}, {3, 4}}` and the brace-elided equivalent, typed
list facts retain nested destination types and PA15 recomputes checked paths
from the canonical root. `RefWrap alias = {pair.first}` carries addressable
storage and emits an alias pointer, not a copied value. Named bit-fields use
the same ordered layout path and the bit-field initialization context. Typed
literal payloads are interned from PA11-owned decoded bytes, so fixed and
inferred-bound string-pointer records do not recover source spelling.

### Findings and bounded repairs

- `collect_demanded_member_functions` now has independent global-root and
  ordinary-runtime visited state. A shared semantic fact can therefore be
  considered once for global aggregate-helper inlining and again for runtime
  helper demand; one context-specific visited bit cannot suppress the latter.
  Both walks remain bounded by the semantic-fact arena and deterministic stack
  order. Course 409 covers the related aggregate-helper/ordinary-default
  constructor boundary; no distinct same-`SemanticFactId` course reproducer
  was found.

- Global address projections, scalar dynamic values, and aggregate actions
  share one pending-action stream. Stable source declaration/declarator order
  is retained through `__cppgm_init`, so implementation kind cannot regroup
  ordered dynamic initialization within one translation unit. Course 415
  observes the interleaved call/store order and executes the result.

- The global aggregate inliner requires an exact synthetic aggregate
  constructor, canonical record/type/owner identity, complete layout, fixed
  arity, declaration-order action range, valid function-scope parameters,
  scalar/pointer member types, checked offsets, and parameter-only initializer
  facts with a bounded cast walk. Invalid, cyclic, reference-unsupported, or
  otherwise non-inlineable facts fail closed to the ordinary demanded helper;
  they are never silently suppressed. Root storage, path recomputation,
  overflow/range checks, and bit-field context are validated before stores.

- Global aggregate data lowering coalesces omitted zero runs while preserving
  one typed scalar slot where needed. It supports fixed and inferred-bound
  string-pointer records through typed literal-content interning. The exact
  formerly residual unknown-bound namespace record handout now passes.

- The canonical-bool shortcut was not retained. The valid affected shape is
  `trunc u8 i64` followed by `zext i32 u8`; the direct `zext i32 u8` from the
  i64 comparison was rejected by `lowir2cy86` for operand/source-type
  mismatch. The final focused output check and backend translation pass.

- The file-audit size findings introduced by the increment were removed by
  extracting global declaration-position validation and keeping declaration
  semantic analysis within the project limit. The audit now reports only its
  five pre-existing header-division warnings.

### Focused and broad evidence

The authoritative turn-start record is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`159/243` passed, `84` failed, and `243/243` identities were covered. Final
`make test-pa16` exits 2 with `164/243` passed, `79` failures, and
`243/243` identities covered. Exact failure-set comparison has five
baseline-only repairs and an empty final-only set:

- baseline-only: `general/200-global-class-array-enum-trivial-dtor.t`,
  `general/200-global-scalar-dynamic-init.t`,
  `general/200-local-struct-array-init.t`,
  `general/300-namespace-aggregate-array-string-members.t`, and
  `general/300-static-member-aggregate-array-dynamic-init.t`;
- final-only: `∅`.

The complete exact baseline and final maps are preserved in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-aggregate-init-audit-final-v1/baseline-failures.txt`
and `final-failures.txt`. The exact 17-test focus is `12/17`, with all
`17/17` identities covered. Its five remaining failures are
`general/100-global-aggregate-nested-array-initializer.t`,
`general/200-defaulted-constructor-still-aggregate.t`,
`general/200-deleted-constructor-still-aggregate.t`,
`general/300-value-init-aggregate-with-nontrivial-member.t`, and
`general/400-bitfield-aggregate-init.t`; the other 12 focus identities pass.

`make -C dev cppgm++` exits 0. The required through-PA15 command exits 0 with
`1167/1167` passing. The required file audit exits 0 with five pre-existing
`bad-division` warnings in `abi_mangle.h`, `cpp_semantic_core.h`,
`lowir_model.h`, `pa11_semantic_model.h`, and `pa15_lowering.h`.
Controls 404, 409, 412, and 415 each exit 0; `git diff --check` exits 0.
Exact outputs and statuses are preserved under
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-aggregate-init-audit-final-v1`.

### Performance and structural evidence

The immutable historical evidence at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-aggregate-init-evidence`
was preserved. The separate final replay is at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-aggregate-init-audit-final-v1`.
The final compiler SHA-256 is
`62f6feea601662cb601f12c3ad3b9083f4da85639c2e2b741cf24c7a31721d4b`.
The replay has 30/30 zero-status runs, nine semantic pairs and six LowIR
pairs, with zero repeated-hash mismatches. It records source/output sizes and
semantic list/action/literal plus LowIR store/call/projection counts in
`structural-counts.tsv`.

The omitted-tail bounds 16, 1024, and 1000000 each produce 17 semantic lines,
one aggregate list, and zero per-element aggregate descendants; their output
sizes are 593, 601, and 612 bytes. Explicit nested, brace-elided,
reference/class, fixed string-pointer, and exact unknown-bound handout probes
all pass twice with matching hashes. The exact unknown-bound handout produces
52 semantic lines/2616 bytes and 118 LowIR lines/2564 bytes. The bool and
ordered-initialization replays are in `bool-shape-check.log` and
`ordering-evidence.log`. These are structural/deterministic observations only;
no timing, RSS, allocation, or speedup claim was measured.

### Boundaries, residuals, and next checkpoint

The five focused LowIR residuals listed above remain open; defaulted/deleted
aggregate eligibility is not broadened beyond the typed C++11 decision, and
the value-init/bit-field/global nested cases retain their valid typed lowering
even where fixture LowIR shape still differs. The full stage remains at 79
failures, so PA16 is not complete. No same-fact shared-demand course case was
constructed, and no timing/RSS evidence exists. Unions, bases, transfer,
templates, virtual/multiple inheritance, and unrelated PA16 surfaces remain
out of scope. The next checkpoint should select the remaining aggregate
LowIR/semantic identities or a separate staged surface; it must not treat this
checkpoint as completion.

## Historical Member-Function-Definition Declarator Review

This historical review covers landed commit 9718b98797312753e33023fe97d36d74afd0a84a
(PA16: type member-function definition declarators) relative to parent
97d1e7a5, plus the bounded follow-up corrections in the PA11 typed
declarator path. The source audit is limited to pa11_semantic.cpp,
pa11_semantic_core.cpp, pa11_semantic_model.h, pa11_semantic_types.cpp,
pa12_semantic.cpp, and pa12_semantic_construction.cpp. The only added test
artifact is cppgm.tests/course/pa16/413-typed-member-definition-declarator-validation-regression.sh.
No handout test, fixture, .ref file, comparator, harness, generated output,
or source-set list was changed.

The PA16 contract here is in-class member definitions and qualified
out-of-class ordinary non-static member definitions, including typed trailing
returns and private nested leading return types. The README explicitly
excludes out-of-class constructor and destructor definitions. The out-of-
contract special-member widening from 9718b987 is removed: process_special_member
again requires a class-scope owner, and the namespace/root PA12 special-member
analysis and preparation additions are gone. Only the pre-existing in-class
special-member path remains in this scope. The excluded nested out-of-class
constructor remains a failing identity and is not claimed as PA16 coverage.

### Contract and ownership

N3485 [dcl.fct], [dcl.fct.def], and [class.mfct] define the function
parameter-and-qualifier sequence, trailing-return-type, and member-definition
context relevant here. The implementation is checked against that standard
text in [N3485](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3485.pdf).

The root architecture alignment is:

- spec.md §2 keeps classified declaration, type, scope, binding, parameter,
  and body facts typed; presentation is not used to recover identity.
- spec.md §4 keeps this validation as local, deduplicated work over the
  declarator and its typed operation list.
- spec.md §5 keeps PA12 preparation/analysis and PA15 lowering on the existing
  typed FunctionFact, TypeId, and downstream facts.
- spec.md §7 records executable conformance and structural determinism
  evidence without inventing a timing claim.

The affected fact path is:

PA10 declarator shape (qualified name, parameter clause, cv/noexcept/ref/
trailing-return nodes)
  -> PA11 SpecFact::is_auto, explicit DeclaratorBaseKind state, and
     DeclaratorOp carrying the trailing TypeId
  -> canonical class ScopeId, NamedRecordId, BindingId, FunctionFact,
     function scope, parameter facts, and body scope
  -> PA12 preparation/analysis and existing typed conversion/call consumers
  -> PA15 typed function reachability, ABI facts, and LowIR emission

For an ordinary member definition, process_function_definition resolves one
qualified owner. That owner is reused for trailing-return lookup, parameter
lookup, binding ownership, FunctionFact.owner, function-scope parentage, and
body lookup. The focused owner path emits the declared const member parameter,
the declared parameter, and the body member access without reconstructing an
owner from rendered text.

### Findings and bounded repairs

- The trailing return remains a typed PA10 TypeId inside a DeclaratorOp.
  SpecFact::is_auto is the canonical classification. Each application entry
  creates an explicit DeclaratorBaseKind; nested declarators share that state.
  The trailing-return operation requires AutoPlaceholder and consumes it by
  changing the state to Typed. No invalid TypeId is used as an auto marker.
  An unrelated invalid TypeId remains invalid and is rejected by the existing
  typed-result checks. auto (*callback)() -> int remains valid.

- spec_fact rejects a duplicate auto, auto combined with another base
  type, leading cv-qualified auto, and typedef auto. Storage qualifiers do
  not broaden the type rule: the valid static auto trailing-return control in
  course 413 passes.

- A trailing return requires a parameter clause and the auto placeholder.
  Object arrows, missing auto, suffixes after the arrow, invalid cv/noexcept
  ordering, and unsupported ref-qualified forms fail closed. Ref qualifiers
  are rejected because the existing TypeKey and binding identity do not
  represent them; silently ignoring them would merge distinct declarations.
  Auto in parameter types and auto in type-ids is rejected. The public course
  control's auto f() -> auto case is parser-accepted as a TypeId and reaches
  typed rejection; the other public malformed forms are asserted as
  rejections without claiming more parser reachability than observed.

- The special-member owner change was narrowed back to the parent behavior.
  process_special_member is class-scope-only; root SpecialMemberDefinition and
  SpecialMemberDeclaration handling in PA12 is absent, and the root
  special-member preparation body is absent. The in-class constructor-member-
  init control still passes. The excluded nested out-of-class constructor
  now fails at PA11 special-member owner validation, as required by the
  contract boundary.

- The implementation uses one bounded child walk and one typed operation
  application per declarator, with no textual semantic key, test-name
  shortcut, reference/host-compiler shell-out, whole-program retry, or
  unbounded scope scan. Invalid IDs are rejected before binding or
  FunctionFact publication.

### Focused and broad evidence

The audit-turn start was exactly 132/243 passed, 111 failed, and 243/243
identities covered. The authoritative baseline is
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log. Its
diagnostic totals are 61 expected-success exit mismatches, 2 expected-failure
exit mismatches, and 48 LowIR comparison mismatches.

The final focused build exits 0; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/build.log
The public course 413 regression exits 0 and covers mixed/duplicate/cv/
typedef auto, auto in a type-id, missing auto, non-function arrows, suffix
ordering, ref qualifiers, auto parameters, auto without a trailing return,
auto simple declarations, a valid static auto return, and the valid nested
function pointer; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/course-413.log

The exact seven-test focus exits 2 with 5/7 passing; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/focused-matrix-7.log
Passing identities are
300-member-function-trailing-return.t,
100-out-of-class-methods.t,
300-out-of-class-private-nested-return-type.t,
200-constructor-overload-default-arg-nonfirst-argument.t, and
200-return-preserves-value.t. The residual
300-out-of-class-member-trailing-return.t fails with PA12 invalid conversion
in its existing member-typedef pointer-return path. The explicitly excluded
200-nested-out-of-class-constructor-enclosing-type.t fails with
PA11 special member has no class owner. The separate
200-constructor-member-init.t control is 1/1; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/focused-constructor-control.log

The required prior-through command exits 0 with 1167/1167; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/through-pa15.log
The exact required file audit exits 0 with five pre-existing header-division
warnings; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/file-audit.log
The final make test-pa16 exits 2 because PA16 remains incomplete, with
132/243 passed, 111 failures, and all 243 identities covered; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/pa16-test.log
The exact failure-set comparison is preserved at:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/identity-compare.log
It reports baseline 111, final 111, inventory 243, baseline-only empty,
and final-only empty. Thus no pass identity regressed, and no added pass was
used to offset a new failure. git diff --check exits 0; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/diff-check.log

### Performance evidence

The current executable structural run is:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/structure.log
Its log SHA-256 is bcb4f7ac160a94f1cbb499ca5a823c657d8487659d58d0b55664b1b9f4a4d1a1
and the executable SHA-256 is 375482e808d7c1c1251e9c9ebf186eee8b4ff6014f6e4410c5d2ca5a18d12379.
Using the existing N=1,4,16,64 member-definition inputs, each run has N
declarations and N definitions. Two semantic runs per size exit 0 with
identical hashes. Output lines/bytes are 14/468, 32/1227, 104/4269, and
392/16461; function-record counts are 2, 5, 17, and 65. This is structural
boundedness and determinism evidence only; it is not a timing, RSS,
allocation, or speedup claim.

### Next checkpoint

The next checkpoint is a later PA16 residual audit focused on the existing
member-typedef pointer-return residual and the remaining explicitly staged
PA16 boundaries. It is not broad validation. PA16 remains incomplete until
those residuals are separately resolved or contractually closed.

## Historical Fixed-Bound Array-Lifetime Checkpoint Review

This historical review was retained from the preceding fixed-bound array
lifetime checkpoint.  Its original content follows unchanged.

This review covers landed commit `0a6be82d9bf17db2585772f2be28d45e6af781de`
(`PA16: add typed array lifetime cleanup`) relative to parent
`5d91986f166e000daddecaf112e0cb58df6a8e8b`, plus bounded audit repairs and
the focused course regression in
`cppgm.tests/course/pa16/410-typed-lifetime-activation-control-exit-regression.sh`.
The scope is fixed-bound local automatic arrays of class objects and recursive
synthesized array-member lifetime: typed array shape, canonical class and
destructor identity, PA12 lifetime/action facts, PA15 recursive construction
and destruction, completed-prefix EH cleanup, lexical/control-exit state, and
LowIR serialization.  Global/static/TLS lifetime and guards, copy/move or
by-value transfer, virtual/multiple inheritance, templates, new/delete, and
unrelated operator/access/temporary machinery remain outside this review.

The authoritative checkpoint-turn-start full-stage state was `93/243` passed,
`150` failed, and `243/243` covered, from
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The final `make test-pa16` command exited `2` with log
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-test.log`:
`93/243` passed, `150` failed, and all `243` tests were covered.  The
normalized failure map is exactly the baseline map: `150` identities in each
log, baseline-only `∅`, and final-only `∅`; the `93` passing complement is also
unchanged.  The test inventory contains exactly `243` identities, and the
baseline and final runs each report every identity, so coverage additions and
removals are both `∅`.  The normalized set/count record is preserved at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-identity-compare.log`.

The affected ownership path is:

```text
PA10 local class-object/array declaration and synthesized-member syntax
  -> PA11 canonical TypeId array shape, NamedRecordId, BindingId, and
     destructor FunctionFact ownership
  -> PA12 automatic LifetimeFact plus ordered ConstructorActionFact and
     DestructorActionFact ranges (base/member order and recursive arrays)
  -> PA15 typed destructor demand, constructor/destructor lowering, checked
     array strides, active-lifetime state, and completed-prefix EH chains
  -> LowIR constructor/destructor calls, lexical/control-exit cleanup,
     eh_try/eh_cleanup/eh_end/resume, and truthful unwind metadata
```

### Findings and bounded repairs

- The original lowering path had a per-function scan of every lifetime fact and
  its scope ancestry to decide whether `goto` must fail closed.  The repair
  adds `index_lifetime_facts()`, called once from `index_binding_facts()`.  It
  builds a dense `ScopeId`-indexed byte flag after walking each lifetime's
  ancestry once.  `lower_function` validates its typed FunctionFact scope and
  performs one O(1) flag lookup; any nontrivial lifetime in that function still
  conservatively blocks `goto`.
- The one-time lifetime index enforces exact typed continuity: the object's
  `Binding.type` equals `LifetimeFact.object_type`; the object is a variable
  owned by the fact's scope; the array/object type resolves to a class record;
  the fact destructor equals `model_.destructor_binding(record)`; and
  `checked_destructor_function` validates the complete destructor FunctionFact
  and action range.  Every scope ID in the ancestry is range-checked, the walk
  is bounded by the total scope count, it must reach a Function scope with a
  valid non-self parent, and malformed or cyclic ancestry fails closed.
  Duplicate lifetime bindings and declaration lifetime ranges are rejected.
- The index's ancestry walk is now in `pa15_lowering_construction.cpp`, keeping
  the affected `pa15_lowering.cpp` under the 3000-line file-audit limit.  This
  is a source-ownership correction, not a behavior change: indexing remains
  once per completed semantic model, with O(S) dense flags and O(L log L) map
  publication for `L` lifetime facts and `S` scopes, plus bounded ancestry
  work `O(sum depth) <= O(L*S)`.
- Constructor/destructor actions remain canonical typed ranges.  PA12 publishes
  base-first and declaration-order member construction, reverse member/base
  destruction, and recursive array actions.  PA15 validates member owner
  bounds and base record identities before lowering an action, rechecks the
  active destructor FunctionFact, and uses typed demand worklists without
  textual recovery.
- Array element paths validate the bound and checked `ordinal * type_size(child)`
  offset before converting the index.  Completed elements retain a typed root
  and path; cleanup recomputes their addresses, so arena growth or later
  LowIR emission cannot invalidate a saved temporary.  The shared prefix chain
  materializes each completed element once and emits one reverse destructor
  call per chain node before transferring to its predecessor and finally
  `resume`.
- Automatic lifetimes activate only after initialization.  Lexical scope
  markers, unbraced substatement cleanup, branch-state restoration, loop
  condition/iteration joins, for-init normal exit, switch-arm recovery, return,
  fallthrough, break, and continue all preserve only the active typed suffix;
  unsupported `goto` remains fail closed.  Destructor-body early return still
  emits remaining base destruction.
- The affected implementation is deterministic and bounded: typed fact/action
  ranges are snapshotted before recursive demand can grow arenas, demand scans
  reachable typed facts once, lowering performs bounded path/layout checks, and
  no reference binary, host compiler, whole-scope retry, or test-specific
  output shortcut is used.

## Focused Evidence

`make -C dev cppgm++` exited `0`; durable log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-build.log`.
Syntax checks and focused course controls 408, 409, and 410 all exited `0`;
the durable focused log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-focused.log`.
Course 410 specifically verifies activation before later declarations,
unbraced if/while/for statement cleanup, for-init normal exit, destructor
early-return base cleanup, loop and switch join state, branch exits, nested
array reverse addressing, and the E=8/16/32 structural scale controls.  Its
exact output was:

```text
PA16 structural flat E=8 cleanup_calls=7 main_lines=129
PA16 structural flat E=16 cleanup_calls=15 main_lines=257
PA16 structural flat E=32 cleanup_calls=31 main_lines=513
```

The full run still reports the four affected-path handout comparison
identities `200-destructor-body-local-before-base-destruction.t`,
`200-local-default-class-array-lifecycle.t`,
`200-member-object-lifetime.t`, and
`300-synthesized-array-member-lifecycle.t` in the unchanged baseline failure
map.  Their checked-in fixtures and references were not changed; no current
pass or failure claim is inferred from a reference-only shape difference.

The exact prior gate command (`n=16` followed by
`make test-report-through-pa15`) exited `0` at `1167/1167`; durable log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-through-pa15.log`.
The required file audit exited `0` and reported five existing
header-division warnings; durable log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-file-audit.log`.
The warnings are `abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
`pa11_semantic_model.h`, and `pa15_lowering.h`; there were no fatal issues.
`git diff --check` exited `0`; durable log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-diff-check.log`.

### Performance Evidence

Course 410's E=8/16/32 cleanup calls are exactly `E-1`, with main-line deltas
`128` and `256`; its nested `[2][3]` control verifies six reverse destructor
calls, outer strides `1,0`, and inner indices `2,1,0,2,1,0`.  These are
structural scale controls, not a timing claim.

The refreshed smoke/scale run used the immutable `0555` executable
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-perf/cppgm++-immutable`
with SHA-256
`be89e2a8efdc723f3e2947f8df48bf00b72333f52750f61dddbc6bd61539ad14`.
For each E, the same generated input was used for five interleaved batches of
20 compiler invocations; `/usr/bin/time` measured the batch and the reported
values are medians and ranges across the five batches.  The complete output,
including current input/output hashes, is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-performance.log`.

| E | wall median (range) | user median (range) | system median (range) | RSS median (range) |
| --- | --- | --- | --- | --- |
| 32 | `0.09s (0.09..0.09)` | `0.04s (0.04..0.04)` | `0.05s (0.04..0.05)` | `6516 (6448..6564) KiB` |
| 128 | `0.18s (0.18..0.19)` | `0.09s (0.09..0.10)` | `0.09s (0.08..0.09)` | `8500 (8496..8628) KiB` |

The input hashes are E=32 `f37786713c510300b1a9e5884285f7ae4ae7e16a5c1337616a087aac9bf79e54`
and E=128 `c2a66edb4b0088e64e49a70b57dda93c935a0469a853c2a621de72fcc9422c0f`.
Final output hashes are E=32
`67b17d7e3f7b2a3507dd795ed9cd05285dc1050c1eec600d15f92b70a6b16d0b` and
E=128 `cc0554ce1ed562f67be832da79110737001cbf9b96aa40c406960803c3e96399`.
The outputs have main-line counts `513` and `2049`, cleanup nodes/calls
`31/31` and `127/127`; the fourfold element increase gives fourfold main-line
growth and cleanup calls remain `E-1`.  These are representative smoke/scale
measurements, not a benchmark comparison or an allocation claim.

### Next Implementation Checkpoint

PA16 is not complete.  The next implementation checkpoint remains within
PA16: resolve the remaining local automatic/synthesized lifetime reference
shape and semantic cases after separately scoping unrelated PA16 failures.
Global/static/TLS lifetime, value transfer, virtual/multiple inheritance, and
the other exclusions above remain deferred; do not advance this path to PA17
on the unchanged full-stage map alone.

## Historical Static Member-Function Checkpoint Review

This review covers landed commit `021ef63927293f62e13a29b5b8265c7105fb35a9`
relative to parent `15e133af`, plus the bounded audit repairs and one focused
course regression in that checkpoint.  It is limited to typed
static member-function lookup and reachable emission: qualified and
unqualified calls, class/base hiding and overload filtering, access, canonical
owner/binding/type continuity, PA15 demand, declaration/definition emission,
recursion, and the raw static ABI.  Static data storage, constructors and
lifetime, operators/ADL, broad initialization, friends/using, and
multiple/virtual inheritance remain outside that review.

The authoritative turn-start full-stage state was `55/243` passed, `188`
failed, and `243/243` covered, from
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The final `make test-pa16` exited `2` at the same `55/243`, with `188`
failures and `243/243` coverage; sorted failure-identity comparison was exact,
with no additions or removals.

The affected ownership path was:

```text
PA10 qualified/unqualified IdExpression or parenthesized call
  -> PA12 typed qualifier/class-scope and MemberLookup selection
  -> first owning class ScopeId + canonical BindingId + raw function TypeId
  -> static-only candidates without an implicit object, or one mixed
     static/non-static member set when a non-static body supplies this
  -> access check at the selected owner; raw static or hidden-object fact
  -> PA15 namespace roots and reached static bodies walk typed call facts
  -> FunctionFactId identity chooses definition demand or declaration plan
  -> LowIR symbol uses the retained owner and raw source parameters
```

### Findings

- The landed helper correctly resolves a qualified class type through
  `lookup_type_path`, uses `member_lookup` so the first direct/base
  declaration set hides later bases.  With no implicit object, it filters that
  set to static functions; in a non-static member body, current/base-qualified
  and unqualified calls retain both static and non-static functions in one
  typed overload set.  PA12 call facts keep the selected raw callable `TypeId`
  for a static winner and the owner-qualified hidden-object type for a
  non-static winner; the inherited owner remains the selected `ScopeId`/
  `BindingId`.  PA15's `function_binding_fact_index_` then follows that same
  identity into a definition or declaration boundary, and
  `function_components`/`abi_function_symbol` retain the declaring owner.
- The mixed-set comparator follows N3485 §13.3.1 and §13.3.3: a static
  candidate's implicit-object ICS1 matches any object but establishes no
  conversion sequence, so it is neither better nor worse than another
  candidate on that dimension.  Object qualification is therefore compared
  only between two non-static candidates; explicit argument conversion ranks
  remain the common ranking criteria for static/non-static pairs.
- The audit found a fail-open class-qualified case: if the selected class name
  had only a non-static function, a type, a blocked name, or no static
  candidate, the old boolean result could reopen ordinary value lookup and
  manufacture a no-object call.  The repair separates “class-qualified name
  claimed” from the static candidate vector, unwraps parenthesized callees,
  and rejects the claimed spelling when no static target exists.  A valid
  current/base-qualified call in a non-static member body instead leaves the
  class claim to the unified member selector, so a viable static or non-static
  candidate can win without the first category suppressing the other.
- Static-body unqualified lookup now searches nearer block/function lexical
  declarations first, then the enclosing class's typed direct/base member set.
  An inherited static function therefore retains its base owner even when an
  outer namespace function has the same spelling; a direct non-static
  declaration still hides base declarations before static-only filtering in a
  static body.  In a non-static body, the first owning set supplies both
  categories to the same argument/object ranking.  The lookup is bounded by
  the scope vector and never reopens an outer value set after a class member
  has claimed the name.  Parenthesized callees use the same typed branches.
- The landed access gate covered qualified static candidates only.  The audit
  adds the same `member_accessible` check after selection for every
  class-owned static binding, including an unqualified static-body call.
  Protected inherited static access is granted by the derived access-class
  proof without a non-static object relation; private or unrelated access
  fails closed.
- The landed PA15 demand branch treated every indexed static `FunctionFact` as
  a definition.  Static declarations can have a valid fact with no body, so
  the repair validates binding/owner identity and distinguishes a complete
  function/body scope (definition demand) from a bodyless declaration
  (declaration demand).  Missing or contradictory definition facts, scopes,
  body ranges, owner records, and callable types fail closed.  Recursive
  static edges still use the existing visited function/fact worklists and are
  emitted once.  Before traversal, PA15 builds one dense
  `BindingId -> class ScopeId` index from class-scope-owned bindings; duplicate
  or out-of-range ownership fails closed, and each reached static fact checks
  that index in O(1) instead of scanning the owner's values.
- The added course regression covers parenthesized qualified and unqualified
  static calls, qualified non-static rejection, both directions of mixed
  static/non-static overload ranking in qualified and unqualified member
  bodies, tied-explicit-rank neutral-ICS ambiguity in each of those spellings,
  inherited static-body lookup against an outer same-spelled function,
  protected/private access, an inherited declaration-only static call,
  same-binding redeclaration identity, recursive demand deduplication, and
  raw parameter-only static ABI.  The existing handout matrix covers
  static/non-static filtering and qualified inherited owner retention.

## Focused Evidence

`make -C dev cppgm++` exits `0`.  The focused handout command

```sh
make -C pa16 check TEST='tests/general/100-static-member-qualified-call.t tests/general/100-static-member-overload-skips-nonstatic-this.t tests/general/200-inherited-static-member-qualified-call.t tests/general/200-static-nonstatic-same-pointer-signature.t tests/general/100-builtin-prefix-static-member-call.t tests/general/100-member-methods.t tests/general/200-protected-base-method.t'
```

exits `0` with `7/7` passing.  Course regressions 401--406 each exit `0`; 402's
`PA12 inherited member name is not callable` line is from its expected
negative case.  Course 406 independently rejects the qualified and
unqualified tied-explicit-rank mixed calls with `PA12 ambiguous member call`.
`sh -n` on the new script exits `0`.  No handout test, fixture, or `.ref` file
changed.

Five measured invocations of the new bounded regression after the neutral-ICS
repair (including its recursive/inherited static demand chain) took
`0.31--0.34s`, with RSS `7,064--7,268KB`; the seven-test handout probe took
`0.20s` and `9,824KB`.  These are representative smoke measurements, not a
formal benchmark.  The
new lookup performs only bounded lexical/class/base walks, and PA15 performs
one class-binding owner-index setup followed by O(1) selected-owner checks
and dense visited worklists; no whole-program retry, textual recovery, or
generated-artifact dependency was added.

The existing `pa16/tests/general/200-out-of-class-member-default-argument.t`
still fails in that tree before reaching the static declaration audit; that
pre-existing default-argument merge gap was not part of the landed static
ownership increment.  The focused declaration control consequently used a
bodyless static declaration without a default argument.  The required
through-PA15 command exited `0` at `1167/1167`; the PA16 file audit exited `0`
with five pre-existing header `bad-division` warnings.  `git diff --check`
passed before the final commit, and no handout test, fixture, or `.ref` file
changed.

## Historical Protected-Access Checkpoint Review

This review covers landed commit `8b445ee6e7b7090a1f2d19edebbc96d756f438ad`
relative to parent `9f158daa4cf5fd8326123ca9ccded1b4c59df382`, plus one
narrow source repair, one lexical-access repair, and the course regression
expansion in this audit.  It is limited to protected member access-scope
handling for field and method expressions: protected static object spelling
uses only the access-class/owner proof, while protected non-static members
also impose the object-expression rule.  Lookup, owner-path lowering,
constructors/lifetime, friends/using, operators/ADL, and other PA16 failures
are not re-audited here; PA15 is traced only where it consumes the affected
typed facts or reports the existing static projection boundary.
The bounded audit is complete and committed, and final validation leaves the
working tree clean.

The complete owned path is:

```text
PA10 MemberExpression/CallExpression or member-body IdExpression syntax
  -> PA12 typed actual-record lookup over class/direct-base scopes
  -> selected BindingId + owner ScopeId + typed base path
  -> actual object TypeId (this, dot object, or arrow pointee)
  -> member_accessible(binding, owner, access scope, object)
  -> semantic MemberExpression/CallExpression fact with selected owner
  -> PA15 typed owner/path validation and ordered base-subobject projections
  -> LowIR field address or non-static call with the hidden object pointer
```

### Findings

- The landed call-site changes carry the actual object into all three affected
  non-static paths: implicit member fields pass the typed `this` record,
  explicit fields pass the dot record or arrow pointee, and selected calls pass
  the normalized dot/arrow object.  The selected `BindingId` and owner
  `ScopeId` remain the semantic facts consumed downstream.
- `member_accessible` walks the bounded lexical scope chain and records each
  class scope from innermost to outermost.  For protected members it obtains
  each candidate's canonical named `TypeId` from the typed `TypeKey` index and
  accepts the first candidate that derives from the declaring owner with
  `member_base_path` and, for non-static members, also has the actual object
  type derived from that candidate.  This gives a nested class inside
  `Derived` the enclosing `Derived` access rights required by N3485 §11.7,
  while same-owner access still returns at the existing scope boundary and
  private/unrelated access remains rejected.
- For protected non-static members, the second proof strips the supported
  reference/cv layers from the actual object and requires a named record whose
  direct-base path contains the access class.  Thus a `Derived` or
  further-derived object is accepted in a `Derived` body, while a `Base&`,
  `const Base&`, or `const Base*` object is rejected.  The check is identity-
  based and does not render or recover a type name.
- The audit found one narrow exception in the landed helper: the new object
  proof was also applied to protected static members.  The repair returns
  after an eligible access-class/owner proof for a static binding, because
  C++'s additional object-expression restriction is only for non-static
  members.  The existing PA15 static-member projection boundary remains
  outside this checkpoint.
- The nested-class probe initially reached `PA12 record member is
  inaccessible` with only the innermost `Nested` class considered.  A
  constructor-free out-of-class `Derived::Nested` member definition reaches
  the same helper without widening PA15 nested-function emission; the lexical
  candidate walk then accepts its enclosing `Derived` access class for both a
  protected field and method through `Derived&`.  The corresponding `Base&`
  object reduction remains rejected by the second proof.  An inline nested
  call still encounters the pre-existing `PA15 direct call target was not
  emitted` boundary, so it is not used as a lowering claim here.
- The lexical walk is explicitly bounded by the scope-vector size and now
  requires an invalid cursor on exit.  A valid out-of-range cursor or a valid
  cursor left after cycle exhaustion fails closed before any collected class
  can grant protected access; ordinary invalid-parent termination and the
  same-owner return inside the walk are unchanged.
- Access is checked after member-call overload/cv selection, and field/call
  facts retain their selected binding and owner.  The existing semantic tail
  guard rolls back failed member-call probes; the new helper is const and its
  path walks use only local vectors, so failed accessibility does not publish
  a fact, demand edge, or fallback ordinary-name lookup.
- PA15 independently validates the selected actual-object-to-owner relation
  and complete zero-offset layouts before emitting each typed base-subobject
  projection.  This preserves the existing fact continuity from PA12 through
  the field/call LowIR consumers.

## Focused Evidence

`sh cppgm.tests/course/pa16/405-protected-object-access-regression.sh` exits
`0`.  Its positive source covers same-owner access, implicit and qualified
`this`, explicit dot and arrow on `Derived`, const-reference and pointer
normalization, dot and arrow on a further-derived object, and both field and
method paths.  It checks the expected typed projection counts and
`@Base__protected_method` calls.  Its constructor-free nested `Nested` member
source accepts both protected field and method access through `Derived&`; the
parallel nested `Base&` source returns `EXIT_FAILURE` with the exact PA12
inaccessible diagnostic.  Its separate ordinary field-through-`Base&` and
method-through-`const Base*` sources also return `EXIT_FAILURE`.

The checked-in protected positive control
`make -C pa16 check TEST='tests/general/200-protected-base-method.t'` exits
`0` with `1/1` passing.  The existing course controls
`401-typed-member-projection-boundary-regression.sh`,
`402-typed-member-call-demand-roots-regression.sh`,
`403-typed-inherited-member-field-regression.sh`, and
`404-typed-implicit-default-demand-regression.sh` each exit `0`; 402's
`PA12 inherited member name is not callable` line is the expected diagnostic
from its negative reduction.  No handout test, fixture, or `.ref` file was
changed.

The permanent course-405 protected-static object-spelling source returns
`EXIT_FAILURE` at the pre-existing `PA15 static member projection is
unsupported` boundary, rather than at `PA12 record member is inaccessible`;
this verifies that the access gate no longer imposes the non-static object rule
on static bindings without expanding the static lowering surface.

The turn-start authoritative log records full-stage PA16 at `49/243` passed,
`194` failures, and `243/243` covered.  The authorized final `make test-pa16`
also exits `2` at `49/243`, with `194` failure identities; sorted identity
comparison against the turn-start log gives added `∅` and removed `∅`, and
the `243`-test inventory remains fully covered (`243/243`).  The earlier
pre-increment history is preserved: the plan records the `48/243` to `49/243`
improvement from removing `pa16/tests/general/200-protected-base-method.t`.
The required through-PA15 command exits `0` at `1167/1167`.  The required
file audit exits `0` with the same five existing header `bad-division`
warnings; no handout fixture or `.ref` file changed.

## Performance and Boundaries

Protected access performs one bounded lexical scope walk of depth `S`, records
`L` class candidates, and performs at most two typed direct-base walks of depth
`D` per candidate; its worst-case check is `O(S + L*D)` (with the ordinary
non-nested case `L=1`).  Same-owner access returns before a base walk, and
protected static access can short-circuit after the owner proof.  Selection
remains bounded by the walked scope/inheritance depths and candidate set; the
new check adds no cache, whole-program retry, textual recovery, or mutation on
a failed probe.  PA15 performs one independent typed owner/layout check.

The representative temporary three-level state-free chain timing sample used
five compiler invocations per size with `/usr/bin/time`: for 1, 128, and 512
local declarations, maximum elapsed times were respectively `0.02s`, `0.01s`,
and `0.02s`; maximum RSS was approximately `5.4MB`, `6.1MB`, and `8.7MB`.
A separate temporary nested-access sample placed 256 protected-field
expressions in one out-of-class nested member and used lexical class depths
`L=1`, `8`, and `32`, with three invocations per depth; all exited `0`, with
maximum elapsed time `0.01s` and maximum RSS `7.4MB`.  These are small
bounded-behavior samples, not formal benchmarks or asymptotic timing claims.
Remaining uncertainties are the pre-existing static and inline-nested-call
lowering boundaries, and unrelated protected typedef/friend/using and
broader PA16 surfaces.

## Historical Previous Checkpoint Review

This review covers landed commit `b1a9e58959cb47835362a654283200831e7b99d6`
relative to parent `25e80541`, plus four narrow audit repairs included in
this checkpoint.  It is limited to direct and inherited unqualified
non-static member calls.  Inherited fields, qualified-base calls,
protected/friend/using access, operators/ADL, constructors/lifetime, virtual
or ref-qualified methods, and general conversion work remain outside it.

The owned path is:

```text
PA10 CallExpression(MemberExpression or unqualified IdExpression) syntax
  -> PA12 typed lexical/class/direct-base lookup and member selection
  -> exact Function-scope implicit-object BindingId as semantic child zero
  -> selected BindingId, owner ScopeId, callable Function TypeId, and args
  -> reachable FunctionFact demand edge
  -> PA15 ABI/owner validation and ordered base-subobject projections
  -> LowIR call with the owner pointer followed by explicit arguments
```

### Findings

- `semantic_call_expression` probes the typed member path before functional
  casts and ordinary direct lookup.  The probe unwraps supported
  parenthesized callees, accepts only a plain unqualified id for the new path,
  and never asks namespace lookup or ADL for member candidates.  Its lexical
  walk checks nearer block/function declaration sets, then the direct class
  and ordered direct-base declaration sets.  At every set the value graph is
  probed before the type graph, so a same-scope ordinary method hides a
  same-spelled class/enum tag.  A value-owned class/base set suppresses
  unrelated enclosing candidates.  A base-owned value set with no supported
  non-static method is blocked by its nonempty typed base path, while a
  `ValueRef` origin from an unsupported import returns explicit `Blocked` and
  cannot silently reopen outer value lookup; nearer lexical/direct-class values
  retain the ordinary resolver's existing fallback.  A using-view remains with
  the ordinary resolver.  A type-only first set returns an explicit typed `TypeId` outcome and is
  consumed by the existing functional-cast producer, so it cannot silently
  reopen outer value lookup.  The separate type probe uses a fresh lookup
  generation after the value probe.
- `member_function_candidates_in_scope` retains only ordinary non-static
  functions from the selected class scope.  The selected `BindingId` and
  `ValueRef` owner remain canonical; the callable `Function TypeId` is built
  with the selected owner and its cv-qualified hidden object pointer.  Object
  qualification is checked before the existing explicit-argument conversion,
  default, overload, access, and deleted-function logic.  Equal best choices
  remain ambiguous rather than depending on traversal order.
- `prepare_pa12_member_parameter` owns one synthetic first parameter and its
  exact `BindingId` in the member Function `Scope`.  The unqualified helper
  now passes the already-validated `BindingId` into `semantic_this_expression`,
  rather than resolving the enclosing `this` binding a second time.  Thus the
  successful call has one stable implicit-object fact at child zero, followed
  by converted/defaulted explicit arguments.
- Inherited fields remain deferred, but an inherited value declaration set is
  still owned by the first base scope that contains the spelling.  Empty
  non-static member candidates and unsupported imported-base value origins are
  therefore blocked by the typed base ownership signal and fail closed;
  ordinary outer/ADL lookup cannot be reopened.  A nearer lexical or
  direct-class value still reaches the existing ordinary resolver, preserving
  its direct/static behavior.
- `direct_base_chain` walks `NamedRecordId` edges with Floyd cycle detection.
  The audit repair validates every class-scope back-reference, rejects a
  non-invalid base on `has_base == false`, rejects virtual and union base
  metadata, and preserves the existing single-direct-base parser rejection.
  The semantic path is bounded by inheritance depth and is passed to the
  shared selector without a second semantic walk; same-owner conversion stays
  constant-time.
- A successful typed member call is the only member demand edge.  PA15 follows
  its selected binding through the existing `FunctionFactId` index, validates
  the selected class owner and hidden ABI, and plans declaration-only members
  from the typed callable boundary.  It scans reachable facts once with typed
  worklists; failed guarded probes publish no fact or demand edge.
- `lower_call` independently reconstructs the actual-object-to-owner path as
  a safety check.  For every edge it validates the current class relation and
  a complete `RecordLayout` whose direct base is the expected record at offset
  zero, then emits ordered `IPK_BASE_SUBOBJECT` projections.  Dot takes one
  address and arrow one pointer expression; free and indirect calls retain
  their existing lowering paths.

### Historical Focused Evidence

The six-test handout probe
`make -C pa16 check TEST='tests/general/200-inherited-member-call-hides-outer-type.t tests/general/200-implicit-member-call-suppresses-adl.t tests/general/200-member-call-implicit-this-cv-overload.t tests/general/200-local-class-direct-init-inherited-member-call.t tests/general/200-parenthesized-member-call.t tests/general/200-single-inheritance.t'`
exits `2` with `1/6` passing.  The inherited outer-type control passes.  The
five remaining failure identities are unchanged prerequisite blockers:
`200-implicit-member-call-suppresses-adl.t`,
`200-member-call-implicit-this-cv-overload.t`,
`200-local-class-direct-init-inherited-member-call.t`,
`200-parenthesized-member-call.t`, and `200-single-inheritance.t`.

The focused control
`make -C pa16 check TEST='tests/general/100-member-methods.t tests/general/200-inherited-member-call-hides-outer-type.t tests/general/200-member-call-return-type-overload-arity.t'`
exits `0` with `3/3` passing.  The existing course regressions
`400-typed-layout-boundary-regression.sh`,
`401-typed-member-projection-boundary-regression.sh`, and
`402-typed-member-call-demand-roots-regression.sh` each exit `0`.
The extended 402 script asserts typed LowIR ownership for a base tag/method
collision (zero-offset projection and `@Base__f`) and for direct and inherited
type-only first declaration sets (typed zero cast in `Derived__call`, with no
outer `@f` call).  It also rejects an inherited `Base::f` data member in
`Derived::call` and confirms that no outer `@f` call is emitted.  `sh -n` over
those scripts and `git diff --check` also exit `0`.

Bounded stdin reductions (not additional suite coverage) compile successfully
for direct, inherited, and parenthesized unqualified calls.  The inherited
and parenthesized outputs each contain one zero-offset
`projection=base_subobject` before the base call; a three-level reduction
contains two ordered projections before `@A__f`.  The new 402 reductions show
that a same-scope tag does not hide an ordinary base method, while direct and
inherited type-only declarations return the typed functional-cast zero rather
than calling the unrelated outer function.  The inherited non-callable-value
reduction fails closed rather than reaching the outer function.  A nearer block
variable named like
the method still fails at the local non-callable target and does not fall
through to the base method.

The required through-PA15 command
`n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
exits `0` with `1167/1167` passing.  The required file audit
`perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0` with
these five warnings: `dev/src/abi_mangle.h:1`,
`dev/src/cpp_semantic_core.h:1`, `dev/src/lowir_model.h:1`,
`dev/src/pa11_semantic_model.h:1`, and `dev/src/pa15_lowering.h:1`, each
`bad-division` for a substantial implementation body in a header.
`make test-pa16` exits `2` with `48/243` passing, `195` failures, and
`243/243` coverage.  Comparing the exact failure identities with the
turn-start map in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` gives
added set `∅` and removed set `∅`; all `195` identities are unchanged.  No
fixture, reference, or coverage identity was changed.

### Historical Performance and Boundaries

The lexical/type/value probes inspect only the walked scopes.  Base metadata
validation and lookup are bounded by inheritance depth; same-owner conversion
returns before a base walk.  For `C` candidates, `A` explicit arguments, and
`P` parameters, member viability/ranking remains approximately
`O(C * (P + A))`.  PA15's typed function/fact worklists scan each reachable
node once, and lowering performs one independent bounded path/layout check.
No whole-program retry or textual recovery was added.

No timing, RSS, allocation, or structural-counter measurement was collected;
these are structural bounds only.  Unsupported inherited type construction
and inherited non-callable value calls are fail-closed, while the focused
scalar type-only cases use the existing functional-cast producer.  The
remaining uncertainties are the unchanged
`195`-identity PA16 failure map and the explicitly deferred
protected/friend/using, inherited-field, qualified-base, static,
constructor/lifetime, virtual/ref-qualified, operator/ADL, and broader
conversion slices.

## Audit ledger

| checkpoint | result and disposition |
| --- | --- |
| `dea01c52` aggregate-initialization checkpointAudit | Completed bounded PA10--PA15 aggregate audit and repair: RecordLayout now owns declaration order/indexes, PA12 arena values survive reallocating publication, aggregate facts remain sparse and typed, global/runtime demand visitation is independent, pending global actions preserve source order, and the global aggregate inliner is checked and falls back to demanded helpers when unsupported. Final PA16 is `164/243` with `79` failures and `243/243` identities covered versus the authoritative `159/243` and `84` failures at turn start; the exact delta is five baseline-only repairs and final-only `∅`. The exact focus is `12/17` with `17/17` covered; course 404/409/412/415, through-PA15 `1167/1167`, file audit, and diff-check pass. Final structural replay is preserved in `pa16-aggregate-init-audit-final-v1` with `30/30` zero-status runs, zero repeated-hash mismatches, and no timing/RSS claim. The unknown-bound namespace string-record handout now passes. No handout, fixture, reference, comparator, harness, or source-set list changed. |
| `fb4348b6` typed parameterized class-constructor checkpointAudit | Complete: bounded PA10--PA15 constructor audit repaired canonical hidden-destination callable typing, protected-constructor access, shared candidate owner validation, and aggregate copy/direct-list dispatch for explicitly-defaulted/deleted constructors. The focused constructor matrix is `17/17`; course controls 400--409 pass with syntax checks, including new self-pointer/protected/private and aggregate field/helper coverage. The two aggregate handout controls retain the known LowIR address/bool shape comparison difference. Final PA16 is `91/243` with `152` failures and `243/243` coverage; failure and coverage identity additions/removals are both `∅`/`∅`. Through-PA15 is `1167/1167`; the file audit passes with five pre-existing warnings; diff-check passes; representative scale smoke is recorded above. No handout, fixture, reference, or `.ref` changed. |
| `32c45463` typed class-object construction checkpointAudit | Completed bounded audit of the landed typed construction increment relative to `a2ac5256`: repaired canonical empty named-class constructor identity, fail-closed FunctionFact ownership, value-initialization zeroing semantics, aggregate DMI fallback, typed range/owner/index validation, demand-driven empty-helper elision, and the course-404 ordering controls. Focused copied handout comparison is `10/11`; course controls 400--407 are green; final PA16 is `80/243` with `163` failures and `243/243` coverage, with exact failure and coverage additions/removals `∅`/`∅`; construction stress smoke is five successful `0.00s` runs with RSS `5824--6056KB` (timings in `/tmp/codex-pa16-stress-final.Tn9MSH/stress-1.time` through `stress-5.time`), 14 constructor helpers, 14 constructor calls, 13 base projections, 45 field projections, and 45 stores. Through-PA15 is `1167/1167`; the file audit passes with five pre-existing header-division warnings; no handout, fixture, reference, or `.ref` changed. |
| `2f130396` typed static-data storage/access checkpointAudit | Completed bounded audit/repair: canonical direct class-owner merging, inherited/nested typed owner retention, initializer-fact preservation, demand-aware class-static/TLS emission, access checks, exactly-once static object evaluation, and PA12 fail-closed class claims are traced and repaired. `make -C dev cppgm++`, course controls 400--407, the focused probe, and exact through-PA15 gate pass their bounded criteria; full PA16 is `61/243` with `182` failures and `243/243` coverage, with failure-identity additions/removals `∅`/`∅`. The file audit exits `0` with five pre-existing warnings; no handout or reference changed. |
| `021ef639` typed static member-function lookup/reachable-emission checkpointAudit | Completed bounded audit/repair: class-qualified lookup fails closed, current/base-qualified and unqualified member-body calls rank one mixed static/non-static set, static-body lookup preserves inherited owner/hiding, access and raw-vs-hidden-object facts remain typed, and PA15 uses a dense class-binding owner index with O(1) selected-owner checks. The focused handout matrix is `7/7`, course controls 401--406 exit `0`, final PA16 is `55/243` with the exact turn-start `188` failure identities and `243/243` coverage, through-PA15 is `1167/1167`, and the file audit passes with five pre-existing warnings. |
| `8b445ee6` protected object access scope checkpointAudit | Completed and committed bounded audit/repair: static protected object spelling stops after the typed access-class/owner proof, nested protected access considers eligible enclosing class scopes while retaining the non-static object proof, and malformed valid scope ancestry fails closed after the bounded walk. Course 405 covers the field/method matrix, nested `Derived&`/`Base&` controls, and the exact existing PA15 static boundary. Final PA16 is `49/243` with `194` failures and `243/243` coverage, with exact failure additions/removals `∅`/`∅`; through-PA15 is `1167/1167`, the file audit passes with five pre-existing warnings, and the final working tree is clean. |
| `b1a9e589` direct + inherited unqualified member-call checkpointAudit | Bounded audit completed with four narrow fixes: exact synthetic-`this` BindingId reuse, value-before-type lookup with explicit `Type`/`Blocked` outcomes, fail-closed direct-base metadata validation, and inherited value-set ownership blocking. Direct/inherited/parenthesized reductions, the three focused controls, and course regressions pass; the six-test handout probe remains `1/6` on the same five prerequisite identities. Through-PA15 is `1167/1167`, the file audit exits `0` with five pre-existing warnings, and full PA16 is `48/243` with `195` failures and `243/243` coverage, with zero failure-identity additions or removals. |
| `37265733` typed member projection audit/repair | Direct/nested dot and arrow ownership is traced through PA12, PA11 `RecordLayout::member_offsets` keyed by the object's canonical `NamedRecordId`, and PA15 LowIR; the reference-cv and class anonymous-injection defects are repaired. Broad validation and exact identity/coverage checks pass their bounded invariants; PA16 remains incomplete with the existing 205 failures. |
| `0b534f2f` typed direct member-call checkpointAudit | Completed bounded audit/repair: implicit-object cv subset ranking, N3485 variadic comparison, single-owner typed reachable member demand, dense PA15 reachability metadata, declaration-only member declarations with hidden-object/cv ABI boundaries, hidden-object call formation, and source-file sizing are repaired. Focused PA16/PA15 controls and all relevant course regressions pass; through-PA15 is `1167/1167`, the file audit passes with five pre-existing warnings, and full PA16 remains `47/243` with `196` failures and `243/243` coverage, with zero failure-identity additions or removals. |
| `0a6be82d` typed fixed-bound local/synthesized array lifetime checkpointAudit | Completed bounded audit/repair: typed lifetime ownership and destructor continuity are validated once, dense `ScopeId` flags replace the former per-function lifetime scan, checked array paths/actions and arena-safe recursive cleanup are retained, and lexical/control-exit/EH state is covered by course 410. Final PA16 is `93/243` with the exact turn-start `150` failure identities and `243/243` coverage; through-PA15 is `1167/1167`; the file audit passes with five existing warnings; diff-check passes; current structural and interleaved smoke/scale evidence is recorded above. |
| `2d93a5e9` ordinary non-template overloaded-operator checkpointAudit | Completed bounded audit/repair of the `20f14d30` -> `23a26df5` implementation span as tightened at `2d93a5e9`: the follow-up corrects exact friend-definition lexical ownership and typed private/protected/public base-reference accessibility while retaining enum identity/promotion ranking, narrow converting-constructor participation, reference/address facts, and typed bool boundaries through PA10--PA15. Final PA16 is `127/243` with `116` failures and `243/243` coverage; exact comparison to the `122/243` turn-start map has five baseline-only repaired identities and zero final-only identities. Through-PA15 is `1167/1167`, final file audit has five known warnings, focused status is `29/32` with three documented pre-existing holdouts, course 411 passes, and state-matched performance is in `pa16-operator-perf-followup-v5` with final/immutable SHA-256 `e5ffb4e9869c619552f193e16ef063ab2feba7c27f809887ebdd187960196580`. No handout, fixture, reference, comparator, or generated output changed. |
| `da4252b6` typed bit-field boundary checkpointAudit/follow-up | Completed bounded PA10--PA15 audit and repair: canonical typed operation/promotion facts, const-reference temporary ownership, semantic-owner rejection of invalid bit-field references and bool decrement, overload-before-address-of ordering, mixed/zero-width/unnamed/union layout, checked oversized allocation spans, masked signed/unsigned PA15 projection, and isolated initialization roots. Final PA16 is `131/243` with `112` failures and `243/243` identities; exact comparison to the turn-start `112`-failure map is baseline-only `0`, final-only `0`. Course 412, direct alias control, through-PA15 `1167/1167`, file audit, and diff-check pass; the focused bit-field matrix is `5/11` with six documented LowIR mismatches. Corrected state-matched bit-field performance is in `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-bitfield-perf-final-v1` with 30/30 zero-exit runs, 32-owner/544-declaration/832-use scaled counters, final/immutable SHA-256 `c98edbf143904e0b09b451310de38e7966149b4374ad912b55a1b9f8c96aaf02`, and final wall medians `0.00/0.07/0.00s` for small/large/nested cases. No handout, fixture, reference, comparator, or generated output changed. |
| `9718b987` member-function-definition declarator audit/follow-up | Final audit/follow-up: the out-of-contract special-member widening is reverted to the parent class-scope-only behavior, while explicit auto-placeholder state and typed/fail-closed ordinary declarator validation are complete. Final `make test-pa16` is `132/243` with `111` failures and `243/243` identities; the exact baseline/final failure sets are identical with baseline-only `∅` and final-only `∅`. Through-PA15 is `1167/1167`; course 413 passes, the focused matrix is `5/7`, the constructor-member-init control is `1/1`, file audit passes with five pre-existing warnings, and diff-check passes. The excluded nested out-of-class constructor fails closed and is not PA16 coverage; next is a later residual audit, not completion. |
| `4efddaae` typed single-inheritance standard-conversion checkpointAudit | Complete: typed endpoint, access-scope, and path ownership is retained from PA12 publication into PA15; the typed comparator enforces standard > `UserDefined` > `Ellipsis`, leaves user-defined/user-defined first-standard ranks incomparable, and preserves standard legacy plus derived distance/cv ordering. Member-object cv subset ordering, malformed-record bounds checks, final-fact scope-range validation, and the strengthened course-414 operator regression are repaired. Comparator bodies are owned by `pa12_semantic_calls.cpp` while declarations remain in `pa12_semantic_selection.h`, restoring the prior file-audit warning set. Final PA16 is `144/243` with `99` failures and `243/243` identities covered; exact comparison with the turn-start map has baseline-only `∅` and final-only `∅`. Focused conversion is `8/10`, access/rank/parser controls `7/9`, and PA15 conditional controls `2/2`; the residual identities are documented above. Through-PA15 is `1167/1167`; file audit exits `0` with five header-division warnings; diff-check exits `0`. Final-v3 immutable replay is 9 cases x 2 with 18 expected-hash matches and zero pair mismatches; frozen compiler SHA-256 is `5347a2abb876d9492501f70e6fa8fa9f6d3c27f2da0c35283f702d4a2652ab81`, current compiler SHA-256 is `d1352cd1c16bcd58587ee9ad201a56665819e671933db979c8df1aea6124c41b`. |
