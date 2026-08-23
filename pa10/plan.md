# PA10 Checkpoint Plan and Evidence

## Stage Design

The single production path is:

```text
phase-3 buffer -> posttoken facts -> typed PA10Token -> PA10Parser
    -> PA10Ast typed names/declarators and cold presentation sidecars -> renderer
```

This follows root `spec.md` Purpose and §§1-4: one parser/model/renderer
pipeline, typed fact continuity, component-owned qualified names, cold
presentation only at the dump boundary, and bounded work.  The grammar owner
is `declarator-id -> id-expression -> unqualified-id`, including qualified
identifier/template/operator/conversion/literal/destructor finals, with the
special-member/function-definition and postfix-suffix routes sharing it.
`dev/src/pa10_parser_support.{h,cpp}` owns typed posttoken collection, indexed
template/delimiter facts, generic attribute balancing, and bounded special-
member lookahead; it is linked only into `cppgm++`.

## Failure Map

Turn-start baseline: 157 discovered, 123 passed, 34 failed; through-PA9 was
457/457.  The exact 34 identities were:

Owned checkpoint cluster (12):

```text
pa10/tests/general/100-member-operator-name-call.t
pa10/tests/general/200-allocation-array-operator-ids.t
pa10/tests/general/200-attributed-partial-specialization-current-class-constructor.t
pa10/tests/general/200-attributed-virtual-destructor-member.t
pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t
pa10/tests/general/200-literal-operator-id.t
pa10/tests/general/200-qualified-conversion-operator-definition.t
pa10/tests/general/200-template-qualified-conversion-operator-dependent-result.t
pa10/tests/general/200-template-qualified-inline-attribute-constructor-definition.t
pa10/tests/general/200-template-qualified-inline-constructor-definition.t
pa10/tests/spec/200-explicit-instantiation-declaration.t
pa10/tests/spec/200-qualified-special-member-definition.t
```

Residual cluster (22):

```text
pa10/tests/general/200-builtin-function-style-cast-expression.t
pa10/tests/general/200-builtin-function-style-cast-member-body.t
pa10/tests/general/200-conditional-simple-type-shift-return.t
pa10/tests/general/200-elaborated-enum-member-declarators.t
pa10/tests/general/200-friend-function-template-declaration.t
pa10/tests/general/200-friend-type-declaration.t
pa10/tests/general/200-global-struct-paren-declaration.t
pa10/tests/general/200-lambda-capture-forms.t
pa10/tests/general/200-local-typedef-paren-declaration.t
pa10/tests/general/200-member-template-parameter-value-vs-template-name.t
pa10/tests/general/200-mock-type-declaration-ambiguity.t
pa10/tests/general/200-parenthesized-new-type-vs-placement.t
pa10/tests/general/200-partial-specialization-current-class-constructor.t
pa10/tests/general/200-placement-new-identifier-led-initializer.t
pa10/tests/general/200-placement-new-pack-init.t
pa10/tests/general/200-qualified-enumerator-call-argument.t
pa10/tests/general/200-sizeof-elaborated-class-type-id.t
pa10/tests/general/200-template-conditional-simple-type-shift-return.t
pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t
pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t
pa10/tests/general/200-trailing-parameter-vendor-attribute.t
pa10/tests/general/200-typeid-postfix-member-suffix.t
```

Final PA10 result: 157 discovered, 135 passed, 22 failed.  Removed identities
(12, with no additions) were:

```text
pa10/tests/general/100-member-operator-name-call.t
pa10/tests/general/200-allocation-array-operator-ids.t
pa10/tests/general/200-attributed-partial-specialization-current-class-constructor.t
pa10/tests/general/200-attributed-virtual-destructor-member.t
pa10/tests/general/200-literal-operator-id.t
pa10/tests/general/200-qualified-conversion-operator-definition.t
pa10/tests/general/200-template-qualified-conversion-operator-dependent-result.t
pa10/tests/general/200-template-qualified-inline-attribute-constructor-definition.t
pa10/tests/general/200-template-qualified-inline-constructor-definition.t
pa10/tests/general/200-partial-specialization-current-class-constructor.t
pa10/tests/spec/200-explicit-instantiation-declaration.t
pa10/tests/spec/200-qualified-special-member-definition.t
```

The exact final residual set is:

```text
pa10/tests/general/200-builtin-function-style-cast-expression.t
pa10/tests/general/200-builtin-function-style-cast-member-body.t
pa10/tests/general/200-conditional-simple-type-shift-return.t
pa10/tests/general/200-elaborated-enum-member-declarators.t
pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t
pa10/tests/general/200-friend-function-template-declaration.t
pa10/tests/general/200-friend-type-declaration.t
pa10/tests/general/200-global-struct-paren-declaration.t
pa10/tests/general/200-lambda-capture-forms.t
pa10/tests/general/200-local-typedef-paren-declaration.t
pa10/tests/general/200-member-template-parameter-value-vs-template-name.t
pa10/tests/general/200-mock-type-declaration-ambiguity.t
pa10/tests/general/200-parenthesized-new-type-vs-placement.t
pa10/tests/general/200-placement-new-identifier-led-initializer.t
pa10/tests/general/200-placement-new-pack-init.t
pa10/tests/general/200-qualified-enumerator-call-argument.t
pa10/tests/general/200-sizeof-elaborated-class-type-id.t
pa10/tests/general/200-template-conditional-simple-type-shift-return.t
pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t
pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t
pa10/tests/general/200-trailing-parameter-vendor-attribute.t
pa10/tests/general/200-typeid-postfix-member-suffix.t
```

There is no discovery or coverage loss: the final set is a strict subset of
the turn-start set.

## Active Checkpoint

Scope is complete: one structured name/declarator-id route now carries typed
unqualified-id, operator, conversion, literal, destructor, and name-component
facts into the existing renderer; special-member routing is contextual;
attributes use generic balanced delimiters; member specifiers are typed; and
explicit instantiation uses the existing declaration route.  The forward
nested-template constructor remains red at its constructor-body
`typedef typename alloc<Y>::type alloc_t;` declaration, outside this coherent
name/special-member increment.

Non-goals remain lookup, deduction, unrelated cast/new/lambda/declaration
ambiguity residuals, grammar/fixture/harness changes, and any parallel
parse/render representation.  The focused command was:

```text
make -C pa10 check TEST='tests/general/100-member-operator-name-call.t tests/general/200-allocation-array-operator-ids.t tests/general/200-attributed-partial-specialization-current-class-constructor.t tests/general/200-attributed-virtual-destructor-member.t tests/general/200-forward-unknown-nested-template-in-ctor-body.t tests/general/200-literal-operator-id.t tests/general/200-qualified-conversion-operator-definition.t tests/general/200-template-qualified-conversion-operator-dependent-result.t tests/general/200-template-qualified-inline-attribute-constructor-definition.t tests/general/200-template-qualified-inline-constructor-definition.t tests/spec/200-explicit-instantiation-declaration.t tests/spec/200-qualified-special-member-definition.t tests/general/100-special-member-definitions.t tests/general/100-member-declarations.t tests/general/100-operators-pm.t tests/general/200-global-qualified-pointer-conversion.t tests/general/200-member-pointer-data-declarator.t tests/general/200-member-pointer-function-declarator.t tests/general/200-member-pointer-const-function-declarator.t tests/spec/100-nested-declarator.t tests/spec/100-array-declarator.t'
```

It reported `FAIL (20/21)`: the 11 repaired owned identities had expected
`EXIT_SUCCESS` and exact output, the forward identity had expected success but
actual failure, and all nine named regression siblings were exact green.

## Performance Evidence

The structural bound is one charged monotonic pass over the indexed token
stream for template/delimiter facts; special-member prefix lookahead advances
once, balanced attributes consume each token once, and template closes are
indexed.  No qualified name or conversion type-id is reparsed or backtracked.
The final parser source is 2917 lines; the private support implementation is
552 lines.

Measured with `/usr/bin/time` and `/tmp` outputs, repeating each valid fixture
256 times to avoid timer granularity:

```text
qualified Box<T>::operator Val<Box::value_type>() x256: elapsed=0.02s, peak RSS=4064 KB, exit=0
inline __attribute__((visibility("hidden"))) C<T>::C(C&&) x256: elapsed=0.02s, peak RSS=4300 KB, exit=0
C::operator+(int) const { return 0; } (malformed boundary): elapsed=0.00s, peak RSS=4432 KB, exit=1
```

Required gates: exact `n=10` through-PA9 command passed `457 / 457` with exit
0; `perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src` passed with
one pre-existing warning for `dev/src/cpp_semantic_core.h:1`; and
`git diff --check` passed.

## Checkpoint Ledger

| checkpoint | status | evidence / intent |
| --- | --- | --- |
| `a2b82dcb` template/angle ownership | landed | typed template components/sidecars and bounded close ownership |
| `27623d64` declarator/member-declaration boundary | landed | unified declarator/member path; historical focused and through-PA9 evidence |
| `b9b58b9c` declarator-boundary audit | landed baseline | 157 discovered, 123/157 pass, exact 34 failures; through-PA9 457/457; one pre-existing audit warning |
| `PA10: route structured names and special members` | committed by subject | 157/157 discovered, 135 pass, exact 12 removed and 22 residual; focused 20/21; through-PA9 457/457; file audit pass; measured probes above |
