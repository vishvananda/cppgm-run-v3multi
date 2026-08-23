# Stage Design

PA8 uses one typed production path.  `CppSyntaxDeclaratorOp` carries decoded
array bounds and prefix/suffix structure into `DeclaratorShape` and
`DeclaratorOp`; `SemanticCore` owns canonical `TypeId` formation, reference
collapsing, pointer/reference/void/array-reference invariants; and PA8 owns
value category, cv-aware binding, referent identity, typed lifetime-extended
temporaries, relocations, and image planning.  No fact is rendered, reparsed,
duplicated, or selected by test spelling.

# Spec Alignment

The path follows N3485 8.3.2 [dcl.ref] for reference kinds, forbidden direct
forms, and typedef collapsing; 8.5.3 [dcl.init.ref] for direct versus converted
binding, lvalue/rvalue category, and cv qualification; and 12.2
[class.temporary] for reference-bound lifetime.  PA8's image contract keeps
named entities in block 1, lifetime-extended temporaries in block 2, and applies
typed relocations only after both orders are planned.

# Failure Map

The original turn-start broad result was **80/89** over **89 cases**, with
exactly the nine failures listed below.  The final checked-in PA8 suite is
**83/92**: all three durable array/reference regressions pass, and the original
89-case set remains **80/89** with exactly the same nine failure identities and
no new failure.  No handout test changed.  The final through-PA7 report is
**339/339**; through-PA8 is **422/431**; and the file audit exits 0 with the
existing header warning.

Remaining failures:

- `pa8/tests/310-array-str-lit.t.1`, `pa8/tests/340-array-const.t.1`,
  `pa8/tests/500-static-assert.t.1`, `pa8/tests/500-static-assert3.t.1`;
- `pa8/tests/600-qualified-redeclaration.t.1`,
  `pa8/tests/600-qualified-redeclaration2.t.1`;
- `cppgm.tests/course/pa8/120-constexpr-pointer-cross-tu.t.1`,
  `cppgm.tests/course/pa8/120-constexpr-qualified-pointer.t.1`,
  `cppgm.tests/course/pa8/300-function-typedef-definition-bad.t.1`.

The focused repaired identities and new cases pass: `300-bad-ref1`,
`300-bad-ref2`, `300-bad-ref3`, `300-uninit-ref`, `450-reference`,
`700-reference-to-reference`, `450-cv-dropping-reference-bad`,
`450-lvalue-to-rvalue-reference-bad`, `300-cv-through-typedef-constant`,
`430-array-reference-direct-bad`, `430-array-reference-typedef-bad`, and
`430-reference-to-array-valid`.  The reference workflow agrees on the grouped
valid case.  It accepts the two standard-invalid array-of-reference cases;
their checked-in `EXIT_FAILURE` sidecars pin the standard-required result and
the divergence is recorded in `pa8/audit.md`.

# Active Checkpoint

The audit repaired the array-of-reference escape left by flattened declarator
application.  Canonical `array(...)` now rejects reference children, and the
PA8 adapter rejects an array-before-reference declarator shape; grouped
reference-to-array, reference-to-pointer/function, direct forbidden forms, and
typedef reference collapsing remain distinct.  `PA8Value` keeps named and
post-dereference identities.  Binding performs indexed type/category/cv checks;
known conversions retain bytes, unknown arithmetic conversions create typed
zero-initialized non-constant temporaries, and temporary/reference relocation
facts are patched after deterministic block-1/block-2 layout.

# Performance Evidence

No timing claim is made.  Formation and validation are linear in the current
declarator.  Binding uses indexed identity plus bounded type/category checks
and one temporary append; it does not scan the entity arena.  Image planning is
two linear entity passes plus the existing linear relocation pass.  These are
structural complexity statements only.

# Next Checkpoint

Preserve the nine-item failure map and address array/string initialization and
cv completion, static-assert evaluation, qualified/cross-TU pointer facts, and
function-typedef definition rules separately.  The next checkpoint must retain
the original 89-case comparison, the final 92-case checked-in coverage, the
current reference pass set, through-PA7 339/339, and the existing file-audit
warning result.

# Checkpoint Ledger

| checkpoint | result | evidence |
| --- | --- | --- |
| `pa8-baseline` at `affab90c` | 71/89, 18 failures, 89 cases covered | supervisor-provided baseline; prior through-PA7 and file audit passed |
| `referenceFormationPhase1` | focused 8/8 pass; semantic edge probes pass | exact eight-case `make -C pa8 check TEST=...`; volatile/prvalue, converted-unknown, alias, and grouped-declarator probes |
| `referenceFormationMeasured` | PA8 80/89; through-PA7 339/339; through-PA8 419/428 | `make test-pa8` exit 2 with only the nine mapped failures; `make test-report-through-pa7` exit 0; `make test-report-through-pa8` exit 2; 89 cases unchanged |
| `pa8-file-audit` | pass with one existing warning | `perl scripts/cppgm_file_audit.pl --stage pa8 --paths dev/src` exit 0; warning is the existing header implementation-body division note |
| `checkpointAudit` at `657e5559` + array/reference repair | completed bounded audit with segment-scoped nested-layer correction; PA8 85/94 and the original 89-case set remains 80/89 with exactly the same nine failures; through-PA7 339/339; through-PA8 424/433 | focused 14/14; five durable course regressions and prescribed fixtures; both new legal cross-layer function forms reference-pass with `EXIT_SUCCESS`; canonical array formation owns array-of-reference rejection, while adapter validation is limited to same-segment direct prefix spellings; all broad gates passed with no new failure and the existing file-audit warning; structural performance evidence only, no timing claim |
