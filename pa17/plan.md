# PA17 Checkpoint Plan

## Spec Alignment

The landed `868403c6` increment, whose checkpoint entry was recorded at
`18aafb67`, keeps PA12 conversion, selection, construction, and call facts
typed; uses canonical record/layout identity for `ClassValueTransferFact`; and
lets PA15 lower only complete, non-polymorphic, no-base, flat trivial
same-class transfers. The bounded audit correction also records a declared
same-class rvalue assignment operator in the owning typed sidecar before the
eligibility fact can be used. References, volatile sources, unions, nested or
nontrivial class objects, and unsupported arrays remain guarded. Parameters,
returns, hidden indirect results, materialization, and direct `copyobj` follow
the PA17 Assignment Boundary and the typed PA13 object ABI. Implementation is
under `dev/` and `dev/src/`; the only added test is a compliant status-only
course regression with no LowIR fixture.

## Failure Map

The historical initial checkpoint baseline was 37/228 passing with 191
failures. The landed milestone improved that to 45/228 (+8), with 183 failures
and all 228 original tests covered. The overlapping residual clusters remain
copy/value (55), ref/xvalue (20), allocation (26), conversion/operator (41),
union (7), and temporary/lifetime (23); these counts are diagnostic overlaps,
not a partition. The current full stage is 51/229: 50/228 original tests pass,
178 original tests fail, and the added course regression is the 1/1 additional
status pass. Identity comparison against the supplied primary log found no
former-pass regression or new failure and five recovered original failures.

The original focused evidence was 5/5, and the post-correction focused
evidence was also 5/5. The current focused set is 9/9: eight existing
supported/reference-backed probes plus the new move-assignment status probe.
The two named nested/nontrivial guards,
`tests/general/200-nested-subobject-pass-return-by-value.t` and
`tests/general/300-generated-move-constructor-nontrivial-member.t`, remain
rejected with `EXIT_FAILURE` / `PA12 no viable function`; they are residual
out-of-scope behavior, not hidden fixture edits.

## Performance Evidence

Eligibility is O(m + w) on first demand, for the bounded layout member list and
cv/known-array wrapper chain, with O(1) cached hits. The typed move-assignment
marker performs a bounded function-signature/owner lookup at declaration or
definition time; it does not scan class scope, retry broadly, or create a
recursive child dependency. Call lowering remains O(parameters + arguments +
emitted IR), including hidden-result and class-argument mapping.

Historical/non-comparative evidence retained from the earlier checkpoint is a
24.4s correction build with RSS not sampled and a stage sample of
`wall=1.04s`, `maxrss=9928kB`. The current descriptive full-stage sample is
`wall=1.27s`, `maxrss=20808kB`; it is recorded for context, not as a
comparative performance claim.

## Validation and Identity

The exact required prior-through command exited 0:
`===== ALL TESTS PASSED SUCCESSFULLY! (1410 / 1410) =====`.
`perl scripts/cppgm_file_audit.pl --stage pa17 --paths dev/src` exited 0 with
the six established nonfatal header-division warnings. `make test-pa17`
exited 2 with `===== TEST SUMMARY: 51 / 229 TESTS PASSED =====`, within the
checkpoint allowance of no more than 183 failures. The 228 original test
identities were all present in the current run; the five recovered identities
were `100-copy-constructor-default-parameter`,
`100-derived-converting-ctor-beats-base-copy`,
`200-pass-return-forwarding`, `300-empty-class-copy-member-address`, and
`300-function-pointer-class-return-call`. No original former pass regressed.
The separately added course case expected and produced `EXIT_FAILURE`.

## Next Checkpoint

Next substantive checkpoint: select and implement a separately bounded PA17
general special-member propagation slice, beginning with deleted/defaulted
copy/move construction under its own typed eligibility boundary. Assignment
synthesis, ref-qualified members, allocation, unions, conversion operators,
and broad temporary/lifetime work remain separate checkpoints.

## Checkpoint Ledger

| item | state | evidence |
| --- | --- | --- |
| landed ClassValueTransferFact increment | completed before this audit | implementation `868403c6`; ledger entry `18aafb67` |
| checkpointAudit | completed | source/spec/test audit; move-assignment correction; focused 9/9; prior-through `1410/1410`; file audit passed with 6 warnings; full stage `51/229`; identity comparison clean; final bounded changes committed |

Durable historical evidence: the original/post-correction focused result was
5/5; the two nontrivial guards were rejected; the original milestone result
was 45/228 (+8) with 183 failures and 228 tests covered; prior through-PA16
was 1410/1410; the prior file audit passed with six warnings; and the prior
24.4s build plus `wall=1.04s`, `maxrss=9928kB` sample were historical and
non-comparative.
