# PA16 implementation plan

## Stage Design

PA11 owns the typed semantic identity of fixed compiler builtins.  PA12
recognizes the four supported names, creates a binding and immutable boundary
fact only when a call is selected, and sends arguments through the ordinary
typed function-selection/conversion path.  PA15 consumes that binding and
fact to plan a direct declaration and maps the typed effects, unwind, return,
and parameter facts to LowIR.  Builtin bindings remain outside lexical scope
lookup; `__builtin_constant_p`, `__builtin_abort`, ordinary user lookup, and
existing direct `noexcept` sidecars retain their established paths.

The design follows spec §§1--5 and 7: one typed pipeline, canonical semantic
identity, demand-driven bounded work, and typed LowIR without source-spelling
rediscovery.  PA16 object-model, aggregate, lifetime, and unrelated parser
surfaces remain outside this checkpoint.

## Failure Map

Typed-builtin checkpoint turn-start commit
`3c2114b6ddd911989c45f52b36890743adbbd490` (parent
`dea01c52089fe78b8d23cce0b72ecbe8686ddb26`) is the clean baseline:
`164/243` PA16 identities pass, `79` fail, and `243/243` are covered;
PA1--PA15 pass `1167/1167`.  The complete authoritative failure context is
in `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`;
this checkpoint intentionally scopes work to these three pre-lowering
`PA12 unknown expression name` failures:

- `general/200-function-boundary-metadata-emission.t`
- `general/200-parameter-access-metadata-emission.t`
- `general/200-parameter-alias-metadata-emission.t`

The prior checkpoint's exact before/after maps remain in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-aggregate-init-audit-final-v1`:
five baseline-only repaired identities, final-only `∅`, and no coverage loss.
Its focused aggregate result was `12/17`, with `17/17` identities covered;
the current exact failure map is recorded in the final external evidence
directory below.

## Active Checkpoint

Implement demand-driven typed call boundaries for `__builtin_strlen`,
`__builtin_unreachable`, `__builtin_memcpy`, and `__builtin_memmove`.
Required LowIR facts are: fixed typed signatures; truthful `readonly`,
`readnone`, or `readwrite` effects; `unwind=no`; `noreturn` for unreachable;
and the specified pointer capture/access/alias metadata.  Bare `noexcept` on
`pure()` must remain `unwind=no` through its existing sidecar.

Result: broad PA16 is `167/243` passing with `76` failures and `243/243`
identities covered.  Relative to the turn-start `164/243` and `79` failures,
the three named builtin-boundary identities are baseline-only fixes and there
are zero final-only regressions.  PA16 remains incomplete because 76 residual
identities remain.

## Performance Evidence

The fixed builtin descriptor arena has at most four entries.  Binding/fact
lookup is a bounded scan over that fixed set; each call performs one fixed
selection plus ordinary typed checking and conversion work proportional to its
argument count.  PA15 declaration planning and materialization iterate only
the instantiated descriptor set and demanded bindings.  No whole-program
rescan, textual round-trip, host compiler, or shell-out is introduced.

Representative structural evidence is in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-typed-builtin-final-A6D3WT/probe-results.md`:
wrong-arity and incompatible-argument probes both exit `1`; an unused source
has zero fixed-builtin declarations/calls; a one-`strlen` source has exactly
one of each; two runs with compiler SHA-256
`33e595c64780ed17766c95194b9b141863b51c1ca6d9e4001c723ddd9d9e402c` produce
the identical LowIR SHA-256
`7937fc3b81da70a71efb35e7e02910c5358b4818036b23e25faf6461cdde833c`.
No timing, RSS, allocation, or speedup claim is made.

## Validation

Completed:

- `make test-pa16` — exit `2`; `167/243` pass, `76` fail, `243/243` covered.
  Comparing exact identities with the authoritative turn-start map gives
  baseline-only `{200-function-boundary-metadata-emission.t,
  200-parameter-access-metadata-emission.t,
  200-parameter-alias-metadata-emission.t}` and final-only `∅`.
- Exact prior command `n=16; ... make test-report-through-pa$((n - 1))` —
  exit `0`, `1167/1167` through PA15.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` — exit `0`,
  five known warnings.
- `git diff --check` — exit `0`.
- Final focused targets — `3/3` pass, exit `0`.
- Legacy `__builtin_constant_p`/`__builtin_abort` plus reserved-prefix
  controls — `4/4` pass, exit `0`.
- Ordinary user-function lookup — `1/1` pass, exit `0`.
- Bare/conservative `noexcept` controls — `2/2` pass, exit `0`.

The external evidence directory above contains the probe source/output
artifacts and structural counts.  No test or `.ref` fixture changed.

## Checkpoint Ledger

| checkpoint | result |
| --- | --- |
| `3c2114b6` typed-builtin turn-start | Clean baseline: `164/243` passing, `79` failures, `243/243` covered; parent `dea01c52`; PA1--PA15 `1167/1167`. |
| `dea01c52` aggregate-initialization implementation | Earlier historical implementation commit (parent `36b93869`): `164/243` passing, `79` failures, `243/243` covered versus its `159/243`, `84`-failure turn-start; five baseline-only repairs, final-only `∅`; aggregate focus `12/17`; through-PA15, file audit, and diff-check passed; PA16 remained incomplete. |
| `36b93869` handoff | Historical aggregate parent/handoff state: `159/243` passing, `84` failures, `243/243` covered; PA1--PA15 `1167/1167`; immutable evidence preserved. |
| `PA16 typed-builtin-boundary` | Added the demand-driven typed semantic owner and PA15 LowIR declaration path; final PA16 `167/243`, `76` failures, `243/243` covered; exactly three baseline-only fixes and zero final-only regressions; through-PA15 `1167/1167`, audit exit `0` with five warnings, diff-check exit `0`; focused/legacy/ordinary/noexcept controls `3/3`, `4/4`, `1/1`, `2/2`; PA16 remains incomplete. |
