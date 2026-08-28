# PA16 implementation plan

## Stage Design

This increment implements the ordinary PA16 bit-field boundary as one typed
pipeline. PA10 already preserves `BitFieldDeclaration` and
`BitFieldDeclarator`; PA11 now validates the integral/enum declaration and
constant width, creates one canonical `BitFieldFact` per named field, and
records ordinary and unnamed bit-field events in a vector owned by each
`NamedRecordId`. The fact carries owner scope/record, binding, declared and
effective storage types, width, value width, unit size/offset, bit offset,
masks, and effective signedness. Layout consumes that owner-stable stream in
declaration order, packs adjacent fields into addressable units, advances on
unnamed zero-width fields, and leaves the existing named anonymous aggregate
owner paths intact.

PA12 keeps the selected bit-field binding on member glvalues through
member access, assignment, and built-in prefix/postfix increment; built-in
address-of rejects the projection. Aggregate and constructor initialization
uses the same typed binding and layout facts. PA15 projects the storage unit
for reads and masked updates, including sign extension for explicitly signed
integral and signed-underlying enum fields and zero extension for unsigned
underlying enums. No lowering decision recovers a width, owner, mask, or
signedness from rendered text.

The per-record event vector and the binding-owner index make nested class
definitions deterministic and prevent an inner declaration from entering an
outer record's layout stream. Ordinary members share one append predicate for
both event recording and alignment metadata; layout also checks the filtered
ordinary binding order for omissions or duplicates. This checkpoint does not
add class-by-value transfer, copy/move, virtual/multiple inheritance, or
general conversion semantics, and it preserves the earlier operator-boundary
work recorded in the ledger below.

## Failure Map

The original implementation baseline, before the `23a26df5` operator landing,
was `93/243` passed and `150` failed.  The audit turn-start baseline after the
landed implementation and `2d93a5e9` tightening was `122/243` passed and
`121` failed, with all `243/243` identities covered; its authoritative log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The final audit run is `127/243` passed, `116` failed, and `243/243` covered;
it exits `2` because PA16 still has failures.  The durable final log and exact
identity comparison are:

```text
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-followup-test-pa16.log
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-followup-identity-compare.log
```

The comparison has five baseline-only repaired identities:

```text
pa16/tests/general/200-inherited-member-overload-set.t
pa16/tests/general/300-basic-operator-overloads.t
pa16/tests/general/300-enum-operator-adl-selects-matching-overload.t
pa16/tests/general/300-hidden-friend-operator-nullptr-compare.t
pa16/tests/general/300-stream-shift-selection-chain.t
```

Final-only is `0`; the failure count is therefore no greater than the audit
turn-start `121`, and coverage remains exactly `243`.  The final direct
focused matrix is `29/32` in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-followup-focused-final.log`:
the original 29-row operator matrix is `28/29`, and the three mismatches in
the extended matrix are the pre-existing `nullptr_t`, private-base static-cast
member, and inherited-protected-field friend controls.  Course 411 passes
with the exact lexical-owner/access-friend and public/private/protected
further-derived cases.  The semantic rows cover member/nonmember ranking,
enum identity/ADL and nested friend lookup, friend visibility/redeclaration,
derived/base references, reference-result chaining, fallback, logical
operators, and shift/string chains.

The bit-field checkpoint started from the required `127/243` passed,
`116` failed, and `243/243` covered PA16 baseline. The checked-in focused
matrix was rerun after the final build with:

```text
make -C pa16 check TEST='tests/general/300-anonymous-bitfield-helper-member.t tests/general/300-bit-field-layout-sizeof.t tests/general/300-zero-width-bit-field-layout.t tests/general/400-bit-field-constructor-member-init.t tests/general/400-bit-field-member-access-bad.t tests/general/400-bit-field-prefix-postfix-increment.t tests/general/400-bit-field-sparse-member-init.t tests/general/400-bitfield-aggregate-init.t tests/general/400-signed-bit-field-read.t tests/general/400-signed-enum-bit-field-read.t tests/general/400-address-of-bit-field-bad.t'
```

It is `5/11` exact passes. These pass: anonymous helper/layout, ordinary
`sizeof`, zero-width layout, sparse member initialization, and the
address-of rejection control. These six remain expected LowIR mismatches:
`400-bit-field-constructor-member-init.t`,
`400-bit-field-member-access-bad.t`,
`400-bit-field-prefix-postfix-increment.t`,
`400-bitfield-aggregate-init.t`, `400-signed-bit-field-read.t`, and
`400-signed-enum-bit-field-read.t`. The last two retain required signed
integral and signed-underlying-enum sign extension; their checked-in LowIR
references are stale and were not edited. The four other mismatches are
normal LowIR-shape differences with successful typed compilation; no
test-identity branch was added.

The full PA16 run is `131/243` passed and `112` failed, with all `243`
original identities still exercised. The final failure list and comparison to
the authoritative turn-start log are in:

```text
/tmp/pa16-bitfield-perf-20260828-v4/full-test-pa16-final.log
/tmp/pa16-bitfield-perf-20260828-v4/final-failures.txt
/tmp/pa16-bitfield-perf-20260828-v4/identity-comparison.txt
```

The exact comparison is `116 -> 112` failures, four baseline-only repaired
identities (`300-anonymous-bitfield-helper-member.t`,
`300-bit-field-layout-sizeof.t`, `300-zero-width-bit-field-layout.t`, and
`400-bit-field-sparse-member-init.t`), zero final-only regressions, and no
lost original coverage. The turn-start authoritative source is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The bounded public course probe
`cppgm.tests/course/pa16/412-typed-bit-field-initialization-root-regression.sh`
also passes; it covers two same-record locals, nested subobjects, and array
elements without asserting private LowIR shape.

## Active Checkpoint

The current bit-field changes are limited to these existing modules plus one
bounded public course probe:

- `dev/src/pa11_semantic_model.h`
- `dev/src/pa11_semantic_core.cpp`
- `dev/src/pa11_record_layout.cpp`
- `dev/src/pa12_semantic_facts.cpp`
- `dev/src/pa15_lowering.h`
- `dev/src/pa15_lowering.cpp`
- `dev/src/pa15_lowering_construction.cpp`
- `dev/src/pa15_lowering_flow.cpp`
- `dev/src/pa15_lowering_member.cpp`
- `cppgm.tests/course/pa16/412-typed-bit-field-initialization-root-regression.sh`

The typed flow is PA10 bit-field AST -> PA11 declaration validation and one
`BitFieldFact` per named binding plus one owner-stable declaration vector per
record -> the shared PA11 layout service -> PA12 member/glvalue provenance,
address-of rejection, aggregate/constructor facts, assignment, and built-in
inc/dec -> PA15 masked unit load/store and effective-underlying-type sign or
zero extension. `BitFieldInitializationContext` is a compact typed current
unit carried only through one destination/root initialization sequence;
nested aggregates and array elements receive fresh contexts, so distinct
locals, subobjects, constructor targets, globals, and repeated sites cannot
alias state. Named nested aggregates retain their inner record owner, and the
existing unsupported class-anonymous-union injection boundary is not silently
widened. Ordinary fields and bit-fields are appended once through their
owner-checked event path; layout validates the filtered binding order before
consuming it. No handout test, fixture, reference, comparator, or coverage rule
is changed.

## Performance Evidence

The implementation keeps event append constant time and performs one
owner-stable event walk plus one filtered-order check per complete record.
Representative immutable compiler copies and five interleaved O0 runs per
case are recorded outside the repository:

```text
/tmp/pa16-bitfield-perf-20260828-v4/summary.tsv
/tmp/pa16-bitfield-perf-20260828-v4/hashes.tsv
/tmp/pa16-bitfield-perf-20260828-v4/compiler-hashes.txt
/tmp/pa16-bitfield-perf-20260828-v4/runs/
```

Both compiler copies have SHA-256
`71fe9e1645ccf43efc41bc77515ef0709d9b7d003d98f013c590a8048d18bad8`.
Small (2 declarations, 2 initialization events, 1 object) has 18 LowIR
instructions, 0.011764s median wall time, and 5,388 KiB median RSS. Large
(256 declarations, 512 events, 2 objects) has 3,562 instructions, 0.034294s,
and 13,120 KiB. Nested (2 declarations, 20 events, 1 outer object with
nested/array subobjects) has 146 instructions, 0.012777s, and 5,628 KiB.
Each case has one deterministic LowIR hash across all five runs; structural
counts are in `summary.tsv`. These are representative whole-compiler
measurements, not an isolated phase or timeout claim.

## Next Checkpoint

The PA16 bit-field increment is complete, but work remains in PA16. Freeze the
`131/243` passed, `112`-failure, `243/243` coverage map and the six focused
LowIR mismatches documented above; the signed-reference mismatch is
intentionally retained until its fixture contract is refreshed. Classify and
implement the next coherent PA16 boundary against this map. Do not begin PA17
until `make test-report-through-pa16` is clean; the address-of rejection,
nested/anonymous owner boundaries, and `1167/1167` through-PA15 result remain
preserved.

## Checkpoint ledger

| checkpoint | result |
| --- | --- |
| Ordinary non-template overloaded-operator boundary | Implemented: typed member/nonmember/fallback calls, hidden-friend identity/visibility, implicit-object ranking, bool boundary, and PA15 ABI consumption; 29 existing PA16 failures removed. |
| Through PA15 compatibility | `make test-report-through-pa15`: `1167/1167` passed; final log `pa16-operator-followup-through-pa15.log`. |
| PA16 coverage and regression gate | Final `127/243` passed, `116` failed, `243/243` covered; exact comparison to the `122/243` audit baseline has five baseline-only repairs and zero final-only identities. |
| Follow-up bool/index audit | Expression-owned typed bool provenance, composite hidden-friend key, corrected same-name performance evidence; all 29 removed identities retained. |
| File audit and diff check | File audit passed with 5 pre-existing warnings; log `pa16-operator-followup-file-audit.log`; final `git diff --check` is recorded in `pa16-operator-followup-diff-check.log`. |
| `2d93a5e9` ordinary non-template overloaded-operator checkpoint audit | Completed bounded repair and audit of the `20f14d30` -> `23a26df5` implementation span as tightened at `2d93a5e9`: the corrective follow-up separates exact friend-definition lexical ownership from access friendship and records typed public/private/protected base-reference accessibility, including a bounded further-derived protected proof. Enum identity/ranking, narrow constructor-backed reference binding, reference/address facts, and typed bool boundaries remain covered. Final PA16 is `127/243` with `116` failures and `243/243` coverage; comparison to the `122/243` audit baseline has five baseline-only repairs and zero final-only failures. Through-PA15 is `1167/1167`; focused status is `29/32` with three documented pre-existing holdouts; course 411 passes; final state-matched performance is `pa16-operator-perf-followup-v5` with final/immutable SHA-256 `e5ffb4e9869c619552f193e16ef063ab2feba7c27f809887ebdd187960196580`. |
| Ordinary typed bit-field boundary | Complete: owner-stable per-record declaration events, canonical typed facts, packed/zero-width layout, PA12 provenance/address-of rejection, aggregate/constructor initialization, masked PA15 reads/writes, effective-underlying signedness, root-scoped initialization context, and the public distinct-object probe. Full PA16 is `131/243` passed with `112` failures, four baseline failures repaired, zero final-only regressions, and `243/243` original identities covered; focused matrix is `5/11` with six documented LowIR mismatches. Through-PA15 is `1167/1167`; file audit and diff check pass. |
