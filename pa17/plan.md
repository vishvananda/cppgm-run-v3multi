## Stage Design

PA12 owns the semantic value category, selected conversion, and constructor or
call facts.  Record identity and completed `RecordLayout` own the distinct
cached `ClassValueTransferFact`; it is a conservative PA17 proof for a
same-class, complete, non-polymorphic transfer subset, not a claim derived
from `checkpoint_zero_storage_eligible` or a general C++ triviality trait.
The demand-driven fact checks typed special-member declaration metadata and
accepts only flat scalar/pointer/enum/known-array leaves.  PA15 consumes these
typed facts: class-value parameters are indirect object values copied into
parameter storage, direct results remain object results, larger results add
the leading typed indirect result destination, and initializer/argument/return
materialization emits `copyobj` at the source-language transfer boundary.

## Failure Map

Turn-start baseline: 37/228 passing, 191 failures.  Current overlapping
clusters are copy/value (55 keyword-adjacent), ref/xvalue (20), allocation
(26), conversion/operator (41), union (7), and temporary/lifetime (23).
The active boundary is PA16's narrow empty-class value transfer; this
checkpoint generalizes only the stable complete-object trivial transfer path.

## Active Checkpoint

In scope are same-class complete non-polymorphic objects proven by
`ClassValueTransferFact` in pass-by-value and return-by-value paths, temporary
materialization for common copy/call/return cases, and direct `copyobj`
lowering for supported transfers; the existing empty-class/reference behavior
remains compatible.  The fact rejects class and array-of-class subobjects in
this checkpoint, so a nested nontrivial copy/move cannot reach raw copying.
Excluded are field/base-wise synthesized copy/move helpers, assignment helper
synthesis, deleted/defaulted propagation, ref-qualified members, allocation,
unions, and conversion-operator expansion.

Invariants: typed facts never become source text; record/layout identity, not
spelling, decides eligibility; a required construction/materialization cannot
fall back to a raw object load/store; unsupported class transfers stay
rejected.  Validation is the four named transfer tests plus a passing
class-reference guard and the two nontrivial guards; broad gates follow this
correction pass.

## Performance Evidence

`ClassValueTransferFact` setup is O(m + w) for a demanded record, where m is
its cached layout member count and w is the bounded canonical cv/array wrapper
chain; class subobjects stop the check without recursive record traversal.
Its state is cycle-safe for any future recursive proof, and completed hot uses
are O(1).  Function/call lowering remains O(parameters + arguments + emitted
IR), with no per-use class-scope scan, textual identity reconstruction, or
recursive layout walk.  The correction focus rebuilt the affected compiler in
24.4 seconds wall time; peak RSS was not sampled.  Required full-stage
`/usr/bin/time` wall/max-RSS evidence is recorded below.

## Checkpoint Ledger

- Baseline: 37/228 passing, 191 failures.
- Focused result (original milestone): 5/5 (four named transfer cases plus the class-reference guard).
- Focused result (post-correction): 5/5, with the same five tests after the
  cached eligibility and current-owner corrections.
- Nontrivial guards: 0/2 against compile-pass references; both actual statuses
  are `EXIT_FAILURE` with `ERROR: PA12 no viable function`.  No `.check` LowIR
  was emitted and neither guard has a `copyobj` artifact; current artifacts are
  the two `.check.exit_status`/`.check.stdout` pairs under
  `pa17/tests/general/` for the named guards.
- `make test-pa17`: 45/228 passed, 183 failures; all 228 tests remained in the
  stage and the failure count fell by 8 from baseline.  The command exited 2
  because the remaining checked-in failures are expected at this checkpoint.
  `/usr/bin/time` recorded `wall=1.04 maxrss_kb=9928` in
  `/tmp/pa17-stage.time`.
- Prior-through result: `n=17; ... make test-report-through-pa16` exited 0,
  `1410/1410` passed.
- Audit: `perl scripts/cppgm_file_audit.pl --stage pa17 --paths dev/src`
  passed with six existing header-division warnings.
- Remaining scope: nontrivial nested class/array subobjects, synthesized
  field/base-wise special members, assignment synthesis, deleted/defaulted
  propagation, ref-qualified members, allocation, unions, and conversion
  operators remain excluded; residual failures remain in the overlapping
  copy/value, ref/xvalue, allocation, conversion/operator, union, and
  temporary/lifetime clusters listed above.
- Commit: pending final PA17 implementation commit; populate after commit.
