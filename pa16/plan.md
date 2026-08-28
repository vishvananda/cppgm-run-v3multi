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
excluded.

## Failure Map

Frozen authoritative baseline: `173/243` PA16 identities pass, `70` fail,
and `243/243` identities are covered; PA1--PA15 pass `1167/1167` and the
PA16 dev/src audit passes.  This checkpoint selects exactly:

- `pa16/tests/general/500-inherited-constructor-using-access.t`
- `pa16/tests/general/500-inheriting-constructors.t`
- `pa16/tests/general/500-inheriting-external-transitive-constructor.t`

The selected family covers direct using access semantics, direct inherited
construction, transitive inheritance, and an externally declared base
constructor.  All other PA16 residual identities are excluded from this
checkpoint.  No handout test, `.ref`, or existing fixture is edited; no new
course regression was added.  Out-of-scope language features are likewise
excluded.

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

State at final evidence capture: the implementation and this plan remain
uncommitted immediately before staging.  No handout tests, `.ref` files, or
existing fixtures were changed.

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
is involved.

## Validation

Focused and final evidence:

- `make -C dev cppgm++ CXX=g++ CC_FLAGS='-std=gnu++11 -Wall -O3'` — exit `0`.
- Focused `make -C pa16 check CPPGM_SKIP_DEV_REBUILD=1 TEST='tests/general/500-inherited-constructor-using-access.t tests/general/500-inheriting-constructors.t tests/general/500-inheriting-external-transitive-constructor.t tests/general/200-constructor-member-init.t tests/general/200-derived-base-constructor-member-init.t tests/general/200-constructor-overload-default-arg-nonfirst-argument.t tests/general/100-private-method-bad.t'` — `PASS (7/7)`.
- The three selected identities pass; all four preservation controls pass:
  ordinary constructor, base-constructor initializer lowering, overload with
  default-argument selection, and access rejection.  Reference-parameter
  coverage comes from the selected external/transitive constructor identity,
  not from the named overload control.
- `git diff --check` — exit `0`.
- `make test-pa16` — exit `2` for residual failures; `176/243` passed, `67`
  failures, and all `243/243` identities were covered.  Durable log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-inheriting-constructors-final.log`.
- Exact failure-set comparison against the frozen
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
  `70 -> 67`; baseline-only is exactly the three selected identities and
  final-only is empty.  Durable comparison:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-inheriting-constructors-final-identity-compare.log`.
- Exact `n=16` through-PA15 gate — exit `0`, `1167/1167`.  Durable log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/through-pa15-inheriting-constructors-final.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` — exit `0`,
  passed with the five known division warnings.  Durable log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/file-audit-pa16-inheriting-constructors-final.log`.
- The final broad run regenerated the three selected `.my.exit_status` files
  as `EXIT_SUCCESS`; no generated artifact was edited by hand.  Structural
  probe evidence is recorded in the external path named above.

## Next Checkpoint

The checkpoint leaves `67` residual PA16 failures outside this selected family.
Future work should select another residual family after review and preserve
the current inheriting-constructor exclusions; this checkpoint does not claim
full PA16 completion.

## Checkpoint Ledger

| checkpoint | result |
| --- | --- |
| `36b93869` historical aggregate handoff | `159/243` PA16 passing, `84` failures, `243/243` covered; PA1--PA15 `1167/1167`. |
| `dea01c52` aggregate implementation | Historical aggregate increment: `164/243` passing, `79` failures, `243/243` covered; through-PA15 and audit evidence were preserved. |
| `3c2114b6` aggregate audit / typed-builtin turn-start | Clean historical state at `164/243`, `79` failures, `243/243` covered; the exact residual map was carried forward. |
| `d7ed98aa` typed builtin boundary | Added the demand-driven typed builtin semantic owner and PA15 path; `167/243` passing, `76` failures, `243/243` covered, with PA1--PA15 `1167/1167`. |
| `f290784f` typed builtin audit | Clean historical baseline: `167/243` passing, `76` failures, `243/243` covered; PA1--PA15 and file audit pass. |
| `3b7d8e6a` qualified-type checkpoint | Completed the prior qualified-type increment: `173/243` passing, `70` failures, `243/243` covered; prior broad evidence was preserved. |
| `working tree after 3b7d8e6a` inheriting-constructor first stop | Selected family plus four preservation controls `PASS (7/7)`; three selected identities pass; implementation and plan uncommitted; broad gates pending. |
| `PA16 inheriting-constructor checkpoint` | Final `176/243` passing, `67` failures, `243/243` covered; exactly the three selected identities removed, final-only set empty; focused `7/7`, through-PA15 `1167/1167`, audit passed with five known warnings, structural probe deterministic; ready for commit. |
