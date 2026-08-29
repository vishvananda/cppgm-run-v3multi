# PA16 implementation plan

## Stage Design

PA11 owns the canonical `BindingId`, binding owner scope, declared `TypeId`,
definition bit, and storage/linkage facts. PA12 owns the typed initializer
tree, reference-binding/conversion facts, and constant-address targets. PA15
consumes those facts directly: `low_type` is the owned-storage boundary,
while reference and glvalue lowering may carry an address without asking for a
class layout. After `collect_functions`, global demand retains one bounded
structural validation of the typed arenas and then uses roots from emitted
`FunctionPlan`s and namespace/static global initializers; reachable child,
aggregate, and address edges are visited once before global symbols are
collected.

The invariants for this stage are:

- a reference to an incomplete class is pointer storage and never an owned
  class object; a value load still requires a complete layout;
- a declaration-only namespace variable is collected only when a typed demand
  root requires its external declaration; definitions and the existing
  class-static demand rule remain unchanged;
- namespace and static-member demand accepts only a canonical variable with a
  valid namespace/class owner; malformed owner/range data fails closed;
- the reference initializer address is lowered exactly once from its PA12 fact
  into `__cppgm_init`;
- no source spelling recovery, textual downgrade, full-TU retry, second
  semantic model, or repeated class-member scan is introduced.

## Failure Map

The authoritative clean turn-start baseline is HEAD `68b549f2`: `187/243`
PA16 identities passed, `56` failed, and `243/243` identities were covered.
The complete residual map from
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` is:

- `pa16/tests/general/100-function-pointer-nested-param-name-shadow.t`
- `pa16/tests/general/100-global-aggregate-nested-array-initializer.t`
- `pa16/tests/general/100-global-reference-incomplete-referent.t`
- `pa16/tests/general/200-aliased-base-mem-initializer-match.t`
- `pa16/tests/general/200-const-subobject-member-call.t`
- `pa16/tests/general/200-defaulted-constructor-still-aggregate.t`
- `pa16/tests/general/200-deleted-constructor-still-aggregate.t`
- `pa16/tests/general/200-destructor-body-local-before-base-destruction.t`
- `pa16/tests/general/200-elaborated-member-forward-type.t`
- `pa16/tests/general/200-extern-class-object-declaration.t`
- `pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t`
- `pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t`
- `pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t`
- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-member-object-lifetime.t`
- `pa16/tests/general/200-mutable-member-const-method.t`
- `pa16/tests/general/200-nested-braced-member-aggregate-init.t`
- `pa16/tests/general/200-nested-out-of-class-constructor-enclosing-type.t`
- `pa16/tests/general/200-nonliteral-field-condition-not-folded.t`
- `pa16/tests/general/200-placement-new-expression-aggregate-brace.t`
- `pa16/tests/general/200-placement-new-expression-constructor-call.t`
- `pa16/tests/general/200-pointer-subscript-class-reference-return.t`
- `pa16/tests/general/200-qualified-friend-function-member-access.t`
- `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
- `pa16/tests/general/200-reference-member-class-init.t`
- `pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t`
- `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
- `pa16/tests/general/300-adl-associated-namespace-does-not-climb-parents.t`
- `pa16/tests/general/300-adl-using-declaration-source-point.t`
- `pa16/tests/general/300-callable-field-hides-private-base-method.t`
- `pa16/tests/general/300-compound-assignment-adl-nonmember-after-member-reject.t`
- `pa16/tests/general/300-const-pointer-explicit-destructor-call.t`
- `pa16/tests/general/300-enum-class-nonmember-operator-bitand.t`
- `pa16/tests/general/300-explicit-destructor-call-enclosing-namespace-type.t`
- `pa16/tests/general/300-friend-function-definition-skip.t`
- `pa16/tests/general/300-member-vs-nonmember-operator-implicit-object-cv-rank.t`
- `pa16/tests/general/300-mixed-member-free-shift-stress-chain.t`
- `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
- `pa16/tests/general/300-operator-nullptr-t-from-zero.t`
- `pa16/tests/general/300-operator-shift-stress-chain.t`
- `pa16/tests/general/300-overloaded-deref-user-assignment.t`
- `pa16/tests/general/300-packed-class-layout.t`
- `pa16/tests/general/300-pragma-pack-followed-by-endif.t`
- `pa16/tests/general/300-prvalue-derived-base-friend-operator.t`
- `pa16/tests/general/300-scalar-pseudo-destructor-call.t`
- `pa16/tests/general/300-synthesized-array-member-lifecycle.t`
- `pa16/tests/general/300-unary-address-of-builtin-fallback.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`
- `pa16/tests/general/400-bit-field-constructor-member-init.t`
- `pa16/tests/general/400-bit-field-member-access-bad.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-bitfield-aggregate-init.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

Owned subset for this checkpoint:

- `pa16/tests/general/100-global-reference-incomplete-referent.t`: PA15
  requested a complete layout for a non-owning reference referent;
- `pa16/tests/general/200-extern-class-object-declaration.t`: PA15 emitted an
  unused declaration-only extern class object instead of applying demand roots.

The landed-head authority and the post-repair result are `189/243` passing,
`54` failing, with `243/243` identities covered. The exact sorted comparison
against the landed `final-failures.txt` is recorded under
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-typed-nonowning-namespace-object-20260829/`:
baseline-only and final-only are both empty. The two identities removed from
the 56-failure parent map are exactly
`100-global-reference-incomplete-referent.t` and
`200-extern-class-object-declaration.t`; the post-repair residual map is
byte-for-byte the landed 54-failure authority.

## Active Checkpoint

Landed implementation files:

- `dev/src/pa15_lowering_flow.cpp`
- `dev/src/pa15_lowering_globals.cpp`

`low_reference_value_type` now returns a typed pointer for an incomplete named
class object carried through a reference/glvalue boundary. This lets PA12's
typed `ReferenceBinding` initializer preserve the address of
`*forward_declared_object`; a later value materialization still reaches
`low_type` and therefore cannot silently materialize incomplete owned storage.

The landed PA15 global-demand pass recognizes namespace-owned variables as
well as class-static variables from canonical typed bindings and constant
address targets. Both global indexing and collection preserve the preceding
class-scope non-static rejection, then apply one declaration-only no-demand
check. Definitions and demanded declarations remain available through the
same `required_global_bindings_` vector; required extern declarations and
class-static behavior remain demand-driven.

The bounded audit found that the landed all-fact demand marking could promote
an un-emitted member body or unused default argument into a namespace storage
root. The completed repair keeps the complete typed range/DAG validation but
seeds demand from emitted function plans and namespace/static global
initializers, follows typed reachable edges once, and uses a separate address
worklist for address-of/cast targets. It also makes namespace/class owner
eligibility fail closed. No handout, fixture, harness, comparator, coverage
rule, source set, or unrelated source changed.

The implementation repair is confined to `dev/src/pa15_lowering_flow.cpp`;
the two required documentation files are updated as part of this checkpoint
audit. The complete post-repair gate evidence is recorded below.

## Performance Evidence

The new global predicate is O(1) per typed binding demand after canonical
owner/index lookup. Structural range and DAG checks remain bounded passes over
the typed semantic arenas; demand marking now walks only reachable facts and
aggregate/address edges, with dense seen vectors, so its worst-case work is
O(F + E) and temporary mark state is O(F). Constructor action arguments are
seeded only for emitted constructor plans. Global scope/binding indexing
remains O(S + B), with no whole-TU retry or repeated broad demand scan. The
incomplete-class check is a constant-size typed wrapper/layout-state query at
the reference boundary; it does not scan class members.

Collected structural/conformance evidence is in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-typed-nonowning-namespace-object-20260829/determinism-probes.log`.
Each probe used `--emit-lowir -O0` and was compiled twice after the repair;
`cmp` reported byte-identical output:

- complete used class extern (`struct Y { int x; }; extern Y g; ...`):
  `251` bytes, `9` lines, `declare_global=1`, `global=0`, SHA-256
  `04494956d6e5172b8e4e0db01829b613bc32d810143af278417f39b5cfb26b62`, with
  `declare global @g : obj<4x4>` present;
- used scalar address extern (`extern int scalar; int *address = &scalar; ...`):
  `552` bytes, `23` lines, `declare_global=1`, `global=1`, SHA-256
  `0d701eb51278f09ec5e22f13dbb4efbf577f46dba67243c684346c3cabee7f05`, with
  `declare global @scalar : i32` and `global @address ... = addr @scalar`;
- unused handout extern control
  `200-extern-class-object-declaration.t`: `106` bytes, `4` lines,
  `declare_global=0`, `global=0`, SHA-256
  `485fc8e3251fe7b25d56cc0db4e5cc73da7486c3984948de64f662839846898f`, with
  no global declaration.

These are conformance and determinism observations, not timing, RSS,
allocation, or speedup claims. No timing or memory measurement was collected.

## Validation

- `make -C dev cppgm++ -j2`: pass.
- Owned identities pass `2/2`; the expanded 16-test handout matrix passes
  `16/16`, with no focused failure identity.
- Course controls 402, 404, 407, 408, 409, 412, 415, and 420 each exit `0`;
  course 410 also exits `0` and reports the expected structural counts for
  `E=8`, `E=16`, and `E=32`.
- Root-reachability reductions show unused member/default facts do not emit
  `declare global @x`, while used roots do. Incomplete reference/glvalue
  output uses pointer LowIR; direct incomplete value materialization fails
  closed in PA12. Repeated class/scalar/address and unused-extern probes are
  byte-identical.
- `make test-pa16` exits `2` at `189/243` passing, `54` failures, and
  `243/243` coverage. Comparison with the landed `final-failures.txt` has
  baseline-only `0` and final-only `0`; the full log and sorted identity files
  are under
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-typed-nonowning-namespace-object-20260829/`
  (`post-repair-test-pa16-final-20260829.log` and
  `post-repair-identity-comparison-final.log`).
- The exact requested `n=16` through-PA15 command exits `0` at `1167/1167`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0`
  with five known header-division warnings; the warning paths are recorded in
  the audit review and `post-repair-file-audit-20260829.log`.
- `git diff --check`: pass.
- Test and fixture identities remain unchanged because no test or harness file
  was edited.

## Next Checkpoint

Select the next residual PA16 owner without widening this typed non-owning-
storage boundary. Preserve the canonical PA11/PA12 facts and the single
typed demand-root model. The incomplete namespace-object address case is
out-of-contract because PA16 scopes namespace object declarations to complete
class types; it is deferred. The unrelated course-400 DMI control remains
outside this landed increment.

## Checkpoint Ledger

| checkpoint | result |
| --- | --- |
| `36b93869` historical aggregate handoff | `159/243` PA16 passing, `84` failures, `243/243` covered; PA1--PA15 passed. |
| `dea01c52` aggregate implementation | Historical aggregate increment: `164/243` passing, `79` failures, `243/243` covered. |
| `d7ed98aa` typed builtin boundary | `167/243` passing, `76` failures, `243/243` covered. |
| `3b7d8e6a` qualified-type checkpoint | `173/243` passing, `70` failures, `243/243` covered. |
| `30d69fc3` landed inheriting-constructor checkpoint | `176/243` passing, `67` failures, `243/243` covered. |
| `PA16 typed access-control checkpoint` | `179/243` passing, `64` failures, `243/243` covered. |
| `PA16 typed non-automatic lifetime checkpoint` | `184/243` passing, `59` failures, `243/243` covered; focused matrix `9/12`. |
| `PA16 typed ordinary-value-over-tag lookup checkpoint` | `187/243` passing, `56` failures, `243/243` covered; its focused matrix was `8/8`. |
| `b3bbf052` typed non-owning namespace object checkpointAudit | Completed the bounded ownership-path audit and repair: incomplete named-class references/glvalues retain pointer representation while owned values remain layout-gated; namespace declaration-only globals are rooted in emitted typed facts; owner/range checks fail closed; and unused member/default facts no longer create storage roots. Post-repair `make test-pa16` is `189/243` passing, `54` failures, and `243/243` covered; comparison with the landed 54-failure authority has baseline-only `0` and final-only `0`. The focused handout matrix is `16/16`, the exact through-PA15 command is `1167/1167`, file audit exits `0` with five known header-division warnings, repeated structural probes are byte-identical, and `git diff --check` passes. The incomplete namespace-object address case is out-of-contract under the PA16 complete-class namespace-object scope; course-400 DMI remains outside this increment. |
