# PA16 implementation plan

## Stage Design

This checkpoint closes the typed exception-safe suffix boundary for generated
destructors. PA12 is the sole semantic owner of ordered member/base teardown:
`FunctionFact.destructor_action_begin/count` names the contiguous typed
`DestructorActionFact` range. PA15 resolves the active owner through
`active_destructor_record_` and `active_destructor_this_`, validates the
destructor binding and exact range, and lowers that range without source-text
recovery or unrelated-model scans. The bounded audit repair adds only fail-closed
typed checks that each action's target/type/destructor remains canonical and
that every emitted destructor call has a void signature.

For a nonempty typed suffix, `lower_function` allocates one lowering-only
cleanup block and emits `eh_cleanup` immediately after storing `this`, before
the user body or synthetic empty body. A normal fallthrough emits `eh_end`,
emits the ordered normal suffix plus explicit remaining-suffix cleanup
prefixes, and joins at a return block. The cleanup block replays the same
typed sequence, emits `eh_end`, and `resume`s. A reachable return path lowers
active local lifetimes while the body handler is active, emits `eh_end` to
close it, then lowers the normal typed suffix before terminating that path.
Leaf destructors with an empty range retain their existing shape.

The complete ordered action range is flattened into a local typed
`DestructedElement` replay list. A non-array action contributes one terminal;
an array action contributes reverse element terminals. Each entry retains the
original action, typed array-index path, and terminal record. Normal emission
protects every remaining terminal suffix with typed cleanup blocks; body-unwind
cleanup replays the complete sequence. Addresses are recomputed from the
active typed owner and path, so no SSA value or textual spelling crosses a
cleanup edge.

This follows spec sections 2, 3, and 5: facts have one canonical typed owner,
the `(begin, count)` range is preserved, and source-to-LowIR lowering stays in
the typed in-memory model. It follows section 4 by accounting for work in
consumed facts and emitted IR, and section 7 by recording structural evidence
without timing claims. Explicit destructor calls, local automatic lifetime
ordering, constructor cleanup, label/return lowering, demand-driven helper
emission, and the closed typed bit-field/local-class boundaries remain
unchanged. The audit found no justified repair to local-EH chaining: a full
repair for a throwing local destructor would require a separate path-sensitive
lifetime owner, outside this suffix boundary.

## Failure Map

The supplied turn-start/landed stage authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`208/243` passed, `35` failed, and all `243/243` identities were covered.
Fresh post-repair `make test-pa16` reproduces that result with exit `2`; its
durable log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-test-20260830.log`.
The exact sorted comparison is at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-identity-delta-20260830.log`:
fresh-only `0`, authority-only `0`, unrecognized `0`, and `243/243` coverage
against the 243-file inventory.
The exact final failure set is:

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
- `pa16/tests/general/300-synthesized-array-member-lifecycle.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-bitfield-aggregate-init.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

Compared with the preserved pre-landed baseline recorded in the inherited
identity evidence
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-identity-delta-rerun-20260830.log`
(`206/243`, 37 failures, `243/243` covered), baseline-only is exactly:

- `pa16/tests/general/200-destructor-body-local-before-base-destruction.t`
- `pa16/tests/general/200-member-object-lifetime.t`

Final-only is empty. The failure count therefore decreases by two without a
coverage change; the PA16 stage still has unrelated residual failures. The
bounded source repair is fail-closed for malformed model facts and the fresh
broad run confirms that it introduces no failure-identity or coverage delta.

## Active Checkpoint

The reviewed implementation is limited to these four PA15 lowering files:

- `dev/src/pa15_lowering_flow.cpp` installs and closes the destructor-body
  handler, keeps it active through return-owned local destruction, and emits
  normal/unwind suffix paths.
- `dev/src/pa15_lowering_construction.cpp` validates each exact action,
  flattens scalar and array terminals in action order, recomputes typed
  addresses, and emits normal remaining-suffix cleanup prefixes.
- `dev/src/pa15_lowering.h` carries the lowering-only cleanup block and
  transient terminal declarations.
- `dev/src/pa15_lowering.cpp` initializes the new lowering state.

The exact owner/data flow is
`FunctionFact.destructor_action_begin/count` ->
`checked_destructor_function` -> `model_.destructor_actions_` ->
`lower_destructor_action` (one exact action flattened into typed terminals) ->
one ordered terminal sequence -> `destructor_subobject_address` plus typed
array-path replay -> emitted destructor calls. `active_destructor_cleanup_` is
only a block identity; `DestructedElement` is transient lowering data, not
duplicate semantic ownership. The same sequence serves normal emission with
explicit remaining-suffix prefixes and body-unwind emission without borrowing
normal-path SSA values. The audit repair proves exact member/base `TypeId`
ownership, canonical destructor binding, valid target enum, and void call ABI
at the point where the action enters this sequence.

Exclusions are the remaining 35 identities, tests and fixtures, reference
execution, source-text reconstruction, semantic-fact redesign, broad
whole-program passes/caches, and unrelated constructor/local-lifetime changes.
No source sets or generated oracle data were changed.

The lowerer orders a reachable destructor-body return as local lifetime
destruction under the body handler, `eh_end`, then the typed suffix. Ordinary
compound fallthrough removes scope-owned lifetimes before the same suffix.
An end-to-end early-return destructor probe remains unvalidated because the
existing PA12 semantic check rejects it before lowering with
`PA12 retained return owner is not definition-owned`. A further bounded
uncertainty is exception propagation from a local lifetime destructor: the
landed body handler owns the class member/base suffix, while full cleanup of
other live locals is a separate lifetime-path design.

## Performance Evidence

Each emission mode reads the exact semantic range once to build its transient
terminal sequence, then intentionally revisits terminals while emitting
normal and body-unwind paths. For `N` flattened terminals and maximum
array-path depth `D`, collection is `O(ND)` per mode and normal
cleanup-prefix emission is intentionally `O(N^2D)`: the public LowIR contract
has no shared cleanup cursor or implicit remaining-suffix state, so every
potential throw point must carry an explicit, independently recomputed typed
remaining suffix. Body-unwind replay is `O(ND)`. With fixed type-path depth,
the broader output bound is the observed triangular `O(N^2)`; `D` accounts for
required address projections. The audit repair adds constant-time
indexed/sidecar checks per action and does not change these bounds. No
whole-program retry, textual fallback, or unbounded cache/shortcut was added.

Structural evidence from the durable probe log
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-structure-20260830.log`:

- `Derived___Derived`: 3 blocks, 1 body cleanup handler, and 3 calls
  (one local `Guard` call plus the typed `Base` call on each normal/unwind
  path).
- `YB___YB`: 3 blocks, 1 body cleanup handler, and the typed `YA` call on
  each normal/unwind path.
- `Holder___Holder`: 7 blocks, 3 cleanup handlers, 3 resumes, and 9 element
  destructor calls. Its canonical diff has no destructor-function hunk; only
  the pre-existing constructor array shape differs.
- Scaling probes: N=1 has 3 blocks, 1 handler, 2 destructor calls, and 13
  instructions; N=3 has 7, 3, 9, and 44; N=8 has 17, 8, 44, and 174.
  The counts expose the triangular normal prefixes plus the linear unwind
  copy.

## Final Validation and Inherited Evidence

Inherited evidence for landed `70327e4d` remains distinct from the fresh
post-repair gates: full PA16 is at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-test-rerun-20260830.log`,
the inherited identity/coverage delta at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-identity-delta-rerun-20260830.log`,
the inherited through-PA15 gate at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-through-pa15-rerun-20260830.log`,
the inherited file-audit log at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-file-audit-rerun-20260830.log`,
the inherited diff-check log at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-diff-check-final-20260830.log`,
and the inherited changed-file log at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-changed-file-audit-20260830.log`.
Those artifacts describe the landed source before the bounded repair.

Fresh post-repair validation is exact and durable:

- `make test-pa16` exits `2` at `208/243`, with exactly `35` failures and
  `243/243` coverage. The complete output is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-test-20260830.log`.
- `n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
  exits `0` at `1167/1167`. The exact command output is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-through-pa15-20260830.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0`
  with five known header-division warnings and no fatals. Its output is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-file-audit-20260830.log`.
- The exact sorted identity comparison is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-identity-delta-20260830.log`:
  fresh `35` versus authority `35`, fresh-only `0`, authority-only `0`,
  unrecognized `0`, and `243/243` coverage against the `243`-identity
  inventory.
- Fresh post-repair `git diff --check` and bounded changed-file audit each
  exit `0` in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-diff-check-20260830.log`
  and
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-changed-file-audit-20260830.log`.

The earlier focused post-repair evidence remains useful for behavior
localization: `make -C dev cppgm++ CXX=g++` exits `0`, and the seven-test
matrix is `5/7` with only the two known array-presentation mismatches. The
inherited landed nine-test result is `7/9` at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-focused-20260830.log`.

## Checkpoint Ledger

| checkpoint | PA16 identities / coverage | delta / evidence | status |
| --- | --- | --- | --- |
| `1694bc3e` retained baseline | `200/243`, 43 failures, `243/243` covered | prior bit-field baseline | prior landed |
| `7e060b28` typed packed bit-field boundary | `202/243`, 41 failures, `243/243` covered | prior focused bit-field evidence retained | prior landed |
| `d95a6fe7` local-class checkpoint start | `202/243`, 41 failures, `243/243` covered | prior local-class selection | prior checkpoint |
| `d83e927f` typed local-class materialization | `206/243`, 37 failures, `243/243` covered | prior focused matrix, through-PA15, and audit passed | prior landed |
| `70327e4d` typed exception-safe destructor suffix | `208/243`, 35 failures, `243/243` covered | fresh post-repair stage exactly matches the turn-start authority: fresh-only `0`, authority-only `0`, unrecognized `0`; preserved pre-landed baseline `206/243` has exactly the two fixed destructor/lifetime identities; N=1/3/8 structural evidence; bounded repair adds canonical action/type/void-signature checks; focused matrix `5/7` with only two known array-shape residuals; fresh through-PA15 `1167/1167` | final audit complete; checkpoint record and approved repair committed in this checkpoint |

The remaining 35 identities are outside this checkpoint boundary. The next
checkpoint should select one unchanged residual family separately. Future
work must preserve the canonical typed action range, active owner flow,
explicit cleanup semantics, and the recorded output-complexity rationale.
