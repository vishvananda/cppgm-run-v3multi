# PA16 implementation plan

## Stage Design

The checkpoint preserves one typed pipeline:

```text
PA10 MemberExpression syntax
  -> PA12 SemanticFact with selected BindingId and typed object child
  -> PA11 RecordLayout member_offsets
  -> PA15 LowIR index [projection=field]
  -> existing load/store/call consumers
```

PA12 owns member selection, value category, cv-qualified result type, and the
canonical selected binding.  PA11's `RecordLayout` is the sole owner of
complete class storage and direct-field offsets.  PA15 consumes that typed
relation and does one address projection; it does not recover names or build a
second layout model.

## Failure Map

The clean `4e73af06` baseline is `35/243` passing, `208` failing, and `243`
tests covered by `make test-pa16`.  The first required existing failure was
`tests/general/200-simple-class-member-object-access.t`: PA12 produced the
typed member facts, but PA15 had no `MemberExpression` address/value lowering.
The final run covers all `243` tests and reports `38/243` passing and `205`
failing (exit 2).  Compared with the supplied baseline failure set, three
identities were removed—`200-simple-class-member-object-access.t`,
`200-nested-class-member-object-access.t`, and
`spec/300-lazy-class-lookup-ignores-later-using-directive.t`—and zero new
failure identities appeared; no baseline-passing identity regressed.

This checkpoint targets direct and nested non-static data-member lvalues for
both `.` and `->`, including their existing cv/value-category facts.  Static
members, member calls/overload resolution, constructors/destructors/lifetime,
bit-fields, and inherited-field projection without an explicit typed
base-subobject fact remain out of scope.

## Active Checkpoint

PA12 now publishes `selected_binding` for member facts.  PA15's single member
projection path validates the typed object form, selects `.` versus `->`
address handling, reads the completed owner `RecordLayout`, and emits the
existing `IPK_FIELD` LowIR index with the checked byte offset.  Nested members,
stores, loads, address-taking, and reference consumers therefore reuse the
existing lvalue machinery.

Exact focused evidence:

```text
make -C pa16 check TEST='tests/general/200-simple-class-member-object-access.t tests/general/100-class-local-sizeof.t tests/general/100-empty-class-sizeof.t tests/general/100-large-class-local.t tests/general/100-self-pointer-layout.t'
exit 0; pa16 check: PASS (5/5)

make -C pa16 check TEST='tests/general/200-simple-class-member-object-access.t tests/general/100-class-local-sizeof.t tests/general/100-empty-class-sizeof.t tests/general/100-large-class-local.t tests/general/100-self-pointer-layout.t tests/general/100-function-pointer-nested-param-name-shadow.t'
exit 2; pa16 check: FAIL (5/6); the arrow fixture's remaining mismatch is the unrelated omitted load of unused on_immediate

dev/cppgm++ --emit-lowir -O0 -o /tmp/tmp.9DuUlXqq80/arrow-cv-smoke.lowir /tmp/tmp.9DuUlXqq80/arrow-cv-smoke.cpp
exit 0; pointer-to-record store and pointer-to-const-record load each emitted a field projection
```

The primary five-test check was rerun after all corrections.  PA16 completion
is not claimed; this checkpoint proves focused progress, not full PA16 support.

## Performance Evidence

After layout completion, direct member selection is a typed `FlatIndex` lookup
by `BindingId`, so each direct projection is O(1) after the one class-binding
layout scan.  The `->` path adds typed pointer/pointee and reference/cv
normalization, then the same O(1) lookup; no whole-program retry, text keying,
or source re-rendering is introduced.

This is structural evidence only.  No timing, allocation, or RSS measurement
was collected.  The temporary smoke validates both mutable and
pointer-to-const arrow projections, while the checked-in arrow fixture still
has the unrelated unused-parameter-load mismatch.  Future inherited-field
projection remains an unmeasured risk and must carry explicit typed
base-subobject facts rather than infer offsets from names.

## Checkpoint Ledger

| Evidence or gate | Status |
| --- | --- |
| Prior typed layout/alignment checkpoint: `5f8983b6` plus repairs through `4e73af06` | Completed; preserved the `35/243` baseline and did not itself reduce failures |
| Baseline at `4e73af06` | Recorded: `35/243` passing, `208` failing, `243` covered |
| Required member-object failure | PASS in final focused five-test check; nested member identity also removed from the final failure set |
| Preserved layout cases | PASS: local, empty, large, and self-pointer layouts |
| Checked-in arrow fixture | Remaining expected mismatch is unrelated unused-parameter load; projection boundary passes temporary mutable/const smoke |
| `make test-pa16` | PASS threshold: exit 2, `38/243` passing, `205` failing, all `243` covered; 3 baseline failures removed, 0 added |
| `make test-report-through-pa15` | PASS: exit 0, `1167/1167` |
| PA16 file audit | PASS: exit 0 with 5 pre-existing header-division warnings |
| `git diff --check` | PASS: exit 0 |
| Commit and empty final status | Completed; checkpoint committed and post-commit status is empty |
