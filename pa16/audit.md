# PA16 checkpoint audit

## Current Checkpoint Review

This bounded checkpoint audit covers landed commit
`6d2ed09cd4b3daf55ab28282addcf3a878a8adba` (`PA16 narrow typed ToVoid
discarded lowering`) relative to parent
`14cadc0c135156ed20583e3b5adb07b1260cabe2`.  The landed source change is in
`dev/src/pa15_lowering.h` and `dev/src/pa15_lowering_flow.cpp`; the landed
plan change is refreshed below.  The PA16 contract, active plan, this audit,
the required `spec.md` sections, the landed diff, PA12 ToVoid ownership, PA15
conversion consumption, the complete discarded-expression path, and focused
tests/fixtures were read before editing.  The tree was clean at turn start.

The repair found in this audit is narrow and fail-closed: PA15's void-cast
consumer now validates not only the single `ToVoid` kind/range, but also the
typed source and target against the actual child and cast facts.  This is the
nearest consumer-side invariant; the PA12 producer is complete for the valid
typed path, so no PA12 source change is warranted.  Final focused, prior-
through, primary-stage, file-audit, identity, and path-audit evidence is
recorded below.  No tests, fixtures, `.ref` files, exit-status sidecars,
harnesses, comparators, generated outputs, coverage/source-set rules, or
unrelated stage code changed.

### Authority and exact residual boundary

The primary authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The parent/increment provenance is separate: parent `14cadc0c` was recorded
at `222/243` with `21` failures.  The supplied turn-start authority for this
landed checkpoint is `224/243`, exactly `19` failures, and `243/243` identities
covered.  The earlier `222/243` figure is not this checkpoint's regression
budget.

The exact current failure set is:

- `pa16/tests/general/200-elaborated-member-forward-type.t`
- `pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t`
- `pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t`
- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-nested-braced-member-aggregate-init.t`
- `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
- `pa16/tests/general/200-reference-member-class-init.t`
- `pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t`
- `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
- `pa16/tests/general/300-callable-field-hides-private-base-method.t`
- `pa16/tests/general/300-friend-function-definition-skip.t`
- `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
- `pa16/tests/general/300-operator-nullptr-t-from-zero.t`
- `pa16/tests/general/300-overloaded-deref-user-assignment.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

The landed increment fixed exactly the two parent-only identities
`pa16/tests/general/100-function-pointer-nested-param-name-shadow.t` and
`pa16/tests/general/300-enum-class-nonmember-operator-bitand.t`.  The final
primary stage log reports `224/243` passing and the exact 19 identities above.
The final comparison reports authority/fresh failures `19/19`, retained `19`,
authority-only `0`, fresh-only `0`, and complete discovered/reference/fresh
inventories `243/243/243`, with all missing/unexpected counts `0`.  The 19
residuals are outside this audit's scope and are not reclassified.

### Typed ownership trace

```text
PA12 semantic_cast_to_target
  -> CastExpression fact + one typed ConversionFact(ToVoid)
  -> PA15 cast child/range/source/target validation
  -> typed discarded-expression lowering
  -> apply_conversions(ToVoid) -> LowIR
```

For a normal explicit void target, PA12 derives the source from the operand's
`expression_object_type`, selects `ConversionKind::ToVoid`, creates the
`CastExpression` with the operand as its only child, and attaches the one
conversion through `add_conversion`/`set_fact_conversion`.  The special PA12
array-to-void-pointer C-style cast publishes array-to-pointer plus
pointer-to-void conversions and remains on the pointer path.  No alternate
producer, source spelling, or lookup recovery is involved.

PA15's `children` helper validates the child identity/range and the cast case
requires one child.  The new endpoint check then requires one in-range
conversion, kind `ToVoid`, source equal to the child fact's object type, and
target equal to the cast fact's type.  Only after this proof does PA15
enter `DiscardedExpressionContext::ExplicitToVoid`; the already-typed child
still supplies evaluation order, and `apply_conversions` consumes the typed
ToVoid record into an empty void `LoweredValue`.

### Findings and disposition

1. The landed kind/range check had a real fail-closed gap: a malformed
   same-kind conversion could be consumed even if it described a different
   source or target.  The flow consumer now checks in-range source/target IDs
   and exact typed correspondence.  This is O(1), preserves valid PA12 output,
   and does not broaden the PA12 owner.
2. The discarded-value consumer keeps its typed boundaries.  Volatile scalar
   lvalues load; function ids and reference-bound ids do not manufacture a
   result read; explicit class lvalues materialize only their address; comma
   and void-conditional expressions preserve sequencing; assignment and
   increment/decrement retain effects without a redundant result load.  The
   only explicit-ToVoid scalar read exception is a direct non-reference scalar
   parameter binding, matching the checked-in O0 fixtures.  Ordinary locals,
   references, and other lvalue-producing facts remain non-materializing.
3. The PA12 producer, conversion ownership/ranges, LowIR O0 behavior, and
   source-set/file-audit boundary show no further defect in this increment.
   No durable new regression was necessary.  PA16's excluded copy/move,
   virtual/multiple inheritance, templates, and broad class-value semantics
   remain out of scope.

### Focused and broad validation

- Serial reconfirmation `make -C dev cppgm++ CXX=g++`: status `0`; log
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/reconfirm-rebuild.log`.
- Serial PA16 landed targets plus the class-lvalue control: `PASS (3/3)`; log
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/reconfirm-focused-pa16.log`.
- Serial PA15 short-circuit, comma/iteration, reference-return, and void-call
  controls: `PASS (4/4)`; log
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/reconfirm-focused-pa15.log`.
- Exact prior-through command with `n=16`: status `0`,
  `ALL TESTS PASSED SUCCESSFULLY! (1167 / 1167)`; log
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/prior-through-pa15.log`.
- Exact `make test-pa16`: status `2`, `TEST SUMMARY: 224 / 243 TESTS
  PASSED`, with exactly the 19 residual identities above; primary log
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/test-pa16.log`.
- Exact `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`:
  status `0`, five pre-existing header-body warnings; log
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/file-audit.log`.
- Exact identity/inventory comparison against `last-test.log`: status `0`;
  authority/fresh failures `19/19`, retained `19`, authority-only/fresh-only
  `0/0`, discovered/reference/fresh `243/243/243`, and all missing/unexpected
  counts `0`; log
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/identity-comparison.log`.
- `git diff --check`: status `0`; log
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/final-diff-check.log`.
- Bounded changed-path/file audit: status `0`; only the three approved paths
  changed and the index was empty; log
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/changed-path-audit.log`.

All commands above were run serially.  No new failure identity or coverage
loss occurred; the 19 residuals remain outside this audit's scope.

### Architecture and performance

The implementation remains one typed PA12-to-PA15 path.  The new check reads
one child fact, one conversion fact, and the cast type; discarded classification
reads one fact/binding/type tuple.  There is no text recovery, duplicate
pipeline, scan/retry, allocation, cache, or whole-program traversal.  The
prior O0 probe recorded 21 instructions across seven checked functions and
exactly the expected pointer/i32 loads; selected `do_start_op` recorded one
`on_immediate` pointer load, and the enum operator recorded two i32 loads.
These are structural counters, not timing or benchmark claims, but they show
bounded LowIR growth and no evident timeout risk.

The exact file audit passes with only the five pre-existing header-body
warnings.  This repair adds no translation unit and requires no source-set
edit; the validated checkpoint contains only `dev/src/pa15_lowering_flow.cpp`,
`pa16/plan.md`, and `pa16/audit.md`.  The remaining uncertainty is the supplied
19-identity residual map and future lvalue-producing shapes outside the narrow
parameter exception, not an observed regression from this audit.

### Final status

The bounded repair, final evidence, and path/file checks complete this
checkpoint audit.  PA16 remains incomplete with the exact unchanged residual
19-failure map and full 243-identity coverage.  The next separately bounded
checkpoint is the first residual identity,
`pa16/tests/general/200-elaborated-member-forward-type.t`; this audit does not
repair or re-audit it.

## Historical Derived-Base Reference Binding Review (e470e9df)

This bounded review covers landed commit
`e470e9dfed07ca09a373d227640f3c8042cc2cbf` (`PA16 enable prvalue derived-base
reference binding`) relative to parent `f3afe9d5`.  The landed source change
is in `dev/src/pa12_semantic_resolution.cpp`.  The PA12 fact/publication and
PA15 lowering files listed in the checkpoint brief were read as ownership
consumers; no consumer repair was required.  This audit record contains that
source repair and the two documentation updates only.  No
tests, fixtures, `.ref` files, exit-status sidecars, harnesses, comparators,
generated outputs, coverage/source-set rules, or unrelated stage code changed.
The turn-start working tree was clean.

### Authority and exact residual boundary

The authoritative primary log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
It records the audit-turn starting checkpoint as `220/243` with `23` failures
and full `243/243` identity coverage.  The parent baseline before the landed
increment was `219/243` with `24` failures.  That `24 -> 23` comparison is
provenance for the landed increment, not permission to regress: the final
run must retain all `243` identities, add no failure identity, and remain at
or below this current `23`-failure set.  The required broad gates below were
run after the focused milestone and were compared by identity, not only by
pass count.

The current residual identities in the supplied log are:

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
- `pa16/tests/general/300-callable-field-hides-private-base-method.t`
- `pa16/tests/general/300-enum-class-nonmember-operator-bitand.t`
- `pa16/tests/general/300-friend-function-definition-skip.t`
- `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
- `pa16/tests/general/300-operator-nullptr-t-from-zero.t`
- `pa16/tests/general/300-overloaded-deref-user-assignment.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

The selected identity `pa16/tests/general/300-prvalue-derived-base-friend-
operator.t` is not residual.  The 23 identities above remain outside this
bounded audit and are not reclassified or repaired.

### Typed ownership trace

The relevant typed path is:

```text
source fact + value category + target reference cv/type
  -> conversion_for viability and typed ranking
  -> derived_base_choice / derived_base_relation access and path
  -> selected ConversionChoice
  -> add_conversion canonical ConversionFact and path arena
  -> PA15 apply_derived_base_conversion
  -> validated direct-base address projection and reference call argument
```

`conversion_for` consumes the source semantic fact, distinguishing lvalue,
prvalue, and xvalue inputs, and the target reference kind/CV.  The derived
choice is built from canonical `NamedRecordId`/direct-base records.  The
typed relation walk validates the single-inheritance endpoint, distance,
and `access_scope`; no source spelling or host/reference query participates.
`select_typed_function` and `select_typed_operator` compare these choices.

`apply_context_conversion` publishes the selected choice through
`pa12_semantic_facts.cpp::add_conversion`, which validates derived-base
metadata and records the canonical conversion plus its path.  It rejects
base metadata on unrelated conversion kinds.  PA15's structural-conversion
dispatcher sends `DerivedToBase` to `apply_derived_base_conversion`; that
consumer rechecks source/target object identity, access, path count/distance,
and complete direct-base layout before emitting a typed
`base_subobject` projection.  The call lowerer then passes the resulting
reference address to the selected function/operator.  Constructor actions
already provide addressable temporary storage and lifetime ownership, so the
landed path requires no PA15 change or duplicate materialization.

### Findings and disposition

1. The landed non-lvalue cv-reference branch called `derived_base_choice`
   for volatile targets as well as const targets.  That made a `Derived`
   temporary bind to `volatile Base&` and `const volatile Base&`.  The repair
   limits this new derived-to-base choice to exactly const/nonvolatile
   lvalue references and blocks the target-directed constructor fallback for
   volatile lvalue-reference targets.  Rvalue references and ordinary
   `const Base&` temporary binding remain supported.
2. The newly viable `const Base&` candidate could beat an exact
   `const Derived&` candidate: the existing standard-conversion comparator
   gives a derived-to-base candidate precedence over a non-derived candidate
   at the same broad category.  The repair gives an exact same-class
   temporary reference choice exact rank in this branch.  Nearer/farther
   base distance, CV, access, and non-base rejection remain typed; no
   class-by-value conversion is introduced.

Boundary probes covered the selected prvalue hidden-friend operator, an
exact-derived versus base overload, nearer versus farther base selection,
an xvalue direct-derived/base overload, existing lvalue/reference controls,
volatile and const-volatile rejection, non-const and non-base reference
rejection, private-base outside/friend access, and class-by-value rejection.
No new regression or fixture was added.  Multiple inheritance, templates,
copy/move, general temporary materialization, and PA17 value transfer remain
out of scope.

### Focused validation

After the repair, sequential focused validation reported:

- `make -C dev cppgm++ CXX=g++`: status `0`.
- `make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/300-prvalue-derived-base-friend-operator.t tests/spec/200-conditional-derived-base-lvalue-reference.t tests/spec/200-const-reference-binds-derived-pointer-prvalue.t tests/general/300-const-method-array-member-binds-const-reference.t tests/general/300-basic-operator-overloads.t tests/general/200-derived-pointer-overload-prefers-base-over-void.t tests/spec/300-inherited-const-method-base-pointer-cv-bad.t tests/spec/200-derived-base-reference-overload-rank.t'`: status `0`, `PASS (8/8)`.
- Ephemeral source probes outside the repository reported status `0` for
  exact-derived, nearer-base, xvalue, and friend-private-base cases; status
  `1` for volatile/const-volatile base references, inaccessible private base,
  class-by-value, non-const base reference, and non-base reference cases.
- The target LowIR shape retained two constructor calls, two addressable
  temporary objects, two canonical base projections, and one hidden-friend
  operator call.
- `git diff --check`: status `0`.

The exact required root-gate transcripts are preserved under
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-e470-checkpoint-audit-20260830/`:

- `01-through-pa15.log`: the exact `n=16` prior-through command exits `0` at
  `1167 / 1167`.
- `02-file-audit.log`: the exact PA16 file audit exits `0` with only the five
  known `bad-division` warnings.
- `03-test-pa16.log`: the exact `make test-pa16` exits `2` with `220 / 243`
  passing and `23` residual failures.
- `04-identity-comparison.log` and `05-acceptance.log`: fresh and authority
  failure sets are both `23`, with fresh-only and authority-only both `0`;
  discovered/reference/fresh identity inventories are exactly
  `243/243/243`, and all missing/unexpected counts are `0`.

Thus the additional passes do not mask a new failure identity; the fresh set
is within the audit-turn limit and has no fresh-only member.

### Architecture and performance

The new work is bounded by the existing typed candidate set and inheritance
path.  A qualifying derived-base candidate performs one direct-base relation
walk of height `H`; across `C` typed candidates and `A` arguments this is
O(C*A*H).  There is no global scan, textual downgrade, retry/duplicate
pipeline, host/reference shortcut, or unbounded allocation.  The target's
two-constructor/two-projection LowIR shape is representative structural
evidence; this milestone makes no timing or RSS claim.

Current disposition is a complete, coherent bounded audit/repair of e470e9df.
The required prior-through, file-audit, full-PA16, identity, diff, and path
checks pass their authorized criteria.  PA16 remains incomplete because the
same 23 supplied residual identities remain; no unrelated residual was
re-audited.

## Historical Source-Point ADL Review (ab1b2a8c)

This final bounded checkpoint audit is for landed commit
`ab1b2a8c4a20752434d608b5aef04ef328e5fe5e` (`pa16 add source-point-aware
associated ADL`) relative to `a9728454`, including its approved bounded
follow-up.  The landed ownership path is
`dev/src/pa11_semantic_model.h`, `dev/src/pa12_semantic_calls.cpp`,
`dev/src/pa15_lowering.h`, `dev/src/pa15_lowering_calls.cpp`,
`dev/src/pa15_lowering_flow.cpp`, and `pa16/plan.md`.  The audit found two
genuine in-scope defects in that path: associated namespace formation omitted
the standard inline-namespace closure, and typed associated-record formation
stopped before supported pointer, array, and function-type wrappers.  The
bounded source repair remains confined to
`dev/src/pa12_semantic_calls.cpp`; course regression 426 now covers both
repairs.  The documentation changes are this record and `pa16/plan.md`.  No
handout test, `.ref` fixture, sidecar, harness, comparator, reference
material, generated output, coverage rule, source-set file, or unrelated stage
surface changed.

The supplied turn-start/current authority in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` is
`218/243` passing, exactly `25` failures, with all `243/243` test identities
covered.  The parent checkpoint was `217/243` with `26` failures.  The landed
increment removed exactly
`pa16/tests/general/300-adl-using-declaration-source-point.t`; that is the
parent-to-current authority relationship.  The authorized post-repair full
stage also reports `218/243`, exactly `25` failures, and `243/243` coverage.
Sorted failure identities compare exactly: authority `25`, fresh `25`,
fresh-only `0`, authority-only `0`; the reference and fresh status inventories
each contain all `243` identities with no missing artifacts.

### Contract and ownership trace

The representative ordinary-call path is:

```text
function-definition ScopeId
  -> lookup_source_point
  -> ordinary lookup_value_path at that SourcePoint
  -> direct candidates + associated ADL candidates
  -> typed overload selection and CallExpression fact
  -> PA15 reachable-function demand/declaration plan
  -> lower_call and the existing call boundary
```

`semantic_call_expression` admits ADL only for an unqualified, non-template
single `IdExpression`.  It obtains the function-definition `SourcePoint` with
`lookup_source_point(scope)` and passes it to the existing typed
`lookup_value_path`; omitted points follow the same source-point helper.  The
ordinary result is inspected before ADL: a non-function, a class member, or a
function whose canonical origin is block scope suppresses ADL, while
namespace-owned ordinary functions permit the ordinary-plus-ADL union.  The
union condition preserves the ordinary-only and ADL-only cases and does not
retry lookup after overload failure.

When ADL is allowed, PA12 analyzes each ordinary argument once into `ExprInfo`
and uses those typed facts to form associated objects.  It does not rescan
source text or re-run an independent expression analyzer.  The associated
record collector walks typed `TypeId` nodes: cv/lvalue-reference/rvalue-
reference, pointer, and array nodes recurse through `child`, and function
nodes recurse through their typed result and parameters.  Thus the array
element is associated before the later array-to-pointer call conversion, and a
function pointer can associate a class in a reference parameter.  Named
class/enum records are then collected, complete classes use the validated
direct-base chain, and enclosing class records are added.  Type and record
identities are bounds-checked and deduplicated in stable order.  Member-pointer
nodes remain terminal; no template expansion or general class-value semantics
is opened.  An incomplete named class still associates its namespace but does
not expand a base chain without complete scope metadata; malformed incomplete
base metadata fails closed.

Each associated record maps to its first enclosing namespace.  Namespace
parents are not climbed.  The repaired `collect_associated_adl_namespaces`
then applies only the standard inline closure: an inline namespace contributes
its parent, and a namespace contributes directly contained inline namespaces;
the closure is transitive through the typed `Scope::children` relations.  It
does not traverse using-directive targets.  For every associated namespace,
`append_adl_function_candidates` calls the existing
`lookup_value_graph(..., include_using=false, point)`.  Thus direct
`ValueEntry` entries, including direct using-declarations with canonical
origin, participate; using-directive edges do not.  Source visibility is
checked by the existing graph and relation point checks.  Hidden-friend
sidecars are separately filtered by their declaration point and must retain a
canonical namespace owner.

The inline repair is necessary because the existing value graph returns after
finding visible direct values and therefore need not descend into an inline
child.  An associated outer namespace with a direct but nonviable overload
could consequently hide a viable inline overload.  Explicitly collecting the
inline closure makes that case queryable while preserving the graph's ordinary
lookup behavior and its source-point filtering.  A type declared in an inline
namespace can likewise associate its enclosing namespace's function set.

All candidate paths validate canonical `(ScopeId, BindingId)` ownership and
deduplicate that pair in stable order.  Typed overload selection then applies
the existing conversion/default-argument rules and publishes the selected
binding, owner scope, callable type, result category, and typed child facts in
one `CallExpression`.  The token-operator collector shares the associated
record/namespace machinery and `include_using=false` graph while retaining
operator-token filtering and hidden-friend source checks.

PA15 consumes the published call fact through normal reachable-function
demand, declaration materialization, and `lower_call`; no alternate lowering
path is introduced.  The sole class-by-value bridge is deliberately narrow:
one namespace-owned non-constructor, nonvariadic function with one empty-class
by-value parameter and a non-class result, an exact `ClassValue` conversion,
and an lvalue argument with the same canonical object record.  PA15 rechecks
the binding, function ABI, conversion, category, and object identity, lowers
the source expression once with deferred conversion, and passes the existing
opaque `obj<1x1>` temporary/address without class-copy materialization.
General class pass-by-value, class return-by-value, and broader constructor or
object cases remain rejected.  No host compiler, reference binary, text
identity, program-wide scan, second lookup engine, retry, or alternate
lowerer is used.

### Findings and bounded repair

The first defect was the inline-namespace omission: the repair is a small
typed closure over already materialized namespace relations; it neither
changes ordinary namespace-parent association nor opens using-directive
traversal.  The follow-up probes established that the supported wrapper forms
were also a genuine gap, not an out-of-bound feature: the clean committed
binary rejected pointer, array, and function-pointer probes at PA12 expression
publication, while the repaired collector makes each compile, lower, and run.
The wrapper walk is typed and bounded; function result/parameter traversal is
association only and does not introduce class-by-value execution.  The first
wrapper-only broad run temporarily added exactly two fresh identities
(`100-global-reference-incomplete-referent.t` and
`100-incomplete-class-return-function-address.t`); the bounded incomplete-class
guard restored both and the final failure set.  The 25 residual identities
were not re-audited or reclassified here.

### Performance evidence

The typed wrapper walk is structurally bounded by the visited `TypeId` nodes
and each function node's existing result/parameter vectors.  Record work is
bounded by the visited `NamedRecordId` values, validated direct bases, and
enclosing class scopes; namespace queue dedup means each associated namespace
is enqueued once.  Lookup-generation marks bound and cycle-protect each
individual graph traversal, but does not prevent traversal across separate
`begin_lookup` queries.  Candidate deduplication compares only the collected
canonical identity list.  No unrelated program declaration scan, allocation
cache, retry, or second lowerer was added.  This is structural evidence only;
no timing, RSS, or unsupported performance claim is made.

### Focused validation and residual boundary

The compact post-repair validation is:

- `sh -n cppgm.tests/course/pa16/426-typed-adl-inline-namespace-regression.sh`: status `0`.
- `make -C dev cppgm++ CXX=g++`: status `0`.
- Temporary typed wrapper probes outside tracked surfaces
  (`/tmp/pa16-adl-wrapper-probes.KYl3k0/pointer.cpp`, `array.cpp`, and
  `functionptr.cpp`): each application compile, LowIR translation, CY86
  translation, and program run returned status `0` after the repair.  The
  clean committed binary rejected all three at PA12 expression publication.
- `make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/300-adl-using-declaration-source-point.t tests/general/200-implicit-member-call-suppresses-adl.t tests/general/300-hidden-friend-definition-adl-call.t tests/general/300-enum-operator-adl-selects-matching-overload.t tests/general/300-basic-operator-overloads.t tests/spec/300-hidden-friend-not-visible-to-unrelated-adl.t tests/spec/300-hidden-friend-not-visible-to-qualified-lookup.t tests/spec/300-operator-lookup-ordinary-adl-union.t tests/spec/300-lazy-class-lookup-ignores-later-using-directive.t tests/general/300-using-declaration-function-hides-tag.t tests/general/200-nested-out-of-class-constructor-enclosing-type.t tests/general/300-nested-enum-hidden-friend-bitmask-adl.t'`: status `2`, `11/12`; only the known `300-nested-enum-hidden-friend-bitmask-adl.t` LowIR comparison residual fails.
- `make -C pa12 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/200-inline-namespace-unqualified-call.t tests/general/200-using-directive-call.t'`: status `0`, `PASS (2/2)`.
- `sh cppgm.tests/course/pa16/426-typed-adl-inline-namespace-regression.sh`: status `0`; five runtime cases pass (two inline-namespace cases plus pointer, array, and function-pointer association), while ordinary-parent and using-directive association controls reject as expected.
- `git diff --check`: final status `0`.

- `make test-pa16`: exit `2`, `218/243` passed, exactly `25` failures, and
  `243/243` identities covered.
- Exact sorted failure-identity comparison against
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
  authority `25`, fresh `25`, fresh-only `0`, authority-only `0`.
  Inventory is `243` discovered, `243` reference status sidecars, `243`
  fresh status sidecars, with missing reference `0` and missing fresh `0`.
- `n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`: status `0`, `1167/1167`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: status `0`
  with five existing `bad-division` warnings and no fatal findings:
  `abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
  `pa11_semantic_model.h`, and `pa15_lowering.h`.
- Bounded changed-path and coverage audit: status `0`; status paths `4`, tracked
  paths `4`, staged paths `0`, unapproved paths `0`, discovered tests `243`,
  reference sidecars `243`, fresh sidecars `243`, missing reference/fresh
  `0/0`, and course mode `775`.

The complete current 25-name authority map and parent relationship are in
`pa16/plan.md`; no unrelated residual was re-audited.  PA16 remains
incomplete because those same 25 identities remain failures.

## Historical Pack Layout Review (08472cce)

This bounded checkpoint audit covers landed commit
`08472cce8e96daa585f5f07f4ee9d2233e13ade9` (`PA16 typed pragma pack record
layout`) relative to parent `0ff3fdef`, plus the narrow source repairs found
during this audit.  The landed ownership path spans
`dev/src/IPPTokenStream.h`, `dev/src/preproc_session.cpp`,
`dev/src/posttoken.h`/`dev/src/posttoken.cpp`, `dev/src/pa10_ast.h`/
`dev/src/pa10_ast.cpp`, `dev/src/pa10_parser_support.h`/
`dev/src/pa10_parser_support.cpp`, and
`dev/src/pa11_semantic_model.h`, `dev/src/pa11_semantic.cpp`, and
`dev/src/pa11_record_layout.cpp`.

The bounded checkpoint diff repairs only
`dev/src/posttoken.cpp`, `dev/src/pa10_parser_support.cpp`,
`dev/src/pa11_semantic.cpp`, and `dev/src/pa11_record_layout.cpp`, and
refreshes `pa16/plan.md` and this record.  No handout tests, fixtures, `.ref`
files, sidecars, harnesses, comparators, source-set lists, or generated
outputs were changed; the sole public-layer addition is course regression 422,
`cppgm.tests/course/pa16/422-typed-pack-wide-bitfield-layout-regression.sh`.

The latest landed-stage authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`210/243` identities pass, exactly `33` fail, and all `243/243` identities
are covered.  The increment's earlier turn-start authority was `209/243`
with `34` failures; the landed increment cleared
`pa16/tests/general/300-packed-class-layout.t`.  The exact current 33-entry
residual map is preserved in `pa16/plan.md` and in the final identity
comparison log below.  The fresh broad gate preserves this authority exactly.

### Contract and ownership trace

The representative production path is:

```text
PPPreprocessingSession::Impl active cap/stack and conditional state
  -> ordered PPPackDirective at the raw phase-3 boundary
  -> token-transparent PostTokenStream callback handoff
  -> PA10 whitespace-free PA10PackDirective boundary
  -> PA11 binary-search pack lookup / NamedRecord::pack_alignment
  -> canonical member/base/bit-field/final record layout
```

`run` resets pack state per preprocessing session.  Directive text is flushed
before each directive and include; include expansion shares the same active
cap/stack, while `handle_directive` ignores pragmas in inactive conditional
branches.  Recognized `pack(push, integral)` and `pack(pop)` forms are fully
validated before a fact is appended.  Push saves the outer cap, pop restores
it, and an unmatched pop fails closed.  The fact stores both the operation
and effective cap, with zero denoting natural layout; no text is later
rendered or reparsed.  The implementation intentionally does not diagnose a
nonempty push stack at end of translation unit, so the live state remains
available to later included/source text.

`PPPackDirective::token_index` is an ordered raw-token boundary.  The
posttoken stream queues facts without flushing pending adjacent strings or
clearing pending `operator` formation, then flushes strings, forwards all
facts at the boundary, and emits the next independent token.  The adapter
now rejects descending/out-of-range boundaries, invalid enum operations,
inconsistent push/pop state, and facts after an already emitted EOF before
any output is delivered.  PA10 records the callback after whitespace removal
and after the existing `>>` split semantics; its boundary validator repeats
the ordered typed-state proof before publishing the AST side vector.

PA11 finds the last directive at each class node's `source_begin` by binary
search and stores only the active cap on the canonical `NamedRecord`.  The
existing record-layout owner applies the cap to ordinary members, direct
bases, bit-field storage, and final record alignment before size rounding.
Explicit member and class `alignas` requests remain stronger.  The audit
verified that pop-restored records and records outside the active interval
retain natural layout, and that no PA15/LowIR pack inference or second layout
owner was introduced.

### Findings and bounded repair

The audit found two genuine issues in the landed ownership path.

1.  The wide bit-field allocation branch aligned from the natural storage
    alignment instead of the already capped `field_alignment`.  Under
    `pack(1)`, the focused `char; int:33; char` probe consequently emitted
    size `13`; the repair uses `field_alignment`, and the same probe now
    emits packed size `10` versus natural size `16`.
2.  The typed raw-token adapter could queue a fact at an index after an
    existing EOF, after `emit_eof()` had already flushed pending callbacks,
    and then return without delivering that fact.  It also accepted malformed
    operation/state combinations until a later consumer.  Pre-output
    validation in `posttoken.cpp`, revalidation at the PA10 boundary, and
    explicit PA11 push/pop operation checks now fail closed.  Valid facts are
    still delivered in vector order, including multiple facts at one
    boundary.

The repairs are typed, local, and linear in the fact vector.  They add no
retry, timeout, whole-program cache, source-text downgrade, host/reference
shortcut, or alternate layout owner.

### Bounds, evidence, and boundaries

For `T` raw tokens and `D` recognized directives, preprocessing fact handling,
the two ordered-state validations, and posttoken replay are `O(T + D)`.
The preprocessing and temporary validation stacks use `O(P)` and `O(D)`
memory; the persistent typed fact vectors use `O(D)` compact storage.  PA11
does one `O(log D)` lookup per record and retains the existing `O(M)` layout
walk for `M` members, giving `O(M + R log D)` for `R` record definitions.
No per-node strings, node-based pack map, retry, or unbounded cache appears.

Focused structural evidence covers two directives/two records (`B` cap 1
and size 5; `C` restored natural size 8), conditional active/inactive
behavior (`X` cap 1 and size 5), same-boundary push/pop (natural size 8),
explicit `alignas` stronger than pack, and the corrected wide-bit-field
10/16 pair.  The adjacent-string probe with push/pop between string parts
remains accepted.  No timing or RSS measurement was taken in this focused
audit, so no measured performance-regression claim is
made.

### Focused validation and residual boundary

`make -C dev cppgm++ CXX=g++` passed.  `sh -n` and execution of course 422
passed, durably checking packed wide-bit-field size 10 against natural size
16.  Direct compiler probes for packed
layout, conditional packing, explicit `alignas`, pop restoration, the
same-boundary case, adjacent strings, and natural/wide controls all exited
`0` with the expected structural sizes.  A temporary typed-buffer probe
passed valid same-boundary push/pop delivery and rejected post-EOF, invalid
operation, and mismatched push-state facts.  `git diff --check` passed.

`make test-pa16` exited `2` at `210/243`; its complete output is retained at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-final-20260830.log`.
The independent comparison at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-identity-coverage-20260830.log`
reports authority failures `33`, fresh failures `33`, fresh-only `0`,
authority-only `0`, inventory `243`, unexpected `0`, and `243/243` covered.
The exact fresh set is the 33-entry map in `pa16/plan.md`; it is a subset of
the supplied latest authority with no compensation-based pass accounting.
The exact required through-PA15 command exited `0` at `1167 / 1167`; its
complete output and explicit status are retained at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-through-pa15-20260830.log`.
`perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exited `0`
with five known `bad-division` warnings (`abi_mangle.h`,
`cpp_semantic_core.h`, `lowir_model.h`, `pa11_semantic_model.h`, and
`pa15_lowering.h`) and no fatal findings; its complete output and explicit
status are retained at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-file-audit-20260830.log`.

The known pack-adjacent residual is
`pa16/tests/general/300-pragma-pack-followed-by-endif.t`, whose checked
LowIR mismatch is the unrelated canonical `trunc`-before-`zext` shape.  The
other 32 current residual identities, `_Pragma("pack(...)")`, and all later
PA16 residuals remain outside this bounded audit.  Course 422 is limited to
the wide-bit-field pack owner and its natural control; it does not broaden
into unrelated stage surfaces.  This is a checkpoint audit only; PA16 full
completion is not claimed.

## Historical Constructor-Argument Review (ee8f44d5)

This completed checkpoint audit covers landed commit
`ee8f44d5b0e9d4910679c12b443533d787d1cd4c` (`PA16: emit per-throw typed
array cleanup`) relative to `3b2b4882`, plus one bounded fail-closed repair in
the same PA15 lowering owner. The landed source increment is limited to
`dev/src/pa15_lowering.h` and `dev/src/pa15_lowering_construction.cpp`; this
audit updates those allowed source files, `pa16/plan.md`, and this record.
No tests, fixtures, reference outputs, sidecars, harnesses, comparators,
coverage rules, source-set lists, or generated outputs are in the tracked
diff; ignored harness outputs were removed after validation.

The supplied current-stage authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`209/243` identities pass, exactly `34` fail, and all `243/243` identities
are covered. The preserved `3b2b4882` ledger baseline is `208/243` with
`35` failures. The recorded landed delta is baseline-only
`pa16/tests/general/300-synthesized-array-member-lifecycle.t`, with no
fresh-only identity; the complete current residual set is retained in the
Failure Map in `pa16/plan.md`. The completed full-stage comparison below
confirms that the repair adds no failure and removes no coverage identity.

### Contract and ownership trace

The affected typed path is:

```text
PA12 FunctionFact.constructor_action_begin/count
  -> ordered ConstructorActionFact range
  -> lower_constructor_action or placement-array lowering
  -> ArrayAddressRoot
  -> recursive emit_constructor_elements
  -> transient ConstructedElement completed prefix
  -> constructor_elements_may_throw / emit_constructor_call_with_cleanup
  -> fresh typed root/path address replay
  -> model_.destructor_binding(record) cleanup calls
```

PA12 remains the semantic owner. Its constructor fact publishes one contiguous
typed action range; each action records the Base/Member target, exact
`object_type`, selected constructor or initializer, typed argument range, and
value-initialization state. `checked_constructor_function` then verifies the
selected function, owning class, hidden object parameter, function scope, and
action range before PA15 emits a call. The lowerer does not reconstruct source
text, reselect an overload, or create a second semantic action owner.

For a constructor member or base, `ArrayAddressRoot` retains the active
constructor record and the immutable action. For placement construction it
retains the typed storage binding and array type. `emit_constructor_elements`
walks every fixed array dimension in forward lifetime order. At each class
terminal it records only the typed root, array-index path, terminal record,
and canonical `model_.destructor_binding(record)` in a lowering-only
`ConstructedElement` vector. Nested arrays therefore replay all indices from
the same root; no rendered address or cross-block SSA value is retained.

`constructor_elements_may_throw` strips only typed expression/cv wrappers,
checks the terminal class record and constructor identity, validates the
selected signature/range, and treats a synthetic zero-action constructor as a
demand-elided no-op. It now combines the constructor boundary
(`BindingSidecar::nonthrowing` and the synthetic constructor nothrow cache)
with every actual typed argument, whether the fact comes from the ordered
constructor-argument arena or the semantic argument vector. Each argument is
checked through the existing memoized `constructor_initializer_is_nothrow`
walk: a valid but unproven expression is conservatively potentially throwing,
while an invalid nothrow fact fails closed. The combined predicate is computed
once at the array root and threaded through recursive lowering, so a bare
constructor `noexcept` cannot suppress a handler needed for pre-entry
argument evaluation.

A known nonthrowing constructor with known-nothrow arguments remains on the
direct path; a potentially throwing constructor or argument after a nonempty
completed prefix enters `emit_constructor_call_with_cleanup`. That routine
creates a fresh `eh_try`/cleanup/continuation boundary, recomputes the
destination inside the protected block, evaluates the typed call and its
arguments, closes the normal edge with `eh_end`, and replays the completed
prefix in reverse from freshly recomputed addresses before `resume`.

This matches the PA13 handler discipline: normal paths pop the current handler
with `eh_end`; cleanup paths terminate with `resume`; every generated block is
terminated. The first terminal can use its normal destination; later throwing
terminals never import a normal-path address producer into their handler. The
normal destructor suffix remains a separate reverse lifetime path and was not
changed by this checkpoint.

### Audit findings and bounded repair

The landed per-throw design is architecturally sound for the owned path. A
shared `ArrayCleanupChain` would not provide an independent already-constructed
prefix for each possible throw point, while the new transient prefix and
fresh root/path replay do. The primary checked synthesized-member array
fixture exercises the intended two handler prefixes and passes in the focused
matrix.

The audit did find a genuine fail-closed gap at the typed fact boundary. The
old constructor-action lowering accepted a missing or mismatched
`object_type` by falling back to the member/base binding type, and replay did
not compare an action/storage root type with its recorded semantic source. A
malformed fact could consequently select a different array shape or storage
root before a later check rejected it. The bounded repair adds:

- one common `checked_constructor_action_target_type` proof for mutually
  exclusive Base/Member identity, valid direct member ownership, and the
  member/base-derived type;
- exact `action.object_type` equality and constructor argument-range checks in
  `lower_constructor_action`, including zero-argument malformed ranges; and
- action-root and placement-root replay checks for exact type identity,
  binding-table ownership metadata, and storage-binding type equality.

The same audit found the in-scope exception-classification gap: the old query
could return nonthrowing from a bare user constructor `noexcept` without
examining argument evaluation. The repair passes the actual argument
representation to the query, uses the existing bounded semantic nothrow
memo/worklist, and retains the completed-prefix handler for every valid
argument that is not proven nonthrowing. Malformed argument IDs, child ranges,
signature arity, and arena ranges fail closed. The argument query is performed
once per array construction and its result is threaded through the recursive
terminal walk; it is not a per-terminal retry or a reconstructed source scan.

These checks are bounded indexed work over existing typed arenas. They do not
add a retry, whole-program cache, source-text reconstruction, alternate
semantic owner, or valid-input emission shortcut. They are confined to the two
permitted PA15 source files. The previously noted argument-classification
uncertainty is resolved; no in-scope correctness uncertainty remains.

### Bounds, evidence, and boundaries

For `N` flattened class terminals and maximum array-path depth `D`, collecting
terminal records is `O(ND)` because each terminal copies its typed path. The
argument query scans `A` actual typed facts once per array construction and
uses the existing memoized semantic-fact walk, `O(A + F_A + E_A)` for the
reachable argument facts and child edges, rather than `O(NA)`. Each of the
`N-1` later potentially throwing terminals with a live destructor prefix gets
an independent cleanup body, and each body replays its prefix in reverse, so
explicit cleanup/address-projection output is intentionally `O(N^2D)`. With
fixed `D`, the destructor-call portion is the required triangular
`N(N-1)/2`. This superlinear emitted-IR bound is required by the LowIR EH
contract: there is no shared remaining-prefix cursor, and a cleanup body
cannot borrow an SSA address from another block. Nothrow, no-op, and
empty-prefix cases avoid those handlers. No timing, RSS, allocation,
whole-program retry, textual fallback, or unbounded-cache claim is made.

The representative structural evidence at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-constructor-array-focused-20260830.log`
records build exit `0`, flat N=1 as 1 block/0 handlers/1 constructor/7
instructions, nested N=6 and D=2 as 11 blocks/5 handlers/5 resumes/6
constructors/15 cleanup destructors/190 instructions, and a three-element
noexcept case as 1 block/0 handlers/3 constructors/17 instructions. It also
records demand-elision for the synthetic no-op case. The post-repair
constructor-argument probe at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-constructor-argument-throw-focused-20260830.log`
distinguishes the required cases: typed `E(int) noexcept` with `1+2` has
zero handlers; the same constructor with potentially throwing `value()` has
one handler, one resume, and one cleanup destructor; and ordinary potentially
throwing `E(int)` with `1` retains that same one/one/one shape. The typed
no-argument control also has zero handlers and two constructor calls.

Both argument representations were checked. The ordered typed constructor
argument arena is reachable through synthesized constructor actions and is
covered by the three typed cases above. The semantic argument vector remains
hardened for the storage-backed placement path, but current PA16 grammar does
not legally form an array terminal through it: direct `E arr[2](1)` is
rejected by PA12 as an invalid conversion, and placement `new` array forms
are rejected as unsupported expression forms before PA15 lowering. This is a
reachability conclusion from the checked probes, not invented coverage. The
five focused residual identities remain the already-mapped local-array
presentation, two placement-new, value-init aggregate, and friend-access
cases; no unrelated residual family was re-audited.

### Final validation

`make -C dev cppgm++ CXX=g++` exits `0`; the post-repair focused 35-test
constructor/array/lifetime matrix is `30/35`, with the primary passing and
the five selected residuals already in the authority. The matrix is durably
recorded in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-constructor-argument-focused-matrix-20260830.log`.

`make test-pa16` exits `2` at `209/243`, with exactly `34` failures and
`243/243` identities covered. The exact sorted comparison with the supplied
authority is in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-constructor-argument-identity-coverage-20260830.log`:
authority `34`, fresh `34`, authority-only `0`, fresh-only `0`, inventory
`243`, covered `243`, missing `0`, and unexpected `0`.

The required command

```text
n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
```

exits `0` at `1167/1167`; its durable output is in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-constructor-argument-through-pa15-20260830.log`.
`perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0`
with the five pre-existing header-division warnings, recorded in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-constructor-argument-file-audit-20260830.log`.
The final changed-file audit is recorded in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-constructor-argument-changed-file-audit-20260830.log`;
it is restricted to the two permitted PA15 source files and these two PA16
records. `git diff --check` exits `0`, with the durable result in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-constructor-argument-diff-check-20260830.log`.

The PA16 stage remains incomplete because the recorded 34 residual
identities remain; this checkpoint makes no full-stage completion claim.

## Historical Typed Destructor-Suffix Review (70327e4d)

This final checkpoint audit covers landed commit
`70327e4d72ad5d223018565ec78d290ea4ac6f0a` (`PA16 typed destructor suffix
cleanup`) relative to `a3de5c21`, plus one narrowly bounded repair in the same
lowering owner. The landed source files are
`dev/src/pa15_lowering.cpp`, `dev/src/pa15_lowering.h`,
`dev/src/pa15_lowering_construction.cpp`, and
`dev/src/pa15_lowering_flow.cpp`. The bounded repair is only in
`dev/src/pa15_lowering_construction.cpp`; the record updates are
`pa16/audit.md` and `pa16/plan.md`. The final checkpoint diff contains only
those three files. No tests, fixtures, reference outputs, sidecars, harnesses,
comparators, coverage rules, or source-set lists changed.

The turn-start authority is the supplied
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`208/243` passed, exactly `35` failed, and `243/243` identities were covered.
The preserved parent baseline was `206/243` with `37` failures and full
coverage. Its exact two baseline-only identities are
`pa16/tests/general/200-destructor-body-local-before-base-destruction.t` and
`pa16/tests/general/200-member-object-lifetime.t`; the current-only set is
empty. Fresh post-repair `make test-pa16` reproduced `208/243`, exit `2`,
exactly `35` failures, and `243/243` coverage at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-test-20260830.log`.
The complete sorted comparison is recorded at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-identity-delta-20260830.log`:
fresh-only and authority-only are both empty, and the inventory contains all
`243` PA16 identities.

### Contract and ownership trace

The affected typed path is:

```text
PA12 FunctionFact.destructor_action_begin/count
  -> contiguous DestructorActionFact range
  -> PA15 checked_destructor_function and demand walk
  -> active_destructor_record_ / active_destructor_this_
  -> typed member/base root address
  -> reverse array-path replay and destructor call
  -> normal suffix or body-unwind LowIR EH path
```

PA12 is the semantic owner. `build_destructor_actions` copies the class
binding list before child demand can grow it, appends non-static members in
reverse declaration order, then the direct base, and publishes one contiguous
`(begin, count)` range. Each action retains its typed target, object type,
selected destructor, and base/member owner. No spelling, source scan, or later
re-resolution participates in this handoff.

PA15's demand walk consumes that range once per reachable destructor fact and
demands the typed child destructor functions. `lower_function` validates the
function scope and hidden object parameter, stores `this`, and installs
`eh_cleanup` only for a nonempty action range.
`active_destructor_record_` and `active_destructor_this_` are the only
active-object state used by replay. `destructor_subobject_address` reloads the
typed `this` slot and projects the direct base at the PA16-required zero
offset or the direct member through the complete `RecordLayout` offset.

`collect_destructor_elements` strips only typed cv/expression wrappers, checks
fixed array bounds, walks arrays from last element to first, and records a
`DestructedElement` containing the original action, typed array-index path,
and terminal record. At each leaf, `checked_destructor_function` validates the
function fact, sidecar owner, function scope, hidden parameter, and action
range. `recompute_destructor_element_address` rebuilds the root and every
array projection in each emitting block; no SSA address or temporary is
borrowed across a cleanup edge.

The normal sequence emits member/base order and, for every potentially
throwing terminal except the last, an explicit cleanup block for the remaining
prefix. The body handler's cleanup block replays the complete sequence,
closes with `eh_end`, and `resume`s. The return path destroys active local
lifetimes while the destructor-body handler is still installed, emits
`eh_end`, then emits the normal typed suffix and the return. On ordinary
compound fallthrough, compound lowering pops its scope-owned
`active_lifetimes_` before `lower_function` emits the suffix. This establishes
local-before-member/base order for the two repaired identities.

The PA13 EH contract is respected: normal paths pop handlers with `eh_end`,
cleanup paths terminate with `eh_end` and `resume`, and generated blocks are
typed and terminated. A bounded uncertainty remains in inherited local
lifetime lowering: if a local lifetime destructor itself throws, full
path-sensitive cleanup of still-live earlier locals would require a broader
lifetime-owner change than this suffix checkpoint. The audit therefore did
not widen that design.

### Audit findings and repair

The landed suffix collector rejected malformed enum/identity fields only
indirectly and did not prove that `action.object_type` was the exact type
owned by its selected member or direct base. A malformed action could
therefore flatten a different array shape before the later address routine
rejected or misinterpreted it. The repair now requires a valid `Base`/`Member`
target, the active direct-base or direct-member owner, exact member/base
`TypeId` equality, a non-union terminal record, and the canonical
`model_.destructor_binding(record)`. `checked_destructor_function` applies the
same canonical destructor-binding rule to its consumers.

The landed `emit_destructor_call` also set the LowIR call result to `void`
without rejecting a malformed non-void destructor binding. It now requires a
non-variadic, parameterless function whose typed result is `void`. These are
fail-closed typed checks only: they add no demand, cache, scan, alternate
owner, or valid-input output. The source repair remains in the declared PA15
construction/lowering path.

### Bounds, evidence, and boundaries

For a consumed action range with `N` flattened terminals and maximum
array-path depth `D`, collection is `O(ND)` and normal cleanup-prefix emission
is intentionally `O(N^2D)`: the public LowIR EH ABI exposes no shared
remaining-suffix cursor, so every possible throwing terminal needs an
independently materialized typed prefix. Body-unwind replay is `O(ND)`. With
fixed type-path depth this is the observed triangular `O(N^2)` output bound;
the `D` factor accounts for required address projections. The repair adds
constant-time indexed/sidecar checks per action and does not change these
bounds. There is no whole-program retry, textual fallback, or unbounded
cache/shortcut.

The inherited landed structural evidence at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-structure-20260830.log`
records `N=1/3/8` as respectively `3/7/17` blocks, `1/3/8` cleanup handlers,
`1/3/8` resumes, `2/9/44` destructor calls, and `13/44/174` instructions.
The Holder array replay has seven blocks, three suffix handlers, three
resumes, and nine calls; the Derived and YB probes show the body cleanup plus
normal suffix shape. These counts corroborate the semantic prefix copies; no
timing, RSS, allocation, or speedup claim is made.

Earlier focused post-repair evidence is `make -C dev cppgm++ CXX=g++` (exit
`0`) followed by the seven-test local matrix covering both baseline-only repairs, three
explicit/pseudo-destructor controls, and the two known array-presentation
cases. It produced `5/7` and exit `2`: the five non-array controls passed,
while only `200-local-default-class-array-lifecycle.t` and
`300-synthesized-array-member-lifecycle.t` retained their known LowIR-shape
mismatches. The inherited landed checkpoint's durable 9-test result is `7/9`
at `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-focused-20260830.log`;
the fresh broad stage, through-PA15, and file-audit results are recorded below.

The two known array mismatches are presentation/constructor-shape residuals,
not new destructor suffix failures. The source ownership review covers the
four landed lowering files, while the final changed-file audit covers only
the one repaired source file and the two PA16 records. Fresh through-PA15,
file-audit, diff-check, and changed-file evidence are recorded in the final
validation section and durable logs below. The next checkpoint is a separate
audit of one unchanged residual family; it must preserve the typed action
range, active owner/`this`, explicit EH structure, and `243/243` coverage
authority.

### Historical Validation and Inherited Evidence

Inherited landed evidence is kept separate from the fresh post-repair run:
the landed full-stage log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-test-rerun-20260830.log`,
its prior identity delta is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-identity-delta-rerun-20260830.log`,
its through-PA15 log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-through-pa15-rerun-20260830.log`,
the inherited file-audit log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-file-audit-rerun-20260830.log`,
the inherited diff-check log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-diff-check-final-20260830.log`,
and the inherited changed-file log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-changed-file-audit-20260830.log`.
Those logs describe the landed source before this bounded repair.

Fresh post-repair evidence is:

- `make test-pa16`: exit `2`, `208/243` passed, `35` failures, `243/243`
  covered; log
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-test-20260830.log`.
- `n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`:
  exit `0`, `1167/1167`; log
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-through-pa15-20260830.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: exit `0`,
  five known header-division warnings and no fatals; log
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-file-audit-20260830.log`.
- The exact sorted failure/coverage comparison is at
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-identity-delta-20260830.log`:
  `35 -> 35`, fresh-only `0`, authority-only `0`, unrecognized `0`, and
  `243/243` coverage.
- `git diff --check` and the bounded exact changed-file audit both exit `0`
  in their fresh post-repair logs
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-diff-check-20260830.log`
  and
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-audit-changed-file-audit-20260830.log`.

The final checkpoint result is complete. No source, test, fixture, reference,
sidecar, harness, comparator, coverage-rule, source-set, or generated oracle
file changed outside the three-file diff.

## Historical Typed Local-Class Materialization Review (d83e927f)

This bounded review covers landed source checkpoint
`d83e927fd18429d37c3818a80e295f0a7c521905` (`PA16: materialize typed local
class defaults`) relative to `d95a6fe7`, plus one narrowly scoped ownership
repair found during the audit. The landed increment touched
`pa15_lowering_construction.cpp`, `pa15_lowering_flow.cpp`, and
`pa15_lowering_calls.cpp`; the repair adds one typed automatic-local predicate
in `pa15_lowering.cpp`/`.h` and uses it at those three consumers. The only
PA16 record changes in this checkpoint are this file and `pa16/plan.md`.

The supplied turn-start authority in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` is
`206/243` identities passed, `37` failed, and `243/243` covered. The d95
checkpoint authority was `202/243` with `41` failures and full coverage; the
exact identity delta is the four selected baseline-only identities below and
no final-only identity. Fresh final validation preserves that authority. The
fresh commands and results are `make test-pa16` (exit `2`, `206/243`, exactly
`37` failures, `243/243` covered), the exact `n=16` prior-stage gate (exit
`0`, `1167/1167`), and `perl scripts/cppgm_file_audit.pl --stage pa16
--paths dev/src` (exit `0`, five known header-division warnings in
`abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
`pa11_semantic_model.h`, and `pa15_lowering.h`, with no fatals). Durable logs
are:

- `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-d83-checkpoint-audit-final-test-20260830.log`
- `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-d83-checkpoint-audit-final-through-pa15-20260830.log`
- `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-d83-checkpoint-audit-final-file-audit-20260830.log`

The selected residuals are exactly:

- `pa16/tests/general/300-operator-shift-stress-chain.t`
- `pa16/tests/general/300-mixed-member-free-shift-stress-chain.t`
- `pa16/tests/general/300-compound-assignment-adl-nonmember-after-member-reject.t`
- `pa16/tests/general/300-member-vs-nonmember-operator-implicit-object-cv-rank.t`

Their typed selected calls already matched; the d83 change supplies the
declaration-owned address for `Token token`, `boost::Cstring s`, and
`Period period`, leaving the remaining 37 residual identities unchanged.

### Contract and ownership trace

The affected path is the typed PA11-to-PA12-to-PA15 declaration and call
path required by `spec.md` sections 2, 3, 4, 5, and 7:

```text
PA11 DeclarationFact/BindingId
  -> PA12 semantic_variable_initializer
  -> selected typed ConstructorAction
  -> PA15 canonical variable/declaration indexes
  -> condition or Variable consumer
  -> storage_for + address_of_storage
  -> ordered LowIR action/lifetime/use
```

PA11 publishes `DeclarationFact::automatic_storage` from the typed scope and
storage-class facts. PA12's `semantic_variable_initializer` recognizes the
named class object and publishes one `ConstructorAction` with its selected
constructor binding, selected scope, callable type, and `value_initialize`
state. `semantic_constructor_action` builds the typed hidden destination and
selected call facts; it does not recover source text or re-resolve spelling.

Before lowering, `index_binding_facts` validates the declaration ranges and
builds the canonical `variable_facts_` and keyed `declaration_by_binding_`
indexes. `lower_variable_expression` consumes the former for a condition
declaration, and the `Variable` statement path consumes it for an ordinary
declaration. For a no-op, non-value-initializing class action, both now use
`storage_for` and call `address_of_storage` once, but only when the keyed
`DeclarationFact` proves `automatic_storage` and block scope. The predicate is
centralized; it is a bounded map lookup, not a cache or a text-derived test.

The audit found a real edge defect in the landed d83 branches: their original
object/type checks did not exclude a block-scope `static` declaration. The
repair adds the typed owner predicate to both consumers and reuses it in the
narrow class-value overlap check. A direct non-checking probe of
`static Empty e;` confirms that the repaired path no longer emits d83's extra
`addr $e`; the reference's separate local-static guard/global-storage model
is outside this checkpoint. Namespace-scope and class-static objects remain
on `collect_globals`/namespace-lifetime paths and cannot satisfy the
automatic-local predicate.

The class-value ABI overlap path remains narrow and typed. It requires one
`ClassValue` conversion and one argument, then checks an `IdExpression`'s
canonical binding, its exactly-one-child variable fact, the no-op
`ConstructorAction`, and the keyed automatic block declaration. Only the
redundant pre-copy address is suppressed. The source expression is lowered
once, and the later `address_of_storage(class_value_source)` remains before
the temporary copy argument. Namespace, static-local, and other nonautomatic
owners therefore retain the pre-copy address. No source scan, retry, text
reconstruction, or alternate semantic owner was introduced.

### Findings and preservation checks

- `value_initialize` still calls the existing value-initialization path;
  non-no-op actions still lower their typed action; braced initializers and
  scalar initializers still use their existing consumers.
- `activate_lifetime` remains separate from declaration address materialization.
  No eager constructor/destructor helper demand was added, and the no-op path
  does not fabricate a lifetime fact.
- Declaration order is preserved: `run` indexes typed facts before function
  collection/lowering, each declaration consumer emits its address at the
  declaration point, and the existing action/lifetime ordering is unchanged.
  The class-value source is not evaluated a second time.
- `address_of_storage` is the sole address producer for the affected storage
  path; it emits one `IK_ADDR` for a slot/global and has no address cache.
  The new predicate adds no whole-program or per-operator scan.

No further implementation defect was found in this bounded ownership path.
The prior typed bit-field checkpoint and the other 37 residual identities are
out of scope.

### Focused evidence and structural bounds

`make cppgm++ CXX=g++` in `dev/` rebuilt the changed lowering objects and
linked successfully. The documented PA16 wrapper was then run separately for
each of the four selected identities and nine nearby controls, with
`CPPGM_APP_ARGS='--emit-lowir -O0'` and isolated suffix `audit`; the matching
`compare_results.pl ref audit` runs passed `13/13`. The controls include
unary-address-of, overloaded comma, three local constructor calls, value
initialization, pointer-member initialization, and namespace/class-static
initialization.

The generated LowIR structural counts for the four selected tests are:

| identity | main instructions | named declaration/source addresses |
| --- | ---: | --- |
| operator-shift stress | 76 | `Token 25`, `Stream 5` |
| mixed member/free shift stress | 412 | `Token 129`, `Stream 5` |
| compound ADL assignment | 19 | `Cstring 2`, `Holder 3` |
| member-vs-nonmember cv rank | 14 | `Period 2`, `Date 2` |

The counts show one declaration-time address plus the addresses required by
the source operations; the two stress chains retain linear growth in their
operator links. The ownership checks are two `std::map` lookups in the
class-value overlap path, hence O(log V) each, with bounded typed checks after
them and no new scan. These are structural observations only; no timing,
RSS, allocation, or speedup claim is made. `git diff --check` is the final
focused repository check for this milestone.

A valid out-of-class narrow class-value constructor probe was accepted by both
the current compiler and its reference observer (exit `0` in each); its
automatic local `e` has one declaration address and one later source address
in `main`. A separate namespace-static probe retains both source addresses in
the current output, as required by the nonautomatic guard. These probes are
observational only and are not added fixtures.

### Disposition and next checkpoint

The bounded source repair is justified and limited to the affected PA15
ownership path. No test, fixture, `.ref`, exit sidecar, harness, comparator,
coverage rule, source set, or unrelated surface changed. Fresh broad
validation, exact identity comparison, file audit, changed-file audit, and
diff-check are complete; the durable identity, changed-file, and diff-check
records are
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-d83-checkpoint-audit-final-identity-comparison-20260830.log`,
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-d83-checkpoint-audit-final-changed-file-audit-20260830.log`, and
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-d83-checkpoint-audit-final-diff-check-20260830.log`. The final
disposition is the single PA16 audit/repair commit at current HEAD; the
handoff hash is reported separately.

The next checkpoint must choose one of the unchanged 37 residual identities
separately. It must not reopen this automatic-local materialization boundary
or the prior bit-field path without new evidence; preserve typed
`BindingId`/`DeclarationFact` ownership, one-evaluation ordering, and the
`243/243` coverage authority.

## Historical Typed Packed Bit-field Review (7e060b28)

This final bounded review covers landed source checkpoint
`7e060b28e76e551cbce68a3254b87fe8f9f72eaa` (`PA16: lower typed packed
bit-field operations`) relative to `1694bc3eb9e3fd9abb6bfe566e8183acda0bb7b2`.
Its implementation scope is exactly the five PA15 lowering owners listed
below. The supplied current-stage authority in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` is
`202/243` identities passed, `41` failed, and `243/243` covered. The fresh
required `make test-pa16` reproduces `202/243`, `41` failures, and `243/243`
coverage; its durable output is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-final-test-20260829.log`.

The exact sorted comparison of that supplied authority with the fresh final
failure set has `41` versus `41` failures, `baseline-only 0`, `final-only 0`,
and `243/243` coverage on both sides. It is preserved at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-final-identity-comparison-20260829.log`.
The landed source evidence for the earlier `43 -> 41` checkpoint delta remains
at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-compact-worker-identity-delta-final.txt`.
No implementation repair was found necessary in this bounded ownership path;
this audit changes only the two PA16 records. No test, fixture, reference,
exit sidecar, harness, comparator, source-set, or unrelated stage surface is
in scope.

### Contract and ownership trace

The bounded path is the single typed PA11-to-PA12-to-PA15 path required by
`spec.md` sections 2-5 and 7:

```text
PA11/PA12 process_bit_field_declaration
  -> canonical BindingId + one BitFieldFact
  -> record layout fills storage offset/bit offset/masks by BindingId
  -> PA12 lvalue ExprInfo/SemanticFact retains selected BindingId + operation type
  -> PA15 lower_member_address / constructor path
  -> LoweredValue { bit-field BindingId, ProjectionId }
  -> contiguous projection arena descriptor
  -> load / encode / packed-unit RMW-store
```

`process_bit_field_declaration` creates the fact with declared, physical
storage, promoted operation, signedness, width, and mask data, publishes named
facts through the canonical `FlatIndex<BindingId, BitFieldFact>`, and records
the declaration event in the owning record. `record_layout` updates that same
BindingId-keyed fact with the computed storage-unit and bit offsets. The
`RecordMemberDeclaration::bit` value is layout-event metadata, not a second
semantic fact model; PA15 never looks it up by spelling or scans it.

PA12's `bit_field_fact_for_expression` accepts only a typed lvalue semantic
fact, checks the selected BindingId against the canonical fact, and passes its
`operation_type` through `conversion_for`. Address-of, non-const-reference,
bool-decrement, and `sizeof` restrictions remain semantic checks. Ordinary
implicit member uses are published as typed member expressions, so the PA15
member path receives the same selected binding as direct dot and arrow access.

`lower_member_address` selects the one class owner and validated layout offset.
Direct non-reference objects capture a storage-address root; arrow and other
evaluated roots capture the already-produced pointer value; constructor paths
capture the typed `this` slot for reload. `emit_bit_field_index` is the only
production projection-arena append site. `mark_bit_field_address` carries only
the compact typed handle in `LoweredValue`; `reproject_bit_field_address`
replays the saved root/index without reevaluating source code.

### Findings and repair disposition

- The ownership trace found no duplicate bit-field fact, source spelling
  reconstruction, projection scan/retry, or hot-record descriptor bloat. The
  arena is cleared per lowering run and handles are range-checked.
- `emit_bit_field_load` performs one storage-unit load, typed shift/mask, and
  signed narrow-field extension. `encode_bit_field_value` performs the typed
  mask/shift, while the shared stores clear only the field mask and OR the
  encoded value when preserving an existing packed unit.
- Compound assignment reloads the current unit after RHS evaluation and
  reprojects the final destination. Prefix/postfix compute the update before
  their shared RMW; prefix uses a fresh projection and postfix keeps its
  evaluated destination. Constructor scalar actions evaluate the initializer
  before destination capture, and aggregate bit-field initializers evaluate
  once and reuse the typed value.
- No source edit was justified. The known focused residuals are presentation
  or oracle differences: prefix/postfix promoted-type shape, signed integral
  and enum reads whose emitted sign extension is semantically required, the
  aggregate signed read with the same sign-extension difference, and the
  pre-existing packed-layout residual outside this lowering increment.

### Focused and required evidence

Fresh bounded focused evidence is recorded at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-preauth-focused-20260829.log`.
The focused six-test cluster exits `2` with `2/6` passing: constructor member
initialization and direct member access pass; prefix/postfix, aggregate, signed
integral, and signed-enum read fixtures retain their known comparison residuals.
The explicitly named exploratory 11-test matrix in that log exits `2` with
`6/11` passing; its additional `300-packed-class-layout` failure is the known
unrelated layout residual. The PA16 course control
`sh cppgm.tests/course/pa16/412-typed-bit-field-initialization-root-regression.sh`
exits `0`.

The fresh required through-PA15 gate exits `0` at `1167/1167`; its durable
output is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-final-through-pa15-20260829.log`.
The required file audit exits `0` with the five known header-division warnings;
its output is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-final-file-audit-20260829.log`.
The final diff check exits `0`; its durable output is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-final-diff-check-20260829.log`.

The signed-enum existing fixture's semantic/translation path produces the
expected sign extension, but its downstream CY86 runner reports
`invalid CY86 operand`; this is a scaffold limitation for that probe only, not
a PA16 contract failure or an implementation repair target.

### Structural, performance, boundaries, and changed-file audit

The source audit finds one production
`bit_field_address_projections_.push_back` in `emit_bit_field_index`; ordinary
`emit_index`, decay, and storage-address paths do not append projection data.
The earlier append-site, source-size, and type-size probe output is retained at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-preauth-structure-20260829.log`; the fresh final structural/performance record is at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-final-performance-20260829.log`.
The structural size probe reports `sizeof(LoweredValue)=336`,
`sizeof(BitFieldAddressProjectionId)=8`, and
`sizeof(BitFieldAddressProjection)=352`; the descriptor is arena-only. The
earlier structural log contains only the append-site, source-size, and
type-size observations; it does not directly record constructor or
evaluated-root execution traces. Those action-order findings are based on the
source trace and the focused/course evidence. The fresh final record directly
measures `initialize_aggregate_value` from signature line 823 through its
closing brace at line 1068, with an opening-brace span of exactly 240 lines.
These are structural observations only: no timing, RSS, allocation, or speedup
claim is made. `pa15_lowering.cpp` is 2991 lines.

The authorized final validation completed the broad PA16/through-stage checks,
exact identity comparison, file audit, and diff-check. The next checkpoint must
select a future residual separately; it must not reopen this typed projection
path absent new evidence, and it must preserve the 41-identity authority plus
the canonical BindingId/BitFieldFact and typed owner invariants.

The landed implementation files are exactly:

- `dev/src/pa15_lowering.cpp`
- `dev/src/pa15_lowering.h`
- `dev/src/pa15_lowering_aggregate.cpp`
- `dev/src/pa15_lowering_construction.cpp`
- `dev/src/pa15_lowering_member.cpp`

The final audit record changes are only `pa16/audit.md` and `pa16/plan.md`;
no implementation source, test, fixture, `.ref`, status sidecar, harness,
comparator, coverage rule, source-set, or unrelated file changed. The landed
source authority remains `7e060b28`; the audit commit is the current HEAD after
this final validation, with its handoff hash recorded in the final report.

## Inherited Predecessor Checkpoint Review

This review covers landed c39d45634bb029a02c938c190f8ac703bd275050,
PA16: preserve typed canonical truth boundaries, plus the bounded
checkpoint-audit correction and its behavior-preserving structural extraction.
The clean turn-start authority was c39d4563:
199/243 PA16 identities passed, exactly 44 failed, and all 243/243
identities were covered. The final result is compared against that exact
authority below; this review does not treat the older e92 194/49 result as
current.

The audit follows pa16/README.md, the relevant spec.md sections 2-5 and 7,
pa16/plan.md, and the preserved PA11-PA15 typed contracts. The only tracked
changes in this checkpoint are these six source owners and the two checkpoint
documents:

- dev/src/pa11_semantic_model.h
- dev/src/pa12_semantic.cpp
- dev/src/pa12_semantic_construction.cpp
- dev/src/pa12_semantic_facts.cpp
- dev/src/pa12_semantic_resolution.cpp
- dev/src/pa15_lowering.cpp
- pa16/audit.md
- pa16/plan.md

No test, fixture, .ref, status sidecar, harness, comparator, source-set, or
unrelated stage surface changed.

### Contract and ownership trace

The canonical-truth path is one typed PA12-to-PA15 pipeline:

```text
PA12 retained SemanticFact / ConversionFact / BindingId / FunctionFact
  -> one post-construction finalizer with local semantic, binding-summary,
     and defined-function-summary ordinal ranges
  -> explicit result edges and one monotonic dense worklist
  -> publish contains_member_value, direct_bool_boundary, and conversion policy
  -> PA15 resets LoweredValue policy from each current ConversionFact
  -> typed LowIR chooses Preserve or Materialize for that conversion
```

PA12 first completes ordinary retained construction in deterministic source
order. set_semantic_children only publishes the child range; it neither
demands a body nor publishes provenance. analyze_pa12 then invokes exactly
one finalize_canonical_truth pass after all retained function bodies exist.
Direct PA12 member/object-derived facts and the justified implicit-this result
facts are the only initial semantic seeds. The finalizer records a typed
(SemanticFactId, FunctionFactId) return-owner pair for each retained return
statement and maps selected call bindings to the canonical defined
FunctionFact. A declaration-only or
external call has no definition edge and therefore fails closed to the normal
Materialize policy; a malformed retained return owner raises a runtime_error.

The local graph is ephemeral. Its dense local ResultNodeId ranges cover
semantic facts, BindingId conservative may-summaries, and defined FunctionFact
summaries; these IDs do not persist after publication. Its append-only edge
records use dense source heads and next links. Result edges are explicit:
variable and return
initializer, unary except address-of, postfix/cast, non-comma binary operands,
comma RHS only, assignment result operands, conditional result arms, and
subscript object. Calls use only the selected function summary. Call
arguments and indirect callees, comma-left, conditions, constructor
arguments, member-object/control/statement edges, and other non-value edges
do not propagate. Variable/assignment definitions point to a BindingId
summary, and every typed variable IdExpression use reads that summary.
ReturnStatement results point to their owning defined-function summary; that
summary points to calls. The single queue reaches a fixed point through
recursion without demand retries, body rescans, or whole-program retry.

The BindingId summary is intentionally conservative may-provenance. Every
committed possible variable/assignment definition contributes, so branches,
loops, and reassignment remain safe over-approximations; reassignment is not
claimed to clear earlier possible provenance. Shadowing uses distinct typed
BindingIds. No BindingSidecar provenance, latest-fact scan, deferred ambient
call owner, dynamic relation key, or second production model remains.
Because finalization runs after speculative tails are gone, discarded
type-only facts never enter this graph and no graph/head/summary rollback is
needed.

Each true node is queued at most once, each generated edge is linked once,
and each reachable edge record is visited once. Function summaries converge
through the same graph: the adversarial A-to-B-to-A cycle reaches A's later
direct member-derived return seed without source-order retry. PA12 publishes
Preserve on every owned conversion whose semantic source is bool, including
bool-to-int/zext canonical-truth conversions. PA15 copies the current
conversion policy into LoweredValue before applying that conversion, so a
Preserve record cannot stick to a later Materialize record on the same fact.

### Findings and disposition

- Generic child taint was removed. Only the typed result-edge switch and
  selected function-summary edge can propagate result provenance; plain unary
  results, call arguments, comma-left, conditions, constructor arguments,
  and unrelated body members are excluded.
- Function provenance is source-order independent at ownership time. All
  retained bodies are built first, declaration-to-definition mapping is
  indexed once by BindingId, and only retained ReturnStatement results feed
  the definition summary. No body is demand-analyzed by a setter or call.
- BindingSidecar mutation and deferred ambient-function recovery were
  removed. The conservative may-summary is dense BindingId-keyed local graph
  state, published only after construction, with no speculative update.
- CanonicalTruthPolicy remains conversion-owned. Preserve is restored for
  bool semantic sources even when the target is int; PA15 resets from each
  ConversionFact. The two-conversion probe recorded Preserve followed by
  Materialize and the typed output retained the required per-conversion
  shapes.
- Ownership is direct and typed: a direct member/object result or implicit
  this can seed; an unrelated member read in a body cannot. Calls inherit
  only their selected defined function result, and aggregate/member
  comparisons and procedural comparisons retain their separate boundaries.
- Finalizer hardening checks size_t domain addition before allocation, rejects
  out-of-range edge endpoints, and rejects malformed or non-definition-owned
  retained return owners. Declaration-only/external calls remain the sole
  intentional no-definition case.
- The ephemeral graph now lives in a private CanonicalTruthFinalizer with
  separate domain/edge construction, propagation, and publication methods;
  finalize_canonical_truth is a small orchestration method. The refactor
  restores one declaration or statement per changed source line without
  changing the typed owners, edge rules, or observable LowIR.

### Focused evidence

The clean rebuild was `make clean` followed by `make -j2`; both exited 0. The protected
command
`make -C pa16 check TEST='tests/general/100-global-aggregate-nested-array-initializer.t tests/general/200-defaulted-constructor-still-aggregate.t tests/general/200-deleted-constructor-still-aggregate.t tests/general/200-qualified-friend-function-member-access.t tests/general/300-unary-address-of-builtin-fallback.t'`
exited 0 with `pa16 check: PASS (5/5)`.

These ephemeral probes were rerun against the clean binary and each exited
0: call-after, call-forward, call-before, cycle-observable, result-edges,
call-argument, binding-observable, type-only, conversion-sequence,
two-conversion, nested-calls, and nested-calls-reversed. The after/forward
LowIR files compare byte-for-byte (`cmp` 0), proving declaration-to-definition
and call-before-definition equivalence. The reversed nested pair has the
expected source-order function presentation difference only; both contain
the same nested call/result path and exit 0.

The cycle probe emits the A/B calls and a typed main comparison with zext.
Result-edge LowIR shows member_cmp using direct zext while plain comparison,
call-argument, and comma-left results retain trunc; a member-only condition
does not taint its literal result arms. The binding probe shows branch and
reassigned uses preserve the conservative may result, while the outer
shadowed binding remains procedural (trunc) and the inner shadowed binding
uses the member-derived shape (zext).

The fresh outputs for all twelve probes compare byte-for-byte with the
pre-refactor outputs; call-after and call-forward also compare byte-for-byte
with each other. The type-only probe emits exactly one base-subobject projection and one
Base::get call for the retained expression, with no builtin call; the
discarded operand is absent from the final output. The conversion-sequence
probe's typed LowIR shows the member-derived ref_int path as cmp plus zext
and a plain procedural comparison as cmp plus trunc then zext. Temporary
policy tracing on the same fact recorded `[14->28:Preserve]` followed by
`[28->29:Materialize]`; the instrumentation and all counters were removed
before this clean build. No DBG/CONV output or temporary iostream include is
present in the changed files.

### Broad baseline, exact delta, and residual map

The preserved turn-start log is
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log.
`make test-pa16` exited 2 only because the expected residual comparisons
remain, and reported 199/243 passed, 44 failed, and 243/243 identities
covered. Exact sorted ERROR identity comparison against that log gives
baseline-only 0, final-only 0, missing coverage 0, and unexpected coverage 0.
The final failure count is 44 (not above the authorized 44), so no extra pass
offsets a new failure. The preserved final log is
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-structural-refactor-test-pa16-20260829.log.
The exact set comparison is preserved at
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-structural-refactor-identity-compare-20260829.log.
The exact final residual set is:

- pa16/tests/general/100-function-pointer-nested-param-name-shadow.t
- pa16/tests/general/200-aliased-base-mem-initializer-match.t
- pa16/tests/general/200-const-subobject-member-call.t
- pa16/tests/general/200-destructor-body-local-before-base-destruction.t
- pa16/tests/general/200-elaborated-member-forward-type.t
- pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t
- pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t
- pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t
- pa16/tests/general/200-local-default-class-array-lifecycle.t
- pa16/tests/general/200-member-object-lifetime.t
- pa16/tests/general/200-mutable-member-const-method.t
- pa16/tests/general/200-nested-braced-member-aggregate-init.t
- pa16/tests/general/200-nonliteral-field-condition-not-folded.t
- pa16/tests/general/200-placement-new-expression-aggregate-brace.t
- pa16/tests/general/200-placement-new-expression-constructor-call.t
- pa16/tests/general/200-reference-indexed-pointer-member-access.t
- pa16/tests/general/200-reference-member-class-init.t
- pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t
- pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t
- pa16/tests/general/300-adl-associated-namespace-does-not-climb-parents.t
- pa16/tests/general/300-adl-using-declaration-source-point.t
- pa16/tests/general/300-callable-field-hides-private-base-method.t
- pa16/tests/general/300-compound-assignment-adl-nonmember-after-member-reject.t
- pa16/tests/general/300-enum-class-nonmember-operator-bitand.t
- pa16/tests/general/300-friend-function-definition-skip.t
- pa16/tests/general/300-member-vs-nonmember-operator-implicit-object-cv-rank.t
- pa16/tests/general/300-mixed-member-free-shift-stress-chain.t
- pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t
- pa16/tests/general/300-operator-nullptr-t-from-zero.t
- pa16/tests/general/300-operator-shift-stress-chain.t
- pa16/tests/general/300-overloaded-deref-user-assignment.t
- pa16/tests/general/300-packed-class-layout.t
- pa16/tests/general/300-pragma-pack-followed-by-endif.t
- pa16/tests/general/300-prvalue-derived-base-friend-operator.t
- pa16/tests/general/300-synthesized-array-member-lifecycle.t
- pa16/tests/general/300-user-defined-string-literal-operator.t
- pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t
- pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t
- pa16/tests/general/400-bit-field-constructor-member-init.t
- pa16/tests/general/400-bit-field-member-access-bad.t
- pa16/tests/general/400-bit-field-prefix-postfix-increment.t
- pa16/tests/general/400-bitfield-aggregate-init.t
- pa16/tests/general/400-signed-bit-field-read.t
- pa16/tests/general/400-signed-enum-bit-field-read.t

### Structural bounds and next checkpoint

Representative structural samples from the finalizer are:

- conversion-sequence: 71 semantic nodes, 21 BindingId summary nodes, 6
  defined-function summary nodes, 58 append-only edge records, 43 reachable
  edge visits, 44 queue pushes, and 44 true nodes;
- cycle-observable: 30 semantic nodes, 7 BindingId summary nodes, 3
  defined-function summary nodes, 22 append-only edge records, 17 reachable
  edge visits, 17 queue pushes, and 17 true nodes.

These are structural samples, not timing/RSS/speedup claims. The extracted
finalizer methods remain below the 240-line function limit; the file audit
also passes with the header at its existing 2400-line limit and no changed
source line needlessly packs multiple statements. The graph build
is linear in retained nodes plus generated result edges; the worklist is
deduplicated by dense queued/true state and visits each reachable linked edge
once. The local vectors are discarded after owner publication. PA15 consumes
the published conversion policy and typed fact fields without a second
provenance model.

The exact prior-PA15 command was run as requested and exited 0 with
`1167 / 1167`:
`n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`.
The required file audit exited 0 with five known warnings (the existing
header bad-division warnings for abi_mangle.h, cpp_semantic_core.h,
lowir_model.h, pa11_semantic_model.h, and pa15_lowering.h). Its log is
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-structural-refactor-file-audit-final-20260829.log.
`git diff --check` exits 0 after the document edits.

PA16 remains incomplete only by the unchanged 44-identity residual map. The
next checkpoint should select one of those residuals; it must preserve this
single finalizer, conservative may-provenance policy, typed function mapping,
and PA15 per-conversion reset.
## Historical Prior Namespace-Object Review

This review covers landed commit `b3bbf052cc218ab5a66f42b785f1606f7c5e7040`
(`PA16: fix typed non-owning namespace objects`) relative to its clean parent
`68b549f2`, plus the bounded demand-root repair completed in this checkpoint.
The landed source increment changed `dev/src/pa15_lowering_flow.cpp` and
`dev/src/pa15_lowering_globals.cpp`; the repair is confined to
`dev/src/pa15_lowering_flow.cpp`. The required checkpoint documentation is
also updated here and in `pa16/plan.md`. No handout test, fixture, `.ref` or
exit-status sidecar, harness, comparator, coverage rule, or source-set file
changed.

### Contract and ownership

The path was checked against `pa16/README.md`, the relevant `spec.md`
requirements on one typed pipeline, canonical identity/demand, typed LowIR,
and bounded work, plus the preserved PA11--PA15 interfaces. The representative
ownership trace is:

```text
PA11 BindingId + binding owner ScopeId + declared TypeId + definition/linkage/storage facts
  -> PA12 typed initializer/reference-binding conversions and constant-address facts
  -> PA15 FunctionPlan/global-initializer roots, typed demand indexing, symbol collection,
     and reference/glvalue LowIR
```

PA11 remains the source of the canonical binding, its owner, declared type,
definition bit, and storage/linkage distinctions. PA12 retains the binding
and declared type in `IdExpression`/`MemberExpression` facts, publishes
`ReferenceBinding`/`DerivedToBase` conversions, and records typed constant
address targets. PA15 runs demand indexing after function collection and
before global collection. `low_type` remains the owned-storage boundary;
`low_reference_value_type` queries the PA12 expression object type and returns
a pointer for a named incomplete class, while ordinary class values still
reach the complete-layout check.

For the incomplete-referent case, PA12's reference initializer carries the
address of `*forward_declared_object`; PA15's reference/glvalue lowering uses
the pointer representation and stores that address in the reference object.
It does not load or allocate an incomplete class value. A direct incomplete
class value probe still fails in PA12, and an owned namespace object cannot
reach storage without a complete class layout.

The landed globals change removes the old class-static-only declaration
exception: a declaration-only namespace variable is emitted only when its
canonical binding is in `required_global_bindings_`, while a definition keeps
its existing definition path. Class-static behavior, used scalar/class
externs, address targets, nested namespaces, unnamed-namespace/internal
linkage, `thread_local`, and wrapper metadata remain in the existing typed
global symbol path. The bounded repair tightens the demand helper so a
variable is eligible only with a valid owner in the namespace scope or, for a
static member, the class scope; malformed owner/range data fails closed.

### Findings and bounded repair

- The landed incomplete-class fix is semantically in the correct PA15
  boundary: only reference/glvalue representation is relaxed, and later
  value materialization still calls the complete-layout-owned `low_type` path.
- The landed namespace-demand fix correctly changes declaration-only
  collection to `!has_definition && !required_global_bindings_[id]`, while
  definitions remain roots and class-static collection remains eligible.
  Its demand marking, however, visited every semantic fact in the arena. An
  unused emitted-free member body and an unused default-argument fact could
  therefore make an otherwise irrelevant namespace extern appear as a global
  declaration.
- The repair retains the existing complete-arena range and semantic-DAG
  validation, then marks demand from facts PA15 will actually consume:
  emitted `FunctionPlan` bodies and constructor actions, namespace/static
  global initializer roots, and their typed child, aggregate, conversion,
  and constant-address edges. A separate deduplicated address worklist walks
  unary-address operands through cast facts so address demand is preserved
  even when the target has a constant value. Dense `SemanticFactId` seen
  vectors prevent repeated traversal.
- The repaired path has no source-spelling recovery, textual downgrade,
  second semantic model, full-TU retry, repeated broad demand scan, invalid
  fallback, or test-specific shortcut. It derives all decisions from
  canonical typed facts and uses deterministic `function_plans_` order and
  the ordered `variable_facts_` map for roots.

### Focused evidence and disposition

The clean-parent baseline is `187/243` passing, `56` failures, and
`243/243` identities covered. The landed-head authority and the post-repair
result are both `189/243` passing, `54` failures, and `243/243` covered. The
exact comparison against the landed `final-failures.txt` has baseline-only
`0` and final-only `0`; the two identities removed from the 56-failure parent
map are
`100-global-reference-incomplete-referent.t` and
`200-extern-class-object-declaration.t`.

- `make -C dev cppgm++ -j2` exits `0` after the bounded repair.
- The two owned handout identities pass `2/2`. A 16-test handout matrix
  containing both owned identities plus incomplete-class, class/scalar,
  aggregate, static, address, and TLS controls passes `16/16`.
- The focused course controls `402`, `404`, `407`, `408`, `409`, `412`,
  `415`, and `420` each exit `0`; course 410 also exits `0` and reports its
  expected `E=8/16/32` structural cleanup counts. Course 402's expected
  rejected inherited-noncallable case prints its diagnostic but the script
  passes.
- Reductions show an unused member body and unused default argument do not
  emit `declare global @x`, while used member/default roots do. A repeated
  static-const address through transparent casts still emits its required
  declaration. Incomplete class reference/glvalue lowering emits pointer
  LowIR; direct incomplete value materialization fails closed in PA12.
- Two compilations of each representative class-extern, scalar-address, and
  unused-extern probe compare byte-identically. The outputs are respectively
  `251`/`552`/`106` bytes and `9`/`23`/`4` lines, with hashes
  `04494956d6e5172b8e4e0db01829b613bc32d810143af278417f39b5cfb26b62`,
  `0d701eb51278f09ec5e22f13dbb4efbf577f46dba67243c684346c3cabee7f05`, and
  `485fc8e3251fe7b25d56cc0db4e5cc73da7486c3984948de64f662839846898f`.

The broad `make test-pa16` exits `2` only because the known residuals remain;
its exact identity set matches the landed 54-failure authority. The required
through-PA15 command exits `0` at `1167/1167`, and the required file audit
exits `0` with five pre-existing header-division warnings. The course-400
DMI control still reports its status-0/expected-1 mismatch outside this
landed increment; no lifetime/zero-storage surface was widened to address it.
The direct incomplete namespace-object address reduction is out of contract,
because `pa16/README.md` scopes namespace object declarations to complete
class types; it is explicitly deferred and is not an in-scope correctness
defect for this checkpoint.

The durable post-repair full-stage log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-typed-nonowning-namespace-object-20260829/post-repair-test-pa16-final-20260829.log`.
The exact sorted comparison is
`post-repair-identity-comparison-final.log` in the same directory. Durable
through-stage and file-audit logs are
`post-repair-through-pa15-20260829.log` and
`post-repair-file-audit-20260829.log` there.

### Performance and structural bounds

Owner/range eligibility is constant-time after the canonical dense binding and
owner tables are indexed. The existing structural checks visit the semantic
fact/child/conversion arenas and DAG edges in bounded passes; demand marking
then visits each reachable typed fact and each reachable aggregate/address
edge at most once, with dense seen storage. Constructor action arguments are
seeded only for emitted constructor plans. The resulting demand work is
bounded by reachable facts and edges, with O(F + E) worst-case arena work and
O(F) temporary mark state; no whole-TU retry or repeated broad marking loop
was added. Global scope/binding indexing remains O(S + B). No timing, RSS,
allocation, or speedup claim is made.

The byte-identical repeated probes above are the structural determinism
evidence. They also show the intended distinction: a used complete class
extern has one declaration and no definition, a scalar address target has its
declaration plus the address global, and the unused declaration-only class
extern has neither global emission nor a declaration.

### Boundaries and next checkpoint

No unrelated PA16 surface was re-audited. The next checkpoint is to run the
next residual PA16 audit without widening this typed non-owning-storage
boundary. The complete final gate evidence and exact identity comparison are
in `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-typed-nonowning-namespace-object-20260829/`.
The incomplete namespace-object address case remains explicitly out of
contract/deferred, and the unrelated course-400 DMI control remains outside
this landed increment.

## Historical Member Lookup Review

This review covers landed commit `a5b496e81d0bc9592900c6bb19715343fc6a960c`
(`PA16: fix typed member lookup boundary`) relative to its audit parent
`1093c2b7`. The bounded scope is the landed ordinary-value-over-tag,
member-enumerator, and member-call increment. The audit also makes one narrow
PA11 conflict correction and hardens the same PA12 path against malformed
enumerator owners/types/widths and malformed member default facts. The working
tree stays within the six permitted implementation paths, this documentation,
and the focused executable regression
`cppgm.tests/course/pa16/421-typed-using-separate-namespaces-regression.sh`.
No handout test, `.ref` or exit-status fixture, harness, comparator, coverage
rule, frontend source set, or unrelated source changed.

### Contract and ownership

The path was checked against `pa16/README.md`, `spec.md` §§2, 3, 4, 5, and 7,
and the preserved PA10--PA15 typed-owner boundaries. The representative flow
is:

```text
PA10 name/declarator facts and source point
  -> PA11 typed TypeId/declaration BindingId and ValueRef(binding, origin ScopeId,
     source point), with independent type and ordinary-value indices
  -> PA12 member lookup/selection or enumerator Literal fact, retaining the
     selected binding, owner, type, conversions, and exactly-once child facts
  -> PA15 typed LowIR call/projection/literal lowering; no lookup reconstruction
```

For a using-declaration, `process_using_declaration` first performs typed
`lookup_type_path` with the declaration source point and then typed
`lookup_value_path`. A target can therefore publish a canonical type identity
and canonical ordinary values independently. Type publication records the
introduced declaration binding and source point; each value entry retains its
canonical binding, owning origin scope, using view, and source point. The
corrected conflict boundary uses the canonical declaration identity from
`type_declaration_identity` and the origin returned by `lookup_type_path`:
cross-space coexistence is permitted only between an ordinary value/function
and a real class or enum tag (`BindingKind::Type` backed by `NamedKind::Class`
or `NamedKind::Enum`). A `BindingKind::TypeAlias` never grants that exception;
an ordinary target still conflicts with an existing direct typedef/alias.
Namespace collisions and same-space type/value conflicts remain rejected.
Downstream value lookup and the existing typed function selector consume the
ordered `ValueRef` candidates; no spelling is recovered.

For a direct or inherited member call, `member_lookup` checks the first
declaring class and then the validated direct-base chain in deterministic
order. Using views carry their canonical origin and access view. The parser's
call-shaped/declaration ambiguity asks typed `unqualified_member_value` whether
the implicit object's member claims the name before an outer type can claim a
declaration. Both ordinary member calls and this ambiguity path feed the same
`select_typed_member_function`, which walks the complete supplied member set
and ranks non-static implicit-object cv/base conversions, explicit argument
conversions, trailing defaults, and variadic arguments. Function-id arguments
retain their typed target-resolution path. Selection is followed by canonical
access/deleted checks, default-fact publication, contextual and call-argument
conversions, and a call fact carrying selected binding/scope/callable type and
the real implicit-this flag. PA15 emits the hidden `this` operand first.

For `object.member` and `pointer->member` enumerators, PA12 evaluates the
object expression once, validates the dot/arrow object and base path, and
selects the canonical enumerator binding and owner. The shared enumerator
producer now requires a valid enum type, canonical owner, underlying type, and
bounded value; it creates a declared-type prvalue literal with one typed child
for the object evaluation. PA15 validates that child as an enumerator-bearing
fact, lowers the child in discarded context exactly once, and only then emits
the literal. A call returning the pointer used by `->` therefore remains in
LowIR exactly once.

The implementation has one typed semantic model and one shared typed member
selector. It has no textual spelling recovery, whole-translation-unit scan,
retry loop, reduced duplicate overload algorithm, invalid fallback, or
order-dependent second pass. Source-point, owner, binding, candidate, access,
child-range, default-fact, and type-range checks fail closed. Unsigned enum
normalization uses preserved value bits and a checked width before shifting;
signed minimum values use the overflow-safe `-(raw + 1) + 1` representation.

### Findings and bounded repairs

- The real defect was in `process_using_declaration`: after finding a valid
  target type, the old conflict check rejected any direct destination value and
  the no-value branch returned before publishing a target ordinary value. This
  incorrectly merged the C++ type and ordinary-value namespaces. This repair
  retains independent type/value publication but limits the
  cross-space exception to a canonical real class/enum tag declaration
  (`BindingKind::Type`), not a typedef/alias. It validates the target and
  existing direct type declaration identity, owner, range, name, type, and
  kind; namespace, same-space, alias/value, and value/alias conflicts remain
  fail-closed. Course 421 covers class and enum tags in both directions, and
  four alias-conflict cases with exact `EXIT_FAILURE` status.
- The audit found two fail-closed holes in the affected typed consumers. The
  enumerator producer now validates canonical owner identity, enum record and
  underlying type, type range, and unsigned width before constructing a
  modulus. The shared member selector and its finalizer reject a valid-looking
  default `SemanticFactId` outside the semantic-fact arena instead of treating
  it as a missing or readable fact.
- Existing access, source-point, base-conversion, static/non-static, inherited
  and using-view checks remain in the typed lookup/candidate path. The normal
  member-call route and grammar ambiguity route both use the same selector;
  no reduced recovery resolver was introduced. PA15's discarded enumerator
  child path retains effectful calls and does not fold away evaluation.
- The readability follow-up is a line-neutral refactor of
  `process_using_declaration`: it keeps the file at exactly `3000` lines and
  replaces physical line packing with ordinary continuation, decision, throw,
  and loop formatting. Its cohesive local simplification merges the value
  binding validation, function classification, duplicate detection, and
  staging work, and uses `base_path_accessible` as the single canonical
  relation/access walk after removing the redundant `member_base_path` pass.
  No owner, range, kind, access, or type validation boundary is weakened.

### Focused evidence and disposition

The authoritative turn-start result for landed `a5b496e8` is `187/243`
passing, `56` failures, and `243/243` identities covered; the complete
turn-start failure map and the exact three-identity landed delta are preserved
in `pa16/plan.md` and `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The authorized final result is also `187/243` with `56` failures and full
`243/243` identity coverage. The sorted final-vs-turn-start comparison has
baseline-only `0` and final-only `0`, preserving the complete residual map and
stage progress; the durable v4 derivation is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/v4-final/failure-identity-comparison.log`.

- `make -C dev cppgm++ -j2` exits `0`.
- The readability follow-up keeps `dev/src/pa11_semantic.cpp` at exactly
  `3000` lines; its `67` added physical lines have maximum length `118`, with
  no newly added follow-up line over `118`. The condition/throw formatting is readable
  and the refactor is line-neutral.
- The focused handout matrix containing the three landed identities plus five
  preservation controls is `8/8`; the final log is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-focused-matrix-final-20260829-v3.log`.
- Courses 402, 403, 405, 419, and the new 421 regression pass, including
  `sh -n`; 402 prints its expected rejected inherited-noncallable control
  diagnostic while the script exits `0`. Course 421 reports four legal cases
  at status `0` and these exact-status negative cases: source typedef plus
  destination value `1`, source alias plus destination function `1`, source
  function plus destination typedef `1`, and source value plus destination alias
  `1`. The v4 syntax/run log is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/v4-final/course421.log`,
  and the final 402/403/405/419 controls log is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-course-controls-final-20260829-v3.log`.
- The refreshed structural probe log is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-structural-correction-20260829.log`.
  Typed using output repeats byte-identically (`442` bytes, `17` lines,
  SHA-256 `f872964c24e02f053e386733d9f5567c03d86f2c4d94abede253fea531ae1fc6`)
  and contains two `type f` and two `function f` identities. The effectful
  enumerator probe emits exactly one `call ptr @get_holder()`; the ambiguity
  probe emits one `Base__f` call with its default; the static ambiguity probe
  emits `C__f` without an implicit object; and the function-id probe emits one
  typed `addr @selected`. Unsigned-32, signed-minimum, and 64-bit-width enum
  probes all exit `0`.
- Course 406 is a pre-existing residual, not a regression from this audit:
  both the current rebuilt binary and a clean archive/build of exact commit
  `a5b496e8` stop at its first `qualified-parenthesized-static.cpp` positive
  with `ERROR: unknown PA11 type name` and exit `1`. Both ASTs are identical
  (`1943` bytes, SHA-256
  `b25a06952b8e4cf9e40a1fed597daeb0c920b4208e41f3d8d54a47cc8491c303`) and
  contain a cast-shaped `type-id Qualified::f`; PA12 therefore enters
  `semantic_cast_expression` and PA11 `type_from_type_id` before
  `semantic_call_expression`, `qualified_static_member_candidates`, or
  `select_typed_member_function`. Fixing that parser/cast ambiguity would
  widen this audit beyond the affected selector. Current and baseline logs are
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-course406-current-final-20260829-v3.log`
  and
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-course406-baseline-trace-20260829.log`.
- Focused handout and course results are status/structural evidence only. No
  timing, RSS, allocation, or speedup claim is made. The exact final
  through-PA15 log is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/v4-final/through-pa15.log`
  (`1167/1167`, status `0`); the exact PA16 log is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/v4-final/test-pa16.log`
  (status `2`, `187/243`); and file audit passes with five warnings in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/v4-final/file-audit-pa16.log`.
  The final `git diff --check` log is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/v4-final/diff-check.log`.

### Performance and structural bounds

The correction uses the existing per-scope typed indices, so each direct index
query is average O(1). Relevant lexical/using and member/base walks are
bounded by visited scopes `S`, base work `B`, and returned candidates `C`.
The shared selector performs one candidate walk with explicit argument work
`A` and parameter/default work `P`, bounded by O(C * (A + P + B)) for the
existing base checks; its score storage is O(C * A). The ambiguity case does
not scan the translation unit or retry with another resolver. The current
probe records deterministic output bytes/lines/hashes and candidate/call
cardinalities only; it does not support a timing or memory claim. The
line-neutral follow-up keeps `process_using_declaration` at `3000` lines with
normal control-flow formatting and no newly added line over `118` characters.

### Boundaries, residuals, and next checkpoint

The final authoritative residual is exactly the 59-item turn-start map minus
the three landed identities: `56` failures, with no final-only identity and
full `243/243` coverage. Unrelated lifetime, constructors, bit-fields,
operators, virtual/multiple inheritance, and broader PA16 residuals remain out
of scope. This checkpoint's final through-PA15, PA16, file-audit, and
diff-check gates preserve stage progress; the v4 durable results and exact
identity comparison are recorded in the current review. Course 406 remains
excluded as the clean-a5 pre-existing qualified static-call residual described
above; its failure occurs before the shared selector.

## Historical Non-Automatic Lifetime Review

This review covers landed commit `a1a2cf83d5673e0eda7e76878233eeec0a42f5d2`
(`PA16: emit typed non-automatic lifetimes`) relative to parent `c2247924`.
It audits the namespace/local class-object lifetime boundary, declaration-owned
construction facts, namespace-scope static-member definitions, thread-local
construction, aggregate-array preservation, recursive subobjects, local
automatic cleanup, deterministic initialization/finalization order, and
demand-driven helper emission. The audit also includes two narrow repairs in
the same ownership path: PA11 now carries its exact per-declarator definition
bit in a compact typed arena, PA12 consumes that bit without syntax
reclassification, a bodyless `extern` redeclaration no longer publishes a
second construction/lifetime fact, and PA15 no longer lets such a redeclaration
move the source position of an earlier typed fact. No handout, fixture,
existing test, exit-status file, harness, comparator, or unrelated stage
surface changed; the one new executable course regression is
`cppgm.tests/course/pa16/420-typed-redeclaration-lifetime-order-regression.sh`.

### Contract and ownership

The path is checked against PA16 README, `spec.md` §§1--5 and 7, and the
existing PA10--PA15 typed ownership boundaries. The representative fact flow
is:

```text
PA10 declaration syntax and storage qualifiers
  -> PA11 canonical BindingId/TypeId, owner scope, definition/linkage/storage
     classification, exact per-declarator definition flags parallel to
     declaration_bindings_, declaration ranges, and complete class/array layout
  -> PA12 declaration-owned Variable SemanticFact, typed constructor actions,
     recursive member/base actions, and LifetimeFact(object, TypeId, dtor,
     owner scope, storage kind)
  -> PA15 typed semantic/emission demand: global roots are scanned per
     ordinary/TLS mode, constructor/destructor bindings are demanded through
     typed edges, and local LifetimeFacts are indexed by function scope
  -> deterministic global/TLS storage, init/fini/guard/wrapper materializers
     and typed LowIR operands/calls
```

PA11 owns the canonical declaration and type identities and computes the exact
per-declarator `definition` bool in its declaration pass. That bool is copied
at the same append point as each `declaration_bindings_` entry into the compact
typed `declaration_definition_flags_` arena. `Binding::has_definition` remains
the canonical redeclaration-merged state; it is not used to reclassify the
current declarator. PA12 uses the carried bit and the declaration's own storage
context when publishing a variable fact. `record_namespace_lifetime` records
only non-thread-local namespace/static-member objects whose typed class record
requires runtime destruction. Thread-local construction remains a deferred
typed global action and is deliberately not a namespace destruction edge.

`class_record_for_object_type` preserves the named element record through
complete array tails. Aggregate initialization remains represented by typed
aggregate ranges and element facts; named class objects use constructor-action
facts. `ensure_implicit_constructor` and `ensure_implicit_destructor` retain
the class owner and typed callable identity. Constructor actions are base
first and declaration/layout ordered; synthesized destructor actions are
member reverse ordered followed by the direct base. Local automatic facts are
activated at declaration time and unwound on lexical/control exits through the
dense function-scope lifetime index.

PA15 validates the binding, definition-flag, semantic, and lifetime ranges once,
keeps ordinary-global and TLS root traversal distinct through
`scanned_global_fact_modes`, and follows only typed semantic children and
demanded helper edges. `declaration_by_binding_` is updated by the typed
definition flag, so the actual definition declaration owns ordering while a
bodyless redeclaration leaves that owner intact. Global initialization is
sorted by the declaration/declarator that owns the typed initializer. Namespace
finalization uses the same source order and emits the list backwards, so
destruction is reverse construction order. TLS guard, wrapper, and initializer
symbols are derived from typed owner/name identities and generated-kind
prefixes. LowIR receives typed symbols, operands, projections, and calls
directly; no rendered spelling is parsed back, no second production model is
introduced, and no whole-TU retry loop is used.

### Findings and bounded repairs

- PA12's centralized initializer path previously used the canonical binding's
  `has_definition` bit for every redeclaration. After `A a;`, a later
  `extern A a;` could therefore create another constructor action and append a
  duplicate `LifetimeFact`, failing PA15 with `duplicate lifetime fact identity`.
  PA11 already computes the exact `definition` bool at
  `dev/src/pa11_semantic_core.cpp:2477-2488`; the repair records that bool in
  `declaration_definition_flags_` parallel to `declaration_bindings_`. PA12
  validates the bounded range and flag value, then passes the typed bit into
  initializer/lifetime ownership. It no longer derives definition status from
  `declaration.is_extern` or AST child count. Alias, condition, and bit-field
  declaration producers keep the parallel arena continuous, and function
  definitions retain their exact flag without being mistaken for objects.
- PA15's `declaration_by_binding_` previously overwrote the source declaration
  on every canonical binding occurrence. The repair validates the typed flag
  range and updates the owner only for the exact per-declarator definition;
  bodyless redeclarations leave the actual definition owner intact. Thus
  definitions before or after a bodyless declaration retain their own
  construction/destruction position, while the canonical binding and typed
  variable fact remain shared. Zero/no-op definitions still retain the actual
  owner even when they produce no emitted init/fini action.
- The durable course regression `420-typed-redeclaration-lifetime-order-
  regression.sh` covers definition-before-extern and extern-before-definition
  ordering, a static class-member definition followed by a qualified extern,
  and a TLS definition followed by a bodyless TLS redeclaration. It checks
  typed LowIR storage/helper/call cardinality and init/fini order, and executes
  the generated ordinary/static/TLS programs where the runtime has a relevant
  entry path.
- The audit found no need to alter the landed TLS model: a TLS object is
  materialized through one deferred typed construction action and one guarded
  helper family, without a synthetic program-shutdown destructor edge. The
  collision-isolation case confirms that user spellings do not capture the
  generated guard/initializer namespace.
- The audit found no eager-helper regression. State-free classes and empty
  value-initialized subobjects retain zero/no-op behavior without useless
  implicit helpers; runtime-requiring members/bases demand their typed helpers
  through the reachable constructor edge. Existing aggregate arrays continue
  through typed element/layout paths; the three known lifecycle LowIR-shape
  residuals were not widened or papered over.

### Focused and authority evidence

The target turn-start authority is `184/243` passed, `59` failed, and
`243/243` identities covered, from
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The parent comparison recorded in `plan.md` is `c2247924` at `179/243` with
`64` failures; the landed increment removed exactly the five named
baseline-only identities and introduced no final-only identity. The final
`make test-pa16` remains `184/243` with `59` failures and `243/243` coverage;
the sorted comparison against the turn-start log is baseline-only `0` and
final-only `0`. The complete current 59-identity map remains in
`pa16/plan.md`; no coverage identity was changed by this audit repair.

- `make -C dev cppgm++ -j2` exits `0` after the two source repairs.
- The five fixed identities — `200-global-constructor`,
  `200-global-function-style-constructor`, `300-header-static-class-init`,
  `300-static-class-member-object-definition`, and
  `300-thread-local-synthetic-symbol-family-isolation` — pass the focused
  PA16 harness `5/5`.
- Existing course controls `404-typed-implicit-default-demand-regression.sh`,
  `407-typed-static-data-ownership-regression.sh`,
  `409-typed-constructor-boundary-regression.sh`,
  `410-typed-lifetime-activation-control-exit-regression.sh`, and
  `415-typed-global-initialization-order-regression.sh` each exit `0`.
  Course 410 reports the expected flat cleanup counts for `E=8,16,32`.
- `sh -n` and executable course regression
  `420-typed-redeclaration-lifetime-order-regression.sh` exit `0`. Its
  definition-before-extern case checks one constructor/lifetime per object and
  init `first,second` / fini `second,first`; its extern-before-definition case
  checks the definition-position order `second,first` / `first,second`; the
  static-member case checks one `Cell__object` storage, init, and fini family;
  and the TLS case checks one `tls` storage, one guard, one guarded init family,
  and one constructor call. The generated ordinary/static/TLS programs run
  successfully where applicable.
- The focused relevant handout matrix is `9/12`. The unchanged three
  comparison residuals are `200-local-default-class-array-lifecycle.t`,
  `200-member-object-lifetime.t`, and
  `300-synthesized-array-member-lifecycle.t`; their failures are LowIR-shape
  differences, not a new status/coverage result.
- A typed stdin probe with `A a; B b; extern A a;` exits `0`, emits one
  `A`/`B` constructor call in `a,b` order and one finalization family in
  `b,a` order. The declaration-before-definition variant also exits `0` and
  emits the definition order `b,a` with finalization `a,b`. A static class
  member definition followed by a bodyless qualified extern redeclaration
  emits one storage definition, one constructor call, and one destructor call.
  A TLS definition followed by a bodyless TLS redeclaration emits one guarded
  `__cppgm_tls_init__t` helper.
- A temporary typed `Item` family was compiled twice at each of `N=8` and
  `N=32`; `cmp` matched both repetitions. Each input has `destroyed`, an
  `Item` with an integer constructor/destructor, one source-ordered declaration
  per item (`item1(1)` through `itemN(N)`), and a trivial `main`. The `N=8`
  output has 102 LowIR lines, one init/fini function, and 8 constructor/8 destructor calls, with
  init objects `item1..item8` and fini objects `item8..item1`. The `N=32`
  output has 270 lines, one init/fini function, and 32 constructor/32
  destructor calls, with init `item1..item32` and fini `item32..item1`.
  The observed hashes are N=8
  `f2b10beb5a642aa2d176762572f9590088c4f5fa74c48927f415a392a42fe1b3`
  and N=32
  `33b39208fea1238e270880616f99c3c838370648c4fe4064bc97257ab5eb3bda`.

The final through-PA15 command exits `0` at `1167/1167`. The final
`make test-pa16` exits `2` at `184/243` with `59` failures and full `243/243`
coverage; the sorted failure-set comparison is exact with baseline-only `0`
and final-only `0`. The final file audit exits `0` with the five known
header-division warnings, and `git diff --check` exits `0`. Durable logs are
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-test-pa16.log`,
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-test-report-through-pa15.log`,
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-file-audit-pa16.log`,
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-pa16-failure-set-comparison.log`,
and `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-focused-pa16-full.log`.
No timing, RSS, allocation, or speedup claim is made.

### Performance and structural bounds

PA12's declaration pass publishes each typed variable fact once and delegates
recursive class/array work to bounded typed layout/action ranges. PA15 builds
the global root worklist once, scans each semantic fact at most once per
ordinary/TLS mode, and scans each reachable function fact once. Lifetime
ancestry is charged once into a dense function-scope flag vector; lowering
then uses constant-time scope membership and reverse active stacks. Ordered
global materialization is a stable `O(k log k)` sort of the typed action/lifetime
items followed by one linear emission pass, where `k` is the typed action
frontier. Maps are keyed by compact typed identities, so the implementation
has no textual recovery, whole-TU retry, incomplete demand key, or
unbounded optimization loop.

The N=8/N=32 replay above is representative structural evidence: output
repetition is byte-identical, constructor calls grow with the object family,
and finalization is visibly reverse construction order. It is not a timing,
RSS, allocation, or speedup claim.

### Boundaries, residuals, and next checkpoint

The exact target authority remains the 59-identity map in `pa16/plan.md`; the
three focused lifecycle shape residuals and all unrelated residuals remain
untouched. Thread-local objects intentionally have no namespace fini edge,
and copy/move/value transfer, virtual/multiple inheritance, and broader
special-member behavior remain outside this checkpoint. No handout or fixture
was rewritten to make the focused result green. The new course regression is
the only added test surface, and it does not alter handout fixtures or
references.

The next checkpoint is selected from the unchanged 59-identity map and must
preserve declaration-owned definition status, typed initializer/lifetime
continuity, source-order initialization, reverse-order destruction, TLS
collision isolation, and demand-driven helper reachability. This checkpoint
audit's broad validation is complete.

## Historical Access-Control Review

This review covers landed commit `135e3a953563d2356621b18ee1c37826ffce7c1c`
(`PA16 typed access-control boundary`) relative to parent `0fb73ad4`, plus the
bounded PA11 audit repair and corrected type-using ownership/source-access
handling. The
scope is canonical member owner/access, friend-class identity,
using-declaration source access and views, publishing scope, qualified
type/value and direct-base access, private/protected/friend rules, protected
object expressions, and PA15 selected binding/base projection. Virtual or
multiple inheritance, templates, constructors, lifetime/layout, and unrelated
PA16 residuals remain outside. Operator access-view propagation through
member/operator candidate sets is traced; unrelated operator behavior is out of
scope. No handout test, `.ref`, exit-status reference, harness, comparator, or
fixture changed.

### Contract and ownership

The path is checked against PA16 README, `spec.md` §§2--5 and 7, and N3485
§§7.3.3 p17--18, 11.2 p4--6, 11.3 p1--10, and 11.4 p1. Each named declaration
and base path is accessible; a using alias has the access of its
member-declaration context; PA11-supported public class-member using at
namespace or block scope remains valid; friendship is directional, neither
inherited nor transitive; and protected access observes the additional object
rule.

```text
PA10 declarations, qualified components, source offsets, and base syntax
  -> PA11 canonical BindingId/NamedRecordId owner/access sidecars;
     sparse direct friend relation plus friend-record reverse index;
     source-point-aware typed type/value lookup
  -> PA11 ValueEntry (canonical binding/origin, declaration point, optional
     MemberAccess view, publishing ScopeId) and direct base-edge metadata
     type using: canonical TypeId + introduced declaration BindingId/access
     value using: canonical BindingId/origin + paired publishing view
  -> PA12 ValueRef -> MemberLookup -> member/operator candidate sets,
     retaining canonical owner and using view; member_accessible validates
     source declaration, base path, private/protected/friend/object rules
  -> PA12 selected typed BindingId, owning ScopeId, and complete base path
  -> PA15 consumes selected facts and retains every validated direct-base edge
     as a LowIR projection; no lowerer-side reconstruction
```

The canonical source owner remains distinct from a using view. Friend reverse
lookup expands only directly recorded lexical friend classes. Qualified
resolution walks typed components, and all source/base checks use bounded
identity-bearing paths. No rendered-name recovery, duplicate access model,
whole-TU scan, retry loop, incomplete provenance tuple, stale canonical owner,
or lowerer reconstruction is present.

### Findings and bounded repairs

- In the type-valid using branch, the canonical `TypeId` is preserved but
  `record_type_declaration` now names the introduced binding. Its member access
  is set from `process_class_body`'s current access, so public/private/protected
  re-exposure is evaluated on the alias declaration rather than the base
  binding.
- The using source is checked before publication, preserving N3485 §7.3.3 p17.
  PA11-supported public class-member type/value using remains valid at namespace
  or block scope, while inaccessible private/protected sources are rejected
  unless the publishing context has access. Namespace-to-namespace using remains
  valid, and friend access can permit an otherwise inaccessible source before a
  class member view is published.
- The landed PA11/PA12 path retains canonical binding/origin and paired
  publishing view through `ValueEntry`, `ValueRef`, `MemberLookup`, static and
  non-static member candidates, and operator candidates. Fail-closed identity
  checks cover binding/origin, view ownership, friend records, and direct base
  paths. PA15 lowering remains unchanged and keeps all per-edge projections.
- The existing `member_accessible` boundary remains responsible for canonical
  access, view-relative access, friendship, protected object expressions, and
  object/base-path validation. Friendship is not inherited or transitive.

### Final evidence and disposition

The turn-start authority is `179/243` passed, `64` failed, and `243/243`
identities covered:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.

- `make -C dev cppgm++` exits `0`. The expanded
  `419-typed-using-access-regression.sh`, courses 405 and 411, and `sh -n`
  for 419 all exit `0`. 419 covers public protected-type re-exposure,
  private/protected alias views, friend/private source type access,
  public namespace-scope class-member using, valid namespace using, and the existing
  friend/protected-object/value cases.
- The selected 14-test PA16 command exits `2`, matching `12/14`; only
  `200-friend-derived-private-base-defaulted-constructor.t` and
  `200-friend-intermediate-derived-protected-base-method.t` retain checked-in
  LowIR-shape residuals, while their semantic status paths pass. Durable
  output:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-access-control-focused-corrected-20260829-v4.log`.
- `make test-pa16` exits `2` at `179/243`, with `64` failures and all
  `243/243` identities covered:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-access-control-final-corrected-20260829-v4.log`.
  The exact identity comparison is recorded at
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-access-control-final-identity-compare-corrected-20260829-v4.log`:
  authority failures `64`, final failures `64`, final-only `0`, and
  baseline-only `0`.
- The exact `n=16` through-PA15 command exits `0` at `1167/1167`. The checked
  PA11 class-constants/using fixture passes unchanged. Durable output:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-access-control-through-pa15-corrected-20260829-v4.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0`
  with the five known header-division warnings. Source sizes are
  `pa11_semantic.cpp=3000`, `pa11_semantic_core.cpp=2991`, and
  `pa11_semantic_model.h=2400`. Durable output:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-access-control-file-audit-corrected-20260829-v4.log`.
- The structural/noise replay uses only `dev/cppgm++`: 40/105 input lines,
  59/59 LowIR output lines, six/six base projections, identical LowIR hash
  `a994e25767151654c710b2724364f1b5f3d9b071c3b9326aef284b962a1b2fd6`, and
  identical negative stderr hash
  `37e6f8ed897d209b62c1b3b33e831cb114a86a06e792e0aa8c0645df156d3fd3`.
  Durable summary:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-access-boundary-probe-final-20260829-v4/summary.log`.
- `git diff --check` exits `0` after the documentation update. No timing, RSS,
  allocation, or speedup claim is made.

### Performance and structural bounds

Access checks use a sparse friend reverse index, bounded lexical ancestry
walks, named-component qualification, and the relevant direct-single-base
chain. Publication and lookup validate only typed identity/view tuples;
candidate work remains bounded by the selected class/base chain, and PA15
per-edge lowering is unchanged. The replay above is representative
noise-isolation evidence, not a timing claim.

### Boundaries, residuals, and next checkpoint

The PA16 failure set remains exactly the 64 turn-start identities with no
final-only additions, and the required through-PA15 gate is `1167/1167`. The
next checkpoint is a remaining PA16 semantic/lifecycle/layout residual family
outside typed access control and must preserve canonical ownership,
non-transitive friendship, protected object rules, supported using behavior, and
full direct-base edge lowering.

This review covers landed commit `3b7d8e6a228ec43a54d7eb97f1d5b45b450f6c57`
(`PA16: resolve typed qualified class names`) relative to parent
`f290784f9a63c8723fcf617bfcd36c9dc080de7e`.  It is bounded to qualified
class/nested-type parsing and typed PA11 ownership, including the single
`decltype` nested-name root.  Qualified casts, elaborated class headers,
injected names, inherited typedefs, source-point visibility, and the existing
PA12/PA15 typed consumers are included; access control, constructors,
operators, lifetime behavior, broad templates, and unrelated PA16 residuals
are controls or boundaries.  No handout test, `.ref` fixture, harness,
comparator, reference output, or source-set list changed.

### Contract and ownership

The path is checked against spec.md §§2--5 and 7: one typed pipeline, one
canonical owner, no textual downgrade, bounded work, source-point-aware
visibility, and deterministic lookup/emission.  The representative ownership
trace is:

```text
PA10 token/index tables and qualified components
  -> one optional decltype root sidecar plus unflattened name components
  -> PA11 scoped NamePath (root TypeId, then ScopeId/TypeId traversal)
  -> source-point-aware lookup graph and typed BindingId/NamedRecord owner
  -> PA12 type/member/base facts and PA15 typed lowering consumers
  -> LowIR/object facts retain the typed IDs; no name reconstruction
```

PA10's delimiter, angle/template-close, and right-shift-piece indexes are
sentinel-filled and bounds-checked before every qualified lookahead step.
`name_node` retains each component separately and records exactly one
`decltype` root in the AST sidecar.  PA11's scoped `name_path` validates the
sidecar range and resolves the root at the use source point; the remaining
components use typed namespace/class scope transitions.  The final qualified
lookup first checks the requested class scope, then walks the validated direct
single-inheritance chain in deterministic order.  Injected class identity is
the existing typed `NamedRecord` fact, while stored typedef/member declarations
return their owning `BindingId`.  PA12 and PA15 consume these typed results;
they do not split, render, or re-intern the spelling.

### Findings and bounded repairs

- Qualified C-style cast lookahead now follows all identifier/template
  components and indexed template closes, including the split `>>` pieces,
  before deciding whether the parenthesized sequence is a type-id.  Relative
  and absolute indexes are checked before access.  The selected
  `owner::mask` same-spelling operand case passes, and the parser continues to
  preserve parenthesized calls such as `(N::f)(0)`.

- Elaborated-specifier classification consumes qualified components and
  template arguments through the indexed close table before class-definition
  delimiters.  The out-of-class nested-class and `alignas` cases therefore
  retain the full qualified declaration name.

- `decltype(source)::type` is routed as a type specifier, not as a rendered
  prefix.  The new scoped overload validates the one-root sidecar and its
  range, resolves `source` at the use point, and then performs typed nested
  lookup.  The old context-free `name_path` overload explicitly rejects a
  decltype prefix; the affected type-specifier owner (`spec_fact`, and the
  qualified base/pointer type routes) uses the scoped overload, so no new
  decltype prefix is accidentally sent through the old route.  Other
  expression/declarator routes remain fail-closed at that existing boundary.

- The audit repair is in `dev/src/pa12_semantic_resolution.cpp`, at the
  existing functional-cast target owner.  When PA10 deliberately represents
  `(N::T)(operand)` as a call whose callee is a parenthesized qualified
  `IdExpression`, PA12 unwraps only those parenthesized callee layers, checks
  ordinary typed value lookup first, and then resolves the `NamePath` through
  typed type lookup.  Thus a visible `N::f` remains an ordinary direct call,
  while `N::T` owns the functional cast's typed `TypeId`; existing
  class-qualified C-style casts remain on their original parser path.

- `lookup_type_qualified` rejects invalid scopes, non-class records, missing
  members, and malformed base metadata.  Validated direct-base traversal is
  cycle-checked and single-inheritance ordered; current and inherited
  injected identities are typed records, while inherited typedefs remain
  stored bindings.  Source-point checks stay in the ordinary lookup graph;
  class/base facts used as qualifiers are already source-point validated by
  their owning typed path.

The audit repair closes the namespace-qualified parenthesized type-conversion
case within the accepted PA16 namespace/type grammar.  The focused control
also verifies that `(N::f)(0)` remains a call and that the existing
class-qualified same-spelling cast remains valid.  No relevant correctness
issue remains in this qualified-name ownership path.

### Focused and broad evidence

The authoritative result for this increment is `173/243` passed, `70` failed,
and `243/243` identities covered, versus the parent baseline `167/243`, `76`
failed, and `243/243` covered.  Exact comparison has these six baseline-only
identities and no final-only identity:

- `general/100-qualified-typedef-cstyle-cast-same-name-operand.t`
- `general/200-inherited-injected-class-name-qualified-type.t`
- `general/200-nested-class-private-enclosing-access.t`
- `general/200-qualified-inherited-member-typedef.t`
- `general/300-alignas-out-of-class-nested-type.t`
- `spec/100-decltype-qualified-nested-type-local.t`

The exact 76-to-70 identity comparison, including coverage and empty
final-only set, is preserved in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-qualified-type-call-audit-identity-compare.log`;
the broad result log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-qualified-type-call-audit-final.log`.

This turn's focused evidence is:

- `make -C dev cppgm++ CC_FLAGS='-std=gnu++11 -Wall -O3'` — exit `0`.
- The six repaired tests plus six preservation controls — `PASS (12/12)`.
- Course control `417-qualified-parenthesized-type-call-regression.sh` — exit
  `0`; namespace-qualified conversion, namespace function call, and the
  existing class-qualified same-spelling cast all pass.
- Focused command transcript:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-qualified-type-call-audit-focused.log`.
- Outside-repository qualified-type probes — target/noise LowIR hashes are
  repeat-stable (`c5eaa8b9876dbc16d7fc8acaba16ee2e4d49b5e45b7e619eac53a7794fb69cc5`),
  the five-component bounded-depth hash is repeat-stable
  (`e8c8a69279f71202a5bb35539e0665838bf2624a758d0f2edddd415e66d9a350`),
  and non-class-decltype/missing-member negatives reject with status `1`.
- `make test-pa16` — exit `2` with `173/243` passing, `70` failures, and all
  `243/243` identities covered; no final-only identities.
- `n=16` through-PA15 report — exit `0` at `1167/1167`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` — exit `0`
  with the five pre-existing header-division warnings.
- `git diff --check` — exit `0`.

No timing, RSS, allocation, or speedup claim is made.  No handout fixture,
reference binary, or comparator/harness behavior was changed.

### Performance and structural bounds

The qualified parser work is bounded by token count and prebuilt
delimiter/template indexes; each component advances monotonically, and
`>>` is consumed only when both split-piece bounds are valid.  PA11's
namespace/using lookup visits only relevant graph edges, and qualified class
lookup walks the validated single-inheritance chain once.  The no-noise and
noise probes produce byte-identical repeated LowIR, while the bounded-depth
probe exercises a longer path without whole-program scanning.  These are
structural/repeatability observations only: no timing, RSS, allocation, or
speedup claim was measured.

### Boundaries, residuals, and next checkpoint

PA16 remains incomplete with the authoritative 70 residual identities; the
exact set is preserved by the comparison log.  Those residuals are unrelated
object-model, aggregate, lifetime, access, constructor, operator, and
broad-template surfaces.  The next checkpoint should be selected from the
70-identity map and must preserve this typed qualified-type owner; the
qualified-name path itself has no unresolved audit uncertainty.

## Historical Aggregate Initialization Review

This review covers landed commit `dea01c52089fe78b8d23cce0b72ecbe8686ddb26`
(`PA16: lower typed recursive aggregate initialization`) relative to parent
`36b93869`, plus the bounded checkpoint-audit repairs and course control 415.
The ownership boundary is C++11 aggregate initialization for the supported
object subset: nested arrays and records, brace elision, omitted tails,
default member initializers, scalar/class/reference members, bit-fields,
string-literal pointer members, value initialization, and namespace/static
aggregate storage. Unions, aggregate bases, copy/move or by-value transfer,
templates, virtual/multiple inheritance, and unrelated residual stage surfaces
remain outside this checkpoint. No handout test, fixture, `.ref` file,
comparator, harness, or source-set list changed.

### Contract and ownership

The implementation follows spec.md §§1, 2, 4, 5, and 7: one shared typed
pipeline, one canonical owner for each fact, demand-driven bounded work, no
source-text reconstruction, and deterministic structural evidence. The
representative ownership trace is:

`PA10 BracedInitList + typed destination`
  -> `PA11/PA12 canonical TypeId, BindingId, SemanticFactId`
  -> declaration-ordered `RecordLayout::members` and sparse
     `AggregateElementFact` ranges
  -> PA12 appertainment, brace elision, DMI/omitted-tail/reference decisions,
     constructor actions, and typed parameter resolution
  -> PA15 independent global-root/runtime demand visitation
  -> one source/declaration-ordered `PendingGlobalAction` stream
  -> typed constant data or `SR_INIT` lazy aggregate-root/path lowering
  -> checked owner/type/range/layout offsets
  -> direct scalar/reference/bit-field stores or a demanded constructor helper
  -> LowIR and backend.

`RecordLayout::members` is the sole declaration-order/index owner in the
changed PA12 paths. It excludes static members and anonymous/zero-width
bit-field layout events while retaining named bit-fields in declaration order.
Sparse aggregate ranges carry only present elements; omitted scalar runs are
zero-filled without a bound-sized semantic arena. PA12 copies arena values
before initializer work that may append bindings or types.

For `Pair rows[2] = {{1, 2}, {3, 4}}` and the brace-elided equivalent, typed
list facts retain nested destination types and PA15 recomputes checked paths
from the canonical root. `RefWrap alias = {pair.first}` carries addressable
storage and emits an alias pointer, not a copied value. Named bit-fields use
the same ordered layout path and the bit-field initialization context. Typed
literal payloads are interned from PA11-owned decoded bytes, so fixed and
inferred-bound string-pointer records do not recover source spelling.

### Findings and bounded repairs

- `collect_demanded_member_functions` now has independent global-root and
  ordinary-runtime visited state. A shared semantic fact can therefore be
  considered once for global aggregate-helper inlining and again for runtime
  helper demand; one context-specific visited bit cannot suppress the latter.
  Both walks remain bounded by the semantic-fact arena and deterministic stack
  order. Course 409 covers the related aggregate-helper/ordinary-default
  constructor boundary; no distinct same-`SemanticFactId` course reproducer
  was found.

- Global address projections, scalar dynamic values, and aggregate actions
  share one pending-action stream. Stable source declaration/declarator order
  is retained through `__cppgm_init`, so implementation kind cannot regroup
  ordered dynamic initialization within one translation unit. Course 415
  observes the interleaved call/store order and executes the result.

- The global aggregate inliner requires an exact synthetic aggregate
  constructor, canonical record/type/owner identity, complete layout, fixed
  arity, declaration-order action range, valid function-scope parameters,
  scalar/pointer member types, checked offsets, and parameter-only initializer
  facts with a bounded cast walk. Invalid, cyclic, reference-unsupported, or
  otherwise non-inlineable facts fail closed to the ordinary demanded helper;
  they are never silently suppressed. Root storage, path recomputation,
  overflow/range checks, and bit-field context are validated before stores.

- Global aggregate data lowering coalesces omitted zero runs while preserving
  one typed scalar slot where needed. It supports fixed and inferred-bound
  string-pointer records through typed literal-content interning. The exact
  formerly residual unknown-bound namespace record handout now passes.

- The canonical-bool shortcut was not retained. The valid affected shape is
  `trunc u8 i64` followed by `zext i32 u8`; the direct `zext i32 u8` from the
  i64 comparison was rejected by `lowir2cy86` for operand/source-type
  mismatch. The final focused output check and backend translation pass.

- The file-audit size findings introduced by the increment were removed by
  extracting global declaration-position validation and keeping declaration
  semantic analysis within the project limit. The audit now reports only its
  five pre-existing header-division warnings.

### Focused and broad evidence

The authoritative turn-start record is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`159/243` passed, `84` failed, and `243/243` identities were covered. Final
`make test-pa16` exits 2 with `164/243` passed, `79` failures, and
`243/243` identities covered. Exact failure-set comparison has five
baseline-only repairs and an empty final-only set:

- baseline-only: `general/200-global-class-array-enum-trivial-dtor.t`,
  `general/200-global-scalar-dynamic-init.t`,
  `general/200-local-struct-array-init.t`,
  `general/300-namespace-aggregate-array-string-members.t`, and
  `general/300-static-member-aggregate-array-dynamic-init.t`;
- final-only: `∅`.

The complete exact baseline and final maps are preserved in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-aggregate-init-audit-final-v1/baseline-failures.txt`
and `final-failures.txt`. The exact 17-test focus is `12/17`, with all
`17/17` identities covered. Its five remaining failures are
`general/100-global-aggregate-nested-array-initializer.t`,
`general/200-defaulted-constructor-still-aggregate.t`,
`general/200-deleted-constructor-still-aggregate.t`,
`general/300-value-init-aggregate-with-nontrivial-member.t`, and
`general/400-bitfield-aggregate-init.t`; the other 12 focus identities pass.

`make -C dev cppgm++` exits 0. The required through-PA15 command exits 0 with
`1167/1167` passing. The required file audit exits 0 with five pre-existing
`bad-division` warnings in `abi_mangle.h`, `cpp_semantic_core.h`,
`lowir_model.h`, `pa11_semantic_model.h`, and `pa15_lowering.h`.
Controls 404, 409, 412, and 415 each exit 0; `git diff --check` exits 0.
Exact outputs and statuses are preserved under
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-aggregate-init-audit-final-v1`.

### Performance and structural evidence

The immutable historical evidence at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-aggregate-init-evidence`
was preserved. The separate final replay is at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-aggregate-init-audit-final-v1`.
The final compiler SHA-256 is
`62f6feea601662cb601f12c3ad3b9083f4da85639c2e2b741cf24c7a31721d4b`.
The replay has 30/30 zero-status runs, nine semantic pairs and six LowIR
pairs, with zero repeated-hash mismatches. It records source/output sizes and
semantic list/action/literal plus LowIR store/call/projection counts in
`structural-counts.tsv`.

The omitted-tail bounds 16, 1024, and 1000000 each produce 17 semantic lines,
one aggregate list, and zero per-element aggregate descendants; their output
sizes are 593, 601, and 612 bytes. Explicit nested, brace-elided,
reference/class, fixed string-pointer, and exact unknown-bound handout probes
all pass twice with matching hashes. The exact unknown-bound handout produces
52 semantic lines/2616 bytes and 118 LowIR lines/2564 bytes. The bool and
ordered-initialization replays are in `bool-shape-check.log` and
`ordering-evidence.log`. These are structural/deterministic observations only;
no timing, RSS, allocation, or speedup claim was measured.

### Boundaries, residuals, and next checkpoint

The five focused LowIR residuals listed above remain open; defaulted/deleted
aggregate eligibility is not broadened beyond the typed C++11 decision, and
the value-init/bit-field/global nested cases retain their valid typed lowering
even where fixture LowIR shape still differs. The full stage remains at 79
failures, so PA16 is not complete. No same-fact shared-demand course case was
constructed, and no timing/RSS evidence exists. Unions, bases, transfer,
templates, virtual/multiple inheritance, and unrelated PA16 surfaces remain
out of scope. The next checkpoint should select the remaining aggregate
LowIR/semantic identities or a separate staged surface; it must not treat this
checkpoint as completion.

## Historical Member-Function-Definition Declarator Review

This historical review covers landed commit 9718b98797312753e33023fe97d36d74afd0a84a
(PA16: type member-function definition declarators) relative to parent
97d1e7a5, plus the bounded follow-up corrections in the PA11 typed
declarator path. The source audit is limited to pa11_semantic.cpp,
pa11_semantic_core.cpp, pa11_semantic_model.h, pa11_semantic_types.cpp,
pa12_semantic.cpp, and pa12_semantic_construction.cpp. The only added test
artifact is cppgm.tests/course/pa16/413-typed-member-definition-declarator-validation-regression.sh.
No handout test, fixture, .ref file, comparator, harness, generated output,
or source-set list was changed.

The PA16 contract here is in-class member definitions and qualified
out-of-class ordinary non-static member definitions, including typed trailing
returns and private nested leading return types. The README explicitly
excludes out-of-class constructor and destructor definitions. The out-of-
contract special-member widening from 9718b987 is removed: process_special_member
again requires a class-scope owner, and the namespace/root PA12 special-member
analysis and preparation additions are gone. Only the pre-existing in-class
special-member path remains in this scope. The excluded nested out-of-class
constructor remains a failing identity and is not claimed as PA16 coverage.

### Contract and ownership

N3485 [dcl.fct], [dcl.fct.def], and [class.mfct] define the function
parameter-and-qualifier sequence, trailing-return-type, and member-definition
context relevant here. The implementation is checked against that standard
text in [N3485](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3485.pdf).

The root architecture alignment is:

- spec.md §2 keeps classified declaration, type, scope, binding, parameter,
  and body facts typed; presentation is not used to recover identity.
- spec.md §4 keeps this validation as local, deduplicated work over the
  declarator and its typed operation list.
- spec.md §5 keeps PA12 preparation/analysis and PA15 lowering on the existing
  typed FunctionFact, TypeId, and downstream facts.
- spec.md §7 records executable conformance and structural determinism
  evidence without inventing a timing claim.

The affected fact path is:

PA10 declarator shape (qualified name, parameter clause, cv/noexcept/ref/
trailing-return nodes)
  -> PA11 SpecFact::is_auto, explicit DeclaratorBaseKind state, and
     DeclaratorOp carrying the trailing TypeId
  -> canonical class ScopeId, NamedRecordId, BindingId, FunctionFact,
     function scope, parameter facts, and body scope
  -> PA12 preparation/analysis and existing typed conversion/call consumers
  -> PA15 typed function reachability, ABI facts, and LowIR emission

For an ordinary member definition, process_function_definition resolves one
qualified owner. That owner is reused for trailing-return lookup, parameter
lookup, binding ownership, FunctionFact.owner, function-scope parentage, and
body lookup. The focused owner path emits the declared const member parameter,
the declared parameter, and the body member access without reconstructing an
owner from rendered text.

### Findings and bounded repairs

- The trailing return remains a typed PA10 TypeId inside a DeclaratorOp.
  SpecFact::is_auto is the canonical classification. Each application entry
  creates an explicit DeclaratorBaseKind; nested declarators share that state.
  The trailing-return operation requires AutoPlaceholder and consumes it by
  changing the state to Typed. No invalid TypeId is used as an auto marker.
  An unrelated invalid TypeId remains invalid and is rejected by the existing
  typed-result checks. auto (*callback)() -> int remains valid.

- spec_fact rejects a duplicate auto, auto combined with another base
  type, leading cv-qualified auto, and typedef auto. Storage qualifiers do
  not broaden the type rule: the valid static auto trailing-return control in
  course 413 passes.

- A trailing return requires a parameter clause and the auto placeholder.
  Object arrows, missing auto, suffixes after the arrow, invalid cv/noexcept
  ordering, and unsupported ref-qualified forms fail closed. Ref qualifiers
  are rejected because the existing TypeKey and binding identity do not
  represent them; silently ignoring them would merge distinct declarations.
  Auto in parameter types and auto in type-ids is rejected. The public course
  control's auto f() -> auto case is parser-accepted as a TypeId and reaches
  typed rejection; the other public malformed forms are asserted as
  rejections without claiming more parser reachability than observed.

- The special-member owner change was narrowed back to the parent behavior.
  process_special_member is class-scope-only; root SpecialMemberDefinition and
  SpecialMemberDeclaration handling in PA12 is absent, and the root
  special-member preparation body is absent. The in-class constructor-member-
  init control still passes. The excluded nested out-of-class constructor
  now fails at PA11 special-member owner validation, as required by the
  contract boundary.

- The implementation uses one bounded child walk and one typed operation
  application per declarator, with no textual semantic key, test-name
  shortcut, reference/host-compiler shell-out, whole-program retry, or
  unbounded scope scan. Invalid IDs are rejected before binding or
  FunctionFact publication.

### Focused and broad evidence

The audit-turn start was exactly 132/243 passed, 111 failed, and 243/243
identities covered. The authoritative baseline is
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log. Its
diagnostic totals are 61 expected-success exit mismatches, 2 expected-failure
exit mismatches, and 48 LowIR comparison mismatches.

The final focused build exits 0; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/build.log
The public course 413 regression exits 0 and covers mixed/duplicate/cv/
typedef auto, auto in a type-id, missing auto, non-function arrows, suffix
ordering, ref qualifiers, auto parameters, auto without a trailing return,
auto simple declarations, a valid static auto return, and the valid nested
function pointer; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/course-413.log

The exact seven-test focus exits 2 with 5/7 passing; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/focused-matrix-7.log
Passing identities are
300-member-function-trailing-return.t,
100-out-of-class-methods.t,
300-out-of-class-private-nested-return-type.t,
200-constructor-overload-default-arg-nonfirst-argument.t, and
200-return-preserves-value.t. The residual
300-out-of-class-member-trailing-return.t fails with PA12 invalid conversion
in its existing member-typedef pointer-return path. The explicitly excluded
200-nested-out-of-class-constructor-enclosing-type.t fails with
PA11 special member has no class owner. The separate
200-constructor-member-init.t control is 1/1; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/focused-constructor-control.log

The required prior-through command exits 0 with 1167/1167; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/through-pa15.log
The exact required file audit exits 0 with five pre-existing header-division
warnings; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/file-audit.log
The final make test-pa16 exits 2 because PA16 remains incomplete, with
132/243 passed, 111 failures, and all 243 identities covered; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/pa16-test.log
The exact failure-set comparison is preserved at:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/identity-compare.log
It reports baseline 111, final 111, inventory 243, baseline-only empty,
and final-only empty. Thus no pass identity regressed, and no added pass was
used to offset a new failure. git diff --check exits 0; log:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/diff-check.log

### Performance evidence

The current executable structural run is:
 /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/structure.log
Its log SHA-256 is bcb4f7ac160a94f1cbb499ca5a823c657d8487659d58d0b55664b1b9f4a4d1a1
and the executable SHA-256 is 375482e808d7c1c1251e9c9ebf186eee8b4ff6014f6e4410c5d2ca5a18d12379.
Using the existing N=1,4,16,64 member-definition inputs, each run has N
declarations and N definitions. Two semantic runs per size exit 0 with
identical hashes. Output lines/bytes are 14/468, 32/1227, 104/4269, and
392/16461; function-record counts are 2, 5, 17, and 65. This is structural
boundedness and determinism evidence only; it is not a timing, RSS,
allocation, or speedup claim.

### Next checkpoint

The next checkpoint is a later PA16 residual audit focused on the existing
member-typedef pointer-return residual and the remaining explicitly staged
PA16 boundaries. It is not broad validation. PA16 remains incomplete until
those residuals are separately resolved or contractually closed.

## Historical Fixed-Bound Array-Lifetime Checkpoint Review

This historical review was retained from the preceding fixed-bound array
lifetime checkpoint.  Its original content follows unchanged.

This review covers landed commit `0a6be82d9bf17db2585772f2be28d45e6af781de`
(`PA16: add typed array lifetime cleanup`) relative to parent
`5d91986f166e000daddecaf112e0cb58df6a8e8b`, plus bounded audit repairs and
the focused course regression in
`cppgm.tests/course/pa16/410-typed-lifetime-activation-control-exit-regression.sh`.
The scope is fixed-bound local automatic arrays of class objects and recursive
synthesized array-member lifetime: typed array shape, canonical class and
destructor identity, PA12 lifetime/action facts, PA15 recursive construction
and destruction, completed-prefix EH cleanup, lexical/control-exit state, and
LowIR serialization.  Global/static/TLS lifetime and guards, copy/move or
by-value transfer, virtual/multiple inheritance, templates, new/delete, and
unrelated operator/access/temporary machinery remain outside this review.

The authoritative checkpoint-turn-start full-stage state was `93/243` passed,
`150` failed, and `243/243` covered, from
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The final `make test-pa16` command exited `2` with log
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-test.log`:
`93/243` passed, `150` failed, and all `243` tests were covered.  The
normalized failure map is exactly the baseline map: `150` identities in each
log, baseline-only `∅`, and final-only `∅`; the `93` passing complement is also
unchanged.  The test inventory contains exactly `243` identities, and the
baseline and final runs each report every identity, so coverage additions and
removals are both `∅`.  The normalized set/count record is preserved at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-identity-compare.log`.

The affected ownership path is:

```text
PA10 local class-object/array declaration and synthesized-member syntax
  -> PA11 canonical TypeId array shape, NamedRecordId, BindingId, and
     destructor FunctionFact ownership
  -> PA12 automatic LifetimeFact plus ordered ConstructorActionFact and
     DestructorActionFact ranges (base/member order and recursive arrays)
  -> PA15 typed destructor demand, constructor/destructor lowering, checked
     array strides, active-lifetime state, and completed-prefix EH chains
  -> LowIR constructor/destructor calls, lexical/control-exit cleanup,
     eh_try/eh_cleanup/eh_end/resume, and truthful unwind metadata
```

### Findings and bounded repairs

- The original lowering path had a per-function scan of every lifetime fact and
  its scope ancestry to decide whether `goto` must fail closed.  The repair
  adds `index_lifetime_facts()`, called once from `index_binding_facts()`.  It
  builds a dense `ScopeId`-indexed byte flag after walking each lifetime's
  ancestry once.  `lower_function` validates its typed FunctionFact scope and
  performs one O(1) flag lookup; any nontrivial lifetime in that function still
  conservatively blocks `goto`.
- The one-time lifetime index enforces exact typed continuity: the object's
  `Binding.type` equals `LifetimeFact.object_type`; the object is a variable
  owned by the fact's scope; the array/object type resolves to a class record;
  the fact destructor equals `model_.destructor_binding(record)`; and
  `checked_destructor_function` validates the complete destructor FunctionFact
  and action range.  Every scope ID in the ancestry is range-checked, the walk
  is bounded by the total scope count, it must reach a Function scope with a
  valid non-self parent, and malformed or cyclic ancestry fails closed.
  Duplicate lifetime bindings and declaration lifetime ranges are rejected.
- The index's ancestry walk is now in `pa15_lowering_construction.cpp`, keeping
  the affected `pa15_lowering.cpp` under the 3000-line file-audit limit.  This
  is a source-ownership correction, not a behavior change: indexing remains
  once per completed semantic model, with O(S) dense flags and O(L log L) map
  publication for `L` lifetime facts and `S` scopes, plus bounded ancestry
  work `O(sum depth) <= O(L*S)`.
- Constructor/destructor actions remain canonical typed ranges.  PA12 publishes
  base-first and declaration-order member construction, reverse member/base
  destruction, and recursive array actions.  PA15 validates member owner
  bounds and base record identities before lowering an action, rechecks the
  active destructor FunctionFact, and uses typed demand worklists without
  textual recovery.
- Array element paths validate the bound and checked `ordinal * type_size(child)`
  offset before converting the index.  Completed elements retain a typed root
  and path; cleanup recomputes their addresses, so arena growth or later
  LowIR emission cannot invalidate a saved temporary.  The shared prefix chain
  materializes each completed element once and emits one reverse destructor
  call per chain node before transferring to its predecessor and finally
  `resume`.
- Automatic lifetimes activate only after initialization.  Lexical scope
  markers, unbraced substatement cleanup, branch-state restoration, loop
  condition/iteration joins, for-init normal exit, switch-arm recovery, return,
  fallthrough, break, and continue all preserve only the active typed suffix;
  unsupported `goto` remains fail closed.  Destructor-body early return still
  emits remaining base destruction.
- The affected implementation is deterministic and bounded: typed fact/action
  ranges are snapshotted before recursive demand can grow arenas, demand scans
  reachable typed facts once, lowering performs bounded path/layout checks, and
  no reference binary, host compiler, whole-scope retry, or test-specific
  output shortcut is used.

## Focused Evidence

`make -C dev cppgm++` exited `0`; durable log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-build.log`.
Syntax checks and focused course controls 408, 409, and 410 all exited `0`;
the durable focused log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-focused.log`.
Course 410 specifically verifies activation before later declarations,
unbraced if/while/for statement cleanup, for-init normal exit, destructor
early-return base cleanup, loop and switch join state, branch exits, nested
array reverse addressing, and the E=8/16/32 structural scale controls.  Its
exact output was:

```text
PA16 structural flat E=8 cleanup_calls=7 main_lines=129
PA16 structural flat E=16 cleanup_calls=15 main_lines=257
PA16 structural flat E=32 cleanup_calls=31 main_lines=513
```

The full run still reports the four affected-path handout comparison
identities `200-destructor-body-local-before-base-destruction.t`,
`200-local-default-class-array-lifecycle.t`,
`200-member-object-lifetime.t`, and
`300-synthesized-array-member-lifecycle.t` in the unchanged baseline failure
map.  Their checked-in fixtures and references were not changed; no current
pass or failure claim is inferred from a reference-only shape difference.

The exact prior gate command (`n=16` followed by
`make test-report-through-pa15`) exited `0` at `1167/1167`; durable log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-through-pa15.log`.
The required file audit exited `0` and reported five existing
header-division warnings; durable log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-file-audit.log`.
The warnings are `abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
`pa11_semantic_model.h`, and `pa15_lowering.h`; there were no fatal issues.
`git diff --check` exited `0`; durable log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-diff-check.log`.

### Performance Evidence

Course 410's E=8/16/32 cleanup calls are exactly `E-1`, with main-line deltas
`128` and `256`; its nested `[2][3]` control verifies six reverse destructor
calls, outer strides `1,0`, and inner indices `2,1,0,2,1,0`.  These are
structural scale controls, not a timing claim.

The refreshed smoke/scale run used the immutable `0555` executable
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-perf/cppgm++-immutable`
with SHA-256
`be89e2a8efdc723f3e2947f8df48bf00b72333f52750f61dddbc6bd61539ad14`.
For each E, the same generated input was used for five interleaved batches of
20 compiler invocations; `/usr/bin/time` measured the batch and the reported
values are medians and ranges across the five batches.  The complete output,
including current input/output hashes, is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-performance.log`.

| E | wall median (range) | user median (range) | system median (range) | RSS median (range) |
| --- | --- | --- | --- | --- |
| 32 | `0.09s (0.09..0.09)` | `0.04s (0.04..0.04)` | `0.05s (0.04..0.05)` | `6516 (6448..6564) KiB` |
| 128 | `0.18s (0.18..0.19)` | `0.09s (0.09..0.10)` | `0.09s (0.08..0.09)` | `8500 (8496..8628) KiB` |

The input hashes are E=32 `f37786713c510300b1a9e5884285f7ae4ae7e16a5c1337616a087aac9bf79e54`
and E=128 `c2a66edb4b0088e64e49a70b57dda93c935a0469a853c2a621de72fcc9422c0f`.
Final output hashes are E=32
`67b17d7e3f7b2a3507dd795ed9cd05285dc1050c1eec600d15f92b70a6b16d0b` and
E=128 `cc0554ce1ed562f67be832da79110737001cbf9b96aa40c406960803c3e96399`.
The outputs have main-line counts `513` and `2049`, cleanup nodes/calls
`31/31` and `127/127`; the fourfold element increase gives fourfold main-line
growth and cleanup calls remain `E-1`.  These are representative smoke/scale
measurements, not a benchmark comparison or an allocation claim.

### Next Implementation Checkpoint

PA16 is not complete.  The next implementation checkpoint remains within
PA16: resolve the remaining local automatic/synthesized lifetime reference
shape and semantic cases after separately scoping unrelated PA16 failures.
Global/static/TLS lifetime, value transfer, virtual/multiple inheritance, and
the other exclusions above remain deferred; do not advance this path to PA17
on the unchanged full-stage map alone.

## Historical Static Member-Function Checkpoint Review

This review covers landed commit `021ef63927293f62e13a29b5b8265c7105fb35a9`
relative to parent `15e133af`, plus the bounded audit repairs and one focused
course regression in that checkpoint.  It is limited to typed
static member-function lookup and reachable emission: qualified and
unqualified calls, class/base hiding and overload filtering, access, canonical
owner/binding/type continuity, PA15 demand, declaration/definition emission,
recursion, and the raw static ABI.  Static data storage, constructors and
lifetime, operators/ADL, broad initialization, friends/using, and
multiple/virtual inheritance remain outside that review.

The authoritative turn-start full-stage state was `55/243` passed, `188`
failed, and `243/243` covered, from
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The final `make test-pa16` exited `2` at the same `55/243`, with `188`
failures and `243/243` coverage; sorted failure-identity comparison was exact,
with no additions or removals.

The affected ownership path was:

```text
PA10 qualified/unqualified IdExpression or parenthesized call
  -> PA12 typed qualifier/class-scope and MemberLookup selection
  -> first owning class ScopeId + canonical BindingId + raw function TypeId
  -> static-only candidates without an implicit object, or one mixed
     static/non-static member set when a non-static body supplies this
  -> access check at the selected owner; raw static or hidden-object fact
  -> PA15 namespace roots and reached static bodies walk typed call facts
  -> FunctionFactId identity chooses definition demand or declaration plan
  -> LowIR symbol uses the retained owner and raw source parameters
```

### Findings

- The landed helper correctly resolves a qualified class type through
  `lookup_type_path`, uses `member_lookup` so the first direct/base
  declaration set hides later bases.  With no implicit object, it filters that
  set to static functions; in a non-static member body, current/base-qualified
  and unqualified calls retain both static and non-static functions in one
  typed overload set.  PA12 call facts keep the selected raw callable `TypeId`
  for a static winner and the owner-qualified hidden-object type for a
  non-static winner; the inherited owner remains the selected `ScopeId`/
  `BindingId`.  PA15's `function_binding_fact_index_` then follows that same
  identity into a definition or declaration boundary, and
  `function_components`/`abi_function_symbol` retain the declaring owner.
- The mixed-set comparator follows N3485 §13.3.1 and §13.3.3: a static
  candidate's implicit-object ICS1 matches any object but establishes no
  conversion sequence, so it is neither better nor worse than another
  candidate on that dimension.  Object qualification is therefore compared
  only between two non-static candidates; explicit argument conversion ranks
  remain the common ranking criteria for static/non-static pairs.
- The audit found a fail-open class-qualified case: if the selected class name
  had only a non-static function, a type, a blocked name, or no static
  candidate, the old boolean result could reopen ordinary value lookup and
  manufacture a no-object call.  The repair separates “class-qualified name
  claimed” from the static candidate vector, unwraps parenthesized callees,
  and rejects the claimed spelling when no static target exists.  A valid
  current/base-qualified call in a non-static member body instead leaves the
  class claim to the unified member selector, so a viable static or non-static
  candidate can win without the first category suppressing the other.
- Static-body unqualified lookup now searches nearer block/function lexical
  declarations first, then the enclosing class's typed direct/base member set.
  An inherited static function therefore retains its base owner even when an
  outer namespace function has the same spelling; a direct non-static
  declaration still hides base declarations before static-only filtering in a
  static body.  In a non-static body, the first owning set supplies both
  categories to the same argument/object ranking.  The lookup is bounded by
  the scope vector and never reopens an outer value set after a class member
  has claimed the name.  Parenthesized callees use the same typed branches.
- The landed access gate covered qualified static candidates only.  The audit
  adds the same `member_accessible` check after selection for every
  class-owned static binding, including an unqualified static-body call.
  Protected inherited static access is granted by the derived access-class
  proof without a non-static object relation; private or unrelated access
  fails closed.
- The landed PA15 demand branch treated every indexed static `FunctionFact` as
  a definition.  Static declarations can have a valid fact with no body, so
  the repair validates binding/owner identity and distinguishes a complete
  function/body scope (definition demand) from a bodyless declaration
  (declaration demand).  Missing or contradictory definition facts, scopes,
  body ranges, owner records, and callable types fail closed.  Recursive
  static edges still use the existing visited function/fact worklists and are
  emitted once.  Before traversal, PA15 builds one dense
  `BindingId -> class ScopeId` index from class-scope-owned bindings; duplicate
  or out-of-range ownership fails closed, and each reached static fact checks
  that index in O(1) instead of scanning the owner's values.
- The added course regression covers parenthesized qualified and unqualified
  static calls, qualified non-static rejection, both directions of mixed
  static/non-static overload ranking in qualified and unqualified member
  bodies, tied-explicit-rank neutral-ICS ambiguity in each of those spellings,
  inherited static-body lookup against an outer same-spelled function,
  protected/private access, an inherited declaration-only static call,
  same-binding redeclaration identity, recursive demand deduplication, and
  raw parameter-only static ABI.  The existing handout matrix covers
  static/non-static filtering and qualified inherited owner retention.

## Focused Evidence

`make -C dev cppgm++` exits `0`.  The focused handout command

```sh
make -C pa16 check TEST='tests/general/100-static-member-qualified-call.t tests/general/100-static-member-overload-skips-nonstatic-this.t tests/general/200-inherited-static-member-qualified-call.t tests/general/200-static-nonstatic-same-pointer-signature.t tests/general/100-builtin-prefix-static-member-call.t tests/general/100-member-methods.t tests/general/200-protected-base-method.t'
```

exits `0` with `7/7` passing.  Course regressions 401--406 each exit `0`; 402's
`PA12 inherited member name is not callable` line is from its expected
negative case.  Course 406 independently rejects the qualified and
unqualified tied-explicit-rank mixed calls with `PA12 ambiguous member call`.
`sh -n` on the new script exits `0`.  No handout test, fixture, or `.ref` file
changed.

Five measured invocations of the new bounded regression after the neutral-ICS
repair (including its recursive/inherited static demand chain) took
`0.31--0.34s`, with RSS `7,064--7,268KB`; the seven-test handout probe took
`0.20s` and `9,824KB`.  These are representative smoke measurements, not a
formal benchmark.  The
new lookup performs only bounded lexical/class/base walks, and PA15 performs
one class-binding owner-index setup followed by O(1) selected-owner checks
and dense visited worklists; no whole-program retry, textual recovery, or
generated-artifact dependency was added.

The existing `pa16/tests/general/200-out-of-class-member-default-argument.t`
still fails in that tree before reaching the static declaration audit; that
pre-existing default-argument merge gap was not part of the landed static
ownership increment.  The focused declaration control consequently used a
bodyless static declaration without a default argument.  The required
through-PA15 command exited `0` at `1167/1167`; the PA16 file audit exited `0`
with five pre-existing header `bad-division` warnings.  `git diff --check`
passed before the final commit, and no handout test, fixture, or `.ref` file
changed.

## Historical Protected-Access Checkpoint Review

This review covers landed commit `8b445ee6e7b7090a1f2d19edebbc96d756f438ad`
relative to parent `9f158daa4cf5fd8326123ca9ccded1b4c59df382`, plus one
narrow source repair, one lexical-access repair, and the course regression
expansion in this audit.  It is limited to protected member access-scope
handling for field and method expressions: protected static object spelling
uses only the access-class/owner proof, while protected non-static members
also impose the object-expression rule.  Lookup, owner-path lowering,
constructors/lifetime, friends/using, operators/ADL, and other PA16 failures
are not re-audited here; PA15 is traced only where it consumes the affected
typed facts or reports the existing static projection boundary.
The bounded audit is complete and committed, and final validation leaves the
working tree clean.

The complete owned path is:

```text
PA10 MemberExpression/CallExpression or member-body IdExpression syntax
  -> PA12 typed actual-record lookup over class/direct-base scopes
  -> selected BindingId + owner ScopeId + typed base path
  -> actual object TypeId (this, dot object, or arrow pointee)
  -> member_accessible(binding, owner, access scope, object)
  -> semantic MemberExpression/CallExpression fact with selected owner
  -> PA15 typed owner/path validation and ordered base-subobject projections
  -> LowIR field address or non-static call with the hidden object pointer
```

### Findings

- The landed call-site changes carry the actual object into all three affected
  non-static paths: implicit member fields pass the typed `this` record,
  explicit fields pass the dot record or arrow pointee, and selected calls pass
  the normalized dot/arrow object.  The selected `BindingId` and owner
  `ScopeId` remain the semantic facts consumed downstream.
- `member_accessible` walks the bounded lexical scope chain and records each
  class scope from innermost to outermost.  For protected members it obtains
  each candidate's canonical named `TypeId` from the typed `TypeKey` index and
  accepts the first candidate that derives from the declaring owner with
  `member_base_path` and, for non-static members, also has the actual object
  type derived from that candidate.  This gives a nested class inside
  `Derived` the enclosing `Derived` access rights required by N3485 §11.7,
  while same-owner access still returns at the existing scope boundary and
  private/unrelated access remains rejected.
- For protected non-static members, the second proof strips the supported
  reference/cv layers from the actual object and requires a named record whose
  direct-base path contains the access class.  Thus a `Derived` or
  further-derived object is accepted in a `Derived` body, while a `Base&`,
  `const Base&`, or `const Base*` object is rejected.  The check is identity-
  based and does not render or recover a type name.
- The audit found one narrow exception in the landed helper: the new object
  proof was also applied to protected static members.  The repair returns
  after an eligible access-class/owner proof for a static binding, because
  C++'s additional object-expression restriction is only for non-static
  members.  The existing PA15 static-member projection boundary remains
  outside this checkpoint.
- The nested-class probe initially reached `PA12 record member is
  inaccessible` with only the innermost `Nested` class considered.  A
  constructor-free out-of-class `Derived::Nested` member definition reaches
  the same helper without widening PA15 nested-function emission; the lexical
  candidate walk then accepts its enclosing `Derived` access class for both a
  protected field and method through `Derived&`.  The corresponding `Base&`
  object reduction remains rejected by the second proof.  An inline nested
  call still encounters the pre-existing `PA15 direct call target was not
  emitted` boundary, so it is not used as a lowering claim here.
- The lexical walk is explicitly bounded by the scope-vector size and now
  requires an invalid cursor on exit.  A valid out-of-range cursor or a valid
  cursor left after cycle exhaustion fails closed before any collected class
  can grant protected access; ordinary invalid-parent termination and the
  same-owner return inside the walk are unchanged.
- Access is checked after member-call overload/cv selection, and field/call
  facts retain their selected binding and owner.  The existing semantic tail
  guard rolls back failed member-call probes; the new helper is const and its
  path walks use only local vectors, so failed accessibility does not publish
  a fact, demand edge, or fallback ordinary-name lookup.
- PA15 independently validates the selected actual-object-to-owner relation
  and complete zero-offset layouts before emitting each typed base-subobject
  projection.  This preserves the existing fact continuity from PA12 through
  the field/call LowIR consumers.

## Focused Evidence

`sh cppgm.tests/course/pa16/405-protected-object-access-regression.sh` exits
`0`.  Its positive source covers same-owner access, implicit and qualified
`this`, explicit dot and arrow on `Derived`, const-reference and pointer
normalization, dot and arrow on a further-derived object, and both field and
method paths.  It checks the expected typed projection counts and
`@Base__protected_method` calls.  Its constructor-free nested `Nested` member
source accepts both protected field and method access through `Derived&`; the
parallel nested `Base&` source returns `EXIT_FAILURE` with the exact PA12
inaccessible diagnostic.  Its separate ordinary field-through-`Base&` and
method-through-`const Base*` sources also return `EXIT_FAILURE`.

The checked-in protected positive control
`make -C pa16 check TEST='tests/general/200-protected-base-method.t'` exits
`0` with `1/1` passing.  The existing course controls
`401-typed-member-projection-boundary-regression.sh`,
`402-typed-member-call-demand-roots-regression.sh`,
`403-typed-inherited-member-field-regression.sh`, and
`404-typed-implicit-default-demand-regression.sh` each exit `0`; 402's
`PA12 inherited member name is not callable` line is the expected diagnostic
from its negative reduction.  No handout test, fixture, or `.ref` file was
changed.

The permanent course-405 protected-static object-spelling source returns
`EXIT_FAILURE` at the pre-existing `PA15 static member projection is
unsupported` boundary, rather than at `PA12 record member is inaccessible`;
this verifies that the access gate no longer imposes the non-static object rule
on static bindings without expanding the static lowering surface.

The turn-start authoritative log records full-stage PA16 at `49/243` passed,
`194` failures, and `243/243` covered.  The authorized final `make test-pa16`
also exits `2` at `49/243`, with `194` failure identities; sorted identity
comparison against the turn-start log gives added `∅` and removed `∅`, and
the `243`-test inventory remains fully covered (`243/243`).  The earlier
pre-increment history is preserved: the plan records the `48/243` to `49/243`
improvement from removing `pa16/tests/general/200-protected-base-method.t`.
The required through-PA15 command exits `0` at `1167/1167`.  The required
file audit exits `0` with the same five existing header `bad-division`
warnings; no handout fixture or `.ref` file changed.

## Performance and Boundaries

Protected access performs one bounded lexical scope walk of depth `S`, records
`L` class candidates, and performs at most two typed direct-base walks of depth
`D` per candidate; its worst-case check is `O(S + L*D)` (with the ordinary
non-nested case `L=1`).  Same-owner access returns before a base walk, and
protected static access can short-circuit after the owner proof.  Selection
remains bounded by the walked scope/inheritance depths and candidate set; the
new check adds no cache, whole-program retry, textual recovery, or mutation on
a failed probe.  PA15 performs one independent typed owner/layout check.

The representative temporary three-level state-free chain timing sample used
five compiler invocations per size with `/usr/bin/time`: for 1, 128, and 512
local declarations, maximum elapsed times were respectively `0.02s`, `0.01s`,
and `0.02s`; maximum RSS was approximately `5.4MB`, `6.1MB`, and `8.7MB`.
A separate temporary nested-access sample placed 256 protected-field
expressions in one out-of-class nested member and used lexical class depths
`L=1`, `8`, and `32`, with three invocations per depth; all exited `0`, with
maximum elapsed time `0.01s` and maximum RSS `7.4MB`.  These are small
bounded-behavior samples, not formal benchmarks or asymptotic timing claims.
Remaining uncertainties are the pre-existing static and inline-nested-call
lowering boundaries, and unrelated protected typedef/friend/using and
broader PA16 surfaces.

## Historical Previous Checkpoint Review

This review covers landed commit `b1a9e58959cb47835362a654283200831e7b99d6`
relative to parent `25e80541`, plus four narrow audit repairs included in
this checkpoint.  It is limited to direct and inherited unqualified
non-static member calls.  Inherited fields, qualified-base calls,
protected/friend/using access, operators/ADL, constructors/lifetime, virtual
or ref-qualified methods, and general conversion work remain outside it.

The owned path is:

```text
PA10 CallExpression(MemberExpression or unqualified IdExpression) syntax
  -> PA12 typed lexical/class/direct-base lookup and member selection
  -> exact Function-scope implicit-object BindingId as semantic child zero
  -> selected BindingId, owner ScopeId, callable Function TypeId, and args
  -> reachable FunctionFact demand edge
  -> PA15 ABI/owner validation and ordered base-subobject projections
  -> LowIR call with the owner pointer followed by explicit arguments
```

### Findings

- `semantic_call_expression` probes the typed member path before functional
  casts and ordinary direct lookup.  The probe unwraps supported
  parenthesized callees, accepts only a plain unqualified id for the new path,
  and never asks namespace lookup or ADL for member candidates.  Its lexical
  walk checks nearer block/function declaration sets, then the direct class
  and ordered direct-base declaration sets.  At every set the value graph is
  probed before the type graph, so a same-scope ordinary method hides a
  same-spelled class/enum tag.  A value-owned class/base set suppresses
  unrelated enclosing candidates.  A base-owned value set with no supported
  non-static method is blocked by its nonempty typed base path, while a
  `ValueRef` origin from an unsupported import returns explicit `Blocked` and
  cannot silently reopen outer value lookup; nearer lexical/direct-class values
  retain the ordinary resolver's existing fallback.  A using-view remains with
  the ordinary resolver.  A type-only first set returns an explicit typed `TypeId` outcome and is
  consumed by the existing functional-cast producer, so it cannot silently
  reopen outer value lookup.  The separate type probe uses a fresh lookup
  generation after the value probe.
- `member_function_candidates_in_scope` retains only ordinary non-static
  functions from the selected class scope.  The selected `BindingId` and
  `ValueRef` owner remain canonical; the callable `Function TypeId` is built
  with the selected owner and its cv-qualified hidden object pointer.  Object
  qualification is checked before the existing explicit-argument conversion,
  default, overload, access, and deleted-function logic.  Equal best choices
  remain ambiguous rather than depending on traversal order.
- `prepare_pa12_member_parameter` owns one synthetic first parameter and its
  exact `BindingId` in the member Function `Scope`.  The unqualified helper
  now passes the already-validated `BindingId` into `semantic_this_expression`,
  rather than resolving the enclosing `this` binding a second time.  Thus the
  successful call has one stable implicit-object fact at child zero, followed
  by converted/defaulted explicit arguments.
- Inherited fields remain deferred, but an inherited value declaration set is
  still owned by the first base scope that contains the spelling.  Empty
  non-static member candidates and unsupported imported-base value origins are
  therefore blocked by the typed base ownership signal and fail closed;
  ordinary outer/ADL lookup cannot be reopened.  A nearer lexical or
  direct-class value still reaches the existing ordinary resolver, preserving
  its direct/static behavior.
- `direct_base_chain` walks `NamedRecordId` edges with Floyd cycle detection.
  The audit repair validates every class-scope back-reference, rejects a
  non-invalid base on `has_base == false`, rejects virtual and union base
  metadata, and preserves the existing single-direct-base parser rejection.
  The semantic path is bounded by inheritance depth and is passed to the
  shared selector without a second semantic walk; same-owner conversion stays
  constant-time.
- A successful typed member call is the only member demand edge.  PA15 follows
  its selected binding through the existing `FunctionFactId` index, validates
  the selected class owner and hidden ABI, and plans declaration-only members
  from the typed callable boundary.  It scans reachable facts once with typed
  worklists; failed guarded probes publish no fact or demand edge.
- `lower_call` independently reconstructs the actual-object-to-owner path as
  a safety check.  For every edge it validates the current class relation and
  a complete `RecordLayout` whose direct base is the expected record at offset
  zero, then emits ordered `IPK_BASE_SUBOBJECT` projections.  Dot takes one
  address and arrow one pointer expression; free and indirect calls retain
  their existing lowering paths.

### Historical Focused Evidence

The six-test handout probe
`make -C pa16 check TEST='tests/general/200-inherited-member-call-hides-outer-type.t tests/general/200-implicit-member-call-suppresses-adl.t tests/general/200-member-call-implicit-this-cv-overload.t tests/general/200-local-class-direct-init-inherited-member-call.t tests/general/200-parenthesized-member-call.t tests/general/200-single-inheritance.t'`
exits `2` with `1/6` passing.  The inherited outer-type control passes.  The
five remaining failure identities are unchanged prerequisite blockers:
`200-implicit-member-call-suppresses-adl.t`,
`200-member-call-implicit-this-cv-overload.t`,
`200-local-class-direct-init-inherited-member-call.t`,
`200-parenthesized-member-call.t`, and `200-single-inheritance.t`.

The focused control
`make -C pa16 check TEST='tests/general/100-member-methods.t tests/general/200-inherited-member-call-hides-outer-type.t tests/general/200-member-call-return-type-overload-arity.t'`
exits `0` with `3/3` passing.  The existing course regressions
`400-typed-layout-boundary-regression.sh`,
`401-typed-member-projection-boundary-regression.sh`, and
`402-typed-member-call-demand-roots-regression.sh` each exit `0`.
The extended 402 script asserts typed LowIR ownership for a base tag/method
collision (zero-offset projection and `@Base__f`) and for direct and inherited
type-only first declaration sets (typed zero cast in `Derived__call`, with no
outer `@f` call).  It also rejects an inherited `Base::f` data member in
`Derived::call` and confirms that no outer `@f` call is emitted.  `sh -n` over
those scripts and `git diff --check` also exit `0`.

Bounded stdin reductions (not additional suite coverage) compile successfully
for direct, inherited, and parenthesized unqualified calls.  The inherited
and parenthesized outputs each contain one zero-offset
`projection=base_subobject` before the base call; a three-level reduction
contains two ordered projections before `@A__f`.  The new 402 reductions show
that a same-scope tag does not hide an ordinary base method, while direct and
inherited type-only declarations return the typed functional-cast zero rather
than calling the unrelated outer function.  The inherited non-callable-value
reduction fails closed rather than reaching the outer function.  A nearer block
variable named like
the method still fails at the local non-callable target and does not fall
through to the base method.

The required through-PA15 command
`n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
exits `0` with `1167/1167` passing.  The required file audit
`perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0` with
these five warnings: `dev/src/abi_mangle.h:1`,
`dev/src/cpp_semantic_core.h:1`, `dev/src/lowir_model.h:1`,
`dev/src/pa11_semantic_model.h:1`, and `dev/src/pa15_lowering.h:1`, each
`bad-division` for a substantial implementation body in a header.
`make test-pa16` exits `2` with `48/243` passing, `195` failures, and
`243/243` coverage.  Comparing the exact failure identities with the
turn-start map in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` gives
added set `∅` and removed set `∅`; all `195` identities are unchanged.  No
fixture, reference, or coverage identity was changed.

### Historical Performance and Boundaries

The lexical/type/value probes inspect only the walked scopes.  Base metadata
validation and lookup are bounded by inheritance depth; same-owner conversion
returns before a base walk.  For `C` candidates, `A` explicit arguments, and
`P` parameters, member viability/ranking remains approximately
`O(C * (P + A))`.  PA15's typed function/fact worklists scan each reachable
node once, and lowering performs one independent bounded path/layout check.
No whole-program retry or textual recovery was added.

No timing, RSS, allocation, or structural-counter measurement was collected;
these are structural bounds only.  Unsupported inherited type construction
and inherited non-callable value calls are fail-closed, while the focused
scalar type-only cases use the existing functional-cast producer.  The
remaining uncertainties are the unchanged
`195`-identity PA16 failure map and the explicitly deferred
protected/friend/using, inherited-field, qualified-base, static,
constructor/lifetime, virtual/ref-qualified, operator/ADL, and broader
conversion slices.

## Audit ledger

| checkpoint | result and disposition |
| --- | --- |
| `15e9897b` effective-using visibility and typed call-publication checkpointAudit | Final bounded audit of landed `15e9897bc038499f724d69cb3cfe70e806b9fb36` relative to its parent: common-ancestor effective-using registration, canonical lexical owner/source-point filtering, NamePath lookup, typed one-argument overload publication, and the existing PA15 narrow class-value boundary are traced. The directly caused deferred-PA12 local source-order leak is repaired with RAII lookup-point contexts at statement/call and nested identifier owners in the four approved sources. Focused PA16/PA12/PA15 matrices pass `12/12`, `6/6`, `6/6`; final PA16 is `219/243` with the exact supplied 24-failure set, fresh-only `0`, authority-only `0`, and `243/243/243` inventories; through-PA15 is `1167/1167`; file audit passes with five known warnings; exact six-path and artifact/coverage checks pass. PA16 remains incomplete with the same residual 24. No tests, fixtures, references, harnesses, comparators, generated outputs, coverage rules, or source-set files changed. |
| `08472cce` typed pragma-pack record-layout checkpointAudit | Bounded audit of landed `08472cce8e96daa585f5f07f4ee9d2233e13ade9` relative to `0ff3fdef`: the shared preprocessing cap/stack, include and inactive-conditional behavior, ordered typed `PPPackDirective`, token-transparent posttoken handoff, PA10 whitespace-free boundary, PA11 binary-search lookup, `NamedRecord` cap, and canonical member/base/bit-field/final layout are traced. The audit repairs the wide-bit-field path to use capped storage alignment and makes raw/PA10 typed-fact validation reject invalid operation/state/order/boundary data, including facts after EOF; PA11 also checks the operation domain. Focused build, course 422 (`sh -n` plus execution), packed/natural/conditional/alignas/pop/string probes, and the temporary typed-buffer rejection probe pass. Supplied latest and fresh authority are both `210/243`, `33` failures, `243/243` covered; the durable exact comparison reports fresh-only `0`, authority-only `0`, inventory `243`, and unexpected `0`. The known `300-pragma-pack-followed-by-endif.t` LowIR trunc-before-zext shape remains unrelated. Through-PA15 is `1167/1167`; its durable transcript is `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-through-pa15-20260830.log`. File audit exits `0` with five known warnings and no fatals; its durable transcript is `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-file-audit-20260830.log`. Diff-check passes. No handout, fixture, reference, harness, source-set, or generated output changed; course 422 is the sole added public regression. PA16 remains incomplete. |
| `ee8f44d5` per-throw typed array cleanup checkpointAudit | Completed bounded audit of `ee8f44d5b0e9d4910679c12b443533d787d1cd4c` relative to `3b2b4882`: PA12's typed constructor-action range is traced through action/placement `ArrayAddressRoot`, forward recursive terminal collection, combined constructor-boundary plus typed-argument throw classification, independent per-throw LowIR handlers, fresh root/path replay, reverse cleanup, and canonical `model_.destructor_binding(record)` calls. The approved repair in `dev/src/pa15_lowering.h` and `dev/src/pa15_lowering_construction.cpp` adds exact action target/type/range validation, action/storage root replay identity checks, and existing cached semantic nothrow classification for every actual argument; valid unproven arguments remain potentially throwing and malformed facts fail closed. Supplied authority is `209/243`, `34` failures, `243/243` covered; exact fresh comparison is authority `34` -> fresh `34`, authority-only `0`, fresh-only `0`, inventory `243`, covered `243`, missing `0`, unexpected `0`. Focused matrix is `30/35`; typed argument probes distinguish zero-handler known-nothrow/no-argument cases from one-handler potentially throwing arguments and constructors. Semantic argument-vector array reachability is rejected by PA12 before lowering for current grammar. Representative nested N=6 evidence is 11 blocks, 5 handlers, 5 resumes, and 15 cleanup destructors; through-PA15 is `1167/1167`; file audit and diff-check pass. The exact four-file audit/repair is complete; PA16 remains incomplete only because the same 34 residual identities remain. |
| `70327e4d` typed destructor suffix cleanup checkpointAudit | Completed final bounded audit of `70327e4d72ad5d223018565ec78d290ea4ac6f0a` relative to `a3de5c21`, including the approved repair in `dev/src/pa15_lowering_construction.cpp`: PA12's canonical `FunctionFact.destructor_action_begin/count` and `DestructorActionFact` ownership are traced through PA15 demand, active destructor record/`this`, scalar and reverse-array address replay, normal suffix prefixes, body-unwind cleanup, and return/local-lifetime ordering. The repair adds fail-closed target/type/canonical-destructor checks and rejects non-void destructor call signatures in the declared construction path. Fresh post-repair `make test-pa16` is exit `2` at `208/243`, with exactly `35` failures and `243/243` coverage; exact sorted comparison with the turn-start authority is `35 -> 35`, fresh-only `0`, authority-only `0`, and unrecognized `0`. The preserved pre-landed baseline is `206/243` with `37` failures; its exact two baseline-only destructor/lifetime identities remain fixed and no current-only identity appeared. The exact prior gate is exit `0` at `1167/1167`; the file audit is exit `0` with five known header-division warnings; diff-check and the bounded changed-file audit exit `0`. Focused post-repair evidence is `5/7`, with only the two known array-presentation mismatches. Durable fresh logs are listed in the Current Checkpoint Review above. No tests, fixtures, references, sidecars, harnesses, comparators, coverage rules, source sets, or generated oracle files changed; the checkpoint record and approved source repair are complete. |
| `d83e927f` typed local-class materialization checkpointAudit | Completed the bounded audit of `d83e927fd18429d37c3818a80e295f0a7c521905` relative to `d95a6fe7`: PA11/PA12 typed `DeclarationFact`/`ConstructorAction` ownership reaches both PA15 declaration consumers, which materialize one automatic-local class address; the narrow class-value path suppresses only the redundant automatic-local pre-copy address and retains later source addressing. The audit repaired the missing automatic-storage guard by centralizing the keyed declaration-owner predicate; namespace/static owners remain nonautomatic. Supplied authority and fresh result are both `206/243`, `37` failures, `243/243` covered, with exact sorted comparison baseline-only `0`, final-only `0`, and failure set exactly unchanged. Focused PA16 is `13/13`, focused PA15 controls are `2/2`, the valid automatic class-value probe is accepted by current/reference observers, and structural counts are `76/25`, `412/129`, `19/2`, and `14/2`. Fresh `make test-pa16` exits `2`; the exact `n=16` prior-stage gate exits `0` at `1167/1167`; `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0` with five known header-division warnings and no fatals. Durable final logs are listed in the Current Checkpoint Review above: test, prior gate, file audit, exact identity comparison, changed-file audit, and diff-check. No tests, fixtures, references, or harness surfaces changed. Audit/repair commit at current HEAD; handoff hash in final report. |
| `7e060b28` typed packed bit-field projection checkpointAudit | Final bounded audit of the five landed PA15 lowering owners: canonical `BindingId`/`BitFieldFact` continuity, one contiguous `ProjectionId` arena, direct/pointer/evaluated-root/constructor-this replay, typed load/encode/RMW preservation, signed reads, and single-evaluation aggregate/constructor order are traced. No implementation defect was found and no source repair was made. Fresh `make test-pa16` is `202/243` with `41` failures and `243/243` coverage; exact comparison with supplied `last-test.log` is `41 -> 41`, baseline-only `0`, final-only `0`. Through-PA15 is `1167/1167`; file audit and diff-check exit `0`; focused audit is `2/6` and fresh non-equivalent exploratory selection is `6/11`. Landed source `7e060b28`; audit commit at current HEAD (handoff hash). Next checkpoint selects a separate residual and preserves the typed owner invariants. |
| c39d4563 plus typed canonical-truth finalizer checkpointAudit | Completed the bounded audit of c39d45634bb029a02c938c190f8ac703bd275050 plus finalizer hardening and the behavior-preserving structural extraction: PA12 builds all retained facts/bodies once, then one ephemeral dense ResultNodeId graph applies explicit result edges, conservative BindingId may-provenance, canonical declaration-to-definition call mapping, and a convergent recursion worklist. PA12 publishes bool-source Preserve per ConversionFact, including bool-to-int; PA15 resets LoweredValue from each conversion so policy is non-sticky. Generic child propagation, BindingSidecar taint, deferred ambient call state, latest-fact scans, dynamic dependency layers, and diagnostics are absent. The private CanonicalTruthFinalizer separates checked domain/edge construction, propagation, and publication; the finalizer methods are below the 240-line limit and changed source lines are unpacked. Clean build and protected five are 5/5; fresh focused outputs match the pre-extraction outputs; final PA16 is 199/243 with the exact unchanged 44-failure set and 243/243 coverage; through-PA15 is 1167/1167; file audit passes with five known warnings; no new failure or coverage delta. |
| `b3bbf052` typed non-owning namespace object checkpointAudit | Completed bounded audit and repair of the landed increment relative to `68b549f2`: PA11 canonical owner/type/definition facts flow through PA12 typed references, conversions, and address targets into PA15 pointer-vs-owned LowIR and demand-rooted global emission. Incomplete class references/glvalues remain non-owning pointers; namespace declaration-only objects require typed demand; definitions, class-static objects, address targets, nested/internal/TLS cases, and fail-closed owner/range checks remain covered. Post-repair `make test-pa16` is `189/243` with `54` failures and `243/243` coverage; comparison with the landed 54-failure authority is baseline-only `0` and final-only `0`. The 16-test handout matrix is `16/16`, the required through-PA15 command is `1167/1167`, the required file audit exits `0` with five known header-division warnings, and determinism probes are byte-identical. The direct incomplete namespace-object address case is out of contract because PA16 scopes namespace object declarations to complete class types; the unrelated course-400 DMI mismatch remains outside this increment. No handout, fixture, reference, harness, comparator, coverage, source-set, or unrelated file changed. |
| `a5b496e8` typed ordinary-value-over-tag/member-enumerator/member-call checkpointAudit | Completed bounded audit of the landed increment relative to `1093c2b7`: PA11 preserves independent typed identities, but cross-space using coexistence is limited to a canonical real class/enum tag (`BindingKind::Type` backed by `NamedKind::Class`/`Enum`), while typedef/alias value conflicts remain rejected; PA12 member ambiguity and ordinary calls share the complete typed member selector; enumerator facts retain canonical binding/owner/type/value and one object-evaluation child through PA15. The approved follow-up is a readable, line-neutral `process_using_declaration` refactor at exactly `3000` lines, merging redundant value validation/classification/dedup staging work and using `base_path_accessible` as the single canonical relation/access walk; no newly added follow-up line exceeds `118` characters. Final PA16 is `187/243` with `56` failures and `243/243` coverage; v4 sorted comparison with the turn-start `last-test.log` has baseline-only `0` and final-only `0`, preserving stage progress. Focused handout matrix is `8/8`; course 421 has four legal status-0 cases and four exact status-1 alias conflicts. Through-PA15 is `1167/1167`; file audit passes with five warnings; diff-check passes. Course 406 reproduces the same first qualified-static-call failure on current and clean `a5b496e8` (status 1) before the shared selector and remains outside the bounded ownership path. Final v4 logs and exact-set derivation are recorded in the current review. No handout, fixture, harness, comparator, coverage, source-set, or unrelated source changed. |
| `a1a2cf83` typed non-automatic lifetime checkpointAudit | Completed bounded audit of the landed typed lifetime path relative to `c2247924`: canonical BindingId/TypeId and declaration-owned initializer/lifetime continuity, PA11 exact per-declarator definition flags, namespace/static-member storage, TLS mode separation and collision-free helpers, aggregate/local recursive actions, source-order initialization, and reverse-order destruction are traced. The audit repairs typed PA11-to-PA12 definition continuity and PA15 definition-owner retention, preventing bodyless-extern duplicate lifetime publication and redeclaration source-order drift. The five fixed identities pass `5/5`; course controls 404, 407, 409, 410, 415, and the new 420 regression pass; the relevant handout matrix is `9/12` with the same three LowIR-shape residuals. Final PA16 is `184/243` with `59` failures and `243/243` coverage; the exact sorted comparison has baseline-only `0` and final-only `0`. Through-PA15 is `1167/1167`; file audit passes with five known header-division warnings; diff-check passes; final logs and N=8/N=32 hashes are recorded above. No handout, fixture, reference, harness, comparator, or unrelated stage surface changed. |
| `135e3a95` typed access-control checkpointAudit | Completed bounded audit and repair of landed `135e3a95` relative to `0fb73ad4`: canonical owner/access, direct friend identity, paired using view/publishing scope, typed qualified type/value and base access, source declaration accessibility, private/protected/friend/protected-object rules, and PA15 per-edge projection are traced. The type using fix preserves canonical `TypeId` while recording the introduced declaration/access owner; public class-member type/value using remains supported at namespace or block scope, inaccessible sources are rejected by the p17 access boundary, and namespace-to-namespace using remains valid. Operator access propagation is traced through candidates; unrelated operator behavior is out of scope. Final PA16 is `179/243` with `64` failures and `243/243` coverage; exact comparison has final-only `0` and baseline-only `0`. Focused PA16 is `12/14` with two checked-in LowIR residuals, courses 405/411/419 and `sh -n` pass, and structural noise evidence is recorded. Through-PA15 is `1167/1167`; file audit exits `0` with five known warnings. No handout, fixture, reference, harness, comparator, or exit-status file changed. |
| `30d69fc3` inheriting-constructor checkpointAudit | Completed bounded review of the landed PA10--PA15 inheriting-constructor path relative to `05c36f56`: typed relation/source-point ownership, direct/transitive candidate selection and access, reference parameters, complete/base-entry ABI identity, external declarations, demand/slot behavior, and structural noise bounds are traced. The repair is N3485-correct for trailing defaults: each allowed nonzero arity gets a distinct shortened typed wrapper with no inherited default facts, omitted typed defaults reach only the full-signature base entry, and no-argument construction follows the separate implicit-default path. Derived DMI/runtime member actions use one shared declaration-ordered typed owner, with local action/argument ranges and base-before-member order. Recursive expansion now materializes relevant immediate-base typed relation candidates before each derived scan, validates identities, bounds the active single-inheritance walk, and makes hard-only transitive discovery independent of prior `Soft` construction while remaining demand-driven. Focused build, seven selected handout identities (`7/7`), course 403/408/409/418 including the hard-only control, strict structural probe, and diff-check pass; the probe remains deterministic and byte-identical at 68 lines/hash `02de5e72...d78592`. Final PA16 is `176/243` with `67` failures and `243/243` coverage; exact sorted failure sets match the turn-start map with no final-only identities. Durable broad logs are `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-inheriting-constructor-transitive-audit-final-20260829.log` and `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-inheriting-constructor-transitive-audit-identity-compare-20260829.log`. Through-PA15 is `1167/1167`; file audit passes with five known header-division warnings. No handout, fixture, `.ref`, harness, comparator, or source-set file changed. |
| `3b7d8e6a` qualified-type checkpointAudit | Bounded audit of the PA10 qualified-component/decltype-root path and the PA11 source-point-aware typed owner: delimiter/template/rshift bounds, qualified cast and elaborated-header routing, injected identities, inherited typedefs, validated direct-base traversal, and PA12/PA15 typed consumption are traced. The audit repair extends the existing PA12 functional-cast target owner to unwrap parenthesized qualified callees, perform ordinary typed value lookup before typed type lookup, and distinguish `(N::T)(0)` from `(N::f)(0)` without spelling reconstruction. The authoritative result is `173/243` passing, `70` failures, and `243/243` coverage versus parent `167/243`, `76` failures; exact comparison has six baseline-only identities and final-only `∅`. Focused build, six-repair/six-control evidence, and course control 417 pass; broad PA16 exits `2` with the same `70` residuals, through-PA15 exits `0` at `1167/1167`, file audit exits `0` with five pre-existing warnings, and diff-check passes. No handout, fixture, reference, harness, comparator, or source-set list changed. |
| `d7ed98aa` typed builtin call-boundary checkpointAudit | Bounded audit of the landed PA11--PA15 path: exact fixed builtin identities/signatures, ordinary PA12 typed selection/conversion, append-only `BuiltinFunctionFact` ownership, PA15 demand/declaration planning, and LowIR declaration/call boundary consistency are traced; memcpy and memmove alias facts remain distinct and truthful. The audit repairs lookup bypass for visible typed-builtin spellings and arity-only validation in type-only `decltype` calls, with course 416 covering both. Final PA16 is `167/243` with `76` failures and `243/243` coverage versus `164/243` and `79` failures at the parent; exact baseline-only fixes are the three named metadata identities and final-only is `∅`. Final broad PA16 exits `2` with that expected residual set; through-PA15 exits `0` at `1167/1167`; file audit exits `0` with five known warnings; focused controls and diff-check pass. One known address-of-builtin LowIR mismatch remains. PA16 remains incomplete; no handout, fixture, reference, harness, comparator, or source-set list changed. |
| `dea01c52` aggregate-initialization checkpointAudit | Completed bounded PA10--PA15 aggregate audit and repair: RecordLayout now owns declaration order/indexes, PA12 arena values survive reallocating publication, aggregate facts remain sparse and typed, global/runtime demand visitation is independent, pending global actions preserve source order, and the global aggregate inliner is checked and falls back to demanded helpers when unsupported. Final PA16 is `164/243` with `79` failures and `243/243` identities covered versus the authoritative `159/243` and `84` failures at turn start; the exact delta is five baseline-only repairs and final-only `∅`. The exact focus is `12/17` with `17/17` covered; course 404/409/412/415, through-PA15 `1167/1167`, file audit, and diff-check pass. Final structural replay is preserved in `pa16-aggregate-init-audit-final-v1` with `30/30` zero-status runs, zero repeated-hash mismatches, and no timing/RSS claim. The unknown-bound namespace string-record handout now passes. No handout, fixture, reference, comparator, harness, or source-set list changed. |
| `fb4348b6` typed parameterized class-constructor checkpointAudit | Complete: bounded PA10--PA15 constructor audit repaired canonical hidden-destination callable typing, protected-constructor access, shared candidate owner validation, and aggregate copy/direct-list dispatch for explicitly-defaulted/deleted constructors. The focused constructor matrix is `17/17`; course controls 400--409 pass with syntax checks, including new self-pointer/protected/private and aggregate field/helper coverage. The two aggregate handout controls retain the known LowIR address/bool shape comparison difference. Final PA16 is `91/243` with `152` failures and `243/243` coverage; failure and coverage identity additions/removals are both `∅`/`∅`. Through-PA15 is `1167/1167`; the file audit passes with five pre-existing warnings; diff-check passes; representative scale smoke is recorded above. No handout, fixture, reference, or `.ref` changed. |
| `32c45463` typed class-object construction checkpointAudit | Completed bounded audit of the landed typed construction increment relative to `a2ac5256`: repaired canonical empty named-class constructor identity, fail-closed FunctionFact ownership, value-initialization zeroing semantics, aggregate DMI fallback, typed range/owner/index validation, demand-driven empty-helper elision, and the course-404 ordering controls. Focused copied handout comparison is `10/11`; course controls 400--407 are green; final PA16 is `80/243` with `163` failures and `243/243` coverage, with exact failure and coverage additions/removals `∅`/`∅`; construction stress smoke is five successful `0.00s` runs with RSS `5824--6056KB` (timings in `/tmp/codex-pa16-stress-final.Tn9MSH/stress-1.time` through `stress-5.time`), 14 constructor helpers, 14 constructor calls, 13 base projections, 45 field projections, and 45 stores. Through-PA15 is `1167/1167`; the file audit passes with five pre-existing header-division warnings; no handout, fixture, reference, or `.ref` changed. |
| `2f130396` typed static-data storage/access checkpointAudit | Completed bounded audit/repair: canonical direct class-owner merging, inherited/nested typed owner retention, initializer-fact preservation, demand-aware class-static/TLS emission, access checks, exactly-once static object evaluation, and PA12 fail-closed class claims are traced and repaired. `make -C dev cppgm++`, course controls 400--407, the focused probe, and exact through-PA15 gate pass their bounded criteria; full PA16 is `61/243` with `182` failures and `243/243` coverage, with failure-identity additions/removals `∅`/`∅`. The file audit exits `0` with five pre-existing warnings; no handout or reference changed. |
| `021ef639` typed static member-function lookup/reachable-emission checkpointAudit | Completed bounded audit/repair: class-qualified lookup fails closed, current/base-qualified and unqualified member-body calls rank one mixed static/non-static set, static-body lookup preserves inherited owner/hiding, access and raw-vs-hidden-object facts remain typed, and PA15 uses a dense class-binding owner index with O(1) selected-owner checks. The focused handout matrix is `7/7`, course controls 401--406 exit `0`, final PA16 is `55/243` with the exact turn-start `188` failure identities and `243/243` coverage, through-PA15 is `1167/1167`, and the file audit passes with five pre-existing warnings. |
| `8b445ee6` protected object access scope checkpointAudit | Completed and committed bounded audit/repair: static protected object spelling stops after the typed access-class/owner proof, nested protected access considers eligible enclosing class scopes while retaining the non-static object proof, and malformed valid scope ancestry fails closed after the bounded walk. Course 405 covers the field/method matrix, nested `Derived&`/`Base&` controls, and the exact existing PA15 static boundary. Final PA16 is `49/243` with `194` failures and `243/243` coverage, with exact failure additions/removals `∅`/`∅`; through-PA15 is `1167/1167`, the file audit passes with five pre-existing warnings, and the final working tree is clean. |
| `b1a9e589` direct + inherited unqualified member-call checkpointAudit | Bounded audit completed with four narrow fixes: exact synthetic-`this` BindingId reuse, value-before-type lookup with explicit `Type`/`Blocked` outcomes, fail-closed direct-base metadata validation, and inherited value-set ownership blocking. Direct/inherited/parenthesized reductions, the three focused controls, and course regressions pass; the six-test handout probe remains `1/6` on the same five prerequisite identities. Through-PA15 is `1167/1167`, the file audit exits `0` with five pre-existing warnings, and full PA16 is `48/243` with `195` failures and `243/243` coverage, with zero failure-identity additions or removals. |
| `37265733` typed member projection audit/repair | Direct/nested dot and arrow ownership is traced through PA12, PA11 `RecordLayout::member_offsets` keyed by the object's canonical `NamedRecordId`, and PA15 LowIR; the reference-cv and class anonymous-injection defects are repaired. Broad validation and exact identity/coverage checks pass their bounded invariants; PA16 remains incomplete with the existing 205 failures. |
| `0b534f2f` typed direct member-call checkpointAudit | Completed bounded audit/repair: implicit-object cv subset ranking, N3485 variadic comparison, single-owner typed reachable member demand, dense PA15 reachability metadata, declaration-only member declarations with hidden-object/cv ABI boundaries, hidden-object call formation, and source-file sizing are repaired. Focused PA16/PA15 controls and all relevant course regressions pass; through-PA15 is `1167/1167`, the file audit passes with five pre-existing warnings, and full PA16 remains `47/243` with `196` failures and `243/243` coverage, with zero failure-identity additions or removals. |
| `0a6be82d` typed fixed-bound local/synthesized array lifetime checkpointAudit | Completed bounded audit/repair: typed lifetime ownership and destructor continuity are validated once, dense `ScopeId` flags replace the former per-function lifetime scan, checked array paths/actions and arena-safe recursive cleanup are retained, and lexical/control-exit/EH state is covered by course 410. Final PA16 is `93/243` with the exact turn-start `150` failure identities and `243/243` coverage; through-PA15 is `1167/1167`; the file audit passes with five existing warnings; diff-check passes; current structural and interleaved smoke/scale evidence is recorded above. |
| `2d93a5e9` ordinary non-template overloaded-operator checkpointAudit | Completed bounded audit/repair of the `20f14d30` -> `23a26df5` implementation span as tightened at `2d93a5e9`: the follow-up corrects exact friend-definition lexical ownership and typed private/protected/public base-reference accessibility while retaining enum identity/promotion ranking, narrow converting-constructor participation, reference/address facts, and typed bool boundaries through PA10--PA15. Final PA16 is `127/243` with `116` failures and `243/243` coverage; exact comparison to the `122/243` turn-start map has five baseline-only repaired identities and zero final-only identities. Through-PA15 is `1167/1167`, final file audit has five known warnings, focused status is `29/32` with three documented pre-existing holdouts, course 411 passes, and state-matched performance is in `pa16-operator-perf-followup-v5` with final/immutable SHA-256 `e5ffb4e9869c619552f193e16ef063ab2feba7c27f809887ebdd187960196580`. No handout, fixture, reference, comparator, or generated output changed. |
| `da4252b6` typed bit-field boundary checkpointAudit/follow-up | Completed bounded PA10--PA15 audit and repair: canonical typed operation/promotion facts, const-reference temporary ownership, semantic-owner rejection of invalid bit-field references and bool decrement, overload-before-address-of ordering, mixed/zero-width/unnamed/union layout, checked oversized allocation spans, masked signed/unsigned PA15 projection, and isolated initialization roots. Final PA16 is `131/243` with `112` failures and `243/243` identities; exact comparison to the turn-start `112`-failure map is baseline-only `0`, final-only `0`. Course 412, direct alias control, through-PA15 `1167/1167`, file audit, and diff-check pass; the focused bit-field matrix is `5/11` with six documented LowIR mismatches. Corrected state-matched bit-field performance is in `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-bitfield-perf-final-v1` with 30/30 zero-exit runs, 32-owner/544-declaration/832-use scaled counters, final/immutable SHA-256 `c98edbf143904e0b09b451310de38e7966149b4374ad912b55a1b9f8c96aaf02`, and final wall medians `0.00/0.07/0.00s` for small/large/nested cases. No handout, fixture, reference, comparator, or generated output changed. |
| `9718b987` member-function-definition declarator audit/follow-up | Final audit/follow-up: the out-of-contract special-member widening is reverted to the parent class-scope-only behavior, while explicit auto-placeholder state and typed/fail-closed ordinary declarator validation are complete. Final `make test-pa16` is `132/243` with `111` failures and `243/243` identities; the exact baseline/final failure sets are identical with baseline-only `∅` and final-only `∅`. Through-PA15 is `1167/1167`; course 413 passes, the focused matrix is `5/7`, the constructor-member-init control is `1/1`, file audit passes with five pre-existing warnings, and diff-check passes. The excluded nested out-of-class constructor fails closed and is not PA16 coverage; next is a later residual audit, not completion. |
| `4efddaae` typed single-inheritance standard-conversion checkpointAudit | Complete: typed endpoint, access-scope, and path ownership is retained from PA12 publication into PA15; the typed comparator enforces standard > `UserDefined` > `Ellipsis`, leaves user-defined/user-defined first-standard ranks incomparable, and preserves standard legacy plus derived distance/cv ordering. Member-object cv subset ordering, malformed-record bounds checks, final-fact scope-range validation, and the strengthened course-414 operator regression are repaired. Comparator bodies are owned by `pa12_semantic_calls.cpp` while declarations remain in `pa12_semantic_selection.h`, restoring the prior file-audit warning set. Final PA16 is `144/243` with `99` failures and `243/243` identities covered; exact comparison with the turn-start map has baseline-only `∅` and final-only `∅`. Focused conversion is `8/10`, access/rank/parser controls `7/9`, and PA15 conditional controls `2/2`; the residual identities are documented above. Through-PA15 is `1167/1167`; file audit exits `0` with five header-division warnings; diff-check exits `0`. Final-v3 immutable replay is 9 cases x 2 with 18 expected-hash matches and zero pair mismatches; frozen compiler SHA-256 is `5347a2abb876d9492501f70e6fa8fa9f6d3c27f2da0c35283f702d4a2652ab81`, current compiler SHA-256 is `d1352cd1c16bcd58587ee9ad201a56665819e671933db979c8df1aea6124c41b`. |
| `fb4f46ed` placement-new checkpointAudit completed | Completed bounded audit of landed `fb4f46ed64ea8c5743fd4395fe1a8c43112836c3` relative to `9f7101ac`: PA10 placement facts flow through PA12's typed allocation `CallExpression` and `ConstructorAction` into one PA15 allocation call, destination construction, and the same pointer result. The audit repairs the over-broad array-pointer cast special case, rejects array and non-named placement targets before publication, checks exact `size_t`/`void*` allocation signatures, and hardens PA15 owner/range/type/callable/hidden-destination/physical-pointer invariants. For both supported target shapes, allocation is emitted once before construction and a throwing constructor propagates through the existing call boundary; neither target selects or declares matching placement deallocation, while delete/placement-delete lookup and cleanup remain outside this checkpoint. Course 423, aggregate placement `PASS (1/1)`, getter-owner `PASS (1/1)`, valid/invalid direct probes, and the required build pass; the constructor target retains only the known unrelated truth-width LowIR mismatch. Supplied authority and fresh result are both `211/243`, with `32` failures and `243/243` coverage; exact comparison is `fresh_only=0`, `authority_only=0`, and inventory `243`. The through-PA15 gate returned `0` at `1167/1167`; file audit returned `0` with five known warnings; final diff-check and clean commit gates passed. No handout, fixture, reference, harness, comparator, or source-set file changed. PA16 remains incomplete. |
| `96e80152` truth-width checkpointAudit | Bounded review of the landed PA12/PA15 typed-truth continuity increment: all relevant bool call owners agree on result metadata; cmp keeps actual operand type; typed member/size/class-object-pointer truth preserves the i64 carrier while plain procedural and bool storage boundaries materialize. The audit repairs the class-pointer classifier to require an exact cv-stripped `Named` pointee, preventing `Class (*)[N]` overclassification. Focused build is `0`, PA16 controls `7/7`, PA15 controls `5/5`, and direct Class*/Class(*)[2]/scalar-pointer probes pass. Fresh PA16 status is `2` at `214/243`; authority/fresh failures are `29/29`, baseline-only/fresh-only are `0/0`, and coverage is `243/243`. Through-PA15 is `1167/1167`; file audit is `0` with five pre-existing warnings and no fatal finding; final diff/path audits are `0`. No handout, fixture, reference, harness, comparator, or source-set file changed; audit completed. |
| `a5c8e166` typed packed-bit-field value/update checkpointAudit | Final bounded review of `a5c8e1664e5059e2453e3252021f3843d0ab23b6` relative to `7f4fe2d4`: PA10 declaration-specifier tokens, PA12 `BitFieldFact` declared/storage/operation/width/mask/signedness, PA11 layout, and PA15 typed projection/replay, extraction/conversion, encode/RMW store, aggregate initialization, and prefix/postfix ownership are traced. The audit repairs full-width plain-`int` promotion to follow the selected unsigned fact while preserving narrow `int` promotion and explicit signed/signed-enum behavior. The copy-suppression flag is proven to omit only redundant bit-field publication; non-bit-field, floating, bool, pointer, reference, and value-category materialization remains. Direct aggregate, nested, and constructor roots are bounded to storage-address, pointer-value, and pointer-load replay respectively; initializers are single-evaluated and packed neighbors are preserved. Focused build, course 412/422/424, probes, and diff-check pass; the 10-test matrix is `7/10` with only the known prefix and two signed-read oracle tensions. Final PA16 is status `2` at `215/243`, exactly `28` failures and `243/243` covered; independent authority/fresh failures are `28/28`, authority-only/fresh-only `0/0`, inventory/run total `243/243`, and unexpected failures `0`. Through-PA15 is status `0` at `1167/1167`; file audit is status `0` with five known warnings; durable evidence is under `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-plain-int-bitfield-checkpoint-audit-20260830/`. No handout, fixture, reference, harness, comparator, coverage surface, or source-set file changed; the sole added regression is course 424. |
| `1d7e6860` alias direct-base mem-initializer checkpointAudit | Final bounded audit of landed `1d7e68605aec65b6976772ff117a433cbc232749` relative to `727417db`: N3485 unqualified lookup/member-hiding, canonical alias resolution, injected-name fallback, source-point/access context, duplicate/malformed handling, typed action ranges/order, and PA15 owner/type/layout consumption are traced. The repair applies member-before-type classification and exact named-type matching; steering additionally hardens BindingId bounds before sidecar access and keeps blocked-value lookup single-read. Course 425 covers direct base-name/alias-name hiding, the inherited non-constructor collision, duplicate detection, array-alias rejection, and nested-type hiding. Final PA16 is `216/243` with `27` failures and `243/243` covered; exact authority/final comparison is `27/27`, authority-only/fresh-only `0/0`, and missing artifacts `0`. Focused handout `6/6`, courses 408/409/418/425, build, syntax, reductions, smoke evidence, through-PA15 `1167/1167`, file audit `0` with five known warnings, diff-check, and bounded path audit pass. No handout, fixture, reference, harness, comparator, generated output, coverage rule, or source-set file changed. |
| `31a938ac` typed aggregate value-initialization compact zero-store checkpointAudit | Final bounded audit of landed `31a938ac01fdc9b5b5f4c625ecf0658d77284504` relative to `d503a9c0`: PA12 value-initialization/synthetic-constructor facts, canonical PA15 `TypeId`/layout, byte-complete 8/4/2/1 scalar clearing, low-alignment x86_64 safety, declaration-ordered nested construction, array paths, and existing cleanup boundaries are traced. The landed alignment-width repair is sufficient; no additional source or regression repair is made. Fresh PA16 is `217/243` with `26` failures and `243/243` covered; exact comparison against the current 26-failure authority is `26` vs `26`, fresh-only `0`, authority-only `0`, and no missing artifacts. Through-PA15 is `1167/1167`; file audit is `0` with five known header warnings; diff-check is `0`; only the two approved documentation paths changed. |
| `ab1b2a8c` source-point-aware associated ADL checkpointAudit | Final bounded audit of landed `ab1b2a8c4a20752434d608b5aef04ef328e5fe5e` relative to `a9728454` plus its approved follow-up: ordinary definition-`SourcePoint` lookup, ADL suppression/union, typed associated class/enum/direct-base/enclosing records, recursive cv/ref/pointer/array/function result-and-parameter association, first-namespace and inline closure, direct using-declarations without using-directives, hidden-friend visibility, canonical candidate identity/order, typed call publication, PA15 demand/declaration/call lowering, and the narrow empty-class opaque ABI bridge are traced. The follow-up repairs only `dev/src/pa12_semantic_calls.cpp`; incomplete named-class records associate their namespace without unvalidated base expansion, while member pointers, template expansion, and general class-value semantics remain closed. Course 426 now validates five positive runtime cases (inline, pointer, array, and function-pointer association) plus ordinary-parent and using-directive rejection controls. Final `make test-pa16` is exit `2` at `218/243`, exactly `25` failures, and `243/243` identities covered; exact sorted comparison with the supplied authority is `25` vs `25`, fresh-only `0`, authority-only `0`, with `243` discovered identities, `243` reference sidecars, `243` fresh sidecars, and missing `0/0`. Focused PA16 is `11/12` with only the known nested-enum LowIR residual; PA12 controls are `2/2`; all three temporary wrapper probes compile, lower, translate, and run with status `0`; course 426 and `sh -n` pass; through-PA15 is `1167/1167`; file audit exits `0` with five existing `bad-division` warnings and no fatals; final `git diff --check` and bounded path/coverage audit both exit `0`. The same 25 residual identities remain, so PA16 remains incomplete; no unrelated residual was re-audited. | completed audit |
| `e470e9df` prvalue derived-base reference binding checkpointAudit | Final bounded audit of landed `e470e9dfed07ca09a373d227640f3c8042cc2cbf` relative to `f3afe9d5`: the PA12 repair confines prvalue derived-to-base reference binding to const non-volatile lvalue references, restores exact same-class temporary-reference ranking, and retains typed access/path publication into the validated PA15 projection consumer. Focused matrix is `8/8`; required through-PA15 is `1167/1167`; file audit exits `0` with five known warnings; fresh `make test-pa16` exits `2` at `220/243` with `23` failures; exact comparison to the supplied current authority is `23/23`, fresh-only `0`, authority-only `0`, and discovered/reference/fresh coverage `243/243/243` with missing/unexpected `0/0`. No test, fixture, reference, harness, comparator, generated output, coverage rule, source-set file, or unrelated stage code changed; PA16 remains incomplete with the same 23 residual identities. | completed audit |
| `24d555c8` typed no-op construction effects checkpointAudit | Completed final bounded audit of landed `24d555c882a3e15ea3ffe5be42ed5d9953084df6` relative to `d889058c0d159bd4414ffb6e9f5ac75227ce0192`: PA12 typed constructor facts flow through PA15 memoized constructor/zero-init summaries, demand traversal, aggregate/construction lowering, and typed address paths. The audit repairs the semantic demand/action-shape split, fail-closed enclosing action-graph ownership/layout/result validation, and inconsistent direct-base metadata handling before pruning; current-block address reuse, cache cycle handling, leaf retention, DMI/destructor/lifetime barriers, argumented construction, and scalar/value stores remain guarded. Focused targets, course 404/409, and the constructor matrix pass; the exact prior-through gate is `1167/1167`; fresh PA16 is `222/243` with the exact unchanged 21-failure identity set; authority/fresh failures are `21/21`, authority-only/fresh-only are `0/0`, and discovered/reference/fresh inventories are `243/243/243` with all missing/unexpected comparisons `0`. The file audit exits `0` with five pre-existing warnings, the structural scale probe retains 128 base entries while emitting 0 derived wrappers/calls, and final diff-check and clean-tree verification pass. | completed audit |
| `6d2ed09c` typed ToVoid discarded lowering checkpointAudit | Completed audit of landed `6d2ed09cd4b3daf55ab28282addcf3a878a8adba` relative to `14cadc0c`: PA12's typed `ToVoid` producer and PA15's discarded-expression consumer are traced through O0 LowIR. The consumer now fail-closes in-range typed source/target mismatches; the narrow non-reference scalar-parameter read and volatile/function/reference/class/comma/conditional/assignment/increment boundaries remain intact. Serial reconfirmation is build `0`, PA16 `3/3`, and PA15 `4/4`; the exact prior-through gate is `1167/1167`; final PA16 is status `2` at `224/243` with exactly the same 19 failures; identity comparison is `19 -> 19`, retained `19`, authority-only/fresh-only `0/0`, and discovered/reference/fresh `243/243/243` with all missing/unexpected counts `0`; file audit is status `0` with five pre-existing warnings; diff-check and bounded path audit pass. Durable evidence is in the final checkpoint directory. No test, fixture, reference, harness, comparator, generated-output, coverage, source-set, or unrelated stage change. |
