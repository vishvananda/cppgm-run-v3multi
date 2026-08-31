# PA16 final-stage architecture audit

## Final-stage record

This record supersedes the historical checkpoint narration that previously
occupied this file.  It audits the committed PA16 implementation baseline and
the completed typed lifecycle repair, retaining only durable decisions and
current final evidence.

The audited implementation baseline is:

- commit `306078539b51b5ae2be2fa3d31ad0c403e5668f2`,
  `PA16: close typed lifecycle LowIR checkpoint`;
- parent `ff83cef40a28a6c01aa1a7d9eafc0477e6aa7489`, the last typed
  bit-field checkpoint;
- PA16 implementation and regression history from `453d03a6` through
  `30607853`;
- PA16 handout contract in `pa16/README.md`, architecture contract in
  `spec.md`, and the durable inventory in `pa16/plan.md`.

The committed baseline was clean.  The final validation run reports
`make test-report-through-pa16` exit `0` with `1410/1410` tests and all sixteen
stages.  The final source file audit reports exit `0`; its only findings are
six established nonfatal
`bad-division` warnings in `abi_mangle.h`, `cpp_semantic_core.h`,
`lowir_model.h`, `pa11_semantic_model.h`, `pa12_semantic_selection.h`, and
`pa15_lowering.h`.

The final evidence below covers the changed lifecycle paths, affected handoff
controls, structural measurements, and the complete required gates.

## Final disposition

The final source review found and repaired the destructor-suffix complexity
blocker; no unrepaired semantic, identity, cache, retry, or production
pipeline blocker remains.  The final lifecycle implementation is a typed
repair, not a textual workaround:

- PA12's canonical truth finalizer treats both `FunctionDefinition` and
  `SpecialMemberDefinition` as body-bearing owners and resolves a retained
  return owner through the canonical `BindingId` index.
- PA15 replaces repeated full-prefix array-constructor cleanup generation with
  one persistent typed `ArrayCleanupChain` per construction invocation.  The
  chain is used by both automatic storage roots and action/member-array roots.
- PA15 recomputes each completed element address in its own unwind block,
  emits one destructor call there, and transfers to the preceding chain node.
  No address producer or source spelling crosses an exception edge.
- Course regression 434 records the action/member-array scaling contract.
- The five checked-in `.ref` changes already present in baseline `30607853` are legitimate
  public-contract corrections: reverse local-array destruction, typed signed
  widening for an indexed pointer, persistent synthesized-member cleanup, and
  signed bit-field reconstruction for both plain and enum-underlying reads.
- This repair changes only the `Holder` destructor function in
  `300-synthesized-array-member-lifecycle.ref` to record the shared-tail EH
  shape.  Its source and exit-status sidecar are unchanged.

The four residual identities at parent `ff83cef4` are therefore closed by
the current implementation/oracle pair:

1. `200-local-default-class-array-lifecycle.t` now has reverse destruction and
   typed root reuse.
2. `200-reference-indexed-pointer-member-access.t` now widens the typed
   `i32` index before the `i64` multiply.
3. `400-signed-bit-field-read.t` and
   `400-signed-enum-bit-field-read.t` now reconstruct the represented signed
   value after extraction.

## Specification alignment

### One forward production pipeline

The PA16 path remains the single production pipeline required by `spec.md`:

```text
PA10 parser/AST
    -> PA11 canonical scopes, bindings, types, records, layouts, and facts
    -> PA12 typed semantic facts, lookup, conversions, actions, and lifetimes
    -> PA15 typed LowIR operands, blocks, symbols, calls, and EH
    -> PA13 typed LowIR validation/consumer
    -> later backend
```

There is no parallel PA16 parser, semantic model, LowIR model, or backend.
The test runner's process control is test infrastructure; production compiler
lowering does not shell out to a host compiler, reference binary, previous
solution, assembler, or linker.

### Typed continuity and ownership

`NamedRecordId`, `ScopeId`, `BindingId`, `TypeId`, `FunctionFactId`,
`SemanticFactId`, `ProjectionId`, and typed ranges remain the semantic
identity path.  PA15 consumes those identities directly.  Strings and
`SpellingId` values are introduced only at actual ABI, symbol, metadata, or
rendering boundaries; rendered LowIR, mangled text, and source spelling are
not reparsed to recover facts.

Canonical equality and hot-path ownership are backed by dense IDs, direct
indexes, typed maps, and bounded vectors.  Lookup and demand code validates
the owner of every binding before consuming it.  Explicit `Incomplete`,
`Computing`, `Complete`, and `Failed` layout states, plus typed constructor,
destructor, and no-op cache states, prevent malformed cycles from becoming
unbounded work.

### PA16 boundary and out-of-scope separation

This audit covers the PA16 class/object, layout, member, overload, ADL,
conversion, lifetime, bit-field, global-identity, and typed LowIR boundary
specified by `pa16/README.md`.  Copy/move value transfer, virtual or multiple
inheritance, member pointers, templates, conversion operators, and the other
PA17/PA18/PA19 features remain outside this stage.  No PA17 or PA18 behavior
was added to close the PA16 lifecycle path.

## Representative typed ownership traces

### A. Layout and member projection

The checked ownership path is:

```text
PA10 class/member syntax
  -> PA11 NamedRecordId/BindingId and RecordLayout
  -> PA12 typed member lookup, access, and direct-base path facts
  -> PA15 validate_typed_base_path/lower_member_address
  -> typed IPK_BASE_SUBOBJECT/IPK_FIELD LowIR
  -> PA13 validation/consumer
```

`pa11_record_layout.cpp` owns layout completion and its explicit state.  It
validates the class scope and canonical record owner, lays out the direct base
at the PA16-required offset zero, handles empty-base and alignment/packing
rules, records member offsets by `BindingId`, and validates bit-field storage
units.  It does not retain a speculative transitive-base closure.  A complete
layout is the only source accepted by PA15 address projection.

`pa12_semantic_member.cpp` walks the typed owning class and its direct-base
chain, records access and source-point context, and publishes selected
bindings, callable types, base-path ranges, and member candidates in
deterministic order.  `pa15_lowering_member.cpp` checks the selected binding,
owner, complete layout, and every direct-base relation before emitting a
zero-offset base-subobject projection and the typed field projection.  Static
members discard the object at the semantic boundary and use typed global
storage demand.  Renderer and ABI metadata receive names only after these
facts have been established.

### B. Member, overloaded, and ADL calls

The checked path is:

```text
typed candidate identities + source-point ordinary lookup
  + associated records/namespaces and hidden-friend visibility
  -> access/cv/implicit-object/conversion ranking
  -> SemanticFact selected_binding/callable_type/base-path/conversion range
  -> PA15 demand walk and typed function declaration plan
  -> hidden object pointer + typed symbol/declaration/call operands
  -> PA13 LowIR validation
```

PA12's associated-type walk covers class, enum, cv/ref, pointer, array, and
function result/parameter associations, then applies enclosing and inline
namespace rules.  Ordinary lookup and ADL retain source-point and using
declaration context.  Candidate identity is `(ScopeId, BindingId)`; duplicate
bindings are rejected and candidate order is stable.  Member selection checks
the implicit object's cv qualification, base relation, access, deleted state,
default arguments, variadic boundary, and typed conversion scores.

PA15 `collect_demanded_functions` and `collect_function_declarations` use the
selected binding and its `FunctionFact`/sidecar.  `lower_call` validates the
callable signature, emits the hidden object pointer for non-static members,
projects inherited receivers through the retained typed base path, and uses
the planned typed function symbol.  Hidden friends and namespace definitions
are demanded only when reached by a typed call, address, reference, or global
root.  Unused implicit helpers do not receive an eager body sweep.  No
name-based reconstruction, broad retry, or rendered-LowIR lookup participates.

### C. Lifetime, construction, and destruction

The ownership path is:

```text
PA12 ConstructorActionFact/DestructorActionFact/LifetimeFact
  + retained (ReturnStatement, FunctionFactId) owner
  -> typed demand roots and constructor/destructor action ranges
  -> PA15 typed subobject/array address paths
  -> ArrayCleanupChain nodes for completed throwing construction
  -> typed LowIR initialization, EH, cleanup, and reverse destruction
```

PA12 retains the owner relation instead of reconstructing it from a return's
source text.  The canonical truth finalizer's body-owner predicate includes
both ordinary and special-member definition nodes; it requires a valid body
fact and resolves the binding through `definition_by_binding_` in O(1).  PA12
constructor and destructor facts retain typed record, binding, member/base,
argument, and contiguous action-range identities.

PA15 demand walks those ranges once, validates the selected constructor or
destructor function and hidden object signature, and seeds namespace, static,
automatic, local, and special-member base-entry roots from typed facts.  Local
scope activation and control-flow exits destroy only the lifetimes active at
that point; normal arrays and member/base actions are emitted in reverse
construction order.

For a throwing array constructor, `ArrayCleanupChain` creates one base-resume
block and one persistent node for each completed destructible element.  Each
node recomputes its own root and array-index path in that block, calls the
canonical destructor once, and jumps to its predecessor.  The current
constructor installs an `EH_TRY` only after the already-completed chain is
materialized.  Deferred elements recompute their address after that handler is
installed, so no cross-block SSA producer is used.  `materialized` is a typed
progress index; completed prefixes are never regenerated.

The repaired exception-safe destructor-body path materializes one typed
`DestructorSuffixChain` per active destructor action sequence.  For
`1 <= i < N`, its persistent `heads[i]` node recomputes the typed address for
`elements[i]`, calls that one destructor, and jumps to `heads[i + 1]`; the
single `heads[N]` terminal emits `EH_END` and `resume`.  The normal destructor
at index `i` installs a handler for `heads[i + 1]`, so a throw invokes exactly
the remaining suffix in reverse-destruction order.  Tail nodes have no nested
handler, preserving the existing behavior if a cleanup destructor throws.
Each address is recomputed in its own block from the retained typed
action/path/record identity.  For path depth `D`, the repaired exceptional
suffix is `O(ND)` LowIR work with one cleanup call per node.

### D. Packed bit-fields

The checked path is:

```text
PA10 declaration syntax
  -> PA11 BitFieldFact declared/storage/operation types, signedness, width,
     masks, storage offset, and owning BindingId
  -> PA12 promotion and conversion facts
  -> PA15 typed extraction/sign reconstruction/update encoding/packed RMW
  -> typed comparison boundary and PA13 consumer
```

Storage type and operation type are intentionally distinct.  PA15 uses the
storage type for the packed unit, offset, mask, and neighboring-bit
preservation, and the operation type for promotion, conversion, comparison,
and signed reconstruction.  Prefix/postfix and initialization paths retain a
single typed target evaluation.  The checked equality carrier exception is
narrow and directional; unrelated width, signedness, relational, or reverse
operand mismatches remain rejected by the PA13 boundary.

### E. Global/internal identity and emission demand

Global and special-member ownership remains typed from `ScopeId`/`BindingId`
and `FunctionFact` through PA15's dense demand vectors, symbol tables, and
metadata.  Unnamed namespaces retain their owning scope identity and internal
linkage.  Constructor/destructor base-entry symbols are tied to the canonical
special-member binding and record, not to a rendered name.  Static data,
thread-local and namespace lifecycle roots are walked from typed variable and
semantic facts.  A global aggregate fast path can suppress a runtime body only
after the typed fact proves that the data path is sufficient; TLS and internal
special-member demand remain explicit.

The demand walk deduplicates function, semantic-fact, global-fact-mode, and
no-op work in dense/indexed structures.  `scanned_functions` visits each
function fact at most once; `scanned_runtime_facts` visits a reachable runtime
semantic fact at most once; and the global-root walk permits at most one visit
per explicit global mode.  These are separate typed domains, so the claim is
deduplicated work within each domain, not a claim that every fact is globally
visited exactly once across all root modes.  The walk does not retry the whole
program, invalidate a broad cache, scan rendered symbols, or emit every helper
body.
ABI spelling and metadata are generated only after typed ownership has been
validated.

## Complexity, measurements, and limits

The source review establishes these structural bounds:

| path | current bound and invariant |
| --- | --- |
| layout | One validated record state at a time; member work is proportional to the record's members, with a bounded direct-base dependency walk and no speculative transitive closure. |
| lookup/ADL | Scope/base walks are bounded by typed scope/inheritance depth; candidate ranking is approximately `O(C * (A + P))` for `C` candidates, `A` arguments, and `P` parameters, with typed deduplication. |
| demand | Function facts and runtime semantic facts are deduplicated by their respective seen vectors; global roots are deduplicated per explicit root mode. Dense demand/declaration vectors and typed maps avoid broad retry and eager helper emission. |
| ordinary lowering | Each member/base/conversion path is validated once and each array element address is rebuilt from typed root/path data; no source or LowIR text is inspected. |
| array constructor failure cleanup | One persistent chain node per completed destructible element; for fixed path depth `D`, `N` elements require `O(ND)` chain/address work and one cleanup call per node. |
| destructor-body exceptional suffix | One persistent typed tail node per remaining element plus one `EH_END`/`resume` terminal; `O(ND)` for `N` leaves and path depth `D`, with no nested cleanup handler in a tail node. |

No timing, RSS, allocation, or speedup claim is made.  The fresh structural
measurements are:

```text
course 410 automatic arrays:
  E=8   cleanup_calls=7   main_lines=143
  E=16  cleanup_calls=15  main_lines=287
  E=32  cleanup_calls=31  main_lines=575
  line deltas: 144, 288

course 434 action/member arrays:
  E=8   cleanup_nodes=7  cleanup_calls=7   Holder_lines=141
  E=16  cleanup_nodes=15 cleanup_calls=15  Holder_lines=285
  E=32  cleanup_nodes=31 cleanup_calls=31  Holder_lines=573
  line deltas: 144, 288

course 435 user-destructor suffixes:
  E=8   suffix_nodes=7  suffix_calls=7   Holder_lines=191
  E=16  suffix_nodes=15 suffix_calls=15  Holder_lines=375
  E=32  suffix_nodes=31 suffix_calls=31  Holder_lines=743
  line deltas: 184, 368
```

These are generated-LowIR structural counters, not controlled compiler timing
or memory measurements.  They show fixed-rank linear growth, one call per
cleanup node, a single suffix terminal, and reverse order for all three
construction/destruction roots.

## Findings, artifacts, and validation

### Findings and repairs

1. The prior top-level audit was stale: it described `177b845f` relative to
   `ab4fa405` and a `239/243` authority even though the audited baseline is
   `30607853` and the final gate is `1410/1410`.  This document
   repairs that record and keeps only durable decisions and compact final
   facts.
2. The source review found and repaired the real PA15 suffix blocker: every
   potentially throwing destructor no longer receives a freshly emitted full
   remaining suffix.  The typed shared-tail helper preserves normal order,
   exact remaining-suffix cleanup, address recomputation, one terminal, and
   cleanup-destructor throw propagation.
3. The lifecycle source changes remain in the established PA15 header and
   construction owner; course 435 is the earliest public structural
   regression.  No new source file or source-set repair is needed.
4. The five baseline fixture changes were audited against the typed producer
   and PA13 contract.  They preserve source, sidecar, comparator, and coverage
   identities.  This repair changes exactly one existing fixture, only its
   `Holder` destructor function, because the public LowIR EH shape now
   legitimately records the shared typed tails.  The fixture was regenerated
   through the documented `pa16 ref-test` target after reading the reference
   policy; no unrelated generated output was retained.

### Final validation gates

The following results are fresh final validation from this completed audit:

```text
perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src
  exit 0; six established nonfatal header-division warnings
make test-report-through-pa16
  exit 0; ALL TESTS PASSED SUCCESSFULLY! (1410 / 1410); 16/16 stages
```

The PA16 corpus remains `243` `.t` files, `243` matching
`.ref.exit_status` sidecars, and `242` `.ref` LowIR fixtures.  The only absent
LowIR fixture is the intentional rejected
`200-protected-member-typedef-access-bad.t`, whose sidecar is
`EXIT_FAILURE`.  The stage has `36` course scripts, with the course symlink
view containing the same set.  No test, sidecar, harness, comparator, or
coverage path was deleted or renamed.

### Focused evidence

The following commands were run against the completed repair:

```text
make -C dev cppgm++ CXX=g++                                      exit 0
sh cppgm.tests/course/pa16/410-typed-lifetime-activation-control-exit-regression.sh
  PASS; counters shown above
sh cppgm.tests/course/pa16/434-typed-action-array-cleanup-linearity-regression.sh
  PASS; counters shown above
sh cppgm.tests/course/pa16/435-typed-destructor-suffix-linearity-regression.sh
  PASS; suffix counters and reverse chain checks shown below
make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST="tests/general/200-local-default-class-array-lifecycle.t tests/general/200-reference-indexed-pointer-member-access.t tests/general/300-synthesized-array-member-lifecycle.t tests/general/400-signed-bit-field-read.t tests/general/400-signed-enum-bit-field-read.t tests/general/200-member-object-lifetime.t tests/general/200-destructor-body-local-before-base-destruction.t"
  PASS (7/7)
perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src
  PASS; six established nonfatal header-division warnings
sh -n cppgm.tests/course/pa16/410-typed-lifetime-activation-control-exit-regression.sh cppgm.tests/course/pa16/434-typed-action-array-cleanup-linearity-regression.sh cppgm.tests/course/pa16/435-typed-destructor-suffix-linearity-regression.sh
  PASS
git diff --check
  PASS
```

The seven-target check covers the five changed public LowIR fixtures plus
`200-member-object-lifetime.t` and
`200-destructor-body-local-before-base-destruction.t`.  The focused checks
therefore cover reverse lifetime order, signed extraction, typed index
widening, synthesized member-array cleanup, the special-member body-owner
path, and the repaired user-destructor suffix; the required broad through-stage
gate also passed.

### Independent final confirmation and committed evidence

Independent supervisor validation confirms:

- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: exit `0`,
  with the same six established nonfatal header-division warnings;
- `make test-report-through-pa16`: exit `0`,
  `ALL TESTS PASSED SUCCESSFULLY! (1410 / 1410)`, stages `pa1`–`pa16`;
- courses 410, 434, and 435 retain the structural counters and deltas above;
- `git diff --check` and `git fsck --no-dangling --no-progress`: exit `0`;
- post-commit `git status --short`: exit `0`, empty.

The implementation/audit commit being evidenced is
`51a39a0ae942813d5ca30a8ab25544daf165012c`.  `git show --check --stat`
passes, and that commit contains exactly these six paths:

```text
dev/src/pa15_lowering.h
dev/src/pa15_lowering_construction.cpp
cppgm.tests/course/pa16/435-typed-destructor-suffix-linearity-regression.sh
pa16/tests/general/300-synthesized-array-member-lifecycle.ref
pa16/plan.md
pa16/audit.md
```

## Durable stage ledger

The following compact ledger preserves the decisions that should not be
rediscovered from the old narration:

| stage history | durable result |
| --- | --- |
| `453d03a6` through `5f8983b6` | Canonical class records and explicit record-layout ownership; direct-base offset zero and typed alignment/packing rules. |
| `37265733` through `d0470e94` | Typed member/base projections and member-call selection; hidden object and base-path facts are retained into PA15. |
| `9f158daa` through `a2ac5256` | Inherited member/default demand and static data/function ownership use typed roots; static object expressions do not affect static selection. |
| `32c45463` through `5d91986f` | Class-object construction, constructor overload/context boundaries, and typed constructor actions are owned by PA12 and consumed by PA15. |
| `0a6be82d` through `3b2b4882` | Automatic lifetime activation, reverse destruction, and typed destructor suffix ownership; local control-flow exits retain only active lifetimes. |
| `da4252b6` through `727417db` | Bit-field storage/operation separation, signed reconstruction, typed update encoding, and packed-neighbor preservation. |
| `4efddaae` through `d889058c` | Typed derived-base conversions, aggregate initialization, value initialization, and no-op cache/state boundaries. |
| `ab1b2a8c` through `4a5bbdd5` | Source-point ordinary lookup, recursive associated ADL, inline namespaces, using declarations, and cooked-string call ownership. |
| `dff21435` through `8c60e658` | Internal/unnamed-namespace special-member identity and hidden-friend definition demand; no eager helper sweep. |
| `71a40cfd` through `177b845f` | Typed conversion ranges and the narrow PA13 comparison carrier boundary; broad textual or same-width fallback is rejected. |
| `ee8f44d5` | Superseded per-throw constructor cleanup that regenerated a full completed prefix; retained as a rejected design, not current behavior. |
| `30607853` | Audited implementation baseline/checkpoint parent: `SpecialMemberDefinition` owner coverage, persistent `ArrayCleanupChain`, course 434 scaling regression, and five legitimate oracle repairs. |
| final lifecycle repair | `DestructorSuffixChain` in the PA15 construction owner, course 435 at `8/16/32`, and the single minimized synthesized-member destructor fixture update establish shared-tail typed suffix ownership. |

Durable rejected alternatives are: recovering semantic facts from rendered
LowIR or mangled names; retaining transitive base closures instead of direct
typed paths; whole-program retry or broad cache invalidation; eager emission
of unused implicit helpers; conflating bit-field storage and operation types;
and carrying an SSA address producer across an EH edge.  The final source
review found none of those designs in the current PA16 path.

## Final boundaries

- The six header-division warnings are established nonfatal file-architecture
  warnings and remain documented; no new warning was found.
- The structural counters above do not measure wall time, RSS, allocations, or
  compiler throughput.  No such claim is made.
- PA17/PA18/PA19 features remain intentionally unimplemented and are outside
  the PA16 implementation boundary.

This final audit covers the implementation, regression, oracle, and
documentation repair.  The implementation/audit commit and clean-tree
results are recorded above.
