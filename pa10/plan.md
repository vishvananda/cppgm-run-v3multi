# PA10 Checkpoint Plan and Evidence

## Scope and specification alignment

This checkpoint covers the landed template-id, qualified-name,
template-argument, decltype-prefix, logical-angle, and exact-renderer path:

```text
posttoken facts -> typed PA10Token -> PA10 parser/indexes -> PA10Ast -> renderer
```

`PPSpellingId`, typed `PA10TemplateArgumentKind`, component ranges, and
decltype prefix nodes remain canonical facts. `build_template_indexes()` is a
charged linear pass with delimiter-scoped angle stacks. `pa10/pa10.gram`
requires `OP_GT` for relational operators and exactly
`ST_RSHIFT_1 ST_RSHIFT_2` for right shift (lines 474-488); its close-angle
production accepts either logical piece (706-709). PA6 README lines 124-143
specify the same replacement of `OP_RSHIFT`. This satisfies root `spec.md`
§§1-4 and §7 for one path, typed continuity, bounded work, cold presentation,
and measured characterization without claiming a performance comparison.

Standalone versus qualified decltype is structural: a name-prefix sidecar
with no components is standalone, while one or more components means the
prefix is followed by `::`. There is no standalone-decltype boolean in either
`PA10Name` or `PA10AstNode`; `append_name()` follows that invariant. A real
shift requires the adjacent two-piece sequence. The rejected synthetic
leftover-piece transitions are not part of this plan.

## Preserved landed evidence (pre-repair)

The parent `43703613` record was 77/157. The landed `a2b82dcb` record was
105/157, with 52 failures, 28 removed identities, and zero new identities.
Its focused set was 11/11, the warning-clean build passed, through-PA9 was
457/457, and file audit exited 0 with one pre-existing warning:
`dev/src/cpp_semantic_core.h:1` (`bad-division`).

Its bounded executable characterization was: 140 template arguments
`0.00s/4340 KB`; triple closes plus a separate shift `0.00s/4120 KB`;
sibling delimiter scopes `0.00s/4352 KB`; relational 32/128/256
`0.00s/4372 KB`, `0.00s/4812 KB`, `0.02s/5816 KB`; and 512 pairs at the
renderer depth guard `0.02s/7980 KB`. Earlier storage evidence recorded
32/128/512 declarations as source bytes `438/1810/7570`, PP tokens and AST
nodes `289/1153/4609`, child edges/capacity `288/288`, `1152/1152`,
`4608/4608`, literal nodes/bytes/capacity `32/128/128`, `128/512/512`,
`512/2048/2048`, and producer bytes `295/611/2147`. These are historical
measurements, not current executable or layout claims.

## Final failure map and validation

The final `make test-pa10` discovered all 157 tests, exited 2, and passed
106/157. The preserved turn-start log had 52 failure identities; the exact
set delta is one removal and zero additions:

```text
removed: general/200-decltype-base-and-mem-initializer.t
added:   none
```

The exact 46 status-failure identities are:

```text
general/100-member-operator-name-call.t
general/200-allocation-array-operator-ids.t
general/200-attributed-partial-specialization-current-class-constructor.t
general/200-attributed-virtual-destructor-member.t
general/200-builtin-function-style-cast-expression.t
general/200-builtin-function-style-cast-member-body.t
general/200-conditional-simple-type-shift-return.t
general/200-elaborated-enum-member-declarators.t
general/200-extern-c-throw-empty-specification.t
general/200-forward-unknown-nested-template-in-ctor-body.t
general/200-friend-type-declaration.t
general/200-function-throw-typed-specification.t
general/200-function-type-alias-declaration.t
general/200-function-virt-and-noexcept-suffixes.t
general/200-lambda-capture-forms.t
general/200-lambda-declarator.t
general/200-literal-operator-id.t
general/200-member-pointer-const-function-declarator.t
general/200-member-pointer-data-declarator.t
general/200-member-pointer-function-declarator.t
general/200-member-template-parameter-value-vs-template-name.t
general/200-mock-type-declaration-ambiguity.t
general/200-parenthesized-new-type-vs-placement.t
general/200-placement-new-identifier-led-initializer.t
general/200-placement-new-pack-init.t
general/200-qualified-conversion-operator-definition.t
general/200-qualified-enumerator-call-argument.t
general/200-qualified-result-parenthesized-member-pointer-declarator.t
general/200-sizeof-elaborated-class-type-id.t
general/200-template-conditional-simple-type-shift-return.t
general/200-template-function-type-alias-declaration.t
general/200-template-function-type-alias-pack-declaration.t
general/200-template-member-definition-inherited-typedef-cast.t
general/200-template-qualified-conversion-operator-dependent-result.t
general/200-template-qualified-inline-attribute-constructor-definition.t
general/200-template-qualified-inline-constructor-definition.t
general/200-trailing-parameter-carries-dependency-attribute.t
general/200-trailing-parameter-vendor-attribute.t
general/200-typeid-postfix-member-suffix.t
general/200-zero-width-bit-field-declaration.t
general/300-local-typedef-shadows-value-qualified-type.t
general/300-namespace-alias-shadow-qualified-type.t
spec/200-bit-field-declaration.t
spec/200-explicit-instantiation-declaration.t
spec/200-non-type-template-parameters.t
spec/200-qualified-special-member-definition.t
```

The five dump mismatches are
`general/200-friend-function-template-declaration.t`,
`general/200-global-struct-paren-declaration.t`,
`general/200-local-typedef-paren-declaration.t`,
`general/200-partial-specialization-current-class-constructor.t`, and
`general/200-qualified-member-comparison-template-arg.t`. The remaining
51 identities are outside this bounded repair.

Final command results:

- warning-clean PA10 build with `-Wextra -Werror`: exit 0;
- `make test-pa10`: exit 2, all 157 discovered, 106 pass / 51 fail;
- required `n=10 ... make test-report-through-pa$((n - 1))`: exit 0,
  457/457 through PA9;
- `perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src`: exit 0,
  one warning, the pre-existing `cpp_semantic_core.h:1` bad-division warning;
- `git diff --check`: exit 0.

Current executable characterization uses SHA-256
`6cf44a43a293399ec829c6423526516cb502ec3b198b74c30209589922d665df`:

| probe | result | wall / peak RSS |
| --- | --- | ---: |
| 140 template arguments | success | 0.00 s / 4328 KB |
| nested `foo<bar<int>>` closes | success | 0.00 s / 4076 KB |
| real `(x >> 8)` shift | success | 0.00 s / 4148 KB |
| relational pairs 32 / 128 / 256 | success | 0.00 / 0.00 / 0.00 s; 4124 / 4124 / 4308 KB |
| 1100 parenthesized expressions | bounded recursion failure | 0.01 s / 8812 KB |

These are bounded samples, not the interleaved median comparison required for
a general §7 performance claim. A malformed delimiter probe also exits 1.

## Next implementation checkpoint

At the next PA10 implementation checkpoint, select one remaining residual
grammar family under supervisor direction, repair only its public owner in
`dev/`, and repeat the full identity and through-stage gates. Do not reopen
the closed template-name/angle ownership path without new authoritative
evidence.

## Completed checkpoint

| checkpoint | status | evidence / next action |
| --- | --- | --- |
| `a2b82dcb56245406f695c271a44ca55ca82f3949` template-id / qualified-name audit | complete | structural decltype ownership, grammar-faithful RShift handling, renderer range checks, 106/157 PA10 with one removed and zero added identities, 457/457 through PA9, file audit exit 0 with one pre-existing warning; next is one supervisor-selected residual family |
