# PA16 elaborated-member-forward-type checkpoint

## Stage Design

PA10 owns the syntax boundary and indexed parenthesized-group fact used for
declaration/declarator routing.  PA11 owns typed AST/model identities,
including the canonical `ClassForwardDeclaration` -> named class/type fact.
PA12 owns semantic lookup and selected typed calls.  PA15 consumes those facts
for typed LowIR and ABI lowering.  Class value semantics, copy/move transfer,
and pass-by-value class objects remain PA17 scope.

The owner path is:

    tokens -> PA10 indexed parenthesized-group classification
      -> parse_decl_or_function / parse_decl_specifier_seq
      -> ClassForwardDeclaration or enum elaborated declaration
      -> PA11 ensure_named_class + add_type_binding
      -> PA12 typed member lookup and selected call fact
      -> PA15 typed object/member/call LowIR and value_components ABI path

This follows `spec.md` Purpose and sections 1--5 and 7: each stage owns one
fact boundary, the declaration is parsed once, PA11 canonicalizes one typed
identity, and later stages consume it without text recovery or a duplicate
parser/model.

## Baseline Authority and Failure Map

The audited increment is landed commit
`29d9c4ce8cd2d9c85ae8de25ea3c3d8515520f4f`, relative to parent
`6fc0a8124619282302c7ca1759245ca7a550a117`.  The supplied turn-start
authority is `make test-pa16` status `2`, `228/243` passing, with the exact
15-item failure map below and complete `243/243/243` coverage:

```text
pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t
pa16/tests/general/200-local-default-class-array-lifecycle.t
pa16/tests/general/200-nested-braced-member-aggregate-init.t
pa16/tests/general/200-reference-indexed-pointer-member-access.t
pa16/tests/general/200-reference-member-class-init.t
pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t
pa16/tests/general/300-callable-field-hides-private-base-method.t
pa16/tests/general/300-friend-function-definition-skip.t
pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t
pa16/tests/general/300-overloaded-deref-user-assignment.t
pa16/tests/general/300-user-defined-string-literal-operator.t
pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t
pa16/tests/general/400-bit-field-prefix-postfix-increment.t
pa16/tests/general/400-signed-bit-field-read.t
pa16/tests/general/400-signed-enum-bit-field-read.t
```

The final serial rerun is also status `2` at `228/243`, with exactly the same
15 normalized identities: retained `15`, authority-only `0`, and fresh-only
`0`; no failure was compensated by another result.  The target
`200-elaborated-member-forward-type.t` is resolved.  Discovered/reference/
fresh inventories remain `243/243/243`, with missing and unexpected counts
all zero.

## Active Checkpoint

The original failure was PA10 syntax routing, before semantic ownership:
`parameter_clause_kind_at` omitted an elaborated specifier at the first
parameter, so `void set(struct Hidden* q)` could be routed as an expression.
The first-position repair uses the shared
`fact_parameter_specifier_start_at` owner in elaborated-only mode for the four
keys `class`, `struct`, `union`, and `enum`, preserving existing built-in
disambiguation.  The duplicate `fact_elaborated_specifier_start_at` was
removed.

For a bare-name prefix such as `(Y0, Y1, struct S*)`, the existing
delimiter-bounded bare-name scan now passes its actual failure cursor to the
same shared fact.  This handles arbitrary subsequent positions without a
hard-coded offset.  The existing parser consumes the declaration once through
`parse_class_declaration(true)`/`parse_enum_declaration(true)` and
`classify_elaborated_specifier`; malformed forms remain normal failures.

PA11 still canonicalizes one `Outer::Hidden` identity through
`ensure_named_class`/`add_type_binding`; PA12 publishes typed member lookup,
call selection, and implicit-object facts; PA15 consumes those facts directly.
The ABI repair validates the `NamedRecord` and delegates named-type component
construction to `value_components(record.owner, record.name)`, so namespace
and class owner traversal has one implementation.

## Validation

Focused validation:

    make -C dev cppgm++
    status 0

The selected PA16 regression plus four parser/semantic controls passed:

    make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/200-elaborated-member-forward-type.t tests/general/100-self-pointer-layout.t tests/general/100-member-methods.t tests/general/200-member-call-hides-outer-type-declaration.t tests/general/200-inherited-member-call-hides-outer-type.t'
    pa16 check: PASS (5/5)

The earlier PA11 control also passes with the rebuilt shared compiler:

    make -C pa11 CPPGM_SKIP_DEV_REBUILD=1 check TEST=tests/general/200-elaborated-type-hidden-by-function.t
    pa11 check: PASS (1/1)

The PA12 elaborated-enum/struct controls pass `3/3`.  Disposable parser
controls return `0` for class/struct/union/enum starts in second, third, and
fourth positions; scoped-enum forms and a comma expression return `0`; four
malformed forms return `1`.  The target LowIR contains
`object=_ZN5Outer3setEPNS_6HiddenE`.  Namespace and nested class/enum ABI
controls contain `_ZN1N3useEPNS_1SE` and
`_ZN5Outer3useEPNS_5InnerEPNS_4KindE` respectively.

The final broad evidence is:

    make test-pa16
    status 2; TEST SUMMARY: 228 / 243 TESTS PASSED; 15 failures

    n=16; ... make test-report-through-pa$((n - 1))
    status 0; ALL TESTS PASSED SUCCESSFULLY! (1167 / 1167)

    perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src
    status 0; six non-fatal bad-division warnings

The six warnings are the existing header-body findings for `abi_mangle.h`,
`cpp_semantic_core.h`, `lowir_model.h`, `pa11_semantic_model.h`,
`pa12_semantic_selection.h`, and `pa15_lowering.h`.

The exact identity comparison exits `0`: authority `15`, fresh `15`, retained
`15`, authority-only `0`, fresh-only `0`; the inventory check is
`243/243/243` with missing/unexpected `0/0` in every pair.  The final diff
check also exits `0`.


## Performance Evidence

These are structural bounds and representative controls, not timing or RSS
measurements.  The first elaborated-key route performs one charged
fixed-token classification with four constant key comparisons and returns
immediately.  Arbitrary later detection reuses the existing bare-name scan
from the opening parenthesis to its delimiter-bounded end; every token
predicate is charged, so this route is O(L) in the current group span and is
still bounded by the parser's `96 * token_count + 2048` work limit.  The reverse
index pass classifies each delimiter group once, and full elaborated-specifier
parsing remains owned by the existing declaration parser.  PA15 ABI named
types reuse `value_components` for the single typed namespace/class owner
walk.  The second/third/fourth-position, scoped-enum, comma-expression,
malformed, namespace, and nested-class/enum controls pass; no unsupported
timing or RSS claim is made.

## Next Checkpoint

This checkpoint removes
`pa16/tests/general/200-elaborated-member-forward-type.t` from the residual
identity set.  The next separate residual checkpoint is
`pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t`;
the other 14 residual identities remain outside this scope.

## Checkpoint Ledger

| checkpoint | compact result |
|---|---|
| d54e32d1 | Prior PA16 authority was 224/243 with exactly 19 failures and 243/243 identity coverage. |
| b58ddd2a | Completed the typed nullptr_t carrier path through PA11/PA12/PA15, including ABI and LowIR ownership and the bounded endpoint audit. |
| e09d8223 | Recorded the completed nullptr carrier audit state: 225/243 authority, exact 18-item residual map, 243/243/243 inventories, through-PA15 1167/1167, and passing file audit with five known warnings. |
| d5bf2600 checkpointAudit | Completed the bounded typed ownership audit and PA12 repair for constructor-overload viability/materialization, inherited candidate visibility, declaration hiding, access/deleted post-selection diagnosis, ambiguous UDC retention, and same-constructor second-SCS ranking. The inheriting-constructor publisher is extracted to the registered 215-line `pa12_semantic_constructor_candidates.cpp`; constructor/lifetime facts are in the normal 89-line `pa12_semantic_constructor_facts.h`, `pa12_semantic_construction.cpp` is 1876 lines, and `pa11_semantic_model.h` is 2374 lines. The final size probe is `ConversionChoice` 56/56 and `ConversionScore` 20/40 parent/current bytes. Focused build/matrix pass 7/7; all discriminating probes pass their expected statuses; final PA16 is 227/243 with exactly the unchanged 16 failures, exact authority/fresh identity sets 16/16 with zero deltas, and 243/243/243 coverage. Through-PA15 is 1167/1167; file audit exits 0 with six exact warnings; diff-check and changed-path/artifact checks pass. No handout/test/reference mutation; next is the separate residual `200-elaborated-member-forward-type.t`. |
| 29d9c4ce checkpointAudit | Completed the bounded PA10 elaborated-member parameter-clause audit and repair plus the PA15 ABI owner consolidation. First-position elaborated keys use one charged shared fact; arbitrary later positions reuse the delimiter-bounded bare-name scan's failure cursor. The target, namespace, and nested class/enum ABI controls pass. Final PA16 is `228/243` with exactly the unchanged 15 residual identities, retained `15`, authority-only/fresh-only `0/0`, and discovered/reference/fresh `243/243/243`; through-PA15 is `1167/1167`; file audit exits `0` with six known warnings; no tests, references, sidecars, harnesses, comparators, generated outputs, coverage rules, or source-set files changed. |
