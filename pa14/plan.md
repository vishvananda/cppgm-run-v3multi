# Stage Design

The tool boundary reads line-oriented fact text into `AbiFactFile` cases;
definitions and targets remain typed records.  A reusable encoder under
`dev/src` resolves those records, owns one per-name substitution state, and
emits Itanium components.  `dev/abimangle.cpp` handles file/line I/O and
delegates typed cases to that encoder so later stages can call the same API.

# Failure Map

Baseline: 2/111 cases passed and 109 failed.  The existing passes are the
duplicate-definition-id and negative-template-index rejection cases.  The
109 positive failures are grouped by the suite families: 100 basic ABI names
and types; 200 tags, locals, operators, wrappers, and thunks; 300 templates,
standard substitutions, and entity arguments; 400 dependent owners and
aliases; 500 dependent expressions; and 600 nested/inline-namespace cases.
This checkpoint targets all `100-*` cases.  Final PA14 evidence is 35/111
tests passed and 76 remaining failures: 200 has 22, 300 has 30, 400 has 4,
500 has 13, and 600 has 7; the 100 family is fully passing.

# Active Checkpoint

Implement parsing, typed definitions/targets, and reusable Itanium encoding
for the complete checked-in `100-*` family, including deterministic case and
input-file ordering, validation, builtin/compound types, simple and encoding
functions, special names, template values, and C linkage.  Align spelling
with the relevant Chapter 5.1 rules in `doc/itanium-mangling.txt`.  Files:
`dev/abimangle.cpp`, `dev/src/abi_mangle.h`, new `dev/src/abi_mangle.cpp`,
`dev/frontend_source_sets.mk`, and this plan.  Acceptance evidence is the
focused 100-family check, the PA14 suite result, the through-PA13 report, the
PA14 source audit, and a warning-free C++11 build.  Later-family grammar is
explicitly outside this basic ABI checkpoint.

# Performance Evidence

The compact parser advances a cursor through unary wrappers, and the
member-pointer grammar selects its first non-`::` operand separator once;
there is no exception-driven candidate retry or repeated suffix parsing for
those forms.  It still copies bounded name/bound segments and stores the
parsed cases before encoding, so the following smoke is evidence for this
current vocabulary rather than a proof for later grammar.  Generated inputs
used 500, 1,000, and 2,000 cases of the same nested type
`ptr:const:ptr:volatile:array:3:memberptr:ns::C:ptr:const:int`:

- 33,500 bytes / 500 lines / 0.01s / 8,156 KiB RSS
- 67,000 bytes / 1,000 lines / 0.03s / 12,584 KiB RSS
- 134,000 bytes / 2,000 lines / 0.06s / 20,728 KiB RSS

The focused 25-file check measured 0.20s wall time and 9,692 KiB RSS,
including its build/check harness.  These samples show proportional elapsed
time for the exercised input range; RSS includes retained parsed cases and is
not an encoder-only measurement.

# Checkpoint Ledger

- Baseline: clean `bd4bf655`; focused PA14 reported 2/111 passing, 109
  failing; no implementation files changed.
- Changed behavior: line-oriented facts now become typed cases and are encoded
  through the reusable C++11 Itanium encoder; unqualified global variables
  remain unmangled, qualified constructor/destructor owners are complete, and
  duplicate ids and negative indices remain rejected.
- Focused result: `make -C pa14 check TEST='tests/abi/100-*.t'` passed 25/25
  test files, covering 31 cases (29 positive and 2 negative), with zero
  failures.  Ephemeral checks passed for an unqualified global variable and
  namespace-qualified constructor/destructor spellings.
- Broad result: `make test-pa14` exited nonzero with 35/111 tests passed and
  76 failures (200:22, 300:30, 400:4, 500:13, 600:7).  The through-PA13
  command passed 947/947.  The PA14 file audit passed with three existing
  header-division warnings; the C++11 build was warning-free.
- Commit: this checkpoint commit.
- Remaining families/failures: PA14 families 200-600, exactly 76 tests, as
  listed above.
