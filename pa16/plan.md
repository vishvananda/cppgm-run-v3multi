# PA16 implementation plan

## Stage Design

This increment implements the ordinary PA16 bit-field boundary as one typed
pipeline. PA10 preserves `BitFieldDeclaration` and `BitFieldDeclarator`; PA11
validates the integral/enum declaration and constant width, creates one
canonical `BitFieldFact` per named field, and records ordinary and unnamed
events in a vector owned by each `NamedRecordId`. The fact carries owner and
binding identity, declared/storage/operation types, declared/value widths,
physical unit and allocation span, offsets, masks, and effective signedness.
The operation type follows the C++11 converted-bit-field promotion boundary:
bool, int/signed int/unsigned int, and lower-rank fields select the typed int
or unsigned-int result; narrow long/long long retain their declared type.
The declaration/extra-padding model follows C++11
[class.bit](https://timsong-cpp.github.io/cppwp/n3337/class.bit) and the
promotion model follows
[conv.prom](https://timsong-cpp.github.io/cppwp/n3337/conv.prom).

Layout consumes that owner-stable stream in declaration order, packs adjacent
compatible fields, flushes around ordinary/zero-width boundaries, and gives
oversized declarations a checked multi-unit allocation whose excess is
padding. PA12 owns member glvalues, semantic conversion/operator validity,
const-reference temporaries, address-of ordering, assignment/inc/dec, and
aggregate/constructor facts. PA15 projects only the <=64-bit physical unit for
masked reads and updates, with typed sign/zero extension and fresh
root-scoped initialization contexts. No lowering decision recovers width,
owner, mask, signedness, or promotion from rendered text.

Nested records retain their own event owner, unions use the checked allocation
span, and ordinary members share the same owner-checked append/alignment path.
This checkpoint does not add class-by-value transfer, copy/move, virtual or
multiple inheritance, templates, general conversions, or unrelated PA16
semantics.

## Failure Map

The authoritative turn-start baseline after `da4252b6` is `131/243` passed,
`112` failed, and `243/243` identities covered in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The final `make test-pa16` has the same `131/243`, `112`, and `243/243`
state.  The durable final log and exact normalized comparison are:

```text
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-bitfield-audit-final-source-test-pa16.log
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-bitfield-audit-final-source-identity-comparison.log
```

The comparison is baseline failures `112`, final failures `112`,
baseline-only `0`, final-only `0`, and test inventory `243`; there is no
new failure identity and no reduced coverage.  The focused bit-field command
is the documented 11-test matrix and is `5/11`, with six known LowIR
presentation mismatches.  Course 412 and the direct
`200-const-cast-pointer-reference-alias.t` control pass.  The full failure
map remains intentionally incomplete and is not broadened by this checkpoint.

## Active Checkpoint

The current bit-field changes are limited to these existing modules plus one
bounded public course probe:

- `dev/src/pa11_semantic_model.h`
- `dev/src/pa11_semantic_core.cpp`
- `dev/src/pa11_record_layout.cpp`
- `dev/src/pa12_semantic_facts.cpp`
- `dev/src/pa12_semantic.cpp`
- `dev/src/pa12_semantic_calls.cpp`
- `dev/src/pa12_semantic_construction.cpp`
- `dev/src/pa12_semantic_member.cpp`
- `dev/src/pa12_semantic_resolution.cpp`
- `dev/src/pa15_lowering.h`
- `dev/src/pa15_lowering.cpp`
- `dev/src/pa15_lowering_flow.cpp`
- `dev/src/pa15_lowering_member.cpp`
- `cppgm.tests/course/pa16/412-typed-bit-field-initialization-root-regression.sh`

The typed flow is PA10 AST -> PA11 canonical declaration facts and
owner-stable events -> PA11 layout -> PA12 typed member/glvalue provenance,
promotion and conversion selection, semantic-owner rejection, overload-before-
address-of, const-reference temporary, assignment/inc/dec, and
aggregate/constructor facts -> PA15 physical-unit masks, extension, RMW, and
root-scoped materialization. `BitFieldInitializationContext` is carried only
through one destination/root sequence; nested aggregates and array elements
get fresh contexts, so distinct locals, subobjects, globals, and repeated
sites cannot alias state. Named nested aggregates retain their inner owner, and
the existing unsupported class-anonymous-union injection boundary is not
silently widened. Ordinary fields and bit-fields are appended once through
their owner-checked event path; layout validates filtered order before use. The
exact-reference alias repair preserves ordinary `ReferenceBinding` roots while
leaving expression-owned bit-field temporary facts intact. No handout test,
fixture, reference, comparator, or coverage rule is changed.

## Performance Evidence

The implementation keeps event append constant time and performs one
owner-stable event walk plus one filtered-order check per complete record.
Representative immutable compiler copies and five interleaved O0 runs per
case are recorded outside the repository:

```text
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-bitfield-audit-perf-final-source/manifest.tsv
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-bitfield-audit-perf-final-source/timing.tsv
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-bitfield-audit-perf-final-source/medians.tsv
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-bitfield-audit-perf-final-source/structure.tsv
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-bitfield-audit-perf-final-source/determinism.tsv
```

Both compiler copies have SHA-256
`c98edbf143904e0b09b451310de38e7966149b4374ad912b55a1b9f8c96aaf02` and are
mode `0555`. Small (268 lines, 2 target declarations, 128 target
expressions) has 131 LowIR functions/256 calls, `0.01s` median wall time, and
9,860 KiB median RSS. Large (1046 lines, 12 declarations, 512 expressions)
has 525 functions/1024 calls, `0.07s`, and 22,200 KiB. Same-name-noise
(1804 lines, 2 target declarations, 256 unrelated hidden friends, 128 target
expressions) has 387 functions/256 calls, `0.05s`, and 17,960 KiB. Each
variant/case has one output hash across all five runs and final/immutable
hashes match. These are representative whole-compiler measurements, not an
isolated phase or timeout claim.

## Next Checkpoint

The PA16 bit-field checkpoint is complete, but work remains in PA16. Freeze the
`131/243` passed, `112`-failure, `243/243` coverage map and six focused LowIR
mismatches. Classify the unchanged residual identities by ownership before
opening another surface; preserve this bit-field path, its exact-reference
alias behavior, and its bounded exclusions. Do not begin PA17 until
`make test-report-through-pa16` is clean; the `1167/1167` through-PA15 result
remains preserved.

## Checkpoint ledger

| checkpoint | result |
| --- | --- |
| Ordinary non-template overloaded-operator boundary | Implemented: typed member/nonmember/fallback calls, hidden-friend identity/visibility, implicit-object ranking, bool boundary, and PA15 ABI consumption; 29 existing PA16 failures removed. |
| Through PA15 compatibility | `make test-report-through-pa15`: `1167/1167` passed; final log `pa16-operator-followup-through-pa15.log`. |
| PA16 coverage and regression gate | Final `127/243` passed, `116` failed, `243/243` covered; exact comparison to the `122/243` audit baseline has five baseline-only repairs and zero final-only identities. |
| Follow-up bool/index audit | Expression-owned typed bool provenance, composite hidden-friend key, corrected same-name performance evidence; all 29 removed identities retained. |
| File audit and diff check | File audit passed with 5 pre-existing warnings; log `pa16-operator-followup-file-audit.log`; final `git diff --check` is recorded in `pa16-operator-followup-diff-check.log`. |
| `2d93a5e9` ordinary non-template overloaded-operator checkpoint audit | Completed bounded repair and audit of the `20f14d30` -> `23a26df5` implementation span as tightened at `2d93a5e9`: the corrective follow-up separates exact friend-definition lexical ownership from access friendship and records typed public/private/protected base-reference accessibility, including a bounded further-derived protected proof. Enum identity/ranking, narrow constructor-backed reference binding, reference/address facts, and typed bool boundaries remain covered. Final PA16 is `127/243` with `116` failures and `243/243` coverage; comparison to the `122/243` audit baseline has five baseline-only repairs and zero final-only failures. Through-PA15 is `1167/1167`; focused status is `29/32` with three documented pre-existing holdouts; course 411 passes; final state-matched performance is `pa16-operator-perf-followup-v5` with final/immutable SHA-256 `e5ffb4e9869c619552f193e16ef063ab2feba7c27f809887ebdd187960196580`. |
| `da4252b6` typed bit-field boundary checkpointAudit/follow-up | Complete: PA10--PA15 typed operation/promotion facts, semantic-owner validity checks, const-reference temporaries, overload-before-address-of, owner-stable mixed/zero-width/unnamed/union layout, checked oversized allocation spans, masked signed/unsigned projection, and isolated initialization roots. Final PA16 is `131/243` passed with `112` failures and `243/243` identities; exact comparison to the turn-start map is baseline-only `0` and final-only `0`. Course 412 and the direct alias control pass; focused matrix is `5/11` with six documented LowIR mismatches; through-PA15 is `1167/1167`; file audit and diff-check pass. State-matched performance uses final/immutable SHA-256 `c98edbf143904e0b09b451310de38e7966149b4374ad912b55a1b9f8c96aaf02`. |
