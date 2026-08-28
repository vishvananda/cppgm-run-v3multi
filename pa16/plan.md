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

The parent baseline is HEAD `f290784f9a63c8723fcf617bfcd36c9dc080de7e`:
`167/243` PA16 identities passed, `76` failed, and `243/243` were covered;
PA1--PA15 were `1167/1167`, and the file audit passed.  The exact parent/final
identity comparison is preserved below and in the durable audit log.

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

The exact parent/final identity comparison is retained in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-qualified-type-call-audit-identity-compare.log`:
`76 -> 70` failures, `243/243` identities covered, six baseline-only
identities, and final-only `∅`.  The six repaired identities are the five
qualified-type selections above plus
`general/200-nested-class-private-enclosing-access.t`; the full residual map
remains in the durable final result log rather than being duplicated here.

## Active Checkpoint

The landed increment is commit `3b7d8e6a228ec43a54d7eb97f1d5b45b450f6c57`,
subject `PA16: resolve typed qualified class names`, and changes these seven
implementation files:

- `dev/src/pa10_ast.cpp`
- `dev/src/pa10_parser_support.cpp`
- `dev/src/pa10_parser_support.h`
- `dev/src/pa11_semantic.cpp`
- `dev/src/pa11_semantic_core.cpp`
- `dev/src/pa11_semantic_model.h`
- `dev/src/pa11_semantic_types.cpp`

The bounded audit adds the minimal PA12 owner repair in
`dev/src/pa12_semantic_resolution.cpp` and course control
`cppgm.tests/course/pa16/417-qualified-parenthesized-type-call-regression.sh`.
It repairs six handout identities without changing fixtures.  The
preservation controls cover qualified method definitions, inherited
unqualified typedefs, injected-name hiding, lazy enclosing aliases, and
source-point using-directive behavior.  PA11 remains the typed lookup owner;
PA12/PA15 consume TypeId/ScopeId/BindingId facts without spelling
reconstruction.

## Performance Evidence

The parser's new qualified-name probes are bounded by the delimiter/template
indexes and the number of name components; they do not search source text or
the translation unit.  PA11 first probes the requested class scope and then
walks the validated single-inheritance chain once, with the existing marked
lookup graph handling only relevant using/inline edges.  Thus the structural
risk is proportional to qualifier depth plus language-relevant base/lookup
edges, not same-name noise elsewhere in the program.  The PA12 repair unwraps
only the finite parenthesized callee chain and performs the existing typed
value/type lookups once.  The representative
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

Final validation after the bounded PA12 call-shape repair recorded:

- `make -C dev cppgm++ CC_FLAGS='-std=gnu++11 -Wall -O3'` — exit `0`.
- Focused `make -C pa16 check` over the six repaired identities and six
  preservation controls — `PASS (12/12)`.
- Course control `417-qualified-parenthesized-type-call-regression.sh` — exit
  `0`; namespace-qualified conversion and namespace-function-call controls
  pass, with the existing class-qualified same-spelling cast.
- `make test-pa16` — expected residual-test exit `2`, summary `173/243`,
  `70` failures, and `243/243` identities covered.  Durable log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-qualified-type-call-audit-final.log`.
- Exact baseline comparison (the frozen parent map versus the durable final
  log):
  `76` baseline failures, `70` final failures, six baseline-only identities
  (the five selected plus the additional same-owner case), and an empty
  final-only set.  The durable comparison is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-qualified-type-call-audit-identity-compare.log`.
- `rg --files pa16/tests | rg '\.t$' | wc -l` — `243` test identities;
  the final PA16 summary covers all `243`.
- Required `n=16` through-PA15 report — exit `0`, `1167/1167`.
  Durable log: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/through-pa15-qualified-type-call-audit-final.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` — exit `0`,
  passed with the five known nonfatal division warnings.  Durable log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/file-audit-pa16-qualified-type-call-audit-final.log`.
- `git diff --check` — exit `0`.

All six selected handout identities are repaired; no selected identity
remains.  The remaining 70 failures are outside this checkpoint's canonical
qualified-type owner/data flow.

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
| `3b7d8e6a` qualified-type checkpoint | Completed bounded audit of the qualified-type increment plus the PA12 parenthesized-callee repair: `173/243` passing, `70` failures, `243/243` covered; six baseline-only identities and final-only `∅`; focused repaired/control matrix `12/12`, course control 417 pass; through-PA15 `1167/1167`, file audit pass with five pre-existing warnings, and diff-check pass. |
