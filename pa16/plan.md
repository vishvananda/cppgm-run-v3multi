# PA16 empty-base layout/address-projection checkpoint

## Stage design and ownership

PA10 publishes syntax facts; PA11 owns canonical `NamedRecordId` identities
and the sole `RecordLayout`; PA12 publishes typed lookup, selected-call, and
base-path facts; PA15 consumes those facts for LowIR and ABI lowering.  This
checkpoint audits only that flow: no spelling recovery, parallel semantic
model, retained transitive closure, whole-program scan, or host/reference
invocation is introduced.

`PA11SemanticModel::complete_record_layout` owns complete-object size and
alignment, direct-base placement, local member offsets, bit-field facts, and
requested alignment.  Its empty-base query unwraps arrays/CV types and visits
typed zero-offset nested objects with a per-query visited set.  Same-type
subobjects remain distinct; anonymous backing/generated identities and cycles
are handled through typed facts and fail-closed state.  A layout retains one
direct edge plus local facts, not a transitive zero-offset closure.

PA15 validates every typed edge in member-address, member-call,
derived-to-base conversion, constructor-subobject, and destructor paths.
Identity paths emit no `base_subobject`; a valid nonempty all-zero path emits
exactly one canonical projection.

## Authority and exact failure map

The landed implementation is `69bbe80097030fb38b4aed6c6d5cf937a2dd6e87`
relative to `148ef591`; `a5839138` and `3348d274` only update PA16 planning.
The turn-start clean HEAD was `3348d274`.  The supplied authority reports
`make test-pa16` exit `2`, `230/243` passing, complete `243/243` coverage,
and exactly this unchanged 13-item residual map:

```text
pa16/tests/general/200-local-default-class-array-lifecycle.t
pa16/tests/general/200-nested-braced-member-aggregate-init.t
pa16/tests/general/200-reference-indexed-pointer-member-access.t
pa16/tests/general/200-reference-member-class-init.t
pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t
pa16/tests/general/300-friend-function-definition-skip.t
pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t
pa16/tests/general/300-overloaded-deref-user-assignment.t
pa16/tests/general/300-user-defined-string-literal-operator.t
pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t
pa16/tests/general/400-bit-field-prefix-postfix-increment.t
pa16/tests/general/400-signed-bit-field-read.t
pa16/tests/general/400-signed-enum-bit-field-read.t
```

The two checkpoint identities resolved by the landed increment are not in this
map.  No residual identity was re-audited or repaired here, and no coverage
surface changed.

## Current checkpoint

The bounded implementation correction and final validation are complete, based
on clean HEAD `3348d274`.  The first audit/repair commit is still pending, so
the current record uses a truthful pre-commit marker; its hash will be written
in place after that commit.

The in-scope repair is fail-closed PA11 identity validation.  The nested
zero-offset query now verifies canonical class scope, owner bindings, member
offset entries, bit-field sidecars, direct-base identity/kind/offset, and
base-layout consistency before using a completed fact.  Layout completion also
rejects mismatched owner scope, absent/stale base metadata, self/union/nonclass
bases, and invalid direct-base validity.  Valid layout output is unchanged;
the repair prevents malformed typed facts from being silently treated as
non-colliding empty objects.

The PA15 trace remains one production ownership path.  Member address lowering
recomputes and validates every typed edge before the field projection.  Member
calls first use `validate_typed_base_path`, which validates each typed relation,
the completed current layout, and the zero direct-base offset; their following
loop independently revalidates the relation and end owner, but does not itself
recheck layout.  Conversion lowering validates the typed conversion path and
its current/base layouts; constructor and destructor subobject paths validate
their owner, edge, and layout facts.  Identity paths have no base projection,
while the representative nonempty all-zero paths have one.  No PA15 projection
behavior was changed by this milestone.

## Focused evidence

The final serial gates and exact artifact comparison were run after the source
repair:

```text
n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
                                                         exit 0; 1167/1167
make test-pa16                                           exit 2; 230/243
normalized failure comparison                           authority 13, fresh 13,
                                                         authority-only 0, fresh-only 0
artifact inventories                                    discovered/reference/fresh 243/243/243,
                                                         missing/unexpected 0/0/0/0
perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src
                                                         exit 0; 6 nonfatal warnings
git diff --check                                         exit 0
focused build and controls                              exit 0; targets 2/2, matrix 12/12
sh cppgm.tests/course/pa16/401-typed-member-projection-boundary-regression.sh exit 0
sh cppgm.tests/course/pa16/404-typed-implicit-default-demand-regression.sh   exit 0
sh cppgm.tests/course/pa16/408-typed-constructor-explicit-context-regression.sh exit 0
sh cppgm.tests/course/pa16/409-typed-constructor-boundary-regression.sh        exit 0
sh cppgm.tests/course/pa16/418-typed-inherited-constructor-wrapper-regression.sh exit 0
```

The exploratory 400/402/403/405 probes exited `1`, but are not PA16 handout
gates for this checkpoint: 400 stops on its pre-existing DMI expectation
mismatch, 402 at a declaration-only demand-boundary expectation, and 403/405
expect two per-edge projections.  The current PA16 contract requires one
canonical projection for an all-zero path, so those expectations are not a
reason to regress the landed consumer.  These probes caused no source or test
changes.

The PA16 failure identities are byte-for-byte the 13-item authority map above;
there are no fresh-only failures and no compensated pass.  The file-audit
warnings are the six existing header/body findings for `abi_mangle.h`,
`cpp_semantic_core.h`, `lowir_model.h`, `pa11_semantic_model.h`,
`pa12_semantic_selection.h`, and `pa15_lowering.h`.

## Bounds and performance evidence

The persistent model remains linear in records plus local members: one direct
edge and local offsets per layout, with no closure retained.  The empty-base
query allocates its typed ancestry set and visited identities only for the
candidate query, checks each local member once, and recurses through bounded
array/CV/nested zero-offset facts.  The added checks are linear in the local
members and direct edge and add no persistent structure.  The retained
structural scale probe used 64 inheritance edges and 65 records, retained 64
direct edges and zero transitive closure entries, and emitted one deepest
inherited-call projection; it made no timing or RSS claim.

## Active / next checkpoint

Active: final bounded validation is complete with the exact authority
residual map above; the first audit/repair commit and hash-recording follow-up
remain to be made.

Next: commit the bounded source/docs result, convert the existing pending
markers and ledger rows in place to that commit hash, make the docs-only
follow-up, and verify the final clean tree.  No separate residual checkpoint
is selected or re-audited in this milestone.

## Checkpoint ledger

| checkpoint | compact result |
|---|---|
| d54e32d1 | Prior authority was `224/243` with 19 failures and complete identity coverage. |
| b58ddd2a | Completed the typed `nullptr_t` carrier path through PA11/PA12/PA15, including ABI and LowIR ownership. |
| e09d8223 | Recorded the nullptr-carrier audit state: 225/243 authority, 18 residual failures, `243/243/243` inventories, and through-PA15 `1167/1167`. |
| d5bf2600 | Completed the typed constructor-overload/lifetime ownership audit; focused matrix passed and PA16 reached `227/243` with 16 residual failures. |
| 29d9c4ce | Completed the PA10 elaborated-member parameter-clause repair and PA15 ABI owner consolidation; PA16 reached `228/243` with this exact 15-item residual set and `243/243/243` inventories. |
| 69bbe800 | Empty-base layout/address projection: post-commit PA16 `230/243`, two target identities resolved, 13 exact residuals retained; through-PA15 `1167/1167`, file audit pass with six warnings. |
| working-tree (pending; based on `3348d274`) | Completed empty-base checkpoint audit/repair and final evidence; source/docs remain uncommitted until the first audit/repair commit, with exact unchanged 13-failure identity set and `243/243/243` artifact coverage. |
