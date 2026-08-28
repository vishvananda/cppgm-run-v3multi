# PA16 implementation plan

## Stage Design

PA11 owns the typed semantic identity of fixed compiler builtins.  PA12
recognizes the four supported names only as compiler fallbacks when ordinary
value lookup is empty, sends evaluated and type-only arguments through the
ordinary typed function-selection/conversion path, and publishes an
append-only boundary fact for a selected helper.  PA15 consumes that binding
and fact to plan a direct declaration and maps the typed effects, unwind,
return, and parameter facts to LowIR.  Builtin bindings remain outside
lexical scope lookup; `__builtin_constant_p`, `__builtin_abort`, reserved
prefix behavior, ordinary user lookup, and existing direct `noexcept`
sidecars retain their established paths.

The design follows spec §§1--5 and 7: one typed pipeline, canonical semantic
identity, demand-driven bounded work, and typed LowIR without source-spelling
rediscovery.  PA16 object-model, aggregate, lifetime, and unrelated parser
surfaces remain outside this checkpoint.

## Failure Map

Typed-builtin checkpoint turn-start commit
`3c2114b6ddd911989c45f52b36890743adbbd490` (parent
`dea01c52089fe78b8d23cce0b72ecbe8686ddb26`) is the clean baseline:
`164/243` PA16 identities pass, `79` fail, and `243/243` are covered;
PA1--PA15 pass `1167/1167`.  The authoritative landed failure context is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
Relative to the parent, the landed increment repairs exactly these three
pre-lowering `PA12 unknown expression name` failures:

- `general/200-function-boundary-metadata-emission.t`
- `general/200-parameter-access-metadata-emission.t`
- `general/200-parameter-alias-metadata-emission.t`

The landed result is `167/243` passing, `76` failing, and `243/243` covered;
the parent is `164/243`, `79` failing, and `243/243` covered.  Thus the
baseline-only set is exactly the three identities above and the final-only set
is `∅`.  The complete exact current 76-identity set is preserved in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-typed-builtin-final-A6D3WT/failure-map.txt`.
The prior aggregate checkpoint's exact maps and evidence remain preserved in
`pa16-aggregate-init-audit-final-v1`; no coverage identity changed here.

## Active Checkpoint

The d7ed98aa boundary is audited: `__builtin_strlen`,
`__builtin_unreachable`, `__builtin_memcpy`, and `__builtin_memmove` retain
fixed typed signatures; truthful `readonly`, `readnone`, or `readwrite`
effects; `unwind=no`; `noreturn` for unreachable; and the specified pointer
capture/access/alias metadata.  Bare `noexcept` on `pure()` remains
`unwind=no` through its existing sidecar.  The bounded audit also repairs
ordinary lookup shadowing and type-only `decltype` argument validation by
reusing the canonical PA12 path.

Result: the authoritative landed PA16 result is `167/243` passing with `76`
failures and `243/243` identities covered.  The audit repairs are included in
this completed checkpoint, and the final gates found no new handout regression.
PA16 remains
incomplete because 76 residual identities remain.

## Next Checkpoint

For the next checkpoint, select one bounded identity family from the exact
76-test residual map.  Preserve this typed builtin owner and its focused
controls; do not treat the current checkpoint as PA16 completion.

## Performance Evidence

The fixed builtin descriptor arena has at most four entries.  Binding/fact
lookup is a bounded scan over that fixed set; exact typed names add one
ordinary lookup for shadowing, and each call performs one fixed selection plus
typed checking/conversion proportional to its argument count.  Type-only
validation uses a tail guard around the same selector.  PA15 declaration
planning and materialization iterate only the instantiated descriptor set and
demanded bindings.  No whole-program rescan, textual round-trip, host
compiler, or shell-out is introduced.

The landed-increment structural evidence is in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-typed-builtin-final-A6D3WT/probe-results.md`:
wrong-arity and incompatible-argument probes both exit `1`; an unused source
has zero fixed-builtin declarations/calls; a one-`strlen` source has exactly
one of each; two runs produce identical LowIR.  Post-repair probes reproduce
unused `0/0`, one-`strlen` `1/1`, and repeated LowIR SHA-256
`849f81f6e3117a83c74e7f622aad199812090e621c040e3a23f969ade7678274`.
No timing, RSS, allocation, or speedup claim is made.

## Validation

This bounded turn completed:

- `make -C dev cppgm++` — exit `0`.
- The three named PA16 boundary tests — `3/3` pass.
- PA12 legacy builtin controls — `3/3` pass.
- PA16 prefix/noexcept controls — `4/4` pass.
- Course control 416 — pass for ordinary exact/reserved-prefix lookup,
  local shadow rejection, invalid `decltype` argument rejection, and valid
  unevaluated `decltype` with no builtin marker.
- `git diff --check` — exit `0`.

The selected five-test preservation batch was `4/5` because the known
`general/300-unary-address-of-builtin-fallback.t` LowIR mismatch remains; the
other four controls pass.  Final `make test-pa16` exits `2` with `167/243`
passing, `76` failing, and `243/243` covered; exact comparison has
`final-only=0` and `missing-authoritative=0`.  The required through-PA15 gate
exits `0` at `1167/1167`, and final file audit exits `0` with five known
warnings.  No handout test or `.ref` fixture changed; course control 416 was
added under `cppgm.tests/course/pa16/`.

## Checkpoint Ledger

| checkpoint | result |
| --- | --- |
| `3c2114b6` typed-builtin turn-start | Clean baseline: `164/243` passing, `79` failures, `243/243` covered; parent `dea01c52`; PA1--PA15 `1167/1167`. |
| `dea01c52` aggregate-initialization implementation | Earlier historical implementation commit (parent `36b93869`): `164/243` passing, `79` failures, `243/243` covered versus its `159/243`, `84`-failure turn-start; five baseline-only repairs, final-only `∅`; aggregate focus `12/17`; through-PA15, file audit, and diff-check passed; PA16 remained incomplete. |
| `36b93869` handoff | Historical aggregate parent/handoff state: `159/243` passing, `84` failures, `243/243` covered; PA1--PA15 `1167/1167`; immutable evidence preserved. |
| `d7ed98aa` typed-builtin-boundary checkpointAudit | Added and audited the demand-driven typed semantic owner and PA15 LowIR declaration path; final PA16 is `167/243`, `76` failures, `243/243` covered versus parent `164/243`, `79` failures, with exactly three baseline-only fixes and final-only `∅`. The bounded audit repairs visible typed-builtin lookup shadowing and type-only `decltype` validation. Final broad PA16, through-PA15 `1167/1167`, five-warning file audit, focused controls, structural demand/determinism probes, and diff-check pass; the known address-of-builtin mismatch remains. PA16 remains incomplete. |
