# Stage Design

PA15 owns a direct typed lowering spine from the existing PA11/PA12 semantic
owner to `lowir_model::Program`.  The driver preprocesses and parses each
source into PA10, then `PA11SemanticModel::analyze()` and `analyze_pa12()` own
scopes, bindings, types, conversions, and resolved calls; `Pa15Lowerer` in
`dev/src/pa15_lowering.cpp` consumes those typed facts directly.  It does not
consume semantic dumps, AST renderings, generated LowIR, tests, refs, or any
reference/host compiler output.  PA14's typed ABI encoder creates external
function object symbols.  `lowir_model` remains the sole typed LowIR model;
serialization is the cold output boundary.

# Failure Map

The initial PA15 baseline was 0/109: every test returned
`EXIT_NOT_IMPLEMENTED`.  The complete stage is represented by these filename
feature groups:

- focused scalar returns/literals/locals/arithmetic/calls/namespace calls and
  the accepted shadowed-local/ABI-call/argument-width cases: 13 pass (10
  expected-success cases and 3 expected-diagnostic cases);
- control flow, loops, switch, goto, condition declarations, and short-circuit
  filenames: 17 remaining failures;
- globals, arrays, pointers, addresses, decay, and subscripting: 40 remaining
  failures;
- calls, ABI/reference parameters, overloads, linkage declarations, and using
  or namespace lookup: 18 remaining failures;
- enums, `sizeof`, floating cases, character literals, and type conversions:
  13 remaining failures;
- extended lvalues, compound assignments, inc/dec, generated-slot cases, and
  other scalar-expression extensions: 8 remaining failures.

Thus the final PA15 result is 13/109 with 96 failures, all tool exit-status
mismatches for feature groups outside this checkpoint; no generated-LowIR
mismatch remains in the supported or adjacent scalar cases.

# Active Checkpoint

The implemented boundary is namespace-scope procedural function definitions,
including named namespaces and `main`, with integral/bool scalar parameters and
local slots; integer/bool/character literals; lvalue loads; local
initialization and simple assignment; core arithmetic and comparisons; typed
resolved direct calls; return; and basic if/else.  C++ `bool` is represented as
LowIR `u8`.  Unsigned division, modulo, right shift, and comparisons use the
operand/conversion type.  Integral conversions are width-aware (`trunc` when
narrowing, signed/zero extension when widening, and no invalid operation for
same-width representation changes); floating conversions fail before wrong
LowIR is emitted.

Function headers and all binding-to-function/slot identities are collected
before any body is lowered.  A `FunctionPlan` carries both the semantic fact
index and final Program function index, so lowering does not rescan function
facts.  Each function allocates parameters followed by its temporaries in one
contiguous ValueId range.  `Instruction::direct_callee_id`, operand identities,
and `Program::values` producer/owner links are populated in the source-created
model.  Value links are finalized only after all vector growth at the stable
per-function/program boundary, avoiding unstable pointers.

The driver preserves normal alias/I/O/error handling and requires exactly one
entry function.  `main` carries `role=entry`, `binding=strong`, and
`keep_alias=yes`; C++ object symbols are ABI-encoder results.  The serializer
supports the emitted procedural subset, exact `alias object <object-symbol> =
@target` syntax, structured scalar/address/zero globals, and clear exceptions
for invalid or unsupported model states rather than placeholder text.

This aligns with the PA15 README, PA13 LowIR comparison/result and conversion
rules, and spec §§1, 2, 4, 5, and 7: one typed semantic owner, typed identity
continuity, deterministic source order, a shared typed LowIR boundary, and
measured complexity/error behavior.  Validation evidence is the required six
case check, direct-model probes, serializer round-trip, public LowIR boundary,
the full PA15 report, through-PA14 report, file audit, and diff check.

# Performance Evidence

For one source-created Program with `F` functions, `S` slots, `I` emitted
instructions/values, and `P` presentation spellings, function precollection
and body lowering make one fact-range pass and then one body pass: there is no
O(F²) function-fact rescan in `lower_function`.  Direct calls use a typed
binding-to-symbol and binding-to-program-index ordered map, O(log F) lookup,
instead of an O(F) scan.  Slot operands use a SlotId-indexed spelling table,
O(1) instead of an O(S) scan.  Monotonic per-base collision counters visit
each occupied candidate once amortized; presentation interning and binding
maps cost O(log P) / O(log S) per insertion or lookup.  Final producer-link
validation is O(F + I + |Program::values|), and cold serialization is linear in
the serialized model/output (apart from indexed spelling access).  Generated
names are counter-based and collision-safe; hot facts carry identities rather
than strings.

Representative scaling used the immutable `/home/vishvananda/work/v3multi/dev/cppgm++`
binary and equivalent generated scalar sources in `/tmp`, with interleaved
run order `[8,32,16,16,8,32,32,16,8]` and three observations per size.  The
source at size `N` has `N` two-parameter scalar functions with one local and a
chain of direct calls, plus `main`; the output counts below were measured from
the generated LowIR.  Wall times are medians on the same run.

| N | median ms | input B | output B | functions | slots | calls | value instructions |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 8 | 3.894 | 481 | 3130 | 9 | 24 | 8 | 39 |
| 16 | 4.442 | 949 | 6196 | 17 | 48 | 16 | 79 |
| 32 | 6.040 | 1893 | 12340 | 33 | 96 | 32 | 159 |

The exact command was
`python3 /tmp/pa15_scale_run.py /home/vishvananda/work/v3multi | tee /tmp/pa15_scale_results.tsv`;
all generated sources, LowIR, and timing artifacts remained outside the
repository.

# Checkpoint Ledger

- Baseline: clean HEAD `6e0f533c`; PA15 was 0/109 with every case at
  `EXIT_NOT_IMPLEMENTED`.
- Implementation: direct PA11/PA12 typed lowering, shared model serializer,
  driver integration, ABI symbol ownership, dense typed identity maps, and
  stable final ValueRecord links; no fixture, ref, or harness changes.
- Model proof: `/tmp/pa15_model_probe` reported
  `functions=3 values=11 ranges=0+5,5+5,10+1 direct-call=pass`, including
  parameter and instruction producer-pointer checks.
- Scalar proof: `/tmp/pa15_scalar_probe` passed bool `u8`, unsigned `udiv`,
  unsigned `ult`, signed widening `sext`, and narrowing `trunc` checks.
- Serializer/public proof: `/tmp/pa15_roundtrip_probe` reported
  `round-trip=pass alias=pass`; `lowir2cy86` accepted the emitted if/else
  LowIR and produced a non-empty 547-byte CY86 output.  The required six-case
  command passed 6/6; the focused extern-C and shadow-slot checks passed 2/2.
- Broad stage: `make test-pa15` reported exactly `13 / 109 TESTS PASSED`,
  leaving 96 categorized failures above and no coverage reduction.
- Earlier behavior: the required through-PA14 command reported
  `ALL TESTS PASSED SUCCESSFULLY! (1058 / 1058)`.
- Audit/format: `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src`
  passed with the four existing header-division warnings; `git diff --check`
  passed.
- Commit: the authorized PA15 checkpoint commit follows this finalized
  evidence; its hash and clean-tree verification are reported at handoff.
