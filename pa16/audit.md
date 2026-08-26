# PA16 checkpoint audit

## Current Checkpoint Review

This review is bounded to the typed record-layout, complete-type, and PA15
class-storage increment landed in `453d03a6`. It does not claim completion of
PA16.

The ownership path is coherent:

```text
NamedRecordId creation
  -> one PA11 RecordLayout state/size/alignment/member-offset map
  -> typed complete-type and sizeof consumers
  -> PA15 obj<bytesxalignment>, local materialization, or namespace zero storage
```

PA11 gives each named record one stable layout slot with explicit
`Incomplete`, `Computing`, `Complete`, and `Failed` states. Class bindings are
visited in declaration order; functions, nested type declarations, and static
members do not become non-static layout members. Natural padding/alignment,
empty records, arrays, pointers, self-pointers, previously completed member
classes, and checked overflow are handled through typed `TypeId`/`NamedRecordId`
facts. Incomplete members, by-value cycles, and overflow poison the layout
state and do not trigger textual recovery or a retry guess.

The narrow namespace fact is
`RecordLayout::checkpoint_zero_storage_eligible`. PA11 computes it during
completion from typed DMI sidecars and completed member summaries. It is
explicitly narrower than full C++ triviality. PA15 performs an O(1) class
summary read, with only array-wrapper recursion by type depth. Ordinary and
static member methods are skipped and therefore do not block an ordinary
two-int global. A DMI is a typed rejection fact; no constructor/destructor
meaning is recovered from rendered names. Unsupported user-declared
destructor syntax remains an upstream PA11 rejection, which prevents fake
zero/no-op-lifetime output.

Virtual-member collection stops at nested class/enum declarations and other
declarative/executable boundaries, so a nested polymorphic class cannot mark
its enclosing ordinary record polymorphic. Direct bases and virtual records
are recorded and fail before natural layout; they are not silently flattened.
An outer record containing a pointer to such a nested record remains usable.
Unions, references, member-pointers, incomplete types, base/virtual records,
and unsupported lifetime cases remain conservative boundaries.

The non-template layout implementation was removed from the warned header.
The PA11-owned typed summary/dependency portions are in the existing
`pa11_semantic_core.cpp`; layout access/completion/checking definitions are in
the existing `pa15_lowering_flow.cpp` so the source-size audit remains clean.
The model, state, and identity are still single-owner; no new translation
unit or duplicate layout map was added.

## Fresh final evidence

- `make -C dev cppgm++`: exit 0.
- `make test-pa16`: exit 2, `32/243` passed, `211` failed, `243/243` covered.
  Compared with the supplied baseline log, the fresh and baseline failure
  sets are both 211 identities: no additions and no removals. Fresh residuals
  are 5 generated-LowIR mismatches and 206 status mismatches (203 expected
  success/actual failure, 3 expected failure/actual success). The five LowIR
  identities are `general/200-unnamed-namespace-hidden-friend-single-definition`,
  `general/300-alignas-class-layout`,
  `general/300-enum-operator-adl-selects-matching-overload`,
  `general/300-packed-class-layout`, and
  `general/300-pragma-pack-followed-by-endif`. The baseline's
  `general/300-alignas-derived-base-layout` moved from LowIR mismatch to the
  rejected-status category without changing identity membership.
- The requested through-PA15 command: exit 0, `1167/1167` passed.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: exit 0
  with five historical bad-division warnings for `abi_mangle.h`,
  `cpp_semantic_core.h`, `lowir_model.h`, `pa11_semantic_model.h`, and
  `pa15_lowering.h`; no fatal finding.
- `cppgm.tests/course/pa16/400-typed-layout-boundary-regression.sh`: exit 0;
  `sh -n` and `git diff --check`: exit 0. The script checks status and
  absence of fake zero/lifetime output, not diagnostic wording.

The focused boundary cases include nested-class virtuality, direct-base
rejection, ordinary/static methods on a two-int global, DMI/destructor
rejection, reuse of a completed member summary, f80 array alignment
(`obj<32x16>`), and the reachable signed LowIR `sizeof` range check.

## Architecture, performance, and limits

The structural performance evidence is limited but direct: one summary is
published while each record completes, and each namespace object consumes the
typed summary without a class-scope DAG walk. Layout remains bounded by the
class binding vector and wrapper/type depth. No timing, memory, or benchmark
measurement was taken, so none is claimed.

`sizeof` rejects values above `LLONG_MAX` because the actual LowIR size
operand is signed `INTEGER_I64` and `integer_operand` stores `long long`; the
maximum-array probe reaches this branch. The result is an implementation
boundary, not a speculative unreachable guard.

This checkpoint intentionally leaves inheritance, polymorphism, constructors,
destructors, aggregate/member initialization, alignas/packing, bit-fields,
lookup/ADL, and broader PA16 object lifetime to later owners. In particular,
the failed base/virtual boundary must be implemented before any flattening or
zero-storage eligibility claim is widened.

## Audit ledger

| checkpoint | current result | disposition |
| --- | --- | --- |
| `453d03a6` typed class-layout checkpoint | Fresh final gates satisfy the bounded criterion: `32/243`, `211` residual failures, `243/243` covered; through-PA15 `1167/1167`; audit exit 0 with 5 warnings; durable regression passed | Current audited checkpoint; PA16 remains incomplete |
