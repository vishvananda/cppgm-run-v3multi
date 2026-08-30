# PA16 implementation plan

## Current checkpoint

This checkpoint audits landed commit `15e9897bc038499f724d69cb3cfe70e806b9fb36`
(`PA16 fix effective using call publication`) relative to its parent.  The
bounded implementation ownership is `dev/src/pa11_semantic_core.cpp`,
`dev/src/pa11_semantic_model.h`, `dev/src/pa12_semantic.cpp`, and
`dev/src/pa12_semantic_calls.cpp`; PA15 consumers are validation-only.  The
audit also refreshes this plan and `pa16/audit.md`.  No tests, fixtures,
references, harnesses, comparators, generated outputs, coverage rules, or
source-set files are changed.

PA16 remains one typed PA10 NamePath -> PA11 lookup/type/access -> PA12
selection/publication -> PA15 LowIR pipeline.  The checkpoint covers effective
using-directive visibility and the parser's ambiguous one-argument call
publication, not general class value semantics.  This follows `spec.md`
§§1--5 and 7: canonical identities and typed tuples, source-point-aware
lookup, deterministic bounded traversal, one typed LowIR path, no text
downgrade, retry, second lookup/lowering engine, host/reference shortcut, or
global scan.

## Exact current authority

The authoritative full-stage log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`make test-pa16` exit `2`, `219/243` passed, exactly `24` failures, and
`243/243` test identities covered.  The post-repair final transcript is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-final-test-20260830-v2.log`;
its exact sorted failure comparison is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-final-failure-comparison-20260830-v3.log`.
Fresh and authority failures are both `24`, fresh-only and authority-only are
both `0`, and discovered/reference/fresh inventories are exactly
`243/243/243`.  The exact failure identities are:

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
- `pa16/tests/general/300-prvalue-derived-base-friend-operator.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

Coverage remains an identity invariant: discovered, reference-sidecar, and
fresh-sidecar inventories are exactly `243/243/243`.  The final fresh/authority
comparison is exact (`24` versus `24`, fresh-only `0`, authority-only `0`), so
there is no coverage reduction.

## Ownership and findings

`process_using_directive` resolves the target as a typed namespace `ScopeId`,
stores the relation at `common_ancestor(scope, target)`, and retains the
actual lexical owner and `SourcePoint` in `EffectiveUsingDirective`.  Lookup
marks the start scope and its parent chain, then accepts an effective edge
only when its lexical owner is applicable.  Namespace and implicit
unnamed-namespace relations compare their declaration point with the lookup
point.  During deferred PA12 statement/call semantics, a small RAII
`LookupPointGuard` supplies the current AST node's typed source point through
`lookup_source_point`; local block relations therefore compare at the use
site as well.  Nested `semantic_id_expression` evaluation installs a further
guard at the identifier's own `source_begin`, so a control node such as `for`
cannot hide a declaration already in scope for its condition or iteration
expression.  PA11 formation outside that context retains its existing
registration behavior.

The audit found one directly caused source-point defect and repaired it in the four source
files above.  After the landed owner change, deferred semantic lookup treated
a block-scope `using namespace` declared after a use as visible because it
still used the enclosing function point and the local-owner branch was
unfiltered.  The repair adds only the transient point context, makes the
existing relation predicate check `declaration_point <= use_point` during
typed statement/call evaluation, and nests the same RAII context at each
identifier source point.  It does not mutate the canonical scope graph or add
a lookup pass.

The effective call path is:

```text
NamePath
  -> lookup_value_path / effective using graph / source-point filter
  -> resolve_single_argument_function
  -> conversion_for + shared typed overload ranking
  -> exact ClassValue conversion on the argument fact
  -> PA15 lower_call validation and existing opaque object bridge
```

`resolve_single_argument_function` keeps canonical `ValueRef` identities,
requires one parameter, ranks the existing typed `ConversionChoice`, and
publishes `ConversionKind::ClassValue` only through the shared predicate.
That predicate requires a valid function fact, a namespace-owned ordinary
function (or the already narrow canonical constructor exception), one
nonvariadic parameter, matching empty class identity, a non-class result, and
an lvalue argument with the same canonical object type.  References,
variadics, multiple parameters, nonempty or mismatched classes, class results,
and ambiguous candidates do not enter this bridge.  PA15 revalidates the
conversion range, selected binding, ABI, category, and object identity; the
source is lowered once and no general copy/return-by-value support is added.

Ordinary ADL remains limited to the existing unqualified non-template path.
It uses typed associated records/namespaces, excludes ordinary namespace
parents and using-directive edges, keeps hidden-friend declaration points,
and retains deterministic bounded graph traversal.  Constructors, class
members, references, and indirect calls remain on their existing typed
boundaries.  Qualified empty-class namespace-function observation matches the
existing narrow predicate; it is not used to broaden PA16's out-of-scope
class value semantics.

## Focused evidence

After the repair, focused evidence is durably recorded at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-final-focused-20260830-v2.log`:

- `make -C dev cppgm++ CXX=g++`: status `0`.
- The selected PA16 lookup/ADL/using/member controls: status `0`,
  `PASS (12/12)`.
- The PA12 using/inline/condition controls, including the `for` loop and
  local using-directive case: status `0`, `PASS (6/6)`.
- The PA15 loop/condition/initializer controls: status `0`, `PASS (6/6)`.
- `sh -n cppgm.tests/course/pa16/426-typed-adl-inline-namespace-regression.sh`
  and its execution: status `0`, `PASS`.
- `git diff --check`: status `0` at the focused checkpoint.

The exact prior gate is recorded at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-final-through-pa15-20260830-v2.log`:
`1167/1167`, status `0`.  The exact file audit is recorded at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-final-file-audit-20260830-v2.log`:
status `0` with five known warnings.  The bounded changed-path, excluded
artifact, and coverage audit is recorded at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpoint-audit-final-bounded-audit-20260830-v2.log`:
it passes with exactly the four approved sources plus these two documents,
no fixture/reference/harness/generated-output/coverage/source-set path, and
`243/243/243` discovered/reference/fresh identity coverage.

Ephemeral source probes outside the repository exercised the negative and
positive boundaries.  Compile statuses are `0` for nested `for` condition and
iteration identifiers, a nested `for` initializer/condition/iteration, a
nested condition-declaration initializer, local using before call, namespace
using before use, unnamed namespace before use, matching empty namespace
function, `const E&`, and qualified empty namespace function.  They are `1`
for local value lookup before a later using-directive, later namespace using at
the use point, irrelevant lexical scope, later unnamed namespace, nonempty
class, class result, variadic and multi-parameter signatures, mismatched class
argument, rvalue-reference parameter, and ambiguous empty-class overloads.
The narrow out-of-class constructor observation remains `0`; in-class
constructor and class-static by-value observations remain rejected by this
PA16 boundary.

The primary `300-adl-associated-namespace-does-not-climb-parents.t` now emits
the expected typed `boost_no_adl_barrier__nnn__begin` call, while the ordinary
parent and using-directive ADL controls remain rejecting cases.  The focused
tests compare checked-in fixtures only; no fixture or reference regeneration
was performed.

## Complexity and next checkpoint

The repair is O(1) state save/restore plus the existing O(1) relation check.
Effective lookup remains limited to marked lexical ancestors and the existing
generation-marked namespace graph.  Candidate ranking and PA15 demand use
their existing typed bounded vectors and keyed identities.  No retry, whole
program scan, cache, textual reconstruction, or performance claim beyond
these structural bounds is introduced.

The final validation of this exact milestone satisfies the required
failure-set rule and structural bounds.  PA16 is not stage-complete: the same
24 residual identities remain.  The next checkpoint therefore requires a
separate authorization and ownership path for a selected residual family;
this increment does not broaden into that work.

## Checkpoint ledger

| checkpoint | result and disposition |
| --- | --- |
| `ab1b2a8c` source-point-aware associated ADL | `218/243`, 25 failures, `243/243` covered; prior completed checkpoint |
| `15e9897b` effective-using visibility and typed call publication | Completed final bounded audit and narrow nested-identifier source-point repair. Focused PA16/PA12/PA15 matrices are `12/12`, `6/6`, `6/6`; final PA16 is `219/243` with the exact supplied 24-failure set, fresh-only `0`, authority-only `0`, and `243/243/243` inventories; through-PA15 is `1167/1167`; file audit passes with five known warnings; bounded path/artifact/coverage audit passes. PA16 remains incomplete with those 24 residuals. |
