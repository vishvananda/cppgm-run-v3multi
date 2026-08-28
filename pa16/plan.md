# PA16 implementation plan

## Stage design

PA11 owns one sparse `AggregateElementFact` arena and one typed range sidecar
per `BracedInitList`; `AggregateElementFact.initializer` is the sole aggregate
destination edge. PA12 performs declaration-ordered typed appertainment, brace
elision, omitted-tail interpretation, C++11 aggregate eligibility, and tested
list-narrowing rejection. PA15 validates owner, type, range, and index identity
before emitting stores, reference aliases, constructor calls, and zero/default
initialization. Typed literal-content interning uses PA11-owned decoded bytes.
Synthetic aggregate forwarding bindings are lowering-only. Unions, bases,
copy/move transfer, templates, virtual/multiple inheritance, and unrelated
parser/operator/lifetime surfaces remain out of scope.

The spec alignment is §§1, 2, 4, 5, and 7: one shared typed pipeline, one
canonical owner, demand-driven bounded work, complete typed lowering without
rediscovery, and deterministic structural evidence.

## Checkpoint result and failure map

The landed checkpoint is `dea01c52089fe78b8d23cce0b72ecbe8686ddb26`
(`dea01c52`, aggregate initialization), parent `36b93869`. The authoritative
turn-start record in `last-test.log` is `159/243` passing, `84` failures, and
`243/243` identities covered. Final `make test-pa16` is `164/243` passing,
`79` failures, and `243/243` covered. Exact comparison has five
baseline-only repairs and final-only `∅`:

- `general/200-global-class-array-enum-trivial-dtor.t`
- `general/200-global-scalar-dynamic-init.t`
- `general/200-local-struct-array-init.t`
- `general/300-namespace-aggregate-array-string-members.t`
- `general/300-static-member-aggregate-array-dynamic-init.t`

The complete exact before/after maps are preserved as
`baseline-failures.txt` and `final-failures.txt` in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-aggregate-init-audit-final-v1`.

## Focused result

The exact 17-test focus from this plan is `12/17` passing, `5` failing, and
`17/17` covered. The remaining identities are
`general/100-global-aggregate-nested-array-initializer.t`,
`general/200-defaulted-constructor-still-aggregate.t`,
`general/200-deleted-constructor-still-aggregate.t`,
`general/300-value-init-aggregate-with-nontrivial-member.t`, and
`general/400-bitfield-aggregate-init.t`. The other 12 identities pass; the
per-identity result is in `focused-results.tsv` in the final evidence path.
Courses 404, 409, 412, and 415 all exit zero. Through-PA15 is `1167/1167`.
The file audit exits zero with five pre-existing header-division warnings, and
`git diff --check` exits zero.

## Structural evidence

The immutable historical evidence at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-aggregate-init-evidence`
is preserved. The separate final replay is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-aggregate-init-audit-final-v1`.
The final compiler SHA-256 is
`62f6feea601662cb601f12c3ad3b9083f4da85639c2e2b741cf24c7a31721d4b`.
The replay has 30/30 zero-status runs: nine semantic pairs and six LowIR
pairs, with zero repeated-hash mismatches. `probe-runs.tsv` records repeated
hashes and sizes; `structural-counts.tsv` records source/output sizes plus
semantic list/action/literal and LowIR store/call/projection counts.

Bounds 16, 1024, and 1000000 each produce 17 semantic lines, one aggregate
list, and no per-element aggregate descendants. Explicit nested, brace-elided,
reference/class, fixed string-pointer, and the exact formerly residual
unknown-bound namespace handout all pass twice with matching hashes. No timing,
RSS, allocation, or speedup claim was measured. The older unknown-bound
residual result remains only as historical evidence; the current handout path
passes.

## Required validation and next checkpoint

Recorded final commands are `make test-pa16` (exit 2, expected while residuals
remain), the exact `n=16` through-PA15 command (exit 0),
`perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` (exit 0),
controls 404/409/412/415 (all exit 0), and `git diff --check` (exit 0).
Exact logs and statuses are in the final evidence directory. No fixtures or
references changed, so no regeneration was required.

The next checkpoint should select the five focused aggregate LowIR/semantic
residuals or another explicitly staged PA16 surface. This checkpoint does not
claim PA16 completion.

## checkpoint ledger

| checkpoint | result |
| --- | --- |
| `dea01c52` aggregate-initialization checkpointAudit | Completed bounded PA10--PA15 audit/repair; final `164/243`, `79` failures, `243/243` covered versus turn-start `159/243`, `84` failures, with five baseline-only repairs and final-only `∅`; focus `12/17`, all `17/17` covered; controls 404/409/412/415, through-PA15, file audit, and diff-check pass; final deterministic structural replay is `30/30` zero-status runs with no timing/RSS claim. PA16 remains incomplete. |
| `36b93869` handoff | Turn-start aggregate checkpoint state: `159/243`, `84` failures, `243/243` covered; PA1--PA15 baseline `1167/1167`; immutable historical evidence preserved. |
