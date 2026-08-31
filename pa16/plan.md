# PA16 final typed source-to-LowIR architecture audit

## Objective and authority

Close PA16's full typed ownership path from the PA10 AST through PA11/PA12
facts, PA15 LowIR, and the PA13 validator, while preserving one forward
production model and the PA16 boundary in `pa16/README.md` and `spec.md`.
The audited committed implementation baseline/checkpoint parent is
`306078539b51b5ae2be2fa3d31ad0c403e5668f2`, whose parent is
`ff83cef40a28a6c01aa1a7d9eafc0477e6aa7489`.  The final audit and compact
durable stage ledger are in `pa16/audit.md`.

Fresh final validation reports `make test-report-through-pa16` exit `0`,
`1410/1410`, and all `16/16` stages.  The final source file audit exits `0`
with only the six established nonfatal header-division warnings recorded in
the final audit.

## Typed stage design

PA11 owns canonical `NamedRecordId`, `ScopeId`, `BindingId`, `TypeId`,
`RecordLayout`, layout states, lifetime facts, and `BitFieldFact` identities.
The bit-field fact keeps declared, storage, and operation types separate,
including signedness, width, masks, and offsets.

PA12 owns source-point lookup, access, overload/ADL selection, conversion
ranges, constructor/destructor/lifetime actions, and retained
`(ReturnStatement, FunctionFactId)` owner pairs.  Its canonical truth finalizer
accepts both body-bearing `FunctionDefinition` and `SpecialMemberDefinition`
nodes and resolves canonical definitions through `definition_by_binding_`.

PA15 consumes those facts through typed `LoweredValue`, `FunctionFact`,
`integer_i64`, record-layout offsets, selected bindings, callable types,
base-path ranges, symbols, and typed LowIR operands.  PA13 validates the same
typed LowIR model directly.  ABI names, symbol spellings, metadata, and
rendered output are terminal boundaries; they are never semantic lookup
inputs.

Automatic storage-root and action/member-array construction share one typed
`ArrayCleanupChain`.  It creates one persistent reverse cleanup node per
completed destructible element, recomputes each address in its own unwind
block, calls the canonical destructor once, and transfers to its predecessor.
The repaired user-destructor path uses a separate typed
`DestructorSuffixChain`: one persistent tail node per remaining action leaf
and one `EH_END`/`resume` terminal.  Both chains remove repeated full-prefix
regeneration and do not carry an SSA producer across an exception edge.

## Failure closure and boundaries

The parent authority had four residual public identities:

- `200-local-default-class-array-lifecycle.t`: reverse destruction and root
  reuse were absent from the checked oracle;
- `200-reference-indexed-pointer-member-access.t`: an `i32` index was used
  directly in an `i64` multiply;
- `400-signed-bit-field-read.t`;
- `400-signed-enum-bit-field-read.t`.

The audited baseline implementation and its five approved oracle changes
close those four identities.  The current repair also updates only the
`Holder` destructor function in
`300-synthesized-array-member-lifecycle.ref` to record the shared-tail EH
shape; its source and exit-status sidecar remain unchanged.  No tests,
sidecars, harnesses, comparators, or coverage identities were deleted or
renamed.  The PA16 corpus remains
`243/243/242` for source tests/status sidecars/LowIR references; the sole
missing LowIR reference is the intentional rejected protected-member typedef
case.

PA16 does not implement copy/move value transfer, virtual or multiple
inheritance, member pointers, templates, conversion operators, or other
PA17–PA19 features.  Those boundaries remain explicit and are not reopened by
the lifecycle repair.

## Architecture review checklist

The final audit records the full traces for:

1. layout/member projection: canonical record/layout state, direct-base offset
   zero, empty-base/alignment/pack behavior, member-offset indexes, typed
   access/base paths, and `IPK_FIELD`/`IPK_BASE_SUBOBJECT` lowering;
2. member/overloaded/ADL calls: source-point ordinary lookup, associated
   records/namespaces, hidden friends, deterministic candidate identities,
   access/cv/implicit-object/conversion selection, hidden object pointers,
   typed function symbols, and demand-driven body emission;
3. construction/destruction/lifetime: typed action and owner ranges, local and
   namespace roots, special-member body ownership, reverse order, one-time
   evaluation, EH edge address recomputation, and the persistent cleanup chain;
4. packed bit-fields: distinct storage/operation types, promotion,
   extraction/sign reconstruction, packed read-modify-write, and preserved
   neighbors;
5. global/internal identity and demand: unnamed-namespace ownership,
   internal linkage, special-member base entries, dense typed demand vectors,
   and no rendered-name reconstruction or eager helper sweep.

The source review found no duplicate PA16 production model, textual semantic
downgrade, whole-program retry, broad invalidation, incomplete hot-path cache
key, or host/reference/previous-compiler shellout.  Layout, lookup, ADL,
demand, and ordinary lowering are bounded by typed record/scope/candidate
work.  `scanned_functions` and `scanned_runtime_facts` deduplicate their
respective domains; global-root facts are deduplicated once per explicit root
mode.  The final array-constructor and destructor-suffix cleanup paths are
`O(ND)` for fixed-rank typed paths, with one typed cleanup node per applicable
element and one shared terminal for destructor suffixes.

## Structural evidence

Course 410 (automatic roots) reports:

```text
E=8:  cleanup_calls=7,  main_lines=143
E=16: cleanup_calls=15, main_lines=287
E=32: cleanup_calls=31, main_lines=575
line deltas: 144, 288
```

Course 434 (action/member roots) reports:

```text
E=8:  cleanup_nodes=7,  cleanup_calls=7,  Holder_lines=141
E=16: cleanup_nodes=15, cleanup_calls=15, Holder_lines=285
E=32: cleanup_nodes=31, cleanup_calls=31, Holder_lines=573
line deltas: 144, 288
```

Course 435 (user-destructor suffixes) reports:

```text
E=8:  suffix_nodes=7,  suffix_calls=7,  Holder_lines=191
E=16: suffix_nodes=15, suffix_calls=15, Holder_lines=375
E=32: suffix_nodes=31, suffix_calls=31, Holder_lines=743
line deltas: 184, 368
```

These are structural generated-LowIR measurements, not timing, RSS,
allocation, or throughput claims.  All three probes check reverse order and
one destructor call per cleanup node where applicable.

## Final disposition

The final audit has the following focused evidence:

- `make -C dev cppgm++ CXX=g++` exits `0`;
- course 410 exits `0`;
- course 434 exits `0`;
- course 435 exits `0`; shared suffix nodes/calls are linear and the
  reverse chain/terminal invariants pass;
- the five changed public targets plus the two lifetime controls pass `7/7`;
- the single changed public fixture is source-preserving and its sidecar is
  unchanged;
- the final source review and `git diff --check` have no new fatal finding.

The final audit records the fresh broad through-PA16 result and source file
audit:

```text
perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src
make test-report-through-pa16
git status --short       # empty only after the authorized commit
```

The next stage may rely on the typed PA16 facts and LowIR contract here; it
must not treat this plan or `pa16/audit.md` as a second semantic model.
