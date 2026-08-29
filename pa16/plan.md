# PA16 implementation plan

## Stage Design

PA11 remains the typed owner of canonical member access. This checkpoint
extends that owner/data flow with sparse typed friend-class relations
(`NamedRecordId` owner and friend plus the reverse index) and a paired
using-declaration access view: `MemberAccess` plus its publishing `ScopeId`.
The view never rewrites the canonical binding/origin owner. A type using
preserves its canonical `TypeId` while recording the introduced declaration
and its class-member access; a value using carries canonical binding/origin
with the paired publishing view. PA12 carries both facts through `ValueRef`,
`MemberLookup`, and member/operator candidates; `member_accessible` evaluates
canonical private/protected/public access, friendship, base paths, and the
protected object rule. PA15 consumes selected typed facts and retains every
validated direct-base edge as a LowIR projection. No rendered-name recovery,
duplicate access table, whole-TU rescan, or retry loop is used.

The behavior is checked against the PA16 README, `spec.md` §§2--5 and 7,
and N3485 §§7.3.3 p17--18, 11.2 p4--6, 11.3 p1--10, and 11.4 p1: each named
declaration and base path is accessible; aliases have the access of their
member-declaration context; PA11-supported public class-member using at
namespace or block scope remains valid; friendship is neither inherited nor
transitive; and protected object expressions satisfy the additional object-class
rule.
Operator access-view propagation is traced through candidate formation;
unrelated operator behavior remains out of scope. The bounded model is
single, direct, non-virtual inheritance. Templates, variadics, and unrelated
PA16 residuals remain out of scope.

## Failure Map

The final PA16 run remains `179/243` identities passing, `64` failing, with
all `243/243` identities covered. The exact comparison against the turn-start
authority reports `64` authority failures, `64` final failures, final-only
`0`, and baseline-only `0`; durable logs are recorded in `pa16/audit.md`.
The bounded repairs address intermediate qualified type/value components,
friend-context qualified base specifiers, class-owned using source access and
views, and fail-closed binding/view provenance.

The selected existing matrix is `12/14`: the two documented checked-in
LowIR-shape residuals remain, while their semantic status paths pass. Courses
405, 411, and 419 plus the 419 syntax check are green. Course 419 is reduced
observation coverage and is not stage progress. Through-PA15 is `1167/1167`,
including the unchanged PA11 class-constants and using fixture. No handout,
reference, fixture, harness, comparator, or exit-status file changed.

## Active Checkpoint

The audited ownership path is the landed PA16 access increment across:

- `dev/src/pa11_semantic_model.h`
- `dev/src/pa11_semantic_core.cpp`
- `dev/src/pa11_semantic.cpp`
- `dev/src/pa11_semantic_types.cpp`
- `dev/src/pa12_semantic_selection.h`
- `dev/src/pa12_semantic.cpp`
- `dev/src/pa12_semantic_calls.cpp`
- `dev/src/pa12_semantic_member.cpp`
- `cppgm.tests/course/pa16/419-typed-using-access-regression.sh`

The current repair is limited to the first four implementation files and
the permitted 419 control. The type-valid using branch preserves the
canonical `TypeId`, records the introduced binding as the declaration/access
owner, and applies `process_class_body`'s current access. Source access is
validated before publication; public class-member type/value using remains
supported at namespace or block scope, while inaccessible private/protected
sources are rejected unless the publishing context has access. Namespace-to-
namespace using remains valid. Typed lookup checks every class qualifier and
uses the derived class as the base-specifier access context.

`ValueEntry` starts with canonical binding/origin and declaration point, and
carries the optional access view/publishing scope. PA12 preserves the canonical
owner and using view through member, static, non-static, and operator
candidates; `member_accessible` checks source declaration, reachable base,
friend, private/protected, and protected-object rules. PA15 lowering is
unchanged and keeps the full selected direct-base path. Friend relations are
sparse and direct only. The 419 additions cover public protected-type
re-exposure, private/protected alias views, friend/private source access,
public namespace-scope class-member using, and valid namespace using while retaining all
existing coverage.

## Performance Evidence

The access path uses a sparse friend reverse index, bounded lexical ancestry
walks, named-component qualification, and the relevant direct-single-base
chain. Publication and lookup validate only typed identity/view tuples;
candidate work is bounded by the selected class/base chain. The structural
noise replay compares a 40-line case with the same case plus 64 unrelated
empty classes (105 lines): both repository-compiler outputs are byte-identical
at 59/59 LowIR lines and six/six base projections, with LowIR SHA-256
`a994e25767151654c710b2724364f1b5f3d9b071c3b9326aef284b962a1b2fd6`; the
negative stderr SHA-256 is identically
`37e6f8ed897d209b62c1b3b33e831cb114a86a06e792e0aa8c0645df156d3fd3`.
Evidence summary:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-access-boundary-probe-final-20260829-v4/summary.log`.
This is structural noise-isolation evidence only; no timing, RSS, allocation,
or speedup claim is made.

## Validation

Final evidence is durable in the paths cited by `pa16/audit.md`: build and
courses 405/411/419 (including `sh -n`) exit `0`; the selected 14-test
command exits `2` at `12/14` with only the two documented LowIR residuals;
`make test-pa16` exits `2` at `179/243`; the exact identity comparison has
no final-only or baseline-only identities; through-PA15 exits `0` at
`1167/1167` including the unchanged PA11 class-constants/using fixture; and
`perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0`
with five known header-division warnings. `git diff --check` exits `0`.
No existing handout, reference, fixture, harness, comparator, or exit-status
file changed.

## Next Checkpoint

The next checkpoint is a remaining PA16 semantic/lifecycle/layout residual
family outside typed access control. It must preserve the exact 64-identity
map unless a later authorized rerun proves otherwise, canonical ownership,
direct non-transitive friendship, protected object rules, supported using
behavior, and full per-edge single-base lowering.

## Checkpoint Ledger

| checkpoint | result |
| --- | --- |
| `36b93869` historical aggregate handoff | `159/243` PA16 passing, `84` failures, `243/243` covered; PA1--PA15 `1167/1167`. |
| `dea01c52` aggregate implementation | Historical aggregate increment: `164/243` passing, `79` failures, `243/243` covered; through-PA15 and audit evidence preserved. |
| `3c2114b6` typed-builtin turn-start | Historical clean state at `164/243`, `79` failures, `243/243` covered; exact residual map carried forward. |
| `d7ed98aa` typed builtin boundary | `167/243` passing, `76` failures, `243/243` covered; typed builtin semantic/lowering owner added. |
| `f290784f` typed builtin audit | Historical clean baseline: `167/243`, `76` failures, `243/243` covered; PA1--PA15 and audit pass. |
| `3b7d8e6a` qualified-type checkpoint | `173/243` passing, `70` failures, `243/243` covered; prior broad evidence preserved. |
| `working tree after 3b7d8e6a` historical constructor first stop | Selected constructor family and preservation controls passed before the later constructor increment. |
| `30d69fc3` landed inheriting-constructor checkpoint | Historical landed increment: `176/243` passing, `67` failures, `243/243` covered; typed N3485 wrapper/default/DMI/copy/order-independent evidence and durable broad/identity/probe logs are retained above. |
| `0fb73ad4` PA16 access turn start | Clean authority for this checkpoint: `176/243` passing, `67` failures, `243/243` covered; through-PA15 `1167/1167`, audit with five known header-division warnings. |
| `PA16 typed access-control checkpoint` | Completed bounded audit/repair of landed `135e3a95` relative to `0fb73ad4`: canonical owner/access, direct friend identity, paired using view/publishing scope, typed qualified type/value and base access, source accessibility, private/protected/friend/protected-object rules, and PA15 per-edge projection are traced. The type using fix preserves canonical `TypeId` while assigning the introduced declaration/access owner; public class-member type/value using remains supported at namespace or block scope, inaccessible sources are rejected by the p17 access boundary, and namespace-to-namespace using remains valid. Operator access propagation is traced through candidates; unrelated operator behavior is out of scope. Final PA16 is `179/243` with `64` failures and `243/243` coverage, with exact final-only and baseline-only sets both empty. Focused PA16 is `12/14` with two checked-in LowIR residuals; courses 405/411/419 and `sh -n` pass; structural noise evidence is recorded. Through-PA15 is `1167/1167`; file audit exits `0` with five known warnings. No handout, fixture, reference, harness, comparator, or exit-status file changed. |
