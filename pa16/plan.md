# PA16 implementation plan

## Checkpoint status

The completed checkpoint is landed commit
`0a6be82d9bf17db2585772f2be28d45e6af781de` (`PA16: add typed array lifetime
cleanup`), relative to `5d91986f166e000daddecaf112e0cb58df6a8e8b`, with the
bounded audit repairs and course regression 410.  Its active scope is fixed-
bound local automatic arrays of class objects and recursive synthesized
array-member lifetime.  Global/static/TLS lifetime and guards, copy/move or
by-value transfer, virtual/multiple inheritance, templates, new/delete, and
unrelated operator/access/temporary machinery remain excluded.

The exact full-stage identity result is unchanged from the turn-start log
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:

| normalized set | turn start | final | delta |
| --- | ---: | ---: | --- |
| passing | 93 | 93 | `∅` |
| failing | 150 | 150 | additions `∅`, removals `∅` |
| covered | 243/243 | 243/243 | additions `∅`, removals `∅` |

The final failure map is exactly the same 150 normalized `pa16/tests/**/*.t`
identities printed by the final log
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-test.log`;
there is no new baseline failure and no coverage loss.  The four affected-path
handout comparison identities still present in that map are
`200-destructor-body-local-before-base-destruction.t`,
`200-local-default-class-array-lifecycle.t`,
`200-member-object-lifetime.t`, and
`300-synthesized-array-member-lifecycle.t`.  No fixture, reference, comparator,
or generated test output was changed or tracked.  The exact normalized set
comparison is recorded in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-identity-compare.log`.

## Spec alignment

The implementation follows `pa16/README.md` and `spec.md` §§2–5 and §7:

- canonical `TypeId`, `BindingId`, `ScopeId`, `NamedRecordId`, and
  `FunctionFact` identities carry array shape, object ownership, destructor
  selection, and action ranges from PA10/PA11 through PA12 into PA15;
- fixed bounds and checked typed byte strides are used for recursive array
  construction/destruction, with declaration-order construction, reverse
  destruction, and completed-prefix cleanup on a throwing constructor;
- automatic local lifetime becomes active only after initialization, and
  return, fallthrough, branch, switch, break, continue, and lexical scope
  transitions destroy the active typed suffix; unsupported `goto` fails closed;
- PA15 demand and LowIR emission consume typed facts and emit constructor,
  destructor, `eh_try`, `eh_cleanup`, `eh_end`, and `resume` operations with
  checked ranges and truthful unwind metadata; and
- work is deterministic and bounded, with arena ranges snapshotted before
  recursive demand and no textual recovery or reference/host-compiler use.

## Ownership path and completed work

```text
PA10 local class-object/array syntax
  -> PA11 typed array shape + canonical class/destructor identities
  -> PA12 LifetimeFact and ordered constructor/destructor action ranges
  -> PA15 typed demand, recursive construction/destruction, active state,
     checked paths, and completed-prefix EH cleanup
  -> LowIR calls, scope/control-exit cleanup, and unwind metadata
```

`index_lifetime_facts()` is called once while indexing the completed semantic
model.  For each lifetime it validates exact object type equality, variable
binding ownership, ultimate class record, checked destructor identity, and
scope ancestry.  The ancestry walk is bounded by the scope count and must
reach one valid Function scope; malformed or cyclic chains fail closed.  It
sets a dense `ScopeId`-indexed byte flag.  `lower_function` validates its
FunctionFact scope and uses that flag in O(1), preserving the conservative rule
that any nontrivial lifetime anywhere in a function rejects `goto`.  Indexing
cost is O(L log L + sum ancestry depth) for L lifetime facts (bounded by
O(L log L + L*S) for S scopes), with O(S) flags; it does not rescan all L facts
for every function.  The related relocation keeps `pa15_lowering.cpp` below
the 3000-line file-audit limit.

PA15 recursively follows typed constructor/destructor actions and demand roots,
checks array bounds and `ordinal * type_size(child)` before index conversion,
recomputes saved typed root/path addresses during cleanup, and materializes
each shared prefix node once.  Lifetime activation, lexical markers, branch
restoration, loop/switch joins, for-init exit, early return, fallthrough, and
control exits are covered by course 410.  Member and base action ranges,
destructor function identity, and owner/range checks remain fail closed.

## Final evidence and gates

Focused build and controls:

- `make -C dev cppgm++` exits `0`; log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-build.log`.
- `sh -n` plus course controls 408, 409, and 410 exit `0`; log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-focused.log`.
- Course 410 prints E=8/16/32 cleanup calls/main lines `7/129`, `15/257`,
  and `31/513`; its nested `[2][3]` control verifies six reverse calls,
  outer strides `1,0`, and inner indices `2,1,0,2,1,0`.

Required gates:

- `make test-pa16` exits `2` at the unchanged `93/243`; log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-test.log`.
- `n=16; ... make test-report-through-pa15` exits `0` at `1167/1167`; log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-through-pa15.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0`
  with five existing header-division warnings and no fatal issue; log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-file-audit.log`.
- `git diff --check` exits `0`; log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-diff-check.log`.

The final smoke/scale performance log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-performance.log`.
It uses one immutable `0555` compiler copy (SHA-256
`be89e2a8efdc723f3e2947f8df48bf00b72333f52750f61dddbc6bd61539ad14`), the
same input per E, and five interleaved batches of 20 invocations.  Median
batch results are:

| E | wall | user | system | max RSS |
| --- | --- | --- | --- | --- |
| 32 | `0.09s (0.09..0.09)` | `0.04s (0.04..0.04)` | `0.05s (0.04..0.05)` | `6516 (6448..6564) KiB` |
| 128 | `0.18s (0.18..0.19)` | `0.09s (0.09..0.10)` | `0.09s (0.08..0.09)` | `8500 (8496..8628) KiB` |

These are representative smoke/scale measurements, not a benchmark claim or
an allocation measurement.  Final structure is deterministic: E=32 has 513
main lines and 31 cleanup nodes/calls, while E=128 has 2049 main lines and
127 cleanup nodes/calls.  The fourfold element increase gives fourfold main
line growth and cleanup calls remain `E-1`, corroborating the intended
O(E·D) cleanup-chain work for array nesting depth D.  The dense lifetime index
adds one O(1) per-function lookup after its one-time bounded construction.

## Completed checkpoint ledger

| checkpoint | status |
| --- | --- |
| PA16 typed fixed-bound local/synthesized array lifetime | Completed bounded audit and repair; final `93/243`, 150 failures, exact unchanged failure/coverage identities, course 410 green, through-PA15 `1167/1167`, file audit pass with five warnings, and diff-check pass. |
| PA16 typed parameterized constructor selection | Prior checkpoint retained in `pa16/audit.md`. |
| PA16 typed class-object construction | Prior checkpoint retained in `pa16/audit.md`. |
| PA16 typed static data and static member ownership | Prior checkpoints retained in `pa16/audit.md`. |
| Required gate row | Full PA16, through-PA15, file audit, focused controls, and diff-check evidence recorded above; the checkpoint change is bounded to the seven files in this plan. |

## Next implementation checkpoint

PA16 remains incomplete.  The next checkpoint stays within PA16: resolve the
remaining local automatic/synthesized lifetime semantic and checked-reference
shape cases after separately scoping unrelated PA16 failures.  Do not advance
this ownership path to PA17; global/static/TLS lifetime, value transfer, and
virtual/multiple inheritance remain deferred.
