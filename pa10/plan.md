# PA10 Checkpoint Plan and Evidence

## Stage Design

The owned flow is:

~~~text
phase-3 buffer -> posttoken facts -> typed PA10Token -> PA10Parser
    -> PA10Ast -> cold deterministic renderer
~~~

This checkpoint owns the declarator/member-declaration boundary in
`dev/src/pa10_ast.cpp`, its typed vocabulary in `dev/src/pa10_ast.h`, and
presentation-only additions in `dev/src/pa10_renderer.cpp`. Names remain
`PA10Name` components and declarators remain AST structure; parsing does not
flatten either to source text.

`pa10/pa10.gram` directly backs the bit-field list, declarator and
abstract-declarator composition, declarator suffix, parameter, non-type
template-parameter, and ordinary function-suffix productions. In the checked
handout/ref contract, qualified member-pointer operators (`C::*`, `n::C::*`)
extend the grammar's `ptr-operator` production, which currently names only
`*`, `&`, and `&&`. Likewise, checked throw-specification refs require
`throw(...)`, although the current `function-suffix` production lists cv/ref,
noexcept, virt, and trailing-return syntax but not dynamic `throw(...)`.
These are explicitly handout/ref-required syntax extensions, not claims about
the current grammar text. Lambda mutable/noexcept structure follows its
grammar production and checked AST shape. `override`/`final` are classified
once at the posttoken-to-`PA10Token` boundary; their producer spelling stays
cold for rendering.

The prior template-name/angle checkpoint is closed. Its landed typed
template-index/qualified-name path (`a2b82dcb`), historical 457/457
through-PA9 gate, and historical file-audit exit 0 with the one pre-existing
warning remain evidence only; this checkpoint does not reopen that path.

## Failure Map

The turn-start baseline was 106/157 passing with 51 failures. The final full
PA10 run discovered all 157 tests and passed 123/157: 17 baseline identities
were removed, no new identity appeared, and 34 baseline identities remain.

Exact removed identities:

~~~text
pa10/tests/general/200-extern-c-throw-empty-specification.t
pa10/tests/general/200-function-throw-typed-specification.t
pa10/tests/general/200-function-type-alias-declaration.t
pa10/tests/general/200-function-virt-and-noexcept-suffixes.t
pa10/tests/general/200-lambda-declarator.t
pa10/tests/general/200-member-pointer-const-function-declarator.t
pa10/tests/general/200-member-pointer-data-declarator.t
pa10/tests/general/200-member-pointer-function-declarator.t
pa10/tests/general/200-qualified-member-comparison-template-arg.t
pa10/tests/general/200-qualified-result-parenthesized-member-pointer-declarator.t
pa10/tests/general/200-template-function-type-alias-declaration.t
pa10/tests/general/200-template-function-type-alias-pack-declaration.t
pa10/tests/general/200-zero-width-bit-field-declaration.t
pa10/tests/general/300-local-typedef-shadows-value-qualified-type.t
pa10/tests/general/300-namespace-alias-shadow-qualified-type.t
pa10/tests/spec/200-bit-field-declaration.t
pa10/tests/spec/200-non-type-template-parameters.t
~~~

No newly failing identity was observed. Exact remaining identities:

~~~text
pa10/tests/general/100-member-operator-name-call.t
pa10/tests/general/200-allocation-array-operator-ids.t
pa10/tests/general/200-attributed-partial-specialization-current-class-constructor.t
pa10/tests/general/200-attributed-virtual-destructor-member.t
pa10/tests/general/200-builtin-function-style-cast-expression.t
pa10/tests/general/200-builtin-function-style-cast-member-body.t
pa10/tests/general/200-conditional-simple-type-shift-return.t
pa10/tests/general/200-elaborated-enum-member-declarators.t
pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t
pa10/tests/general/200-friend-function-template-declaration.t
pa10/tests/general/200-friend-type-declaration.t
pa10/tests/general/200-global-struct-paren-declaration.t
pa10/tests/general/200-lambda-capture-forms.t
pa10/tests/general/200-literal-operator-id.t
pa10/tests/general/200-local-typedef-paren-declaration.t
pa10/tests/general/200-member-template-parameter-value-vs-template-name.t
pa10/tests/general/200-mock-type-declaration-ambiguity.t
pa10/tests/general/200-parenthesized-new-type-vs-placement.t
pa10/tests/general/200-partial-specialization-current-class-constructor.t
pa10/tests/general/200-placement-new-identifier-led-initializer.t
pa10/tests/general/200-placement-new-pack-init.t
pa10/tests/general/200-qualified-conversion-operator-definition.t
pa10/tests/general/200-qualified-enumerator-call-argument.t
pa10/tests/general/200-sizeof-elaborated-class-type-id.t
pa10/tests/general/200-template-conditional-simple-type-shift-return.t
pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t
pa10/tests/general/200-template-qualified-conversion-operator-dependent-result.t
pa10/tests/general/200-template-qualified-inline-attribute-constructor-definition.t
pa10/tests/general/200-template-qualified-inline-constructor-definition.t
pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t
pa10/tests/general/200-trailing-parameter-vendor-attribute.t
pa10/tests/general/200-typeid-postfix-member-suffix.t
pa10/tests/spec/200-explicit-instantiation-declaration.t
pa10/tests/spec/200-qualified-special-member-definition.t
~~~

The focused checked-in cluster after the audit corrections passed 16/16.
The supervisor's first-stop rerun also passed the 14 former failures 14/14;
the earlier first-stop sample passed 32/32 before this final broad gate.

## Active Checkpoint

The shared owner/data flow implements:

- named and abstract declarator composition, parenthesized declarators,
  function-type suffixes, and one central cv/ref/noexcept/virt/trailing-return
  suffix path;
- structured qualified member-pointer ptr-operators and their function/data
  uses;
- non-type/template-template parameter packs and checked defaults;
- named, unnamed, and unnamed zero-width class bit-fields;
- checked dynamic throw type-id suffixes, linkage declarations, and lambda
  mutable/noexcept nodes.

`member_pointer_operator_start()` scans only the qualified prefix from the
current position. Every absolute access is size-checked; template closing is
looked up through the existing prebuilt index; each material component costs a
`charge()`. The consumer uses the already-established form, avoiding another
qualified-prefix scan. `declaration_start()` uses bounded absolute helpers,
one qualified-prefix scan, charged component work, and deterministic false
results for truncated/malformed prefixes. Ordinary work is linear in the
tokens/components of a declaration (with the existing linear template-index
prepass), bounded by the parser work and recursion ceilings; no backtracking or
whole-input retry loop was added. The closed template-angle path was not
rewritten.

## Performance Evidence

The final implementation binary used for the bounded characterization had
SHA-256 `39157089d24a0a1d7a42a867cb67aee606b983ddc1bf0ba036fce63d6f2ba892`.
Inputs were temporary files under `/tmp`; each valid input was
`typedef int C0::C1::...::C(n-1)::\*p;`, and the malformed input truncated
after the final `::`.

~~~text
components  bytes  expected  exit  elapsed  peak-RSS
32           166    success   0     0.00s    4184 KB
128          674    success   0     0.00s    4060 KB
256          1442   success   0     0.00s    4444 KB
256          1439   failure   1     0.00s    4596 KB  (truncated boundary case)
~~~

All elapsed values were below `/usr/bin/time`'s 0.01-second display
resolution, so no general performance comparison is claimed. The structural
evidence is that the component prefix grows from 32 to 256 while the bounded
lookahead charges once per component, uses O(1) preindexed template-close
lookups, and does not rescan the consumed qualifier in `parse_ptr_operator`.

## Checkpoint Ledger

| checkpoint | status | evidence / intent |
| --- | --- | --- |
| `a2b82dcb` template/angle ownership | landed and closed | typed sidecars; historical 457/457 through PA9; historical audit exit 0 with one pre-existing warning |
| declarator/member-declaration boundary | validated | 16/16 focused; full 123/157; 17 baseline failures removed; zero new identities |
| through-PA9 gate | passed | exact `n=10` command returned `ALL TESTS PASSED SUCCESSFULLY! (457 / 457)` |
| file audit / diff check | passed | audit passed with only pre-existing `cpp_semantic_core.h:1` warning; `git diff --check` passed |
| commit | authorized | one coherent PA10 commit containing exactly the four intended files; subject is recorded below |

Commit intent: `PA10: unify declarator and member declaration parsing`.
