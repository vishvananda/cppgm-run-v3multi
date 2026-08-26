# PA15 typed floating-scalar checkpoint

## Stage Design

The production path is source -> PA10/PA11 typed facts -> PA12 typed
conversion/call facts -> PA15 typed LowIR.  PA10 owns syntax and names, PA11
owns canonical `TypeId` and value category, PA12 owns `ConversionFact`
(`source`, `target`, `kind`, `rank`) plus callable and argument facts, and
PA15 consumes those facts without textual rediscovery or a parallel semantic
model.

This checkpoint closes the typed scalar boundary for f32, f64, and f80:
`sitofp`/`uitofp`, `fptosi`/`fptoui`, `fpext`/`fptrunc`, floating
compound-assignment operands, return/argument conversions, converted
const-reference temporaries, and C variadic default promotions.  Floating
truth is a typed `cmp ne` against a zero with the source float width, yielding
the integer truth operand required by PA13 branches.  The PA13 scalar
cmp/convert/branch contract is the LowIR authority.

The lowering work is bounded by recorded conversions and lowered expressions.
The PA12 call-boundary helper scans the selected call's argument vector once;
constant-address publication and variadic promotions remain local to that
vector.  No whole-program retry, dense/global pass, or per-node owning string
was added.

## Failure Map

Authoritative incoming state at `ea846ea4`: `90/109` passing, `109/109`
covered, exactly these 19 failures:

    100-const-integral-lvalue-overload-category
    100-string-hex-escape-code-unit
    100-unnamed-parameter-storage
    200-comma-expression-xvalue-reference-return
    200-const-ref-converted-float-argument
    200-floating-compound-assign-integral-rhs
    200-floating-condition-declaration-negative-zero
    200-floating-logical-branch
    200-floating-return-integral-conversion
    200-for-iteration-discards-void-comma-rhs
    200-goto-case-block-entry-label
    200-goto-case-block-label-after-statement
    200-included-namespace-global-definition
    200-literal-logical-short-circuit-omits-unreachable-call
    200-nested-conditional-array-decay
    200-qualified-namespace-overload-definition-symbol
    200-return-void-call-expression
    200-variadic-float-argument-promotes-to-double
    300-return-empty-braces-scalar

The selected six-case boundary was:

    200-const-ref-converted-float-argument
    200-floating-compound-assign-integral-rhs
    200-floating-condition-declaration-negative-zero
    200-floating-logical-branch
    200-floating-return-integral-conversion
    200-variadic-float-argument-promotes-to-double

## Active Checkpoint

PA12 now permits a conversion-backed scalar lvalue to bind a converted const
reference, publishes typed variadic array/function decay, float-to-double
promotion, and canonical integral promotions for bool, char/signed and
unsigned char, short/unsigned short, and supported unscoped enums.  PA11's
integral-only constant initializer path no longer treats an ordinary const
floating object as an integral constant.

PA15's central conversion spine selects signedness and width from the typed
facts: `sitofp`/`uitofp`, `fptosi`/`fptoui`, and `fpext`/`fptrunc`; it materializes
converted reference prvalues in generated slots; it preserves typed literal
array addresses; and it avoids generated `$refarg__N`/`$cond__N` collisions
with source slots.  The durable owner probe
`cppgm.tests/course/pa15/404-typed-floating-conversion-boundary-regression.sh`
checks all six conversion opcodes over f32/f64/f80, signed/unsigned integer
directions, f80 negative-zero truth, all narrow integral and enum promotions,
float promotion, and helper-slot collisions.  No existing test or `.ref`
fixture was changed.

Focused and owner evidence:

    make -C pa15 check TEST='tests/general/200-const-ref-converted-float-argument.t tests/general/200-floating-compound-assign-integral-rhs.t tests/general/200-floating-condition-declaration-negative-zero.t tests/general/200-floating-logical-branch.t tests/general/200-floating-return-integral-conversion.t tests/general/200-variadic-float-argument-promotes-to-double.t'

Result: `PASS (6/6)`, recorded in `/tmp/pa15-focused-final.log`.
The executable owner probes 400, 401, 402, 403, and 404 also passed; the
combined record is `/tmp/pa15-focused-final.log` and the standalone owner
record is `/tmp/pa15-owner-probes-final.log`.

Broad movement evidence:

    make test-pa15

Recorded in `/tmp/pa15-test-final.log`: `98/109` passing with `109/109`
covered.  Mechanical comparison in `/tmp/pa15-failure-set-final.log` reports
incoming `19`, final `11`, new/replacement `0`, and removed `8`:

    200-const-ref-converted-float-argument
    200-floating-compound-assign-integral-rhs
    200-floating-condition-declaration-negative-zero
    200-floating-logical-branch
    200-floating-return-integral-conversion
    200-included-namespace-global-definition
    200-qualified-namespace-overload-definition-symbol
    200-variadic-float-argument-promotes-to-double

The 11 residual names are all incoming:

    100-const-integral-lvalue-overload-category
    100-string-hex-escape-code-unit
    100-unnamed-parameter-storage
    200-comma-expression-xvalue-reference-return
    200-for-iteration-discards-void-comma-rhs
    200-goto-case-block-entry-label
    200-goto-case-block-label-after-statement
    200-literal-logical-short-circuit-omits-unreachable-call
    200-nested-conditional-array-decay
    200-return-void-call-expression
    300-return-empty-braces-scalar

Prior callable/reference/reinterpret ownership evidence was rerun: 400, 401,
402, 403, and the new 404 probe all exited zero.  Through-PA14 was run with
`n=15` and the required conditional command; `/tmp/pa15-through-pa14-final.log`
records `1058/1058`.  The source audit is recorded in
`/tmp/pa15-file-audit-final.log` and `git diff --check` in
`/tmp/pa15-diff-check-final.log`.

## Performance Evidence

Raw artifacts are retained in `/tmp/pa15-performance-final`.  The parent
executable is the immutable `ea846ea4` build, SHA-256
`e969765789a92ec74832ee0a9da1bfe04038b172dc26fc2666d54f8988d66655`, and the
candidate executable SHA-256 is
`e66ae2b9c83fc22d9e432109951077d86cb05fc80b5ff4d55daede1114d9bf0d`.
Both were mode `555`; all runs used `--emit-lowir -O0` and
`/usr/bin/time -f '%e\t%M'`.

For equivalent integer-conversion-heavy inputs, seven interleaved rounds
compared parent and candidate at 64, 256, and 1024 repeated function units
(128, 512, and 2048 conversions).  `integer-runs.tsv` contains raw runs and
`integer-medians.tsv` contains these medians:

    mode       size  wall_seconds  rss_kb  converts  cmps  LowIR lines  bytes
    parent       64       0.01      6568       128      0       1284     31234
    candidate    64       0.01      6560       128      0       1284     31234
    parent      256       0.03     12088       512      0       5124    125362
    candidate   256       0.03     12112       512      0       5124    125362
    parent     1024       0.11     33384      2048      0      20484    502546
    candidate  1024       0.12     33644      2048      0      20484    502546

All 21 parent/candidate pairs had exact LowIR equality; pair details are in
`integer-pairs.tsv`.  This supports only the bounded observation that these
inputs had equivalent structure and near-identical measured resource use;
the timer resolution and sample count do not establish a general speed claim.

The parent rejects the floating family with exit 1 and
`PA15 floating conversion is outside checkpoint`, recorded in
`parent-float-probe.status`/`parent-float-probe.stderr`; no parent-vs-float
comparison claim is made.  Candidate-only floating-conversion families used
seven interleaved ascending/descending rounds at 16, 64, 256, and 1024 units.
`floating-candidate-runs.tsv` and `floating-candidate-medians.tsv` record:

    units  wall_seconds  rss_kb  converts  cmps  LowIR lines  bytes
       16       0.01      6360        96    16        1332     31582
       64       0.04     10732       384    64        5316    126430
      256       0.17     28672      1536   256       21252    508162
     1024       0.69    102300      6144  1024       84996   2037634

The structural counts are `6*units` converts and `units` floating truth
cmps; output lines and bytes grow accordingly.  Input and executable hashes,
round ordering, raw timings, and output hashes are retained beside these TSVs.

## Checkpoint Ledger

| checkpoint | result | movement |
|---|---:|---|
| `ea846ea4` incoming | `90/109`, `109/109` covered, 19 failures | six typed floating/call boundaries selected |
| focused owner validation | six `6/6`; owner probes `5/5` | all requested opcode, truth, promotion, and collision probes pass |
| PA15 broad validation | `98/109`, `109/109` covered, 11 residual, 0 new | eight incoming failures removed, including all six selected cases |
| through/audit gates | `1058/1058`; audit passes with 5 pre-existing header warnings | no prior-stage regression and source-shape limits satisfied |
| commit-ready increment | all required evidence retained under `/tmp/pa15-performance-final` | message: `PA15: lower typed floating scalar conversions` |

Changed implementation and regression files are:

    dev/frontend_source_sets.mk
    dev/src/pa11_semantic_core.cpp
    dev/src/pa11_semantic_model.h
    dev/src/pa12_semantic.cpp
    dev/src/pa12_semantic_calls.cpp
    dev/src/pa15_lowering.cpp
    dev/src/pa15_lowering.h
    dev/src/pa15_lowering_flow.cpp
    cppgm.tests/course/pa15/404-typed-floating-conversion-boundary-regression.sh
    cppgm.tests/course/pa15/404-typed-floating-conversion-boundary-regression.source
    pa15/plan.md

Out of scope remains string hex decoding, namespace/control-flow residuals,
overload-category and comma/xvalue residuals, unnamed-parameter storage, and
empty-brace scalar return.  The implementation has no known semantic issue
within the selected boundary; the residual list above is the bounded
uncertainty for this checkpoint.
