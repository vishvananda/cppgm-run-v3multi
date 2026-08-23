# PA10 Checkpoint Plan and Evidence

## Current scope

This bounded checkpoint audits landed commit
`27623d646279d867e58039af60a1cc52e09e090e`, `PA10: unify declarator and
member declaration parsing`. The owned flow is:

```text
phase-3 buffer -> posttoken facts -> typed PA10Token -> PA10Parser
    -> PA10Ast and sidecars -> deterministic cold renderer
```

The implementation scope is limited to `dev/src/pa10_ast.cpp`, the private
typed `dev/src/pa10_declarator_shape.cpp`/`.h` helper, `dev/src/pa10_ast.h`,
and `dev/src/pa10_renderer.cpp`; the helper is listed only for `cppgm++` in
`dev/frontend_source_sets.mk`. Documentation is limited to this file and
`pa10/audit.md`; no tests, refs, grammar, harness, or unrelated stage surface
was edited.

## Spec and grammar alignment

The flow satisfies the applicable root `spec.md` §§1-4 and §7 obligations:
one forward parser/model/renderer path; typed producer spelling and contextual
facts; component-owned qualified names; typed template, literal, operator, and
sidecar facts; bounded indexed lookahead; monotonic parser work; and a cold
presentation boundary with no render/reparse path.

The directly aligned `pa10/pa10.gram` productions are:

- bit-field declaration/list and class-member consumption;
- named/abstract declarators, parenthesized declarators, ptr operators,
  arrays, function suffixes, parameters, and default arguments;
- non-type and template-template parameter structure;
- lambda-declarator pieces.

The checked handout/ref contract additionally includes linkage
specifications (`extern "C++"`/`extern "C"`), although the current grammar has
no named linkage-specification production; it mentions `KW_EXTERN` only in
decl-specifiers and extern-template syntax. It also requires qualified
member-pointer ptr-operators (`C::*`, `n::C::*`, and qualified template forms),
although the current grammar literally lists only `*`, `&`, and `&&`.
Checked throw-specification refs likewise require dynamic `throw(...)`, absent
from the current `function-suffix` production. These are explicit
handout/ref-required extensions, not grammar edits.

## Audit repairs and ownership result

The former recursive `declarator_has_parameter()` test was incorrect at the
function-definition/initializer boundary: it treated `(*p)()` and `(&r)()`
as functions and could return at an identifier before seeing a later array
layer. The public parser owner now uses a tri-state nearest-derived-operator
walk in `declarator_is_function()`. Nested declarators recurse only when they
contain an actual operator; parentheses defer to the enclosing layer. The
first operator outward from the identifier decides: `ParameterClause` means
function, while `PtrOperator`/reference or `ArraySuffix` means object. This
keeps `f()` and `(f)()` definitions, makes pointer/reference and array shapes
take their initializers, and keeps an inner `h()` parameter clause decisive
for a function returning a pointer.

`member_pointer_operator_start()` remains a bounded prefix scan using
charged components and preindexed template closes. `parse_ptr_operator()`
consumes the established qualifier without rescanning it. The type-id
lookahead now passes a one-shot checked-prefix fact into
`parse_abstract_declarator()`, removing that path's former duplicate
qualification scan. Contextual `override`/`final` is classified once at
the posttoken boundary. Bit-fields, suffixes, parameters, template
parameters, linkage, lambda declarators, typed identities, sidecar ranges,
recursion ceilings, and cold rendering remain on the unified landed path.
The shape walk is conventionally formatted in its private helper; no source
line packing or unrelated whitespace squeeze is used to meet an audit limit.

## Coverage and exact failure identity

The turn-start landed-checkpoint result was the no-regression broad baseline:
all 157 PA10 tests were discovered, 123 passed, exit 2, with exactly 34
failures. The final authorized run is identical: 157 discovered, 123 passed,
exit 2, with 34 exact identities and no additions or removals.

The earlier landed-checkpoint comparison was 106/157 with 51 failures. These
17 identities were removed by the landed increment, with no newly failing
identity:

```text
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
```

Exact remaining baseline/final failure map for the landed full result:

```text
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
```

These residual identities are outside this audit's declared family and were
not broadened. Historical focused evidence before this repair included the
changed 16/16 cluster, a 14/14 first-stop former-failure rerun, and a 32/32
sample; those counts are retained as historical handoff evidence. Current
focused checked-in evidence is 23/23 after the repair, with direct,
parenthesized, pointer/reference, function-returning-pointer, nested-array,
array-of-function-pointer, member-pointer-template, and truncation `/tmp`
probes exercised.

The final through-PA9 command passed 457/457. The final file audit exited 0
with only the pre-existing `dev/src/cpp_semantic_core.h:1` `bad-division`
warning. The final source sizes are 2999 lines for `dev/src/pa10_ast.cpp`,
53 for `dev/src/pa10_declarator_shape.cpp`, 16 for its declaration-only
private header, 361 for `dev/src/pa10_ast.h`, and 756 for
`dev/src/pa10_renderer.cpp`; the helper files are below their audit limits.

## Performance evidence

The current repaired executable is SHA-256
`13e4d2f60d7bf1a19599d69d55a61bf958cf3af720aa9153e6695a4f168268b6`.
Single bounded runs, not interleaved comparisons, were below the 0.01-second
timer resolution:

| input | result | elapsed | peak RSS |
| --- | --- | ---: | ---: |
| qualified member-pointer edge probe | success | 0.00 s | 4116 KB |
| nearest-operator boundary probe | success | 0.00 s | 4116 KB |
| checked qualified-member input | success | 0.00 s | 4116 KB |

The structural evidence is one linear prefix scan per consumed
member-pointer form, O(1) preindexed template-close lookup, one-shot
type-id lookahead ownership, and no retry/backtracking. No timing comparison
is claimed.

Historical performance characterization from the prior template/angle
checkpoint is retained but is not a current binary/layout claim:

| components | bytes | expected | exit | elapsed | peak RSS |
| ---: | ---: | --- | ---: | ---: | ---: |
| 32 | 166 | success | 0 | 0.00 s | 4184 KB |
| 128 | 674 | success | 0 | 0.00 s | 4060 KB |
| 256 | 1442 | success | 0 | 0.00 s | 4444 KB |
| 256 truncated | 1439 | failure | 1 | 0.00 s | 4596 KB |

The prior 140-argument, nested-angle, relational, sibling-scope, and renderer
depth probes remain historical evidence only; no unverified comparison is
made.

## Checkpoint ledger

| checkpoint | status | evidence / intent |
| --- | --- | --- |
| `27623d646279d867e58039af60a1cc52e09e090e` declarator/member-declaration boundary | completed bounded audit and clean helper extraction | 23/23 focused; required valid and malformed `/tmp` probes; private shape helper linked to `cppgm++`; PA10 123/157 with exact 34-identity baseline and 157 discovery; through-PA9 457/457; file audit exit 0 with one pre-existing warning |

Historical ledger context: `a2b82dcb` template/angle ownership is landed and
closed; its typed sidecars, historical through-PA9 457/457, and prior
file-audit warning record are not reopened here.

## Next checkpoint

The next checkpoint is a supervisor-selected family from the exact 34 residual
identities. Keep this declarator/member boundary closed and do not broaden the
implementation without a separately bounded audit.
