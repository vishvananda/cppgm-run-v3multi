# PA16 implementation plan

## Stage Design

PA11 owns the canonical `BindingId`, binding owner scope, declared `TypeId`,
and merged definition fact. PA12 owns the typed initializer tree and its
reference-binding/conversion facts. PA15 consumes those facts directly:
`low_type` is the owned-storage boundary, while reference and glvalue
lowering may carry an address without asking for a class layout. Global
demand is indexed once from typed `IdExpression`/`MemberExpression` bindings
and typed constant-address targets before global symbols are collected.

The invariants for this stage are:

- a reference to an incomplete class is pointer storage and never an owned
  class object; a value load still requires a complete layout;
- a declaration-only namespace variable is collected only when a typed demand
  root requires its external declaration; definitions and the existing
  class-static demand rule remain unchanged;
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

The seven requested tests and one available used-extern preservation control
are the focused validation set. The final broad result is `189/243` passing,
`54` failing, with `243/243` identities covered. The sorted identity delta
against the baseline is recorded under
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-typed-nonowning-namespace-object-20260829/`:
baseline-only is exactly
`100-global-reference-incomplete-referent.t` and
`200-extern-class-object-declaration.t`; final-only is empty. Thus the final
residual map is exactly the authoritative baseline map above minus those two
owned identities.

## Active Checkpoint

Changed implementation files:

- `dev/src/pa15_lowering_flow.cpp`
- `dev/src/pa15_lowering_globals.cpp`

`low_reference_value_type` now returns a typed pointer for an incomplete named
class object carried through a reference/glvalue boundary. This lets PA12's
typed `ReferenceBinding` initializer preserve the address of
`*forward_declared_object`; a later value materialization still reaches
`low_type` and therefore cannot silently materialize incomplete owned storage.

The existing PA15 global-demand pass now recognizes namespace-owned variables
as well as class-static variables from canonical typed bindings and constant
address targets. Both global indexing and collection preserve the preceding
class-scope non-static rejection, then apply one declaration-only no-demand
check. Definitions and demanded declarations remain available through the
same `required_global_bindings_` vector; required extern declarations and
class-static behavior remain demand-driven.

No handout tests, fixtures, harnesses, comparators, coverage rules, source
sets, or course regressions changed. This increment changes only the three
intended files.

## Performance Evidence

The new global predicate is O(1) per typed binding demand. The existing demand
walk remains one pass over semantic facts and child/conversion ranges,
O(F + E); global scope/binding indexing remains O(S + B), with no whole-TU
retry or broad invalidation. The incomplete-class check is a constant-size
typed wrapper/layout-state query at the reference boundary; it does not scan
class members. Extra temporary state is bounded by the existing dense demand
vector.

Collected structural/conformance evidence is in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-typed-nonowning-namespace-object-20260829/determinism-probes.log`.
Each probe used `--emit-lowir -O0` and was compiled twice; `cmp` reported
byte-identical output:

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
allocation, or generated-code performance claims. The complexity model above
is unchanged.

## Validation

- `make -C dev cppgm++ -j2`: pass.
- Required matrix: `7/7` pass, with no failure identities:
  `100-global-reference-incomplete-referent`,
  `200-extern-class-object-declaration`,
  `100-incomplete-class-return-function-address`,
  `100-global-class-zero`,
  `200-aggregate-reference-member-binds-storage`,
  `200-global-scalar-dynamic-init`, and
  `200-global-class-array-init`.
- Available used-extern control `300-header-static-class-init.t`: `1/1` pass.
- Combined focused result: `8/8` pass; no new focused failure identity.
- Three typed-demand probes, repeated twice each: byte-identical output;
  used class and scalar declarations present; unused class declaration
  omitted.
- `make test-pa16`: `189/243` pass, `54` failures, `243/243` identities
  covered; command exits nonzero because residual failures remain. Baseline
  only: the two owned identities above; final-only: none. Exact sorted files:
  `baseline-failures.txt`, `final-failures.txt`, `baseline-only.txt`, and
  `final-only.txt` in the checkpoint log directory.
- `make test-report-through-pa15` via the requested `n=16` command:
  `1167/1167` pass.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: pass with
  five pre-existing warnings.
- `git diff --check`: pass.
- Test and fixture identities remain unchanged because no test or harness file
  was edited.

## Next Checkpoint

Select the next residual PA16 owner without widening this typed non-owning-
storage boundary. Preserve the canonical PA11/PA12 facts and the single
demand-root model.

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
| `PA16 typed non-owning namespace object boundary` | `189/243` passing, `54` failures, `243/243` covered; both owned identities removed, final-only identity set empty; focused matrix `8/8`; used class/scalar extern probes deterministic and demand-correct; through-PA15 `1167/1167`; file audit and diff check passed. |
