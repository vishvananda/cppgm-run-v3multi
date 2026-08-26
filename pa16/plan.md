# PA16 implementation plan

## Stage Design

The PA16 path remains `PA10 AST -> PA11 typed semantic model -> PA12 semantic
facts -> PA15/PA16 LowIR`.  `NamedRecordId` is the semantic owner of a
parallel `RecordLayout` fact.  A class body completes that fact once, after
its declarations have been processed.  `type_size`, complete-object checks,
and PA15 storage lowering consume the fact; no textual or whole-program
retry path is involved.  PA12 member semantics, object lifetime, and later
value-semantics stages remain separate boundaries.

## Failure Map

Turn-start evidence was 24/243 passing and 219 failing: 213 exit-status
mismatches and 6 normalized LowIR mismatches.  The completed checkpoint run
is 32/243, leaving 211 failures: 205 exit-status mismatches and 6 normalized
LowIR mismatches.  The residuals are assigned to the deferred owners below;
none is treated as final PA16 completion.

| Owning boundary | Checkpoint disposition |
| --- | --- |
| PA11 complete-type/layout ownership | This slice: typed record state, declaration-order member layout, padding/alignment, arrays, pointers/self-pointers, completed class members, incomplete/overflow rejection. |
| PA15 object-storage boundary | This slice: completed class `obj<bytesxalign>` local slots, class-object declaration address materialization, and trivial no-initializer namespace object zero regions plus their no-op init helper. |
| PA11/PA12 advanced object semantics | Deferred: member lookup/access, nested/friend/using/ADL resolution, operators and implicit-object calls, inheritance/base offsets, bit-fields, anonymous members, references/casts, and type/value-category conversions. |
| PA11/PA16 extended representation | Deferred: `alignas`, packed/pragma-pack behavior, bit-field representation, and the associated normalized LowIR cases; the ordinary natural-layout cases in this checkpoint are covered. |
| PA15 object initialization/lifetime/value lowering | Deferred: constructors/destructors, default/member/aggregate initialization, initialized subobjects, class returns/copies, arrays, namespace/static/thread-local object actions, and broader class-expression lowering. |
| Semantic negative/compatibility boundaries | Deferred: remaining access, narrowing, operator-viability, and related expected-failure diagnostics, plus compatibility cases outside the ordinary completed-layout slice. |
| Incomplete-type compatibility | The valid declaration-only forward-class function-address case is covered by this slice; by-value incomplete object use remains rejected. |

The six normalized residuals are four extended-representation cases and two
friend/operator lookup cases; the other 205 residuals are exit-status cases
owned by the deferred semantic, representation, initialization/lifetime, or
negative/compatibility families above.

## Active Checkpoint

Implement one canonical `RecordLayout` per `NamedRecordId` with explicit
`Incomplete`, `Computing`, `Complete`, and `Failed` states.  For an ordinary
complete non-polymorphic class/struct, process non-static variable members in
scope declaration order, skip methods/types/static members, recursively use
already-completed class facts, and apply natural alignment/padding.  Empty
records are size/alignment `1/1`; arrays, pointers, references, self-pointers,
and arithmetic-overflow checks are included.  A by-value incomplete member or
cycle fails the typed fact and is not retried.

Completed class types feed `sizeof`/`type_size` and PA15 object storage.  A
class local with no initializer materializes its address, and a trivial
namespace class object gets one layout-sized zero region and a no-op init
boundary; constructors, destructors, and member initialization are not
invented here.  The populated typed member-offset map is reserved for the
later member-access consumer.

Validation for this checkpoint is the focused checked-in matrix covering
empty, two-int, self-pointer, padded/larger, incomplete declaration, both
namespace-object sizes, and the expected-negative under-aligned case.  Broad
PA16, through-PA15, audit, and diff checks are now complete.  The focused
matrix passes 8/8 after the final source correction.

## Performance Evidence

Each class completion scans its ordered binding list once; each retained
non-static member is laid out once in that scan.  The per-record state makes
repeat requests an O(1) complete-state read, and completed named-type lookup
uses the parallel vector indexed by typed `NamedRecordId`.  Member offsets are
stored in a typed `FlatIndex` for expected O(1) lookup.  Type wrappers recurse
only through their typed element/member chain, with no retry-until-stable
whole-program loop.  The bounded static-member probe exits 0 and emits
`const i64 1` for a class containing a `long double` static member and a
`char` non-static member.  Checked-in focused output shows the self-pointer
class at size 16, the padded class at `obj<16x8>`/size 16, and the two global
zero-storage cases at sizes 8 and 16.  No timing claim is made.

## Checkpoint Ledger

- Baseline: 24/243 passing; 219 failing (213 status, 6 normalized LowIR),
  supplied at turn start.
- This checkpoint: the required focused matrix passes 8/8; the static-member,
  self-pointer, and padding probes provide bounded structural evidence.  Full
  PA16 passes 32/243 with 211 residual failures and unchanged coverage 243.
  The through-PA15 gate passes 1167/1167.
- Audit: `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`
  passes with five warnings (`abi_mangle.h`, `cpp_semantic_core.h`,
  `lowir_model.h`, `pa11_semantic_model.h`, and `pa15_lowering.h`, all
  `bad-division` header-body warnings) and no fatals.  `git diff --check`
  passes.
- Disposition: committed-candidate evidence recorded for this coherent
  checkpoint.  The residual 211 failures remain explicitly deferred; this is
  not a claim of final PA16 completion.
