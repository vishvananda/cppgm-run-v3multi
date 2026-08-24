# PA12 namespace scope/lookup checkpoint

## Stage Design

`PA11SemanticModel` remains the sole semantic owner. PA10 syntax is reduced to
typed `ScopeId`, `BindingId`, `ValueRef`, and `SourcePoint` facts; PA12 records
resolved expression facts; the cold renderer prints syntax occurrences and
canonical declaration qualification. Each unnamed namespace has one stable
`ScopeId` per enclosing namespace, reused by reopened definitions.

## Failure Map

The exact turn-start baseline was PA12 `149/166` passing, exactly `17`
failures, with all `166/166` paths covered. The six-test checkpoint family
was:

- `pa12/tests/general/300-namespace-function-body-later-anonymous-overload.t`
- `pa12/tests/general/300-qualified-direct-function-hides-using-directive.t`
- `pa12/tests/general/300-reopened-unnamed-namespace-call.t`
- `pa12/tests/general/300-unnamed-namespace-definition.t`
- `pa12/tests/general/300-unnamed-namespace-qualified-call.t`
- `pa12/tests/general/300-unnamed-namespace-unqualified-call.t`

The final PA12 result is `155/166` passing, `11` failures, and `166/166`
paths covered. The exact residual set is:

- `pa12/tests/general/300-decltype-functional-cast.t`
- `pa12/tests/general/300-local-extern-function-declaration.t`
- `pa12/tests/general/300-member-function-pointer-return-pointer-const.t`
- `pa12/tests/general/300-member-function-pointer-type-alias-and-function.t`
- `pa12/tests/general/300-member-pointer-type-alias-and-function.t`
- `pa12/tests/general/300-multidimensional-array-const-reference-binding.t`
- `pa12/tests/general/300-reference-binding-pointee-const-pointer.t`
- `pa12/tests/general/300-scoped-enum-functional-cast-integral.t`
- `pa12/tests/general/300-static-cast-member-overload-prefers-nontemplate.t`
- `pa12/tests/general/300-static-cast-overloaded-function-template-argument.t`
- `pa12/tests/general/300-zero-arg-functional-cast-alias.t`

The residual set is exactly the turn-start set minus the six checkpoint paths:
`0` new failures and `6` resolved failures.

## Active Checkpoint

- `unnamed_namespace_index_` maps each enclosing `ScopeId` to one unnamed
  namespace scope. Its implicit visibility is represented by typed using
  relations on the enclosing namespace and effective ancestor.
- Namespace value entries and using relations carry typed source points. A
  function-definition source-point sidecar filters later namespace scopes,
  values, and relations during body lookup; lookup does not retry the program,
  render/reparse names, or use generated-name text.
- Direct candidates are returned before using-directive candidates in the
  shared qualified/unqualified lookup graph. This is general lookup semantics,
  retained after the nearby qualified and unqualified controls passed.
- Namespace syntax occurrences render deterministically as
  `namespace-definition <unnamed>`. Unnamed scope components are omitted from
  visible declaration qualification.
- Call-shaped `decltype` uses ordinary lookup and accepts one viable direct
  function only; it rejects no viable function and multiple same-arity
  candidates. Indirect function types are checked for arity as well.

## Spec Alignment

Architecture sections 1-3 of `spec.md` are respected by preserving one forward
PA10 -> PA11 -> PA12 fact pipeline and typed scope/binding ownership. Section 4
is handled with declaration-point facts and source-order filtering rather than
full-program retry or broad invalidation. Section 7 is addressed with stable
typed identities, deterministic traversal, compact sidecars/relations, and no
production name rendering or reparsing in lookup.

## Performance Evidence

The layout probe was compiled once against clean HEAD `88a69ead` and once
against the checkpoint header:

| record | clean HEAD | checkpoint | delta |
|---|---:|---:|---:|
| `Binding` | 80 | 80 | 0 |
| `NamedRecord` | 120 | 120 | 0 |
| `Scope` | 440 | 440 | 0 |
| `ValueEntry` | 16 | 24 | +8 |
| `UsingDirectiveRelation` | not present | 16 | new |
| `EffectiveUsingDirective` | 16 | 24 | +8 |

The +8-byte `ValueEntry` increase is intentional: declaration-point
correctness requires each value candidate to retain its point beside its typed
binding/origin identity. A separate map or repeated arena scan would add
lookup bookkeeping or scans; the direct field keeps candidate filtering
linear in relevant entries and avoids global lookup structures. No timing,
scaling, RSS, or asymptotic claim was measured.

Two repeated compiler runs for each of four checked-in namespace shapes exited
`0/0` and produced byte-identical output (`cmp` exit `0`):

| shape | SHA-256 |
|---|---|
| later anonymous overload | `b188ba30a9e560d9d28c472b05dfeea38c849f33237ae9a8de12a8882067f293` |
| reopened unnamed namespace | `51b3d86bc87e2e3d69bad287c38c23d89f0f90cc1dd0e089ac9e975c3ed2d968` |
| unnamed qualified call | `e96947e6b28aea4012231d6869db1cd40709749cb694cb42838081ee309e34a9` |
| qualified direct hide | `bb94887ce1da0d9b5cf4eea0c406e24618f3276b6bc326282e56916c5ee6b3e` |

## Checkpoint Ledger

| checkpoint | exact evidence | result |
|---|---|---|
| Turn-start baseline | PA12 `149/166`, `17` failures, `166/166` covered | recorded |
| Build | `make -C pa12 -j2` | exit `0` |
| Ambiguity probe | out-of-tree two same-arity functions in call-shaped `decltype` | exit `1`, ambiguous rejected |
| Invalid-arity probe | out-of-tree function called with the wrong arity | exit `1`, no-match rejected |
| Checked direct target | `make -C pa12 check TEST='tests/general/300-qualified-direct-function-hides-using-directive.t'` | `1/1` |
| Six-test family | required six checked-in namespace paths | `6/6` |
| Nearby PA12 controls | 16 checked-in qualified/unqualified namespace, using, alias, and conflict controls | `16/16` |
| Narrow PA11 controls | 9 checked-in namespace/using/reopen/anonymous-union controls | `9/9` |
| Broad PA12 | `make test-pa12` | exit `2`, `155/166`, exact 11 residuals above, `166/166` covered |
| Through PA11 | `n=12; ... make test-report-through-pa$((n - 1))` | exit `0`, `685/685` |
| File audit | `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src` | exit `0`; 2 existing header-division warnings, no fatal issues |
| Final checks | `git diff --check`; final diff restricted to five intended paths | exit `0`; ready for one coherent commit |

## Next Checkpoint

The namespace scope/lookup checkpoint is complete. The remaining eleven PA12
paths are the separate decltype/local-extern/member-pointer/reference/enum/
static-cast residual families listed above; no additional family is included
in this commit.
