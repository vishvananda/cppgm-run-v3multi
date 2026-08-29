# PA16 implementation plan

## Stage Design

PA11/PA12 own the canonical `BindingId` and one `BitFieldFact` per named
bit-field. The fact carries declared, storage, and promoted operation types,
signedness, storage-unit offset/width, masks, and owning record membership.
PA15 consumes that fact through keyed lookup only. `LoweredValue` keeps the
binding on a bit-field lvalue, and the shared load/encode/RMW-store helpers
perform all packed projection work. No source spelling, duplicate semantic
model, or current-block scan is part of this boundary.

The invariant is: evaluate a C++ lvalue once; load the canonical storage unit;
extract or encode exactly the fact's value projection; and preserve all other
bits on an RMW store. A value read uses the fact's operation type, including
integral promotions. Explicitly signed integral and signed-underlying enum
fields are sign-extended after masking so represented negative values survive.
Address-of and non-const-reference restrictions remain PA12 semantic rules.

## Failure Map

The turn-start authority at `1694bc3e` was `200/243` identities passed,
exactly `43` failed, with `243/243` covered. The 43-entry list below is the
complete turn-start failure map:

- `pa16/tests/general/100-function-pointer-nested-param-name-shadow.t`
- `pa16/tests/general/200-aliased-base-mem-initializer-match.t`
- `pa16/tests/general/200-const-subobject-member-call.t`
- `pa16/tests/general/200-destructor-body-local-before-base-destruction.t`
- `pa16/tests/general/200-elaborated-member-forward-type.t`
- `pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t`
- `pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t`
- `pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t`
- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-member-object-lifetime.t`
- `pa16/tests/general/200-nested-braced-member-aggregate-init.t`
- `pa16/tests/general/200-nonliteral-field-condition-not-folded.t`
- `pa16/tests/general/200-placement-new-expression-aggregate-brace.t`
- `pa16/tests/general/200-placement-new-expression-constructor-call.t`
- `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
- `pa16/tests/general/200-reference-member-class-init.t`
- `pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t`
- `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
- `pa16/tests/general/300-adl-associated-namespace-does-not-climb-parents.t`
- `pa16/tests/general/300-adl-using-declaration-source-point.t`
- `pa16/tests/general/300-callable-field-hides-private-base-method.t`
- `pa16/tests/general/300-compound-assignment-adl-nonmember-after-member-reject.t`
- `pa16/tests/general/300-enum-class-nonmember-operator-bitand.t`
- `pa16/tests/general/300-friend-function-definition-skip.t`
- `pa16/tests/general/300-member-vs-nonmember-operator-implicit-object-cv-rank.t`
- `pa16/tests/general/300-mixed-member-free-shift-stress-chain.t`
- `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
- `pa16/tests/general/300-operator-nullptr-t-from-zero.t`
- `pa16/tests/general/300-operator-shift-stress-chain.t`
- `pa16/tests/general/300-overloaded-deref-user-assignment.t`
- `pa16/tests/general/300-packed-class-layout.t`
- `pa16/tests/general/300-pragma-pack-followed-by-endif.t`
- `pa16/tests/general/300-prvalue-derived-base-friend-operator.t`
- `pa16/tests/general/300-synthesized-array-member-lifecycle.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`
- `pa16/tests/general/400-bit-field-constructor-member-init.t`
- `pa16/tests/general/400-bit-field-member-access-bad.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-bitfield-aggregate-init.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

The exact final current-stage residual set is `41/243`, consisting of every
turn-start identity above except these two removed identities:

- `pa16/tests/general/400-bit-field-constructor-member-init.t`
- `pa16/tests/general/400-bit-field-member-access-bad.t`

Thus final-only is empty, unchanged failures number `41`, and coverage is
unchanged at `243/243`. The six `400-*bit-field*` identities form this
checkpoint's cluster. The focused turn-start baseline was `0/6`; the final
cluster is `2/6`, with constructor member initialization and direct member
access passing. The prefix fixture still has promoted-type/presentation
differences, while the signed read fixtures retain required sign extension
that their checked references omit. The aggregate signed read has the same
oracle discrepancy; its duplicate initializer evaluation is repaired.

## Active Checkpoint

This checkpoint repairs the typed PA15 boundary for packed bit-field storage:

- Carry only a typed `BitFieldAddressProjectionId` in `LoweredValue`. The
  complete already-evaluated storage-root/final-index descriptor is owned by
  a contiguous `Pa15Lowerer` arena and is captured only by the explicit
  bit-field index/address boundary. Ordinary `emit_index`, decay, and storage
  address operations allocate or copy no projection descriptor. Constructor
  `this` roots can be reloaded, ordinary storage roots can be re-addressed,
  and nested paths reuse their evaluated parent pointer.
- Keep one packed-unit RMW implementation. Preserve the original load
  projection, then reproject only the final store where a fresh destination is
  needed. Prefix/postfix update values are encoded before their shared RMW;
  prefix uses a fresh operation projection and postfix retains its evaluated
  destination.
- Publish an extracted bit-field through an explicit ordinary value boundary
  when a non-boolean-context scalar conversion requires it, without adding
  that copy to direct update loads.
- Evaluate an aggregate bit-field initializer once and reuse that typed value
  for encoding and storage. This preserves side effects and keeps aggregate
  and constructor work linear in initializer actions.

The stable boundary is the canonical `BitFieldFact` plus the PA15 helpers
`emit_bit_field_load`, `encode_bit_field_value`,
`emit_encoded_bit_field_store`, and `emit_bit_field_store`. The repair does
not reconstruct declared or promoted types from text or widths. A fresh store
projection is a general LowIR action-order rule: it replays the saved typed
root/index after the update value has been computed, without reevaluating the
source lvalue. The materialization copy is likewise a general value-boundary
operation for a packed extraction; update paths use the extraction directly.

The implementation follows N3485 `[class.bit]` restrictions and the
`[conv.prom]`/`[conv.integral]` value path: bit-fields are not addressable,
non-const references remain rejected, and explicitly signed narrow fields
retain their negative representation on reads. It excludes the other 37
baseline residuals, reference/fixture edits, semantic-fact redesign, and any
weakening of signedness or promotion merely to match a suspicious LowIR
golden shape.

## Performance Evidence

Canonical fact lookup is O(1) by `BindingId`. Each bit-field load, encode, and
packed-unit store performs O(1) keyed work and a bounded number of LowIR
operations; projection-ID lookup is O(1), and arena growth is O(P) for P
lowered bit-field projections only. Aggregate and constructor handling remains
O(declarations + initializer actions), with one evaluation per action. On the
Linux x86_64 build, a standalone header probe reports
`sizeof(LoweredValue)=336`, `sizeof(BitFieldAddressProjectionId)=8`, and
`sizeof(BitFieldAddressProjection)=352`; the latter is arena-only rather than
part of every hot record. For the representative two-field constructor test,
the two constructor member actions each take the explicit capture path, while
store reprojection reuses the ID and appends no descriptor; ordinary
`emit_index` has no append path. The focused six-test run and 11-test matrix
cover single-unit multi-field construction, member access, and prefix/postfix
RMW; the PA16 course bit-field root regression exits 0. No timing claim is
made. The final audit also bounds the implementation at 2991 lines for
`pa15_lowering.cpp` and within the 240-line aggregate-initializer function
limit.

## Checkpoint Ledger

| checkpoint | PA16 identities / coverage | delta from baseline | focused evidence | prior-through / audit | commit |
| --- | --- | --- | --- | --- | --- |
| `1694bc3e` baseline | `200/243`, `43` failures, `243/243` covered | baseline | six bit-field identities `0/6` | inherited through-PA15 `1167/1167`; prior audit passed with five known header warnings | existing HEAD |
| typed packed bit-field boundary | `202/243`, `41` failures, `243/243` covered | `+2` identities; baseline-only exactly constructor-member-init and member-access; final-only empty | six-test exits 2 (`2/6`); 11-test matrix exits 2 (`7/11`); course regression exit 0; full `make test-pa16` exit 2; identity comparison exit 0; diff-check exit 0 | through-PA15 exit 0 (`1167/1167`); file audit exit 0 with five pre-existing header warnings | final worker commit; hash reported in handoff |

Durable final evidence:

- Full PA16: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-compact-worker-final-rerun.log` (`make test-pa16`, exit 2; `202/243`).
- Sorted identity comparison: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-compact-worker-identity-delta-final.txt` (43 baseline failures, 41 final failures, two baseline-only, final-only empty).
- Focused cluster/matrix/course checks: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-compact-worker-focused-final.log` (2/6, 7/11, course exit 0, diff-check exit 0).
- Prior-stage gate: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-compact-worker-through-pa15-final.log` (exit 0, `1167/1167`).
- File audit: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-compact-worker-audit-final.log` (exit 0; five pre-existing header-division warnings, no fatals).
- Performance evidence: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-compact-worker-performance-final.log` (size probe exit 0; `LoweredValue` 336 bytes, handle 8 bytes, descriptor 352 bytes, one append site, source bounds recorded).

The authoritative baseline log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` and
the progress failure log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/last-stageProgress.log`.
