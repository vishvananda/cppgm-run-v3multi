# PA16 implementation plan

## Stage Design

PA10 retains the `using Base::Base` AST.  PA11 owns the typed
`InheritingConstructorRelation`, keyed by the derived/base `NamedRecordId` and
source point; no constructor is recovered from rendered spelling or source
text.  PA12 walks the relevant direct-base relation and constructor candidate
facts, publishes deduplicated derived `FunctionFact`/`ConstructorActionFact`
wrappers, and follows the selected base binding for access.  The using
declaration's access does not replace the selected base constructor's access.
Typed callable facts preserve parameter/reference types, binding identity, and
transitive relations.  PA12 also publishes a deduplicated typed direct-base
entry for each demanded constructor, while PA15 demand-driven lowering emits
complete/base-object entry points, forwards explicit parameters to those
entries, uses direct-base offset zero, and assigns deterministic aliases and
declarations.  This is the PA16 inheriting-constructor boundary aligned with
`spec.md` §§2--5 and 7 and the README Assignment Boundary; virtual/multiple
inheritance, variadic and other out-of-scope constructor families remain
excluded.  The landed increment is `30d69fc3df2a493fc84eeb52b6be87da18fe429a`
relative to `05c36f56`.  This audit models N3485's notional trailing-default
wrapper signatures (without inherited default facts or a zero-arity wrapper),
the separate implicit-default path, derived DMI/member actions, inherited
explicitness, source points, malformed typed identities, and order-independent
transitive expansion: each relevant immediate-base relation is recursively
materialized before its derived candidate list is scanned, with a bounded
single-inheritance walk.

## Failure Map

Parent baseline before landed commit `30d69fc3`: `173/243` PA16 identities
pass, `70` fail, and `243/243` identities are covered.  The authoritative
turn-start result after that commit is `176/243` pass, `67` fail, and
`243/243` covered; its exact map is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The exact three parent-to-turn-start repairs are:

- `pa16/tests/general/500-inherited-constructor-using-access.t`
- `pa16/tests/general/500-inheriting-constructors.t`
- `pa16/tests/general/500-inheriting-external-transitive-constructor.t`

The selected family covers direct using access semantics, direct inherited
construction, transitive inheritance, and an externally declared base
constructor.  All other PA16 residual identities are excluded from this
checkpoint.  The added wrapper regression covers inherited default arguments,
derived default member initialization, inherited explicit copy rejection, and
hard-only transitive discovery with a typed reference/default variant.
No handout test, `.ref`, or existing fixture is edited; out-of-scope language
features are likewise excluded.  The final broad rerun below proves that this
same 67-failure set is preserved.

## Active Checkpoint

The intended typed owner/data-flow boundary is limited to these implementation
files:

- `dev/src/pa11_semantic_model.h`
- `dev/src/pa11_semantic_core.cpp`
- `dev/src/pa11_semantic.cpp`
- `dev/src/pa11_semantic_types.cpp`
- `dev/src/pa12_semantic_member.cpp`
- `dev/src/pa12_semantic_construction.cpp`
- `dev/src/pa12_semantic_facts.cpp`
- `dev/src/pa15_lowering.h`
- `dev/src/pa15_lowering.cpp`
- `dev/src/pa15_lowering_calls.cpp`
- `dev/src/pa15_lowering_flow.cpp`

Commit `30d69fc3` is landed.  The completed audit repair is limited to
`dev/src/pa11_semantic.cpp`, `dev/src/pa11_semantic_model.h`,
`dev/src/pa11_semantic_types.cpp`, `dev/src/pa12_semantic_construction.cpp`,
`dev/src/pa12_semantic_facts.cpp`, and `dev/src/pa12_semantic_member.cpp`, plus
the wrapper course regression and these documents.  No handout tests, `.ref`
files, or existing fixtures were changed.

The construction owner now expands the relevant immediate-base typed relation
lists before scanning their class-name values.  This preserves demand-driven
publication while removing construction-order dependence for transitive
inherited wrappers; it does not add a whole-scope or textual discovery path.

## Performance Evidence

The external-constructor structural probe is outside the repository at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-inheriting-constructor-probes-20260828`.
The no-noise input is 19 lines; the same target with 64 unrelated classes and
constructors is 84 lines.  Both contain two typed using relations, one
relevant base-constructor candidate, transitive depth two, two demanded
inherited wrappers, and two deduplicated base-entry bindings (one emitted
definition and one external declaration).  The generated target has three
direct-base projections at offset zero.

Each input was compiled twice by the repository `dev/cppgm++` only.  All four
outputs are 68 lines with SHA-256
`02de5e72c8fccc4a0aa6304231b0bed5d29cbeabea1c9b33bd35a67827d78592`; the
no-noise/noise outputs and both reruns compare byte-for-byte equal.  Durable
logs are `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-inheriting-constructor-probes-20260828/compile.log` and
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-inheriting-constructor-probes-20260828/structural-evidence.log`.
This is deterministic bounded structural evidence, not a timing or speed
claim; no reference binary, host compiler, whole-TU rescan, or text recovery
is involved.  The audit repair recursively expands only the active
single-inheritance relation chain before the derived scan, so transitive
discovery is independent of prior construction order without an eager
whole-scope pass.  It also collects derived wrapper actions in a local range
before publishing them, so nested member-constructor demand cannot interleave
unrelated actions into the wrapper.  The source file audit is within its hard
size limits (3000 lines for `pa11_semantic.cpp`, 2400 for
`pa11_semantic_model.h`) with only the five known header-division warnings.
The follow-up structural replay is recorded at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-inheriting-constructor-transitive-audit-structural-20260829.log`.
No timing, RSS, allocation, or speedup claim is made.

## Validation

Final focused evidence:

- `make -C dev cppgm++` — exit `0`.
- Focused `make -C pa16 check CPPGM_SKIP_DEV_REBUILD=1 TEST='tests/general/500-inherited-constructor-using-access.t tests/general/500-inheriting-constructors.t tests/general/500-inheriting-external-transitive-constructor.t tests/general/200-constructor-member-init.t tests/general/200-derived-base-constructor-member-init.t tests/general/200-constructor-overload-default-arg-nonfirst-argument.t tests/general/100-private-method-bad.t'` — `PASS (7/7)`.
- Course controls `403-typed-inherited-member-field-regression.sh`,
  `408-typed-constructor-explicit-context-regression.sh`,
  `409-typed-constructor-boundary-regression.sh`, and
  `418-typed-inherited-constructor-wrapper-regression.sh` — all exit `0`;
  `sh -n` also passes for 418.  The strengthened 418 control asserts the
  shortened D1/D2 wrapper signatures, omitted typed defaults at the base
  entry, scalar DMI, runtime-default member construction, copy filtering, and
  hard-only order-independent transitive expansion.
- The external probe was compiled four times by repository `dev/cppgm++` with
  strict exit checking; all outputs are 68 lines with hash
  `02de5e72c8fccc4a0aa6304231b0bed5d29cbeabea1c9b33bd35a67827d78592`, and
  the no-noise/noise plus rerun comparisons pass.
- `make test-pa16` — exit `2`, `176/243` passing, `67` failures, and
  `243/243` identities covered.  Durable log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-inheriting-constructor-transitive-audit-final-20260829.log`.
  The exact sorted failure identity comparison has empty `comm -3` output,
  matching set SHA-256
  `4fac6d8249a367e090b700fe08efc6a28439d701c2d2f9bab328ffc3e4ce846e`, and
  is recorded at
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-inheriting-constructor-transitive-audit-identity-compare-20260829.log`.
- The exact `n=16` through-PA15 command — exit `0`, `1167/1167`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` — exit `0`
  with five known header-division warnings.
- `git diff --check` — exit `0`.

No timing, RSS, allocation, or speedup claim is made.  No handout test, `.ref`,
fixture, harness, comparator, reference output, or source-set list changed.

## Next Checkpoint

This checkpoint closes the selected inheriting-constructor audit with the
turn-start `67`-failure identity map preserved.  The next work item should
select another residual PA16 family while preserving the direct-single-base,
non-template/non-variadic, non-copy/move/by-value exclusions recorded above.

## Checkpoint Ledger

| checkpoint | result |
| --- | --- |
| `36b93869` historical aggregate handoff | `159/243` PA16 passing, `84` failures, `243/243` covered; PA1--PA15 `1167/1167`. |
| `dea01c52` aggregate implementation | Historical aggregate increment: `164/243` passing, `79` failures, `243/243` covered; through-PA15 and audit evidence were preserved. |
| `3c2114b6` aggregate audit / typed-builtin turn-start | Clean historical state at `164/243`, `79` failures, `243/243` covered; the exact residual map was carried forward. |
| `d7ed98aa` typed builtin boundary | Added the demand-driven typed builtin semantic owner and PA15 path; `167/243` passing, `76` failures, `243/243` covered, with PA1--PA15 `1167/1167`. |
| `f290784f` typed builtin audit | Clean historical baseline: `167/243` passing, `76` failures, `243/243` covered; PA1--PA15 and file audit pass. |
| `3b7d8e6a` qualified-type checkpoint | Completed the prior qualified-type increment: `173/243` passing, `70` failures, `243/243` covered; prior broad evidence was preserved. |
| `working tree after 3b7d8e6a` historical inheriting-constructor first stop | Pre-landing focused stop: selected family plus four preservation controls `PASS (7/7)`; the later `30d69fc3` increment was still pending. |
| `30d69fc3` inheriting-constructor landed checkpointAudit | Landed increment relative to `05c36f56`: turn-start and final `176/243` passing, `67` failures, `243/243` covered, with the exact three selected identities removed from the parent `173/243`/`70`-failure map and no final-only failures. The bounded audit adds N3485-correct typed notional wrapper variants, separate implicit-default selection, derived DMI/runtime member actions through the shared owner, inherited explicit-copy filtering, source-point lookup, identity guards, and recursive order-independent expansion of relevant immediate-base relation lists before derived scans. Focused controls, strengthened hard-only 418, structural probe, through-PA15 `1167/1167`, file audit (five known warnings), and `git diff --check` pass; final evidence is recorded above, with durable broad log and exact identity comparison under the transitive-audit paths dated 20260829. |
