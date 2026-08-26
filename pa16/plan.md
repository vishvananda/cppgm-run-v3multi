# PA16 implementation plan

## Current checkpoint

The target is `pa16` full-stage, phase `checkpointAudit`. The landed
increment is `453d03a6` (`PA16: typed class-layout checkpoint`), parent
`31f2d2af`. This checkpoint covers only its typed record-layout, complete-type,
and PA15 class-storage path; PA16 is not complete.

The production path is one forward flow:

```text
PA10 AST -> PA11 typed model -> PA12 semantic facts -> PA15 LowIR lowering
```

`NamedRecordId` is the stable owner key for one `RecordLayout`. PA11 records
declaration-owned class facts and completes ordinary natural layouts in
declaration order. Complete-type queries, `sizeof`, and PA15 object/global
storage consume that state; no source-text recovery, duplicate layout map, or
whole-program retry is used.

## Spec alignment and ownership

This slice follows the typed declaration/complete-type, natural layout, and
LowIR object-representation boundaries in the applicable PA16/spec contract.
It provides member offsets, natural padding/alignment, empty records, arrays,
pointers, self-pointers, and completed member classes. Methods, types, and
static data are skipped for non-static member layout.

| fact | owner and consumers |
| --- | --- |
| named record identity | PA11 `append_named_record` and its parallel `RecordLayout` |
| size/alignment/offsets | PA11 typed completion and `FlatIndex<BindingId, size_t>` |
| complete object type | PA15 `obj<bytesxalignment>` local/address lowering |
| namespace class object | PA15 narrow typed zero-storage summary, one zero region and existing init boundary |

The narrow owner fact is named `RecordLayout::checkpoint_zero_storage_eligible`;
it is not a claim of full C++ triviality. PA11 publishes it while completing
the record from typed default-member-initializer sidecars and already-completed
member summaries. PA15 reads it in O(1), unwrapping only cv/array wrappers.
Ordinary and static member functions do not invalidate it. References,
member-pointers, unsupported unions, bases, and virtual records are
conservative rejection cases.

The new non-template implementation is out of the header. The typed summary
and dependency fact remain in `pa11_semantic_core.cpp`; layout access,
completion, checked alignment, size, and alignment definitions are in the
existing affected `pa15_lowering_flow.cpp` to keep both sources within the
3000-line audit limit. This is one PA11 model and one state, not a second
layout implementation or translation unit.

Direct bases and virtual members are recorded as typed boundary facts but are
not flattened here. Their layout state becomes `Failed`, so complete-type,
`sizeof`, and storage consumers reject rather than inventing base bytes,
vpointers, or vtables. The broader PA16 inheritance/polymorphism and lifetime
owners remain later checkpoints.

## Fresh final evidence

`make -C dev cppgm++` exited 0. `make test-pa16` exited 2 with
`32 / 243` passing, `211` failing, and all `243/243` tests covered. This is
within the bounded checkpoint criterion. Failure identities were extracted
from the fresh log and compared with
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
both sets contain 211 identities, with no newly failing identity and no
identity removed from the failure set. The fresh residual categories are:

- 5 generated-LowIR mismatches:
  `general/200-unnamed-namespace-hidden-friend-single-definition`,
  `general/300-alignas-class-layout`,
  `general/300-enum-operator-adl-selects-matching-overload`,
  `general/300-packed-class-layout`, and
  `general/300-pragma-pack-followed-by-endif`;
- 206 exit-status mismatches: 203 expected-success/actual-failure and 3
  expected-failure/actual-success.

The supplied baseline category split was 6 LowIR and 205 status mismatches;
`general/300-alignas-derived-base-layout` remains in the same failure identity
set but is now a rejected-status case at this checkpoint boundary. No added
pass compensates for a new failure.

The requested through-PA15 command exited 0 with `1167 / 1167` passing. The
final file audit exited 0 with five warnings: historical substantial-header
implementation warnings for `abi_mangle.h`, `cpp_semantic_core.h`,
`lowir_model.h`, `pa11_semantic_model.h`, and `pa15_lowering.h`; no fatal
source-size issue remains. The durable regression exited 0, `sh -n` exited 0,
and `git diff --check` exited 0.

The durable regression checks statuses and the absence of fake zero/lifetime
output, never diagnostic text. It covers the nested polymorphic boundary,
direct-base boundary, ordinary/static-method global, DMI and destructor
rejections, completed member summary, `long double[2]` as `obj<32x16>`, and
the signed-range `sizeof` rejection.

## Structural limits and next checkpoint

Layout completion has bounded linear scans over one class binding vector and
checked `size_t` arithmetic. The eligibility summary is computed once per
completed record and recursively consumes typed member summaries; PA15 does
not rescan class binding DAGs per namespace object. No timing, RSS, or
benchmark evidence was collected, so no material performance number is
claimed. `sizeof` range rejection is reachable because LowIR uses signed
`INTEGER_I64` operands backed by `long long`.

The next checkpoint must own direct-base layout, polymorphic representation,
and constructor/destructor/initialization lifetime before this failed-state or
zero-storage boundary is broadened. Alignas/packing, inheritance lookup,
bit-fields, and other out-of-scope PA16 work remain deferred.

## Current audit ledger

| checkpoint | result and disposition |
| --- | --- |
| `453d03a6` typed class-layout checkpoint | Fresh final evidence: `32/243`, `211` residual failures, `243/243` covered; through-PA15 `1167/1167`; file audit exit 0 with 5 warnings; durable regression passed. Current bounded checkpoint; PA16 remains incomplete. |
