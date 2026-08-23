# Stage Design

PA8 uses one typed production path.  `SemanticCore` owns canonical `TypeId`
formation and invariants: reference collapsing, pointer-to-reference
rejection, and reference-to-void rejection.  The declaration adapter preserves
typed prefix structure and expression-valued array bounds.  PA8 semantic
analysis owns expression value category, cv-aware C++11 reference binding,
referent identity, temporary lifetime/relocation facts, and image planning.
No semantic fact is rendered, reparsed, or selected by test spelling.

# Failure Map

The measured baseline was **71/89**, with unchanged **89-case coverage** and 18
failures.  The measured checkpoint result is **80/89**, still over all 89 cases,
with nine remaining failures.  The eight requested identities are repaired;
the former `300-cv-through-typedef-constant` baseline failure also now passes.
No handout test or reference fixture changed.

Current complete remaining map:

- Arrays/cv/static assertions: `pa8/tests/310-array-str-lit.t.1`,
  `pa8/tests/340-array-const.t.1`, `pa8/tests/500-static-assert.t.1`, and
  `pa8/tests/500-static-assert3.t.1`.
- Qualified/cross-TU pointer work: `pa8/tests/600-qualified-redeclaration.t.1`,
  `pa8/tests/600-qualified-redeclaration2.t.1`,
  `pa8/course/pa8/120-constexpr-pointer-cross-tu.t.1`, and
  `pa8/course/pa8/120-constexpr-qualified-pointer.t.1`.
- Function typedef: `pa8/course/pa8/300-function-typedef-definition-bad.t.1`.

Repaired identities are `300-bad-ref1`, `300-bad-ref2`, `300-bad-ref3`,
`300-uninit-ref`, `450-reference`, `700-reference-to-reference`,
`450-cv-dropping-reference-bad`, and `450-lvalue-to-rvalue-reference-bad`.

# Active Checkpoint

Canonical formation rejects forbidden direct pointer/reference and
reference/void combinations while aliases retain legal reference collapsing.
Typed prefix ordering preserves ordinary pointer, function, array, and grouped
declarators.  PA8 values retain both the named entity and the post-dereference
referent, so a reference id binds to its referent.  Binding checks lvalue/rvalue
category and cv compatibility; volatile-qualified referred lvalue references
cannot bind prvalue or converted temporaries, while direct compatible volatile
lvalue bindings remain valid.  Arithmetic conversion well-formedness is
separate from constant-byte availability: an unknown source creates a typed,
zero-initialized, non-constant lifetime-extended temporary.  Known scalar
temporaries retain their constant bytes.  Named entities emit in block 1 and
temporaries emit in block 2 in first-use order; the typed relocation pass then
patches reference identities.

# Performance Evidence

No timing claim is made.  Reference binding performs indexed lookup plus O(1)
cv/type/category/identity checks and one typed temporary append; it does not
scan the entity arena.  Canonical reference relocation is checked in one step.
Declarator validation and type formation are linear in the current declarator's
operation list.  Image planning is two linear entity passes plus the existing
linear relocation pass.  These are structural complexity statements, not
measurements; no material performance risk was observed in the focused or
broad runs.

# Next Checkpoint

Preserve the nine-item deferred map and address the array/static-assert,
qualified/cross-TU pointer, and function-typedef boundaries in a later PA8
checkpoint.  Any follow-up must retain 89-case coverage, the eight repaired
identities, the through-PA7 gate, and the file-audit result.

# Checkpoint Ledger

| checkpoint | result | evidence |
| --- | --- | --- |
| `pa8-baseline` at `affab90c` | 71/89, 18 failures, 89 cases covered | supervisor-provided baseline; prior through-PA7 and file audit passed |
| `referenceFormationPhase1` | focused 8/8 pass; semantic edge probes pass | exact eight-case `make -C pa8 check TEST=...`; volatile/prvalue, converted-unknown, alias, and grouped-declarator probes |
| `referenceFormationMeasured` | PA8 80/89; through-PA7 339/339; through-PA8 419/428 | `make test-pa8` exit 2 with only the nine mapped failures; `make test-report-through-pa7` exit 0; `make test-report-through-pa8` exit 2; 89 cases unchanged |
| `pa8-file-audit` | pass with one existing warning | `perl scripts/cppgm_file_audit.pl --stage pa8 --paths dev/src` exit 0; warning is the existing header implementation-body division note |
