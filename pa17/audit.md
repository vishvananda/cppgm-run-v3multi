# PA17 Audit

## Current Checkpoint Review

This checkpoint audits the landed `ClassValueTransferFact` increment and its
complete typed ownership path: PA11 record/layout identity and caching, PA12
class-value conversion, selection, construction, and call facts, PA15 ABI and
materialization, and the LowIR instruction boundary. General special-member
synthesis, assignment, ref-qualified members, allocation, unions, conversion
operators, and broad temporary/lifetime work remain out of scope.

The bounded source/spec audit found and corrected six issues:

- indirect callable signatures now validate every supported class-value
  endpoint; large indirect results carry a typed hidden result destination and
  call-signature parameter;
- class-value argument storage is allocated before its source expression is
  lowered, while class sources retain their argument order and typed ownership;
- direct class initialization and direct member initialization publish the
  same-class transfer fact, evaluating a successful direct operand once and
  falling back to constructor selection only when the shortcut does not match;
- class-valued direct call results are materialized once at the address-demand
  boundary into a typed object slot before member projection;
- volatile class sources are excluded from the trivial transfer shortcut; and
- a typed PA11 sidecar flag rejects a flat class with a declared same-class
  rvalue `operator=`. The marker is conservative for trailing parameters and
  does not mark scalar or ordinary lvalue assignment overloads.

`ClassValueTransferFact` is mutable state on `RecordLayout`, keyed by the
canonical `NamedRecordId`. Layout computation resets it at start, completion,
and failure; `set_named_record_sidecar` preserves the sidecar and invalidates
the cached fact. The move-assignment marker is published from both declaration
and definition processing through the owning class binding and is idempotent.
The eligibility proof rejects incomplete, union, base-bearing, polymorphic, or
otherwise non-class records; declared constructors, declared destructors,
same-class rvalue assignment metadata, and default member initializers; and
references, function types, class subobjects, unknown arrays, and non-scalar
leaves. It intentionally does not claim to reject every kind of declared
special member: this checkpoint records only the constructor/destructor flags
and the typed same-class rvalue assignment condition needed by this proof.

The first eligibility demand costs O(m + w), where m is the bounded layout
member list and w is the cv/known-array wrapper chain; cached hits are O(1).
Move-assignment publication is a bounded typed signature/owner lookup, not a
class-scope or whole-program scan. `Computing` transitions conservatively to
`Failed`, and rejected nested class members require no child cache key because
the proof stops before recursive traversal. PA12 keeps reference parameters as
reference facts, and PA15 validates typed object/reference endpoints, maps the
hidden result before semantic parameters, emits direct `copyobj`, and lowers
successful class sources once.

The two focused nontrivial/nested guards,
`tests/general/200-nested-subobject-pass-return-by-value.t` and
`tests/general/300-generated-move-constructor-nontrivial-member.t`, remain
rejected with `EXIT_FAILURE` and `PA12 no viable function`, without LowIR
reaching `copyobj`. The new standards-certain course regression
`course/pa17/350-move-assignment-deletes-copy.t` retains only `.t` and
`.ref.exit_status`: its expected `EXIT_FAILURE` status is the oracle, so no
external PA17 reference binary or hand-edited LowIR fixture was appropriate.
The scalar-assignment and ordinary-copy-assignment acceptance check was kept as
an external named probe; both calls remained accepted and no course fixture was
needed for that positive compatibility check.

Final evidence: the exact prior-through command exited 0 with `1410/1410`;
the PA17 file audit exited 0 with the six established header-division
warnings; and `make test-pa17` exited 2 with `51/229` passed. All 228 original
tests ran: 50 original passes and 178 original failures. Comparing the
supplied 183-failure/45-pass baseline by identity found zero former-pass
regressions or new failures and five recovered original tests:
`100-copy-constructor-default-parameter`,
`100-derived-converting-ctor-beats-base-copy`,
`200-pass-return-forwarding`,
`300-empty-class-copy-member-address`, and
`300-function-pointer-class-return-call`. The added course test was one
additional pass with expected and actual `EXIT_FAILURE`.

The descriptive full-stage sample was `wall=1.27s,
maxrss=20808kB`; it is not a comparative claim. Historical evidence retained
from the prior checkpoint is the 24.4s correction build with RSS not sampled
and the earlier non-comparative stage sample `wall=1.04s,
maxrss=9928kB`.

### Audit Ledger

| checkpoint | result | evidence |
| --- | --- | --- |
| checkpointAudit | complete; bounded fixes, regression, and documentation committed | prior-through `1410/1410`; file audit passed with 6 warnings; PA17 `51/229` with 228 originals covered and 1 status-only course regression; identity comparison found 0 former-pass regressions; descriptive `wall=1.27s`, `maxrss=20808kB` |
