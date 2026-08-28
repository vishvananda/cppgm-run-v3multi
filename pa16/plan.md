# PA16 implementation plan

## Stage Design

PA11 owns one sparse `AggregateElementFact` arena and one typed range sidecar
per `BracedInitList`; `AggregateElementFact.initializer` is the sole aggregate
destination edge.  PA12 performs declaration-ordered typed appertainment,
brace elision, omitted-tail interpretation, C++11 aggregate eligibility, and
the tested floating-to-integral list-narrowing rejection.  PA15 validates range,
owner, type, and strictly increasing index identities before emitting stores,
reference aliases, constructor calls, and zero/default initialization.  Literal
content interning uses a typed key and PA11-owned decoded bytes on collision.
Synthetic aggregate forwarding bindings are lowering-only and are excluded
from ordinary constructor lookup.  Unions, bases, copy/move transfer,
templates, and unrelated parser/operator/lifetime work remain out of scope.

## Failure Map

Turn-start baseline is HEAD `36b93869`: PA16 `144/243` passed, `99` failed,
and `243/243` identities were covered.  The selected cluster is explicit
nested braces, brace elision, omitted tails, arrays of records,
reference/class subobjects, string members, value initialization,
defaulted/deleted constructors, bit-fields, and global/static aggregates.
The complexity target is O(aggregate subobjects + initializer clauses), with
no bound-sized scalar-tail arena, source-text recovery, whole-program retry,
or per-key rollback erase.

## Active Checkpoint

The exact 17-identity focus is `9/17` passing, `8` failing, `17/17` covered:
the 7 selected baseline-only repairs are `spec/200-aggregate-brace-elision`,
`spec/200-list-init-narrowing-bad`,
`general/200-aggregate-array-member-brace-elision`,
`general/200-aggregate-class-member-subobject-init-target`,
`general/200-aggregate-reference-member-binds-storage`,
`general/200-global-class-array-init`, and
`general/300-value-init-empty-functional-cast-aggregate`; final-only is `∅`.
The remaining focused identities are `general/100-global-aggregate-nested-array-initializer`,
`general/200-defaulted-constructor-still-aggregate`,
`general/200-deleted-constructor-still-aggregate`,
`general/200-local-struct-array-init`,
`general/300-namespace-aggregate-array-string-members`,
`general/300-static-member-aggregate-array-dynamic-init`,
`general/300-value-init-aggregate-with-nontrivial-member`, and
`general/400-bitfield-aggregate-init`.

Full PA16 is `159/243` passing, `84` failing, `243/243` covered.  Against the
exact 99-failure baseline, the exact 15 baseline-only repairs are
`general/100-qualified-const-method-definition`,
`general/100-qualified-typedef-const-method-definition`,
`general/200-aggregate-array-member-brace-elision`,
`general/200-aggregate-class-member-subobject-init-target`,
`general/200-aggregate-reference-member-binds-storage`,
`general/200-global-class-array-init`,
`general/200-member-call-implicit-object-cv-overload`,
`general/200-member-call-implicit-this-cv-overload`,
`general/200-member-pointer-const-typedef-return`,
`general/200-out-of-class-getter-only`,
`general/200-reference-member-conditional-lvalue`,
`general/300-reference-member-same-name-as-class`,
`general/300-value-init-empty-functional-cast-aggregate`,
`spec/200-aggregate-brace-elision`, and `spec/200-list-init-narrowing-bad`;
the exact final-only set is `∅`, and coverage additions/removals are `0/0`.
The complete before/after identity files are `baseline-failures-corrected.txt`
and `final-failures-corrected.txt`.
Course controls 404, 409, and 412 returned zero; 409 contains the aggregate
helper-then-ordinary-default owner regression.  `pa12_semantic_resolution.cpp`
only marks zero-argument functional class construction as value-initialization;
`pa15_lowering_member.cpp` is unchanged.

Validation commands run:

```sh
make -C dev cppgm++
make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 CPPGM_TEST_JOBS=1 check TEST='tests/spec/200-aggregate-brace-elision.t tests/spec/200-list-init-narrowing-bad.t tests/general/100-default-member-initializer-aggregate-member.t tests/general/100-global-aggregate-nested-array-initializer.t tests/general/200-aggregate-array-member-brace-elision.t tests/general/200-aggregate-class-member-subobject-init-target.t tests/general/200-aggregate-reference-member-binds-storage.t tests/general/200-defaulted-constructor-still-aggregate.t tests/general/200-deleted-constructor-still-aggregate.t tests/general/200-global-class-array-init.t tests/general/200-local-struct-array-init.t tests/general/300-array-member-empty-paren-value-init.t tests/general/300-namespace-aggregate-array-string-members.t tests/general/300-static-member-aggregate-array-dynamic-init.t tests/general/300-value-init-aggregate-with-nontrivial-member.t tests/general/300-value-init-empty-functional-cast-aggregate.t tests/general/400-bitfield-aggregate-init.t'
sh cppgm.tests/course/pa16/404-typed-implicit-default-demand-regression.sh
sh cppgm.tests/course/pa16/409-typed-constructor-boundary-regression.sh
sh cppgm.tests/course/pa16/412-typed-bit-field-initialization-root-regression.sh
make test-pa16
n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src
git diff --check
```

Earlier PAs pass `1167/1167`; the final audit passed with five pre-existing
header-division warnings and the final diff check passed.

## Performance Evidence

Evidence is under
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-aggregate-init-evidence`.
The immutable baseline executable hash is
`d1352cd1c16bcd58587ee9ad201a56665819e671933db979c8df1aea6124c41b`; the
current executable hash is
`4b293a9e2a9bf2b634a7f153d17ebbbca6ca44d60d074ed762043e33f8b80b3f`.
The summary hash is `dcbe914034b5f228513bf2b37cb64bedd6865590a576ee86358fbc47ef7efee6`,
the structural table hash is
`cc43d0eceedb1a6a09010aae9c10133cac850511299b63b3eaba932bc82e7453`, and the
artifact manifest hash is `835cbad1660f33a7c027dd3a59d28ab360b6424356d0dd1eb97dc5b264bd809d`.
Repeated semantic and
LowIR output hashes, source/output sizes, and structural counters are in
`probe-runs.tsv` and `structural-counts.tsv`; `evidence-summary.md` records
the representative rows.  Bounds 16, 1024, and 1000000 for an omitted scalar
tail each produce 17 semantic lines, one empty aggregate list, and no
per-element aggregate descendants.  Fixed-bound namespace string-pointer
records succeed; the unknown-bound form is an explicit residual.  Timing and
RSS were not measured, so no such improvement is claimed.

## Next Checkpoint

Resolve the remaining generic aggregate choices only if a later checkpoint
selects them: dynamic global nested-array initialization, unknown-bound
namespace aggregate arrays, canonical address/projection shape for aggregate
constructor paths, and residual bit-field/value-init shapes.  Complete scalar
list-narrowing categories only with typed constant facts; this checkpoint does
not claim complete N3485 8.5.4 coverage.

## checkpoint ledger

| checkpoint | result |
| --- | --- |
| `36b93869` handoff | PA16 `144/243`, `99` failures, `243/243` covered; PA1-PA15 `1167/1167`; immutable baseline preserved at the evidence path/hash above. |
| this checkpoint commit | PA11 sparse arena -> PA12 typed appertainment -> PA15 validated lowering; focused `9/17`, full `159/243`; 15 full baseline-only repairs, final-only `∅`, `243/243` covered; course 404/409/412, through, audit, and diff check pass. |
