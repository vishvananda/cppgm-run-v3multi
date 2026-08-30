# PA16 implementation plan

## Stage Design

PA16 keeps one production pipeline: PA10/PA11 typed syntax and typed lookup
facts feed PA12 semantic selection and PA15 LowIR.  This checkpoint extends
the existing token-operator associated-record walk to ordinary unqualified
calls: ordinary lookup is formed at the function definition `SourcePoint`,
then both ordinary-call and token-operator ADL use the same typed associated
records, first enclosing namespaces, direct namespace using-declaration
entries, and hidden-friend relations.  Both ADL graphs exclude
using-directives.
Candidate identity remains `(ScopeId, BindingId)`; source spelling is not
transported.

The repair follows `spec.md` §§1--5 and 7: no second lookup engine, retry,
whole-program scan, host/reference shortcut, or alternate lowering pipeline.
The ADL fixtures use the existing opaque `obj<1x1>` representation for the
narrow case of one empty class parameter and a non-class result.  General
class pass-by-value and class return-by-value remain rejected.

## Failure Map and Authority

The supplied turn-start/current authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`217/243` passed, exactly `26` failed, and `243/243` identities are covered.
The complete current failure map is:

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
- `pa16/tests/general/300-adl-using-declaration-source-point.t`
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

The pre-landed parent baseline `d503a9c0` was `216/243` with exactly `27`
failures and full coverage.  Its complete 27-name map is exactly the current
26-name map above plus
`pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`.  That
27-name set is retained as baseline context; it is not the turn-start/current
authority.

StageProgressPreserved compares the fresh final PA16 run against the actual
turn-start/current 26-failure set: final failures must be at most `26`, all
`243` identities must be discovered and covered, and fresh-only failure
identities must be `0`.  Extra passes cannot compensate for a fresh failure.

## Active Checkpoint

This checkpoint owns associated-namespace ADL for an unqualified single-name
call at its definition `SourcePoint`.  `semantic_call_expression` performs
ordinary lookup at that point first.  Class members, block-scope functions, and
non-functions suppress ADL; namespace functions do not.  Only the existing
typed direct-call pipeline is extended, with no retry or alternate lookup
engine.

The data flow is:

- PA12 analyzes arguments once, then `collect_associated_adl_records` walks
  each named class/enum, its typed direct-base chain, and enclosing class
  records with deterministic deduplication.
- `collect_associated_adl_namespaces` maps each record to its first enclosing
  namespace and deliberately does not climb namespace parents.
- `append_adl_function_candidates` queries each associated namespace through
  `lookup_value_graph(..., include_using=false, point)`.  The graph therefore
  sees the namespace's own `ValueEntry`/provenance (including a direct
  using-declaration) and visible inline namespaces, but not using-directive
  targets.  Hidden-friend sidecars are admitted only when their declaration
  point is visible.  Every candidate retains canonical `(ScopeId, BindingId)`
  identity and is deduplicated in stable visitation order.  The existing
  token-operator collector uses the same `include_using=false` namespace graph,
  while retaining operator-token filtering, hidden-friend source-point checks,
  and the same stable canonical deduplication.

The accepted PA12-to-PA15 bridge is only the checked-in oracle shape:
namespace-owned, non-constructor, fixed non-variadic function; exactly one
by-value empty-class parameter; non-class result; one `ClassValue` conversion
to that parameter; and an lvalue argument with the same typed object record.
PA15 revalidates binding, function, ABI, conversion, category, and object
identity, suppresses only the parameter store, and passes the existing opaque
`obj<1x1>` argument slot/address.  It does not perform general class copying,
moving, prvalue materialization, or class return-by-value.  The existing narrow
constructor path is unchanged.  PA12 and PA15 intentionally revalidate this
small boundary independently; no broader semantic-model refactor is justified.

## Performance Evidence

Structural evidence is bounded by
`O(language-relevant associated records + direct bases + enclosing-class
records + directly reachable associated namespace/inline-namespace scopes and
their own using-declaration ValueEntries)`.  Each associated record and
namespace is visited once per call; neither ordinary-call nor token-operator
ADL traverses using-directive targets or scans unrelated program declarations.
Candidate deduplication compares only the collected canonical
`(ScopeId, BindingId)`
list, preserving deterministic order.  Focused LowIR is used to verify the
narrow opaque argument bridge; no unsupported timing, RSS, or whole-program
performance claim is made.

## Validation Status

Final validation:

- `make -C dev cppgm++ CXX=g++`: status `0`.
- Ephemeral associated-namespace using-directive negative probe: status `1`
  with `ERROR: PA12 unknown expression name`; the probe source is removed.
- Ephemeral associated-namespace using-directive operator negative probe:
  status `1` with `ERROR: PA12 invalid addition operands`; the probe source is
  removed.
- `make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/300-adl-using-declaration-source-point.t tests/general/200-implicit-member-call-suppresses-adl.t tests/general/300-hidden-friend-definition-adl-call.t tests/general/300-enum-operator-adl-selects-matching-overload.t tests/general/300-basic-operator-overloads.t tests/spec/300-hidden-friend-not-visible-to-unrelated-adl.t tests/spec/300-hidden-friend-not-visible-to-qualified-lookup.t tests/spec/300-operator-lookup-ordinary-adl-union.t tests/spec/300-lazy-class-lookup-ignores-later-using-directive.t tests/general/300-using-declaration-function-hides-tag.t tests/general/200-nested-out-of-class-constructor-enclosing-type.t'`: status `0`, `PASS (11/11)`; no focused control regressed.
- `make test-pa16`: status `2`, `218/243` passed, exactly `25` failures.
- Exact comparison against the turn-start authority in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
  comparison status `1` because residuals remain; authority `26`, fresh `25`,
  fresh-only `0`, authority-only `1`:
  `pa16/tests/general/300-adl-using-declaration-source-point.t`.
- Recursive status inventory: status `0`; discovered `243`, reference
  statuses `243`, fresh statuses `243`, covered `243`, missing `0`, orphan
  artifacts `0`.
- `n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`: status `0`, `1167/1167`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: status `0`
  with five pre-existing `bad-division` warnings.
- `git diff --check`: status `0`.

The only baseline failure eliminated is the source-point using-declaration
identity above; the associated-parent residual remains a PA11 qualified-type
failure before its call.  The complete 26-name turn-start map above remains
the authority.

## Next Checkpoint

Keep ADL namespace candidate formation closed at the first enclosing
namespace; do not absorb the current
`300-adl-associated-namespace-does-not-climb-parents` failure until its earlier
PA11 qualified type-name failure is separately owned.  After this checkpoint,
target that qualified-type data flow or another residual identity from the
preserved 26-name authority, not parent-namespace search or ADL using-directive
traversal.

## Checkpoint Ledger

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
| `ADL/source-point associated-namespace checkpoint` | Fresh `218/243`, `25` failures, `243/243` covered; authority/fresh `26/25`, fresh-only `0`, authority-only exactly `300-adl-using-declaration-source-point.t`; focused `11/11`; ordinary and token-operator using-directive negative probes rejected as required; through-PA15 `1167/1167`; file audit `0` with five known warnings; diff-check `0` | landed |
