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
Access is checked at the selected owner; protected access is admitted only
from a typed derived-class body, while private and unrelated access remain
rejected.

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

Baseline is the turn-start log
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log:
PA16 48/243, 195 failures, 243/243 covered. Final command
make test-pa16 >/tmp/pa16-final-final.log 2>&1 exits 2 with 49/243,
194 failures, and 243/243 covered. Exact sorted identity delta:

- removed: pa16/tests/general/200-protected-base-method.t
- added: empty set

The overload primary remains out of this boundary: its exact current error is
PA15 unsupported address expression. A debug lowering breakpoint showed the
first failing address fact is SemanticFactKind::Literal, fact id 9,
array/string literal element count 2, type id 29, with invalid
constant_address; no lower_expression_impl scalar-default breakpoint was hit.
The local string-literal address issue is therefore not a scalar member-call
fact.

Final focused PA16 check command (exit 2) ran nine tests and passed 5/9:
protected, direct member methods/calls, and direct field controls passed; the
overload literal-address case and the three explicit-constructor field
fixtures failed. The latter remain
PA11 semantic feature not implemented: declaration form. Courses 401, 402,
403, and 404 each exit 0; 402 retains the expected
PA12 inherited member name is not callable negative diagnostic. The PA12
constructor-contract control passes 1/1. A constructor-free inherited
overload probe also exits 0 and lowers to Base__select through one typed
base-subobject projection. No handout fixture or .ref file changed.

## Active Checkpoint

The coherent checkpoint is complete and ready for commit: typed inherited
field selection/lowering, explicit and qualified/unqualified inherited
non-static calls, protected derived-body access, owner-path validation, the
owner invariant audit, and demand-truth for state-free inherited construction
are implemented. Stage progress is proven by one removed checked-in PA16
failure with no added identity. The overload local string address gap,
explicit-constructor parsing/lifetime, broader initialization, friend/using
re-exposure, multiple/virtual inheritance, static members, and full PA16
success remain out of scope.

## Performance evidence and uncertainties

Same-owner member selection is an immediate scope lookup. Inherited lookup is
bounded by O(depth + candidates); member field/call lowering validates a typed
O(depth) path and layout sequence, with no whole-program scan, retry, or
rendered-name recovery. The implicit-default predicate walks only the
reachable typed base/member graph per declaration; its active vector is
bounded by the supported record depth and protects cycles. No cache was
needed for this checkpoint.

Representative command, using temporary non-repository sources with identical
state-free three-level chains and 1, 128, and 512 local declarations, ran five
times per size; all compiler invocations exited 0. On this workspace, 1 and
128 declarations each measured 0.00s elapsed in all five runs; 512 measured
0.01s elapsed in all five runs (user time 0.00-0.01s). This is measured
bounded behavior, not a formal benchmark. The final audit also reports
dev/src/pa12_semantic.cpp at 2975 lines and only the five pre-existing
bad-division warnings.

Mandatory gates after the corrections:
n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
exits 0 with 1167/1167, and
perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src exits 0 with
five warnings.

## Checkpoint ledger

| checkpoint | status |
| --- | --- |
| d0470e94 + working tree | Current, ready to commit: final PA16 49/243, 194 failures versus baseline 48/243, 195; removed protected identity, added empty set, coverage 243/243; through-PA15 1167/1167; audit exit 0 with five pre-existing warnings; all focused/course/constructor controls recorded above. |
| 0b534f2f typed direct member-call checkpoint | Landed implicit-object cv subset ranking, N3485 variadic comparison, typed PA15 member reachability, dense FunctionFact/fact metadata, declaration-only member ABI boundaries, hidden-object call formation, and source-file sizing. |
| b1e8272d + PA16 typed implicit-object boundary | Landed canonical Function-scope hidden-object ownership, fail-closed viability, typed demand indexing, direct PA15 lowering, and focused direct/member-call controls. |
| 37265733 typed member projection audit/repair | Landed direct/nested dot and arrow ownership tracing through PA12, PA11 RecordLayout::member_offsets, and PA15 LowIR. |

This increment is ready for the authorized commit after the final diff,
fixture-immutability, and status checks complete.
