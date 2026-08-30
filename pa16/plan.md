# PA16 implementation plan

## Checkpoint and contract

This checkpoint audits landed commit
`ab1b2a8c4a20752434d608b5aef04ef328e5fe5e` (`pa16 add source-point-aware
associated ADL`) relative to `a9728454`, including the approved bounded
follow-up to its typed associated-record collector.  PA16 keeps one typed
PA10/PA11 -> PA12 -> PA15 pipeline.  The increment adds source-point-aware
ordinary unqualified-call ADL to the existing operator-associated-record
machinery; the follow-up completes the standard typed wrapper association
that the supported pointer, array, and function-pointer argument forms need.

The implementation follows `spec.md` §§1--5 and 7: ordinary lookup is first,
ADL is only for an unqualified single name, associated records and scopes are
typed, the first enclosing namespace is used without an ordinary parent climb,
direct using-declarations participate, and using-directive traversal is
excluded.  Candidate identity is canonical `(ScopeId, BindingId)`; source
spelling is not transported.  There is no second lookup engine, retry,
program-wide scan, host/reference shortcut, or alternate lowering path.

The PA15 bridge remains deliberately narrow: exactly one namespace-owned,
non-constructor, nonvariadic function with one empty-class by-value parameter
and a non-class result, with an exact `ClassValue` conversion from a matching
lvalue object.  General class pass-by-value and class return-by-value remain
rejected, as required by the PA16 assignment boundary.

## Current authority and exact failure map

The supplied turn-start/current authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`218/243` passing, exactly `25` failures, and all `243/243` test identities
covered.  The parent checkpoint was `217/243` with `26` failures.  The landed
increment eliminated exactly
`pa16/tests/general/300-adl-using-declaration-source-point.t`; therefore the
parent's 26-name set is the current 25-name set below plus that one identity.
The post-repair full-stage result is also `218/243` with exactly `25` failures
and `243/243` identities covered.  Exact sorted comparison is authority `25`,
fresh `25`, fresh-only `0`, authority-only `0`; the inventory is `243`
discovered identities, `243` reference status sidecars, and `243` fresh status
sidecars, with missing reference `0` and missing fresh `0`.

- `pa16/tests/general/100-function-pointer-nested-param-name-shadow.t`
- `pa16/tests/general/200-const-subobject-member-call.t`
- `pa16/tests/general/200-elaborated-member-forward-type.t`
- `pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t`
- `pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t`
- `pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t`
- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-nested-braced-member-aggregate-init.t`
- `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
- `pa16/tests/general/200-reference-member-class-init.t`
- `pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t`
- `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
- `pa16/tests/general/300-adl-associated-namespace-does-not-climb-parents.t`
- `pa16/tests/general/300-callable-field-hides-private-base-method.t`
- `pa16/tests/general/300-enum-class-nonmember-operator-bitand.t`
- `pa16/tests/general/300-friend-function-definition-skip.t`
- `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
- `pa16/tests/general/300-operator-nullptr-t-from-zero.t`
- `pa16/tests/general/300-overloaded-deref-user-assignment.t`
- `pa16/tests/general/300-prvalue-derived-base-friend-operator.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

## Ownership trace and repair

`semantic_call_expression` handles only an unqualified, non-template single
`IdExpression` for this ADL path.  It obtains the function-definition
`SourcePoint` through `lookup_source_point(scope)` and sends that point through
the existing typed `lookup_value_path`.  Ordinary findings are inspected
first: a class member, a block-scope function, or any non-function suppresses
ADL; namespace-owned ordinary functions can form the ordinary-plus-ADL union.
The existing lookup graph's source visibility and declaration points remain in
force.

Allowed calls analyze their arguments once into typed `ExprInfo` facts.  The
associated-record collector walks the typed `TypeId` graph, not source
spelling: cv/lvalue-reference/rvalue-reference, pointer, and array nodes
enqueue their typed child, while a function node enqueues its typed result and
parameters in deterministic order.  This preserves the array element before
the later array-to-pointer call conversion and reaches class types nested in a
function pointer.  Named class/enum records are then added, complete classes
follow the validated direct-base chain, and enclosing class records are added.
`TypeId` and `NamedRecordId` vectors provide bounds-checked deterministic
deduplication.  A member-pointer node is terminal, and no template expansion
or general class-value semantics is opened.  An incomplete named class still
associates its namespace but does not expand a base chain without complete
scope metadata; malformed incomplete base metadata fails closed.  Each record
maps to its first enclosing namespace, without climbing ordinary namespace
parents.  The bounded repair applies the standard inline-namespace closure
through typed namespace parent/child relations: inline parents and directly
contained inline children are enqueued transitively.  It does not traverse
using-directives.

For each associated namespace, the existing
`lookup_value_graph(..., include_using=false, point)` supplies direct values,
including direct using-declaration entries with canonical origin, while
excluding using-directive edges.  Hidden-friend sidecars are admitted only
when their declaration `SourcePoint` is visible and their binding owner is a
namespace.  Candidate formation validates and deduplicates canonical
`(ScopeId, BindingId)` pairs.  Typed overload selection applies the existing
conversion/default rules and publishes one `CallExpression` with selected
binding/scope, callable type, result category, and typed children.  The token
operator collector uses the same record/namespace and source-point machinery
with operator-token filtering.

The repair was required for two bounded omissions.  The existing namespace
value graph returns after finding visible direct values and may not descend
into an inline child, so a direct but nonviable overload in an associated outer
namespace could hide a viable inline overload.  In addition, the original
collector stopped at a top-level typed `Named` argument and therefore missed
class association through supported pointer, array, and function-type nodes.
The typed closure and wrapper walk preserve normal graph behavior while making
the standard supported forms queryable.  Function-type traversal is only
associated-type discovery; it introduces no class-by-value execution.  Course
regression 426 now exercises five successful runtime cases (two inline cases,
pointer, array, and function-pointer association) and rejects ordinary-parent
and using-directive association.

The published call enters PA15 through normal reachable-function demand,
declaration materialization, and `lower_call`.  The narrow empty-class bridge
revalidates the function ABI, conversion, lvalue category, and matching object
record, lowers the source expression once with deferred conversion, and passes
the existing opaque `obj<1x1>` temporary/address.  It performs no general
class copy or materialization and does not support class results.  No second
semantic analyzer, lookup retry, text identity, host/reference shortcut, or
alternate lowering path exists in this ownership path.

## Structural and performance evidence

The typed wrapper walk is bounded by the visited `TypeId` nodes and each
function node's existing parameter/result vectors.  Record work is bounded by
the visited `NamedRecordId` values, validated direct bases, and enclosing
class scopes; namespace queue dedup means each associated namespace is
enqueued once.  Lookup-generation marks bound and cycle-protect each
individual graph traversal, but does not prevent traversal across separate
`begin_lookup` queries.  Candidate deduplication examines only the collected
canonical identity vectors.  No unrelated declaration scan, cache, retry, or
second lowering path was added.  This is structural evidence only: no timing,
RSS, or unsupported performance claim is made.

## Focused evidence and final gates

Post-repair focused evidence:

- `sh -n cppgm.tests/course/pa16/426-typed-adl-inline-namespace-regression.sh`: status `0`.

- `make -C dev cppgm++ CXX=g++`: status `0`.
- Temporary typed wrapper probes outside tracked surfaces (`pointer.cpp`,
  `array.cpp`, and `functionptr.cpp` under `/tmp/pa16-adl-wrapper-probes.KYl3k0/`):
  each application compile, LowIR translation, CY86 translation, and program
  run returned status `0`.  The clean pre-repair binary rejected all three at
  PA12 expression publication; the repaired collector accepts all three.
- `make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/300-adl-using-declaration-source-point.t tests/general/200-implicit-member-call-suppresses-adl.t tests/general/300-hidden-friend-definition-adl-call.t tests/general/300-enum-operator-adl-selects-matching-overload.t tests/general/300-basic-operator-overloads.t tests/spec/300-hidden-friend-not-visible-to-unrelated-adl.t tests/spec/300-hidden-friend-not-visible-to-qualified-lookup.t tests/spec/300-operator-lookup-ordinary-adl-union.t tests/spec/300-lazy-class-lookup-ignores-later-using-directive.t tests/general/300-using-declaration-function-hides-tag.t tests/general/200-nested-out-of-class-constructor-enclosing-type.t tests/general/300-nested-enum-hidden-friend-bitmask-adl.t'`: status `2`, `11/12`; only the known nested-enum hidden-friend LowIR residual fails.
- `make -C pa12 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/200-inline-namespace-unqualified-call.t tests/general/200-using-directive-call.t'`: status `0`, `PASS (2/2)`.
- `sh cppgm.tests/course/pa16/426-typed-adl-inline-namespace-regression.sh`: status `0`; five runtime cases pass (two inline-namespace cases plus pointer, array, and function-pointer association), and the ordinary-parent/using-directive controls reject as expected.

Final broad validation and bounded final checks:

- `make test-pa16`: exit `2`, `218/243` passed, exactly `25` failures, and
  `243/243` identities covered.
- Exact sorted failure-identity comparison against
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
  authority `25`, fresh `25`, fresh-only `0`, authority-only `0`; inventory is
  `243` discovered, `243` reference status sidecars, and `243` fresh status
  sidecars, with missing reference `0` and missing fresh `0`.
- `n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`: status `0`, `1167/1167`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: status `0`
  with five existing `bad-division` warnings and no fatal findings:
  `abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
  `pa11_semantic_model.h`, and `pa15_lowering.h`.
- `git diff --check`: status `0`.
- Bounded changed-path and coverage audit: status `0`; exactly the four
  approved paths are changed, with `243` discovered tests and complete
  reference/fresh status inventories (`243/243/243`, missing `0/0`).
- The approved commit was inspected and `git status --short` is empty.

## Next checkpoint

PA16 remains incomplete because the same 25 residual identities remain.  The
next checkpoint, if selected, is the separately owned PA11 qualified-type path
behind
`300-adl-associated-namespace-does-not-climb-parents`, not parent-namespace ADL
or using-directive traversal.  Do not re-audit unrelated residual identities.

## Checkpoint ledger

| checkpoint | result | status |
| --- | --- | --- |
| `1694bc3e` bit-field baseline | `200/243`, 43 failures, `243/243` covered | prior landed |
| `7e060b28` packed bit-field boundary | `202/243`, 41 failures, `243/243` covered | prior landed |
| `d95a6fe7` local-class start | `202/243`, 41 failures, `243/243` covered | prior checkpoint |
| `d83e927f` local-class materialization | `206/243`, 37 failures, `243/243` covered | prior landed |
| `70327e4d` exception-safe destructor suffix | `208/243`, 35 failures, `243/243` covered | prior landed |
| `3b2b4882` checkpoint audit baseline | `208/243`, 35 failures, `243/243` covered | prior baseline |
| `ee8f44d5` typed array cleanup | `209/243`, 34 failures, `243/243` covered | prior landed |
| `08472cce` typed pragma-pack layout | prior landed pack-layout checkpoint | prior landed |
| `9f7101ac` pack-layout audit | `210/243`, 33 failures, `243/243` covered | prior landed |
| `fb4f46ed` placement-new semantic/lowering | `211/243`, 32 failures, `243/243` covered | prior landed; historical |
| `85b819b7` pre-increment authority | `211/243`, 32 failures, `243/243` covered | prior baseline |
| `typed truth-width continuity (parent 85b819b7)` | final `214/243`, 29 failures, `243/243` covered; authority-only 3 named identities; fresh-only 0; through-PA15 `1167/1167`; audit 0 with five known warnings; diff-check 0 | landed in this checkpoint commit |
| `96e80152` truth-width checkpointAudit | Focused build `0`, PA16 `7/7`, PA15 `5/5`; fresh PA16 status `2` at `214/243` with authority/fresh `29/29` failures, baseline-only/fresh-only `0/0`, and `243/243` coverage; through-PA15 `1167/1167`; file audit `0` with five pre-existing warnings; final diff/path audits `0`; exact-pointee class-pointer guard repaired | completed audit |
| `a5c8e166` typed packed-bit-field value/update checkpointAudit | Final PA16 status `2` at `215/243`, exactly `28` failures and `243/243` covered; independent comparison authority/fresh `28/28`, authority-only/fresh-only `0/0`, inventory/run total `243/243`; landed delta is exactly baseline-only `400-bitfield-aggregate-init.t`; through-PA15 `0` at `1167/1167`; file audit `0` with five known warnings; focused 412/422/424, probes, diff-check, and path audit pass. Durable evidence is under `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-plain-int-bitfield-checkpoint-audit-20260830/`; no forbidden surface changed | completed audit |
| `1d7e6860` alias direct-base mem-initializer checkpoint | final `216/243`, `27` failures, `243/243` covered; exact authority/final failure comparison `27/27`, authority-only/fresh-only `0/0`; focused `6/6`, courses `408/409/418/425`, through-PA15 `1167/1167`, file audit `0` with five known warnings, smoke, and diff/path checks pass; four approved paths ready for commit | completed |
| `31a938ac` typed aggregate value-initialization compact zero-store checkpointAudit | Fresh final `217/243`, `26` failures, `243/243` covered; current-authority comparison `26` vs `26`, fresh-only `0`, authority-only `0`; focused matrix `6/6`; through-PA15 `1167/1167`; file audit `0` with five known header warnings; no source repair | completed audit |
| `ab1b2a8c` source-point-aware associated ADL checkpointAudit | Final bounded audit of landed `ab1b2a8c4a20752434d608b5aef04ef328e5fe5e` relative to `a9728454` plus its approved follow-up: source-point ordinary lookup, ADL suppression/union, typed associated class/enum/direct-base/enclosing records, recursive cv/ref/pointer/array/function result-and-parameter association, first-namespace and inline closure, direct using-declarations without using-directives, hidden-friend visibility, canonical candidate identity/order, typed call publication, PA15 demand/declaration/call lowering, and the narrow empty-class opaque ABI bridge are traced. The follow-up repairs only `dev/src/pa12_semantic_calls.cpp`; course 426 now covers five positive runtime cases and two rejection controls. Final `make test-pa16` is exit `2` at `218/243` with exactly `25` failures and `243/243` identities covered; exact authority/fresh comparison is `25/25`, fresh-only `0`, authority-only `0`, and inventories are `243/243/243` with missing `0/0`. Focused matrix is `11/12` with only the known nested-enum LowIR residual; PA12 controls are `2/2`; all three temporary wrapper probes compile, lower, translate, and run with status `0`; course 426 and `sh -n` pass; through-PA15 is `1167/1167`; file audit is `0` with five existing warnings; diff-check and bounded path/coverage audit are `0`; the follow-up preserves the exact 25-failure authority and PA16 remains incomplete only because those same residual identities remain. | completed audit |
