# PA16 implementation plan

## Stage Design

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
syntax token.  PA10's collector maps the completed posttoken count to a
whitespace-free syntax-token boundary and stores `PA10PackDirective` facts in
an AST side vector.  No pragma text is rendered, joined, or reparsed.

At each class definition, PA11 binary-searches the ordered AST facts at the
class node's `source_begin` and stores the active cap on its `NamedRecord`.
The existing `complete_record_members` / `complete_record_layout` path then
caps natural ordinary-member, base, bit-field storage, and final record
alignment before normal size rounding.  Existing explicit member/class
`alignas` requests remain stronger and are not erased.  A pop restores the
saved cap, so unaffected records retain natural layout.  PA15 and LowIR have
no pack-specific inference or parallel layout engine.

## Failure Map

The authoritative turn-start log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`209/243` passed, exactly `34` failed, and all `243/243` identities were
covered.  The complete turn-start failure map is:

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
- `pa16/tests/general/300-packed-class-layout.t`
- `pa16/tests/general/300-pragma-pack-followed-by-endif.t`
- `pa16/tests/general/300-prvalue-derived-base-friend-operator.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-bitfield-aggregate-init.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

The two pack identities are the active checkpoint.  The first is expected
to become clear.  The second's pack size/exit behavior is corrected, but its
checked LowIR currently retains the unrelated pre-existing canonical-truth
`trunc`-before-`zext` shape residual.  No other residual family is broadened.

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

`_Pragma("pack(...)")` is outside this checkpoint and remains on the
existing unknown-pragma path.  Other 32 turn-start identities, tests and
`.ref` files, source-text recovery, host/reference execution, and unrelated
semantic/lowering residuals are excluded.  The checkpoint does not claim
full PA16 completion.

## Performance Evidence

Directive parsing and typed posttoken replay are `O(T + D)` for `T` phase-3
tokens and `D` recognized directives; each fact is pushed, queued, forwarded,
and consumed once.  The preprocessing stack uses `O(P)` memory for `P`
active pushes, and the ordered fact vectors use `O(D)` compact storage.  At
the PA11 handoff, the current lookup is a binary search per record, so the
ordinary layout work is `O(M + R log D)` for `M` members, `R` record
definitions, and `D` directives.  This is within the stage's `O(n log n)`
ordinary-work bound; it is not claimed to be end-to-end `O(T + D)`.
After lookup, member layout remains the existing `O(M)` walk.  No per-node
owning strings, node-based pack maps, textual fallback, or unbounded retry
cache was added.

Representative structural evidence from the focused probes: the packed
fixture has two directives and two record definitions (`B` receives cap 1
and size 5; `C` receives restored natural state and size 8); the conditional
fixture has two directives and one record (`X` receives cap 1 and size 5); a
same-boundary push/pop pair has two directives and one record and remains
size 8.  The adjacent-string probe has push/pop facts between two string
parts and produces the same semantic output as the no-pragma control.

## Validation

Focused repair validation:

- `make -C dev cppgm++ CXX=g++`: passed.
- Temporary adjacent-string probe versus no-pragma control: both exited
  `0`, and `diff -u` of semantic output was empty.
- Temporary same-boundary push/pop probe: exited `0` and emitted natural
  `sizeof=8`.
- Six-test matrix (packed targets, natural self-pointer layout, two alignas
  cases, and zero-width bit-field layout): `5/6`; only
  `300-pragma-pack-followed-by-endif.t` has the acknowledged unrelated LowIR
  residual.  `300-packed-class-layout.t` passes.

Broad validation:

- `make test-pa16` exits `2` at `210/243`, leaving `33` failures.  Compared
  with the turn-start `34` identities, authority-only is exactly
  `pa16/tests/general/300-packed-class-layout.t`; fresh-only is empty.
  The full inventory contains `243` `.t` identities, so `210 + 33 = 243`
  and coverage is `243/243`.
- The required `n=16` through-PA15 command exits `0` at `1167/1167`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` passes with
  only the five known header-division warnings.
- `git diff --check` passes.

The remaining `300-pragma-pack-followed-by-endif.t` trunc-before-zext shape
is unrelated and intentionally remains; PA16 is not complete.

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
| typed ordered pack checkpoint (parent `0ff3fdef`) | `210/243`, 33 failures, `243/243` covered | one baseline-only identity cleared; fresh-only empty; packed size/pop/conditional facts and token-transparency probes pass; through-PA15 `1167/1167`; audit passes with five known warnings | landed in this commit; PA16 remains incomplete |
