# PA16 unnamed-namespace identity and lifecycle-demand checkpoint

## Stage Design

PA11 remains the sole semantic owner of namespace/scope identity and binding
linkage.  An unnamed namespace is a typed `Scope` fact; the parent-keyed
`unnamed_namespace_index_` reuses its `ScopeId` on reopen, while distinct named
parents retain distinct typed owners.  The internal-linkage scope fact is
propagated through namespace, class, and enum scopes and through
`TemplateParameters`, so declarations nested beneath that scope inherit the
owner.  Function and block scopes are lexical boundaries whose local names do
not acquire linkage merely because an enclosing namespace is internal.
Template-parameter bindings themselves remain unlinked while their scope
carries the enclosing declaration's owner fact.
`add_value` and synthesized class special-member bindings consume that typed
owner fact into canonical `Binding::internal_linkage`.  The global namespace
remains a distinct non-unnamed scope.

PA12 creates the typed default-constructor action for the qualifying internal
namespace-scope class object and marks it
`internal_namespace_default_constructor_demand`; it also publishes the typed
constructor base-entry relation.  PA15 consumes that marker through its
existing global-root demand worklist, emits the required internal constructor
pair, and keeps the zero-data object path free of a startup call.  Typed
namespace components render `_GLOBAL__N_1` for each unnamed namespace under
its named owner.  No stage parses rendered names, creates a second semantic
owner, retries the whole program, or eagerly emits unrelated helpers.

This aligns with `spec.md` Purpose and §§1–5/§7: one forward pipeline,
continuity of typed identity/linkage/lifetime/demand facts, distinct runtime
and emission demand, bounded typed worklists/caches, and typed LowIR lowering.
Named namespaces, global controls, nested/reopened anonymous namespaces,
hidden-friend lookup/ADL, local-scope ownership, and explicit/nontrivial
special-member paths retain their typed owners and prior boundaries.

## Checkpoint Authority and Failure Map

The clean starting HEAD is
`8708859c48a0327d4a975e1bce059a066b4676ca` (`PA16: preserve unnamed
namespace symbol identity`), relative to parent `542b136a`.  Supplied
turn-start authority is `make test-pa16` exit `2`, `236/243` passing, complete
`243/243` identity coverage, and exactly these seven residual identities:

1. `pa16/tests/general/200-local-default-class-array-lifecycle.t`
2. `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
3. `pa16/tests/general/300-friend-function-definition-skip.t`
4. `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
5. `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
6. `pa16/tests/general/400-signed-bit-field-read.t`
7. `pa16/tests/general/400-signed-enum-bit-field-read.t`

The active unnamed-namespace hidden-friend identity is not in that residual
set.  Authorized fresh validation reproduces the same seven failures at
`236/243`; the exact authority/fresh comparison is `7/7`, with
authority-only/fresh-only `0/0`.  Discovered/reference/fresh identity
inventories are `243/243/243`, with no new or lost identity.  The through-PA15
gate is `1167/1167`, and the final file audit passes with six known nonfatal
`bad-division` warnings.  No unrelated residual cluster was audited or
repaired.

## Bounded Audit Result

The landed implementation boundary is exactly:

- `dev/src/pa11_semantic.cpp`
- `dev/src/pa11_semantic_core.cpp`
- `dev/src/pa11_semantic_model.h`
- `dev/src/pa12_semantic_construction.cpp`
- `dev/src/pa12_semantic_facts.cpp`
- `dev/src/pa15_lowering.h`
- `dev/src/pa15_lowering.cpp`
- `dev/src/pa15_lowering_calls.cpp`
- `cppgm.tests/course/pa16/430-typed-unnamed-namespace-per-parent-regression.sh`

The bounded audit repair is limited to `pa11_semantic_core.cpp`,
`pa11_semantic_model.h`, and `pa15_lowering.cpp`.  Course regression 431 is
the only additional test surface:
`cppgm.tests/course/pa16/431-typed-internal-special-member-abi-regression.sh`.
Only `pa16/plan.md` and `pa16/audit.md` are documentation changes.  No
handout test, fixture, `.ref` file, exit-status sidecar, harness, comparator,
generated output, or source-set manifest changed.

The representative fact path is:

```text
parent-keyed unnamed-namespace reopen
  -> typed ScopeId and internal owner fact
  -> namespace/class/enum ownership (function/block locals excluded; template
     parameter scope bridges the declaration owner)
  -> hidden-friend function fact and ADL selection
  -> namespace-scope default-object ConstructorAction and typed demand marker
  -> constructor/base-entry BindingIds and PA15 demanded-function worklist
  -> typed namespace ABI component and internal LowIR definitions
```

PA12 special-member bindings, synthesized aggregate/default constructors,
inheriting wrappers, and implicit destructors preserve the owner scope's
internal bit.  `ensure_special_member_base_entry` copies the canonical
binding/sidecar facts, so the base entry is not a second semantic owner.
PA15 assigns internal metadata from the binding and uses the typed
`constructor_base_entry_bindings_`/`destructor_base_entry_bindings_` relation.
It suppresses a complete-entry `C2`/`D2` alias only after the mapped binding
is valid, resolves to the corresponding typed base-entry `FunctionFact` with
the matching owner/source/record, and has a nonzero
`demanded_member_functions` bit.  The constant-sized proof is fail-closed;
otherwise the alias is retained.  When the proof succeeds, the emitted base
entry owns the object spelling.  This was checked with internal
default-member-initializer and both destructor relation/no-relation cases as
well as the existing named controls.

The demand marker is typed and narrow.  It requires a defined namespace-scope
class object, a complete non-union class, the declaration's default-object AST
shape, internal namespace ownership, and no user-declared constructor.  It
publishes one empty constructor action and one base-entry relation for that
object.  PA15 consumes the bit only at a global root; ordinary function roots
and explicit/user-initialized paths retain their existing runtime demand
rules.  The marker is not inferred from `_GLOBAL__N_1`, LowIR names, or text,
and it adds one constant-sized edge per qualifying object rather than a broad
helper sweep.

The suspected propagation defect was real at the typed metadata boundary:
the landed `create_scope` copied internal ownership into function/block
children, and `add_value` consequently marked block/function-local bindings
as internal despite their having no linkage.  The repair keeps the fact on
namespace/class/enum owner scopes and stops it at lexical function/block and
template-parameter scopes.  PA16 does not have a supported local-class
LowIR case that would provide a useful public-output assertion for this
metadata-only boundary; the owner invariant is nevertheless consumed by
binding/redeclaration and special-member metadata code.

## Focused Evidence

Post-repair focused results are:

```text
make build                                             exit 0
sh -n .../430-typed-unnamed-namespace-per-parent...    exit 0
sh .../430-typed-unnamed-namespace-per-parent...       PASS
sh -n .../431-typed-internal-special-member-abi...      exit 0
sh .../431-typed-internal-special-member-abi...         PASS
active handout check                                   PASS (1/1)
named/lifecycle/friend controls                        PASS (5/5)
git diff --check                                       exit 0
```

Regression 430 covers two named parents, unnamed-namespace reopening, one
fixed `_GLOBAL__N_1` component per parent, internal globals/friends, and one
constructor/base-entry definition per owner.  Regression 431 covers an
internal default-member-initializer constructor pair, an in-class destructor
with no base-entry relation, and an internal destructor declared in-class and
defined out-of-class.  It asserts one `D1` owner and one `D2` base-entry
function/object pair for the related case, no duplicate `D2` alias there, and
one retained `D2` alias for the no-relation control.  Temporary nested
anonymous-namespace and internal special-member probes also lower with status
zero and retain deterministic typed components/metadata.

The focused control command was:

```text
make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check \
  TEST='tests/general/100-global-class-zero.t \
tests/general/200-global-constructor.t \
tests/general/300-hidden-friend-definition-adl-call.t \
tests/general/200-friend-simple-declaration-skip.t \
tests/general/300-thread-local-synthetic-symbol-family-isolation.t'
```

## Structural Performance, Uncertainty, and Next Checkpoint

Unnamed-scope reuse, scope-owner propagation, and the alias-relation check are
constant-sized work per consumed scope/function.  PA12 adds one constructor
action/base-entry edge per qualifying object.  PA15 continues to use its
dense typed binding/function/fact demand worklists; no whole-program retry,
rendered-name search, extra reorder pass, or unbounded cache was added.  The
two-parent/reopen and two-special-member regressions are structural scale
evidence.  No timing, RSS, allocation, or generated-program performance claim
is made.

The final broad gate remains at `236/243`, with exactly the seven supplied
residual identities and no added or lost identity; the `243/243` inventory is
complete and the threshold is `7 <= 7`.  The exact through-PA15 gate is
`1167/1167`.  `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`
exits `0` with six known nonfatal warnings for `abi_mangle.h`,
`cpp_semantic_core.h`, `lowir_model.h`, `pa11_semantic_model.h`,
`pa12_semantic_selection.h`, and `pa15_lowering.h`; `git diff --check` exits
`0`.  The checkpoint audit is complete.  PA16 remains incomplete by the
same seven residuals; the next checkpoint is the separately scoped
`200-local-default-class-array-lifecycle.t` owner if work continues.
