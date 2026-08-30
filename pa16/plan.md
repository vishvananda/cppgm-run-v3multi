# PA16 implementation plan

## Stage Design

Spec alignment (§§1, 2, 4, and 7): one forward typed owner carries pack
state from preprocessing to canonical PA11 layout; no textual downgrade or
parallel model is used; ordered facts and bounded validation stacks are linear
in directive count; and structural probes are recorded without unsupported
timing/RSS claims.

This checkpoint owns the typed ordered `#pragma pack(push, n)` / `pop`
state needed by the canonical PA11 ordinary-record layout.  The
`PPPreprocessingSession::Impl` is the single mutable state owner: it keeps the
active cap and a stack of saved caps, validates recognized operations, and
appends a compact `PPPackDirective` to the `PPTokenBuffer` at the exact
phase-3 token boundary.  A push stores its explicit nonzero byte cap; each
fact also stores the effective cap after the operation, with zero denoting
natural layout.  Include expansion shares this state, while inactive
conditional branches produce no facts.  Unknown pragmas retain their prior
ignore behavior; malformed recognized pack operations fail closed.

The production forward path remains typed.  The `PPTokenBuffer` overload of
`posttokenize_cpp_tokens` visits raw tokens and ordered directive boundaries.
`PostTokenStream` queues pack facts without flushing pending strings or
clearing pending `operator` formation.  At the next independent token it
flushes phase-6 string formation, forwards queued facts, then emits that
token.  Thus directives are token-transparent, including multiple facts at
one boundary, while the callback is observed before the first following
syntax token.  The adapter now rejects unsorted/out-of-range facts, invalid
operations, inconsistent saved-cap state, and facts after EOF before
emitting output.  PA10's collector maps the completed posttoken count to a
whitespace-free syntax-token boundary, revalidates the typed state trace, and
stores `PA10PackDirective` facts in an AST side vector.  No pragma text is
rendered, joined, or reparsed.

At each class definition, PA11 binary-searches the ordered AST facts at the
class node's `source_begin` and stores the active cap on its `NamedRecord`.
The existing `complete_record_members` / `complete_record_layout` path then
caps natural ordinary-member, base, bit-field storage, and final record
alignment before normal size rounding.  The wide bit-field allocation path
now uses that same capped storage alignment.  Existing explicit member/class
`alignas` requests remain stronger and are not erased.  A pop restores the
saved cap, so unaffected records retain natural layout.  PA15 and LowIR have
no pack-specific inference or parallel layout engine.

## Failure Map

The latest landed-stage authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`210/243` passed, exactly `33` failed, and all `243/243` identities are
covered.  The increment's earlier turn-start authority was `209/243` with
`34` failures; `300-packed-class-layout.t` is the one baseline identity
cleared by the landed increment and is not in the current residual map.  The
complete latest failure map is:

- `pa16/tests/general/100-function-pointer-nested-param-name-shadow.t`
- `pa16/tests/general/200-aliased-base-mem-initializer-match.t`
- `pa16/tests/general/200-const-subobject-member-call.t`
- `pa16/tests/general/200-elaborated-member-forward-type.t`
- `pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t`
- `pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t`
- `pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t`
- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
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
- `pa16/tests/general/300-enum-class-nonmember-operator-bitand.t`
- `pa16/tests/general/300-friend-function-definition-skip.t`
- `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
- `pa16/tests/general/300-operator-nullptr-t-from-zero.t`
- `pa16/tests/general/300-overloaded-deref-user-assignment.t`
- `pa16/tests/general/300-pragma-pack-followed-by-endif.t`
- `pa16/tests/general/300-prvalue-derived-base-friend-operator.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-bitfield-aggregate-init.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

The two pack identities remain the checkpoint evidence: the packed-class
identity is clear in the latest authority, while the conditional-pack
identity still has the unrelated canonical-truth `trunc`-before-`zext` LowIR
shape residual.  The audit does not re-audit the other 32 residual identities.

## Active Checkpoint

The implementation is limited to the typed preprocessing fact, token-
transparent posttoken handoff, PA10 side-channel boundary, and PA11 layout
consumer in these files:

- `dev/src/IPPTokenStream.h`
- `dev/src/preproc_session.cpp`
- `dev/src/posttoken.h` and `dev/src/posttoken.cpp`
- `dev/src/pa10_ast.h` and `dev/src/pa10_ast.cpp`
- `dev/src/pa10_parser_support.h` and `dev/src/pa10_parser_support.cpp`
- `dev/src/pa11_semantic_model.h`, `dev/src/pa11_semantic.cpp`, and
  `dev/src/pa11_record_layout.cpp`

The fact flow is
`PPPreprocessingSession::Impl` state -> ordered `PPPackDirective` -> queued
typed posttoken callback -> `PA10PackDirective` syntax boundary ->
`PA11SemanticModel::pack_alignment_at` -> `NamedRecord::pack_alignment` ->
the canonical PA11 member/layout computation.  The preprocessing stack is
session-owned and persists through includes and conditional boundaries; the
PA10/PA11 vectors are ordered compact facts, not per-node strings or maps.
The audit repair is limited to the posttoken validator, the PA10 boundary
validator, an explicit PA11 operation check, and the wide-bit-field cap use.

`_Pragma("pack(...)")` is outside this checkpoint and remains on the
existing unknown-pragma path.  Other 32 residual identities, handout tests
and `.ref` files, source-text recovery, host/reference execution, and
unrelated semantic/lowering residuals are excluded.  Course 422 is the sole
public-layer regression added by this checkpoint.  There is intentionally no
end-of-translation-unit empty-stack diagnostic in this increment; a live
push remains the active state for subsequent included/source text.  The
checkpoint does not claim full PA16 completion.

## Performance Evidence

Directive parsing, ordered-fact validation, and typed posttoken replay are
`O(T + D)` for `T` phase-3 tokens and `D` recognized directives; each fact is
pushed, validated, queued, forwarded, and consumed once.  The preprocessing
stack and each temporary validation stack use `O(P)`/`O(D)` memory, while the
ordered fact vectors use `O(D)` compact storage.  At the PA11 handoff, the
current lookup is a binary search per record, so ordinary layout work is
`O(M + R log D)` for `M` members, `R` record definitions, and `D` directives.
This is within the stage's `O(n log n)` ordinary-work bound; it is not claimed
to be end-to-end `O(T + D)`.  After lookup, member layout remains the existing
`O(M)` walk.  No per-node owning strings, node-based pack maps, textual
fallback, retry, or unbounded cache was added.

Representative structural evidence from the focused probes: the packed
fixture has two directives and two record definitions (`B` receives cap 1
and size 5; `C` receives restored natural state and size 8); the conditional
fixture has two directives and one record (`X` receives cap 1 and size 5); a
same-boundary push/pop pair has two directives and one record and remains
size 8.  The wide-bit-field probe is size 10 under cap 1 and size 16 under
natural layout.  The adjacent-string probe has push/pop facts between two
string parts and remains accepted, showing that the token-transparent handoff
does not break phase-6 concatenation.

## Validation

Focused repair validation:

- `make -C dev cppgm++ CXX=g++`: passed.
- Direct packed/natural, conditional-pack, explicit-`alignas`, pop-restore,
  same-boundary, and adjacent-string compiler probes all exited `0`; the
  emitted sizes include packed `B=5`, natural `C=8`, wide packed `X=10`, and
  wide natural `X=16`.
- A temporary typed-buffer probe passed valid same-boundary push/pop facts and
  rejected a post-EOF fact, an invalid operation, and a mismatched push-state
  fact before callback delivery.
- Course 422 `sh -n` and execution pass; it checks packed wide-bit-field size
  10 against the natural-layout control size 16.  Handout tests and fixtures
  remain unchanged.

The fresh `make test-pa16` authority is `210/243` with `33` failures and
`243/243` coverage.  The durable exact comparison is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-identity-coverage-20260830.log`:
fresh-only `0`, authority-only `0`, inventory `243`, unexpected `0`, and
`243/243` covered.  The full output is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-final-20260830.log`.
The exact required through-PA15 command exits `0` at `1167/1167`; its
complete output and explicit status are retained at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-through-pa15-20260830.log`.
The file audit exits `0` with five known warnings and no fatals; its complete
output and explicit status are retained at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-file-audit-20260830.log`.
Diff-check exits `0`.
The remaining `300-pragma-pack-followed-by-endif.t` trunc-before-zext shape
is unrelated and intentionally remains; PA16 is not complete.

## Next Checkpoint

The checkpoint is finalized at the exact 33-failure authority with full
identity coverage.  Any later PA16 work must select a separate residual after
reviewing that map; do not expand this pack audit or reinterpret extra passes
as compensation for a fresh failure.

## Checkpoint Ledger

| checkpoint | PA16 identities / coverage | delta / evidence | status |
| --- | --- | --- | --- |
| `1694bc3e` retained baseline | `200/243`, 43 failures, `243/243` covered | prior bit-field baseline | prior landed |
| `7e060b28` typed packed bit-field boundary | `202/243`, 41 failures, `243/243` covered | prior focused bit-field evidence retained | prior landed |
| `d95a6fe7` local-class checkpoint start | `202/243`, 41 failures, `243/243` covered | prior local-class selection | prior checkpoint |
| `d83e927f` typed local-class materialization | `206/243`, 37 failures, `243/243` covered | prior focused matrix, through-PA15, and audit passed | prior landed |
| `70327e4d` typed exception-safe destructor suffix | `208/243`, 35 failures, `243/243` covered | typed destructor-body suffix and cleanup work | prior landed |
| `3b2b4882` checkpoint audit baseline | `208/243`, 35 failures, `243/243` covered | clean turn-start tree and authoritative stage log | prior baseline |
| `ee8f44d5` per-throw typed array cleanup checkpoint audit | `209/243`, 34 failures, `243/243` covered | exact prior authority comparison preserved all identities and coverage; focused constructor/array evidence passed with mapped residuals | prior landed |
| `08472cce` typed ordered pack checkpointAudit (parent `0ff3fdef`) | `210/243`, 33 failures, `243/243` covered | landed packed-class identity is clear; conditional-pack shape residual remains; bounded audit repairs wide-bit-field capping and typed-fact validation; course 422, broad gate, exact set comparison, through-PA15, file audit, and diff-check pass | completed checkpoint audit; PA16 remains incomplete |
