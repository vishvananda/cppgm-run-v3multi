# PA16 typed constructor-overload checkpoint

## Stage Design

PA11 owns typed AST/model identities.  PA12 owns relevant lookup, direct and
user-defined conversion choices and scores, constructor selection, and typed
conversion/action facts.  PA15 consumes those selected declarations and calls
and lowers the single typed model to LowIR.  The PA16 boundary here is ordinary
overload resolution, constructors, converting constructors, external
declarations, and the supported class subset.  Class value semantics,
copy/move transfer, and pass-by-value class objects remain PA17 scope.

The owner path is:

    AST arguments -> ExprInfo and semantic facts
      -> PA12 relevant candidate set
      -> per-argument ConversionChoice and ConversionScore
      -> selected declaration/constructor and target-aware materialization
      -> ConstructorAction/CallExpression and typed conversion facts
      -> PA15 declaration/call/constructor LowIR lowering

## Baseline Authority and Failure Map

At checkpoint entry, the supplied authority was 225/243 with exactly 18
failures.  Discovered/reference/fresh coverage was exactly 243/243/243;
the supplied authority and fresh failure sets each had 18 identities, all
retained, with authority-only 0 and fresh-only 0.  Through PA15 was 1167/1167.
The file audit passed with five known bad-division warnings.

The final broad PA16 run is 227/243 (status 2 because the 16 known residuals
remain).  Its exact identity set is the entry set minus #2 and #8: compared
with the entry baseline, retained 16, baseline-only/resolved 2, and new 0.
Within the final authority/fresh comparison, authority-only is 0, fresh-only
is 0, and retained is 16.  Discovered/reference/fresh coverage remains
243/243/243.

| # | test | checkpoint status |
|---:|---|---|
| 1 | pa16/tests/general/200-elaborated-member-forward-type.t | authority residual; untouched |
| 2 | pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t | resolved; absent from final residual set |
| 3 | pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t | authority residual; untouched |
| 4 | pa16/tests/general/200-local-default-class-array-lifecycle.t | authority residual; untouched |
| 5 | pa16/tests/general/200-nested-braced-member-aggregate-init.t | authority residual; untouched |
| 6 | pa16/tests/general/200-reference-indexed-pointer-member-access.t | authority residual; untouched |
| 7 | pa16/tests/general/200-reference-member-class-init.t | authority residual; untouched |
| 8 | pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t | resolved; absent from final residual set |
| 9 | pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t | authority residual; untouched |
| 10 | pa16/tests/general/300-callable-field-hides-private-base-method.t | authority residual; untouched |
| 11 | pa16/tests/general/300-friend-function-definition-skip.t | authority residual; untouched |
| 12 | pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t | authority residual; untouched |
| 13 | pa16/tests/general/300-overloaded-deref-user-assignment.t | authority residual; untouched |
| 14 | pa16/tests/general/300-user-defined-string-literal-operator.t | authority residual; untouched |
| 15 | pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t | authority residual; untouched |
| 16 | pa16/tests/general/400-bit-field-prefix-postfix-increment.t | authority residual; untouched |
| 17 | pa16/tests/general/400-signed-bit-field-read.t | authority residual; untouched |
| 18 | pa16/tests/general/400-signed-enum-bit-field-read.t | authority residual; untouched |

## Active Checkpoint

The evidence-backed defect was that shared ordinary-call scoring did not reuse
the existing typed converting-constructor viability path for a class-reference
parameter.  A candidate's argument loop is correctly stopped once any
argument lacks an implicit conversion sequence; the outer loop still considers
the remaining relevant candidates.  The invariant is that every still-viable
candidate is checked argument-by-argument, a candidate is rejected once any
argument lacks an ICS, and all relevant candidates remain considered.

The checkpoint adds one shared implicit-constructor conversion owner.  It
validates the target and source facts, considers only relevant non-explicit
accessible constructors, honors defaulted trailing parameters, scores the
constructor's first parameter, and returns a single user-defined
ConversionChoice only when the best score is unique.  The selected call then
re-enters target-aware semantic construction when the chosen argument needs a
converting-constructor temporary.

For the external Box case, both three-argument overloads are considered:
Box(int,const Token&,int) rejects its second argument, while the declared
Box(int,const char*,int) remains viable and lowers as the selected external
function declaration/call.  Generic array-to-pointer conversion now also
publishes the typed constant-address fact needed by PA15 for a string literal
returned from Source::c_str().

For the library case, the mutable void* overload remains nonviable because
discarding the string literal's const qualification is not permitted.  The
const path& overload obtains one user-defined sequence through path(const
char*), then binds the temporary to the const reference.  The separate
const-void-pointer initializer records the two standard edges
array-to-pointer and pointer-to-void.  No class-value, copy, or move semantics
were added.  The array fact remains the PA15 lowering root, while the returned
ExprInfo now has the target pointer type and prvalue category.

N3485 13.3.1.3, 13.3.2, 13.3.3, 13.3.3.1/.1.2, and 12.3.1 are the governing
overload, viability, implicit-conversion-sequence, and converting-constructor
rules; the implementation follows the typed PA12-to-PA15 boundary described
above.

## Validation

The final relevant compiler rebuild completed with status 0:

    make -C dev cppgm++

The two checkpoint tests passed:

    make -C pa16 check TEST='tests/general/200-external-ctor-overload-nonfirst-argument.t tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t'
    pa16 check: PASS (2/2)

The existing constructor/external-declaration controls passed:

    make -C pa16 check TEST='tests/general/200-constructor-overload-default-arg-nonfirst-argument.t tests/general/200-copy-init-explicit-ctor-overload-refinement.t tests/general/500-inheriting-external-transitive-constructor.t'
    pa16 check: PASS (3/3)

The disposable typed qualification probe used semantic emission rather than
LowIR text.  A function returning const void* from a string literal was
accepted, and its later call result was consumed as a const void* variable and
condition.  The paired void* return was rejected with
PA12 invalid array-to-void conversion.

The final broad gates were run serially from the repository root:

    make test-pa16
    status 2; 227/243 passed; exact residual set is the 16 identities above

    n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
    status 0; 1167/1167 passed

    perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src
    status 0; five known bad-division warnings, no fatal issues

    git diff --check
    status 0

The PA16 inventory stayed at discovered/reference/fresh 243/243/243.  No
tests, fixtures, exit-status sidecars, or reference outputs were changed or
regenerated.

## Performance Evidence

These are structural bounds and representative counts, not timing claims:

- Box evaluates at most 2 relevant constructor candidates across 3 explicit
  arguments.  The Token overload rejects at its second argument; the outer
  candidate loop still evaluates the declared const-char overload.  Ordinary
  candidate scoring is O(C*A) conversion-choice work, stopping each candidate
  at its first failed argument while retaining all candidate identities.
- library evaluates 2 relevant outer constructor candidates for 1 argument.
  The const path& candidate performs one relevant path-constructor probe for
  1 argument; the mutable void* candidate fails its standard conversion.  The
  accepted const-void initializer has two typed standard-conversion edges.
  Each constructor identity probe uses an ordered set, O(I log I) for I
  relevant constructors, plus bounded typed conversion work.
- Source::c_str() publishes one ArrayToPointer constant-address fact, which
  PA15 consumes directly.  Source-to-slot metadata is captured once per owned
  slot with at most an O(log B) declaration-map lookup.  For each generated
  source-anchored slot, the captured metadata lookup, source-order scan, and
  vector shift are O(S + log B), never O(S*B).  SlotId values remain allocated
  once and stable while the presentation vector is source-anchored.

## Checkpoint Ledger

| checkpoint | compact result |
|---|---|
| d54e32d1 | Prior PA16 authority was 224/243 with exactly 19 failures and 243/243 identity coverage. |
| b58ddd2a | Completed the typed nullptr_t carrier path through PA11/PA12/PA15, including ABI and LowIR ownership and the bounded endpoint audit. |
| e09d8223 | Recorded the completed nullptr carrier audit state: 225/243 authority, exact 18-item residual map, 243/243/243 inventories, through-PA15 1167/1167, and passing file audit with five known warnings. |
| current checkpoint | Added shared typed constructor viability/materialization, target-typed array-to-void results with an array lowering root, string-literal qualification handling, c_str address publication, ordered constructor identity checks, and source-anchored generated-slot placement. Focused tests pass 2/2, controls pass 3/3, the qualification probe accepts const void* and rejects void*, PA16 is 227/243 with exactly the 16 unchanged residual identities, through-PA15 is 1167/1167, the audit passes with five known warnings, and diff-check passes. |
