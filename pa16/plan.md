# PA16 implementation plan

## Stage Design

This checkpoint extends the existing typed PA11 owner for class-qualified and
nested type lookup.  PA10 retains qualified components structurally and keeps
one `decltype` nested-name root in its AST sidecar.  PA11 resolves that root
at the source point to a `TypeId`, resolves each remaining component through
typed `ScopeId`/`TypeId` facts, and returns the selected `BindingId` when a
stored declaration owns the result.  Qualified class lookup searches the
current typed class scope first, then only its validated direct-base chain;
the injected class identity is a typed record fact, not synthesized source
text.  PA12 and PA15 consume these TypeId/ScopeId/BindingId results through
the existing type and lowering paths; they do not reconstruct names or
rescan the program.

The implementation follows spec.md §§2--5 and 7: one typed fact pipeline,
source-point-aware lookup, deterministic language-relevant graph work, and
typed lowering.  This checkpoint excludes access control, constructors,
operator lookup, lifetime behavior, unrelated parser recovery, and broad
template behavior.

## Failure Map

The clean turn-start is HEAD `f290784f9a63c8723fcf617bfcd36c9dc080de7e`:
`167/243` PA16 identities passed, `76` failed, and `243/243` were covered;
PA1--PA15 were `1167/1167`, and the file audit passed.  The authoritative
full log is `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.

Selected family and diagnosed owner:

- `general/100-qualified-typedef-cstyle-cast-same-name-operand.t` — PA10
  cast lookahead stopped at the first identifier of `owner::mask`; repaired.
- `general/200-inherited-injected-class-name-qualified-type.t` — PA11 final
  qualified lookup did not traverse the typed base edge or recognize the
  base's injected class identity; repaired.
- `general/200-qualified-inherited-member-typedef.t` — the same PA11
  qualified inherited-type owner path through `I`/`Derived`; repaired.
- `general/300-alignas-out-of-class-nested-type.t` — PA10 elaborated-specifier
  classification stopped before a qualified class-definition name; repaired.
- `spec/100-decltype-qualified-nested-type-local.t` — PA10 declaration
  routing/specifier parsing did not admit `decltype(source)::type`, and PA11
  had no typed nested-name owner path; repaired.

An additional same-owner baseline repair is
`general/200-nested-class-private-enclosing-access.t`: its out-of-class
qualified nested-class header now uses the same PA10 qualified-name route;
no access-control behavior was changed.

The complete 76-identity turn-start set (all paths relative to `pa16/tests/`)
is:

```text
general/100-function-pointer-nested-param-name-shadow.t
general/100-global-aggregate-nested-array-initializer.t
general/100-global-reference-incomplete-referent.t
general/100-object-member-enumerator-constant.t
general/100-qualified-typedef-cstyle-cast-same-name-operand.t
general/200-aliased-base-mem-initializer-match.t
general/200-const-subobject-member-call.t
general/200-defaulted-constructor-still-aggregate.t
general/200-deleted-constructor-still-aggregate.t
general/200-destructor-body-local-before-base-destruction.t
general/200-elaborated-member-forward-type.t
general/200-extern-class-object-declaration.t
general/200-external-ctor-overload-nonfirst-argument.t
general/200-friend-derived-access-inherited-protected-field.t
general/200-friend-derived-private-base-defaulted-constructor.t
general/200-friend-intermediate-derived-protected-base-method.t
general/200-global-constructor.t
general/200-global-function-style-constructor.t
general/200-inherited-injected-class-name-qualified-type.t
general/200-local-default-class-array-lifecycle.t
general/200-member-call-hides-outer-type-declaration.t
general/200-member-object-lifetime.t
general/200-mutable-member-const-method.t
general/200-nested-braced-member-aggregate-init.t
general/200-nested-class-private-enclosing-access.t
general/200-nested-out-of-class-constructor-enclosing-type.t
general/200-nonliteral-field-condition-not-folded.t
general/200-placement-new-expression-aggregate-brace.t
general/200-placement-new-expression-constructor-call.t
general/200-pointer-subscript-class-reference-return.t
general/200-protected-member-typedef-access-bad.t
general/200-qualified-friend-function-member-access.t
general/200-qualified-inherited-member-typedef.t
general/200-reference-indexed-pointer-member-access.t
general/200-reference-member-class-init.t
general/200-string-literal-does-not-convert-to-mutable-void-pointer.t
general/200-unnamed-namespace-hidden-friend-single-definition.t
general/300-adl-associated-namespace-does-not-climb-parents.t
general/300-adl-using-declaration-source-point.t
general/300-alignas-out-of-class-nested-type.t
general/300-callable-field-hides-private-base-method.t
general/300-class-using-declaration-reexposes-protected-field.t
general/300-compound-assignment-adl-nonmember-after-member-reject.t
general/300-const-pointer-explicit-destructor-call.t
general/300-enum-class-nonmember-operator-bitand.t
general/300-explicit-destructor-call-enclosing-namespace-type.t
general/300-friend-function-definition-skip.t
general/300-header-static-class-init.t
general/300-member-vs-nonmember-operator-implicit-object-cv-rank.t
general/300-mixed-member-free-shift-stress-chain.t
general/300-nested-enum-hidden-friend-bitmask-adl.t
general/300-operator-nullptr-t-from-zero.t
general/300-operator-shift-stress-chain.t
general/300-overloaded-deref-user-assignment.t
general/300-packed-class-layout.t
general/300-pragma-pack-followed-by-endif.t
general/300-prvalue-derived-base-friend-operator.t
general/300-scalar-pseudo-destructor-call.t
general/300-static-class-member-object-definition.t
general/300-synthesized-array-member-lifecycle.t
general/300-thread-local-synthetic-symbol-family-isolation.t
general/300-unary-address-of-builtin-fallback.t
general/300-user-defined-string-literal-operator.t
general/300-using-base-static-same-signature-derived-preferred.t
general/300-using-declaration-function-hides-tag.t
general/300-value-init-aggregate-with-nontrivial-member.t
general/400-bit-field-constructor-member-init.t
general/400-bit-field-member-access-bad.t
general/400-bit-field-prefix-postfix-increment.t
general/400-bitfield-aggregate-init.t
general/400-signed-bit-field-read.t
general/400-signed-enum-bit-field-read.t
general/500-inherited-constructor-using-access.t
general/500-inheriting-constructors.t
general/500-inheriting-external-transitive-constructor.t
spec/100-decltype-qualified-nested-type-local.t
```

## Active Checkpoint

The completed coherent increment changes these seven implementation files:

- `dev/src/pa10_ast.cpp`
- `dev/src/pa10_parser_support.cpp`
- `dev/src/pa10_parser_support.h`
- `dev/src/pa11_semantic.cpp`
- `dev/src/pa11_semantic_core.cpp`
- `dev/src/pa11_semantic_model.h`
- `dev/src/pa11_semantic_types.cpp`

It also updates this plan.  It repairs all five selected identities plus the
additional same-owner nested-class header identity without adding tests or
changing fixtures.  The preservation controls cover qualified method
definitions, inherited unqualified typedefs, injected-name hiding, lazy
enclosing aliases, and source-point using-directive behavior.  The final
implementation keeps PA11 as the typed lookup owner and leaves PA12/PA15 as
TypeId/ScopeId/BindingId consumers.

## Performance Evidence

The parser's new qualified-name probes are bounded by the delimiter/template
indexes and the number of name components; they do not search source text or
the translation unit.  PA11 first probes the requested class scope and then
walks the validated single-inheritance chain once, with the existing marked
lookup graph handling only relevant using/inline edges.  Thus the structural
risk is proportional to qualifier depth plus language-relevant base/lookup
edges, not same-name noise elsewhere in the program.  The representative
`owner::mask` cast has the same terminal spelling as its operand, and
`Derived::Base`/`I::T` exercise typed inherited-owner edges.

Representative outside-repository probes live under
`/tmp/pa16-qualified-probes.btTGQp`:

- `target-no-noise.cpp`: 19 lines, target path
  `owner::nested::terminal` (3 components), 2 `terminal` tokens; its 14-line
  LowIR repeated twice with SHA-256
  `c5eaa8b9876dbc16d7fc8acaba16ee2e4d49b5e45b7e619eac53a7794fb69cc5`.
- `target-with-noise.cpp`: 690 lines, the same 3-component target, plus 96
  unrelated namespaces and 192 unrelated `terminal` tokens; its 14-line
  LowIR is byte-identical to the no-noise output and repeated twice with the
  same SHA-256
  `c5eaa8b9876dbc16d7fc8acaba16ee2e4d49b5e45b7e619eac53a7794fb69cc5`.
- `bounded-depth.cpp`: 19 lines and the 5-component path
  `depth0::depth1::depth2::depth3::terminal` (2 `terminal` tokens); its
  14-line LowIR repeated twice with SHA-256
  `e8c8a69279f71202a5bb35539e0665838bf2624a758d0f2edddd415e66d9a350`.

Negative outside-repository probes both reject with status `1`: a non-class
`decltype(scalar)::missing` qualifier and a missing `owner::missing` nested
type.  The final correction rerun summary is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/qualified-type-probes-final-correction.log`.
No timing, memory, or speedup claim is made.  No textual/whole-program
recovery, host compiler, reference binary, or shell-out is used.

## Validation

Final validation on the audited tree:

- `make -C dev cppgm++ CC_FLAGS='-std=gnu++11 -Wall -O3'` — exit `0`.
- Focused `make -C pa16 check` over the five selected failures, the additional
  same-owner case, and six preservation controls — `PASS (12/12)`.
- `make test-pa16` — expected residual-test exit `2`, summary `173/243`,
  `70` failures, and `243/243` identities covered.  Durable log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-qualified-type-final-correction.log`.
- Exact baseline comparison (`last-test.log` versus the durable final log):
  `76` baseline failures, `70` final failures, six baseline-only identities
  (the five selected plus the additional same-owner case), and an empty
  final-only set.  The durable comparison is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-qualified-type-final-correction-identity-compare.log`.
- `rg --files pa16/tests | rg '\.t$' | wc -l` — `243` test identities;
  the final PA16 summary covers all `243`.
- Required `n=16` through-PA15 report — exit `0`, `1167/1167`.
  Durable log: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/through-pa15-qualified-type-final-correction.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` — exit `0`,
  passed with the five known nonfatal division warnings.  Durable log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/file-audit-qualified-type-final-correction.log`.
- `git diff --check` — exit `0`.

All five selected identities and the additional same-owner identity are
repaired; no selected identity remains.  The remaining 70 failures are
outside this checkpoint's canonical qualified-type owner/data flow.

## Next Checkpoint

The next checkpoint should select a residual family from the 70 residual
failures after review.  Keep this checkpoint's exclusions intact:
access control, constructors, operator lookup, lifetime behavior, unrelated
parser recovery, and broad template behavior are not qualified-type repairs.

## Checkpoint Ledger

| checkpoint | result |
| --- | --- |
| `36b93869` historical aggregate handoff | `159/243` PA16 passing, `84` failures, `243/243` covered; PA1--PA15 `1167/1167`. |
| `dea01c52` aggregate implementation | Historical aggregate increment: `164/243` passing, `79` failures, `243/243` covered; through-PA15 and audit evidence were preserved. |
| `3c2114b6` aggregate audit / typed-builtin turn-start | Clean historical state at `164/243`, `79` failures, `243/243` covered; the exact residual map was carried forward. |
| `d7ed98aa` typed builtin boundary | Added the demand-driven typed builtin semantic owner and PA15 path; `167/243` passing, `76` failures, `243/243` covered, with PA1--PA15 `1167/1167`. |
| `f290784f` typed builtin audit (turn start) | Clean current baseline: `167/243` passing, `76` failures, `243/243` covered; PA1--PA15 and file audit pass. |
| PA16 qualified-type checkpoint | Final `173/243` passing, `70` failures, `243/243` covered; all five selected identities plus the same-owner nested-class identity are baseline-only; final-only set empty; focused `12/12`, through-PA15 `1167/1167`, file audit and diff check pass. |
