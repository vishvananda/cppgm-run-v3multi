# PA16 elaborated-member-forward-type checkpoint

## Stage Design

PA10 owns the syntax boundary and the indexed parenthesized-group fact used for
declaration/declarator routing.  PA11 owns typed AST/model identities,
including the canonical `ClassForwardDeclaration` -> named class/type fact.
PA12 owns semantic lookup and selected typed calls; PA15 consumes those facts
for LowIR.  Class value semantics, copy/move transfer, and pass-by-value class
objects remain PA17 scope.

The owner path is:

    tokens -> PA10 indexed parenthesized-group classification
      -> parse_decl_or_function / parse_decl_specifier_seq
      -> ClassForwardDeclaration AST fact
      -> PA11 ensure_named_class + add_type_binding
      -> PA11 member/type facts -> PA12 call resolution
      -> PA15 typed member/call/object LowIR lowering

This follows the `spec.md` one-owner and typed-fact-continuity requirements:
the lookahead is a bounded typed token-shape fact, the class declaration is
parsed once by the existing PA10 owner, PA11 canonicalizes it once, and later
stages consume the resulting identity without text recovery or a duplicate
parser/model.

## Baseline Authority and Failure Map

The pre-landed checkpoint was `225/243` with exactly 18 failures.  Its
discovered/reference/fresh inventory was `243/243/243`; through PA15 was
`1167/1167`; and the file audit passed with five pre-existing bad-division
warnings.  The turn-start authority supplied for landed `d5bf2600` is
`make test-pa16` status `2`, `227/243`, exactly 16 failures, and complete
`243/243/243` coverage.  The two identities removed from the prior 18-item
set are exactly:

```text
pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t
pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t
```

The exact residual budget, unchanged by this bounded audit, is:

```text
pa16/tests/general/200-elaborated-member-forward-type.t
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

The final serial rerun resolves the first identity in this map.  The fresh
residual is exactly the turn-start map minus
`pa16/tests/general/200-elaborated-member-forward-type.t`; authority-only is
that one resolved identity, fresh-only is `0`, and the retained residual count
is `15`.  Discovered/reference/fresh inventories are `243/243/243`.

## Active Checkpoint

The failure is in PA10 syntax routing, before semantic ownership: the indexed
`parameter_clause_kind_at` fact recognizes built-in and identifier-leading
parameter shapes but omitted an elaborated specifier at the first parameter.
Consequently `void set(struct Hidden* q)` was routed as a non-function
parenthesized declarator/initializer and `parse_initializer` reached `struct`
as an expression, producing the observed primary-expression error at token 11.
The data member already reached `parse_decl_specifier_seq` and produced the
same `ClassForwardDeclaration` syntax fact successfully.

The repair adds the four elaborated-specifier keys (`class`, `struct`, `union`,
`enum`) to the shared indexed parameter-clause discriminator.  This is a
bounded, charged token fact at the PA10 parser-support boundary; it does not
parse or resolve a second declaration.  The existing parser then consumes the
specifier through `parse_class_declaration(true)` and
`classify_elaborated_specifier`, while PA11's `spec_fact` continues to call
`ensure_named_class`/`add_type_binding` for one canonical Hidden identity.
PA12 member lookup/call resolution and PA15 LowIR lowering therefore receive
the same typed class, pointer field, method parameter, and implicit object-call
facts as other declarations.

The key invariant is that an elaborated specifier cannot be a primary
expression in a parenthesized declarator: a valid `struct/class/union/enum`
start must stay on the parameter-clause path, while malformed forms still
fail in the normal parser.  No text recovery, duplicate parser/model, or
LowIR-specific behavior was added.

## Validation

Focused implementation validation:

    make -C dev cppgm++
    status 0

The selected PA16 regression plus four parser/semantic controls passed:

    make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/200-elaborated-member-forward-type.t tests/general/100-self-pointer-layout.t tests/general/100-member-methods.t tests/general/200-member-call-hides-outer-type-declaration.t tests/general/200-inherited-member-call-hides-outer-type.t'
    pa16 check: PASS (5/5)

The earlier PA11 control also passes with the rebuilt shared compiler:

    make -C pa11 CPPGM_SKIP_DEV_REBUILD=1 check TEST=tests/general/200-elaborated-type-hidden-by-function.t
    pa11 check: PASS (1/1)

The target emits the checked-in 8-byte Outer layout and lowers
`Outer__set(ptr this, ptr q)`; its status and normalized LowIR comparison pass.

Disposable controls (all outside the repository) compile with status `0`:

    /tmp/pa16-elaborated-class-parameter.cpp
    /tmp/pa16-elaborated-struct-parameter.cpp
    /tmp/pa16-elaborated-union-parameter.cpp
    /tmp/pa16-elaborated-enum-parameter.cpp
    /tmp/pa16-ordinary-parameter-expression.cpp

The final broad evidence is:

    make test-pa16
    status 2; TEST SUMMARY: 228 / 243 TESTS PASSED; 15 failures

    n=16; ... make test-report-through-pa$((n - 1))
    status 0; ALL TESTS PASSED SUCCESSFULLY! (1167 / 1167)

    perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src
    status 0; six non-fatal bad-division warnings

Warnings are the existing header-body findings for `abi_mangle.h`,
`cpp_semantic_core.h`, `lowir_model.h`, `pa11_semantic_model.h`,
`pa12_semantic_selection.h`, and `pa15_lowering.h`.

The fresh residual comparison exits `0`: `15` retained residuals, one
authority-only resolved identity, fresh-only `0`, expected-only `0`, and
unexpected `0`.  The inventory check exits `0` with discovered/reference/fresh
status sidecars `243/243/243`; the failure split is `10` LowIR comparison
failures plus `5` status mismatches.  `git diff --check` exits `0`.  The exact
changed-path audit contains only `dev/src/pa10_parser_support.cpp` and
`pa16/plan.md`; no tests, references, sidecars, harnesses, comparators,
generated outputs, or source-set files changed.


## Performance Evidence

These are structural bounds and representative counts, not timing or RSS
measurements.  The new discriminator performs at most four fixed-token tests
for each indexed parenthesized group, charges each test actually performed,
and returns immediately on an elaborated key.  The existing reverse index pass
still classifies each delimiter group once; this added route does not scan a
class body or qualified name.  Full elaborated-specifier classification remains
owned by the existing parser at the declaration-consumption point.  The four
key controls and ordinary control all pass, with no timing or unsupported
performance claim made.

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
| elaborated-member-forward-type final | Added the PA10 indexed elaborated-specifier parameter-clause fact. Disposable class/struct/union/enum and ordinary controls pass; final PA16 is `228/243` with exactly 15 retained residuals, `243/243/243` inventories, through-PA15 `1167/1167`, file audit status `0` with six warnings, and exact changed paths limited to the implementation and plan. |
