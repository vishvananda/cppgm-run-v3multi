# PA16 implementation plan

## Stage Design/spec alignment and owner/data flow

PA12 owns one typed MemberLookup boundary. It inspects the actual class
scope, then the ordered non-virtual direct-base chain; the first declaration
set hides all bases. The result carries the lookup kind, canonical BindingId,
owning class ScopeId, selected type, and typed NamedRecordId base path. Values
precede types; ambiguous declaration sets and imported/unsupported ownership
are blocked.

Explicit dot/arrow fields and non-static member calls consume that result.
Unqualified and parser-supported qualified inherited fields and calls in
member bodies use the exact synthetic this binding. Calls retain the selected
owner/path through the shared call selector, including cv and argument ranking.
Access is checked at the selected owner. Public and same-owner access retain
their existing behavior; protected access walks the bounded lexical class
scopes and accepts an eligible scope only when its typed class derives from the
owner. For non-static members, the actual object must also be that access class
or a further-derived class; protected static object spelling has no such
second relation. Private and unrelated access remain rejected. This preserves
the N3485 §11.7 nested-class member access right without changing owner or
binding identity. The lexical walk is bounded by the scope-vector size and
fails closed if a valid cursor remains after that bound; ordinary invalid
parent termination and same-owner access retain their existing outcomes.

PA15 consumes the selected owner for member addresses, validates the actual
object-to-owner direct-base path and every complete RecordLayout edge at
offset zero, emits one projection=base_subobject per edge, and then uses the
owner layout's member_offsets for projection=field. Dot/arrow objects are
evaluated once. The ownerless semantic_injected_member producer remains the
separate storage-backed anonymous-union path; PA15 checks that backing marker
before requiring a selected owner.

Ordinary local implicit default construction now validates a bounded typed
walk over direct bases and non-static member TypeKey/sidecar facts, with an
active-record cycle guard. A state-free inherited chain publishes no
constructor action, synthetic helper, or demand edge; a base/member/default
initializer or unsupported subobject fails closed. Anonymous-union storage
keeps its separate constructor action. The pre-existing no-base empty-class
constructor fact is retained for the PA12 semantic contract
(300-reference-binding-pointee-const-pointer.t); this is not a blanket
derived-construction restriction.

## Failure Map and coverage identity

Turn-start baseline is the authoritative log
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log:
PA16 49/243, 194 failures, 243/243 covered. The authorized final
`make test-pa16` exits 2 with the same 49/243, 194 failures, and 243/243
coverage; exact sorted failure identities have added `∅` and removed `∅`.

The earlier pre-increment plan records the historical 48/243 to 49/243
improvement from removing pa16/tests/general/200-protected-base-method.t.
That evidence is preserved separately from this audit's current 49/243
turn-start identity. The final stage retains the same failure identity and
243/243 coverage.

The typed inherited-member checkpoint is landed in 9f158daa. This checkpoint
carries the actual object TypeId into protected field/call accessibility,
repairs the protected static exception, repairs eligible enclosing lexical
access scopes for nested classes, and expands the course-405 matrix. The
required through-PA15 command exits 0 at 1167/1167, and the required file
audit exits 0 with the five existing header warnings.

The overload primary remains out of this boundary: its exact current error is
PA15 unsupported address expression. A debug lowering breakpoint showed the
first failing address fact is SemanticFactKind::Literal, fact id 9,
array/string literal element count 2, type id 29, with invalid
constant_address; no lower_expression_impl scalar-default breakpoint was hit.
The local string-literal address issue is therefore not a scalar member-call
fact.

Focused current evidence: course 405 exits 0 and covers same-owner,
implicit/qualified-this, explicit dot/arrow, const/reference/pointer
normalization, further-derived objects, and field/method positive and
negative paths. It also covers constructor-free nested `Derived&` access,
nested `Base&` rejection, and protected static object spelling through the
existing PA15 static projection diagnostic. Courses 401, 402, 403, and 404
each exit 0; 402 retains the expected PA12 inherited-member negative
diagnostic. The checked-in protected positive control passes 1/1. No handout
fixture or .ref file changed.

## Active Checkpoint

The typed inherited field/call selection, owner-path lowering, protected
derived-body call, and state-free construction-demand work landed in
9f158daa. The `8b445ee6` audit carries the actual typed record into every new
protected field/call check, preserves the static-member exception, and lets a
nested class use an eligible enclosing access scope. Course 405 covers those
boundaries. Focused and broad validation are complete with no failure-identity
addition or coverage loss; this audit is the active completed checkpoint, and
the next checkpoint must select a separate bounded PA16 surface. The
overload local string address gap, explicit-constructor parsing/lifetime,
broader initialization, friend/using re-exposure, multiple/virtual
inheritance, downstream static lowering, and full PA16 success remain out of
scope.

## Performance evidence and uncertainties

Same-owner member selection is an immediate scope lookup. Protected access
walks lexical scopes of depth `S`, records `L` class scopes, and performs at
most two direct-base paths of depth `D` per candidate, so its worst-case bound
is `O(S + L*D)` (ordinary member bodies have `L=1`); static access can stop
after the owner proof. Inherited lookup is bounded by O(depth + candidates); member field/call lowering validates a typed
O(depth) path and layout sequence, with no whole-program scan, retry, or
rendered-name recovery. The implicit-default predicate walks only the
reachable typed base/member graph per declaration; its active vector is
bounded by the supported record depth and protects cycles. No cache was
needed for this checkpoint.

Representative command, using temporary non-repository sources with identical
state-free three-level chains and 1, 128, and 512 local declarations, ran five
times per size; all compiler invocations exited 0. Maximum elapsed times were
0.02s, 0.01s, and 0.02s, with maximum RSS approximately 5.4MB, 6.1MB, and
8.7MB respectively. A separate temporary nested-access source with 256
protected-field expressions in one out-of-class nested member used lexical
depths `L=1`, `8`, and `32`, with three invocations per depth; all exited 0,
with maximum elapsed time 0.01s and maximum RSS 7.4MB. These are small
measured bounded-behavior samples, not formal benchmarks. The final file audit
recorded only the five existing header bad-division warnings.

Final mandatory gates:
n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
exits 0 with 1167/1167, and
perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src exits 0 with
five warnings. No handout fixture or .ref file changed.

## Checkpoint ledger

| checkpoint | status |
| --- | --- |
| 9f158daa | Landed typed inherited field/call selection and lowering, protected derived-body qualified calls, state-free inherited construction demand, and the 48→49/243, 195→194 identity improvement with 243/243 coverage. Through-PA15 is 1167/1167 and audit exits 0 with five pre-existing warnings. |
| 8b445ee6 protected object access scope checkpointAudit | Completed bounded audit/repair: typed protected object proofs are traced through field/call facts, the static-member exception is repaired, eligible enclosing lexical scopes cover nested-class access, and course 405 covers the required matrix. Final PA16 is 49/243 with 194 failures and 243/243 coverage; exact failure additions/removals are ∅/∅. Courses 401–405, through-PA15 at 1167/1167, and the file audit pass; the next checkpoint is a separately bounded PA16 surface. |
| 0b534f2f typed direct member-call checkpoint | Landed implicit-object cv subset ranking, N3485 variadic comparison, typed PA15 member reachability, dense FunctionFact/fact metadata, declaration-only member ABI boundaries, hidden-object call formation, and source-file sizing. |
| b1e8272d + PA16 typed implicit-object boundary | Landed canonical Function-scope hidden-object ownership, fail-closed viability, typed demand indexing, direct PA15 lowering, and focused direct/member-call controls. |
| 37265733 typed member projection audit/repair | Landed direct/nested dot and arrow ownership tracing through PA12, PA11 RecordLayout::member_offsets, and PA15 LowIR. |
