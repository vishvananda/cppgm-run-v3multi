# PA12 audit record

## Current Checkpoint Review

This bounded `checkpointAudit` reviews the latest landed PA12 increment at
`47ca58bec4e11a5defd67a7ca44db7145ba936ff` (`pa12: add structured statement
semantics`) relative to `eee242c6`. The landed checkpoint was clean at audit
start; this audit records the approved source and record changes. The
authoritative full-suite baseline and final result are **103/166**, with
**63 failures** and all 166 tests covered; the through-PA11 gate is **685/685**.
The authorized broad gates have completed and their results are recorded below.

Ownership trace: `cppgm++` creates one `PPPreprocessingSession`, passes its
typed `PPTokenBuffer` to `parse_pa10_ast`, and passes the resulting `PA10Ast`
to one `PA11SemanticModel`. PA10 retains structured statement child order and
producer identities. PA11 owns canonical `TypeId`, `BindingId`, `ScopeId`,
lookup, and declaration facts. PA12 appends pointer-keyed `StatementFact`,
`DeclarationFact`, `SemanticFact`, and `ConversionFact` records to that same
model. `dump_pa12` renders those typed facts cold and deterministically; it
does not reparse rendered text or use text as semantic identity.

The review confirmed that compound scopes are reused through the PA11
compound-scope index, while control and unbraced-substatement scopes are
created once from PA10 node identity and kept out of the PA11 dump tree.
Condition declarations bind in the control scope, for-init declarations bind
in the for scope, and lexical loop/switch depth is passed directly through the
semantic statement walk. Case/default validation uses one local typed
`FlatIndex` per switch; no sibling scan, whole-arena scan, retry loop, or
hash-order rendering is used.

The bounded source repairs in this milestone are:

- One shared fixed-target promotion helper covers bool, signed/unsigned char,
  short/unsigned short, plain char, char16_t, and wchar_t to int, and char32_t
  to unsigned int. Fixed-underlying unscoped enumerations reuse it; scoped
  enumerations remain unpromoted. The dump retains the existing
  source-condition type convention while conversion facts retain the adjusted
  target.
- Switch legality first checks the generic conversion path, then checks exact
  mathematical representability in the promoted fundamental target. Duplicate
  keys are formed only after that check, so negative-to-unsigned and other
  out-of-range values are rejected rather than modulo-normalized. Scoped
  enumeration labels remain valid only for the same enum type.
- Synthesized case-label facts use one uint64_t payload with signed/unsigned/
  negative metadata. Signed output is rendered from magnitude, without an
  implementation-defined uint64_t-to-int64_t overflow cast; ordinary literal
  source spelling remains untouched.
- Empty declaration statements are valid empty control substatements; their
  enclosing typed control/branch facts omit invalid placeholder IDs rather
  than rejecting the statement or dumping an invalid fact.
- The final file-audit shape cleanup keeps the switch-validation context
  constructor compact and isolates lexical break/continue validation in a
  dedicated semantic helper without changing the typed ownership path.

Focused validation after the repair:

- The combined requested structured and neighboring checked-in set — **22/22
  passed**.
- `make test-pa12` completed with exit 2 and summary **103/166 passed, 63
  failures**.
- The exact through-PA11 command completed successfully with **685/685**.
- `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src` passed with
  exactly two known warnings: header-division warnings for
  `dev/src/cpp_semantic_core.h:1` and `dev/src/pa11_semantic_model.h:1`.
- `git diff --check` passed.
- Temporary probes outside the repository passed **17/17 expected outcomes**:
  the new promotion/representability matrix was **9/9** (three valid outputs,
  including full-width unsigned and same-type scoped-enum cases, plus six
  expected rejections, including the former modulo case), existing
  condition/for/switch scope checks were **3/3**, and empty control-
  substatement checks were **5/5**. Two cold dumps were byte-identical. These
  probes are not fixtures.
- A temporary layout probe measured `SemanticFact` as 144 bytes for the landed
  field shape and 136 bytes for the repaired compact shape.
- Normalized failure-path comparison against
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`
  found 63 unique baseline failures and 63 unique current failures, with
  **0 current-only**, **0 baseline-only**, and **103 + 63 = 166** covered.
- The changed-path audit found exactly the five approved files below and no
  test, reference, harness, grammar, or script changes.

The known residual `general/300-switch-scoped-enum-condition` still has the
pre-existing ordinary-enumerator dump mismatch and was not expanded under
this checkpoint. No source repair here changes the supplied 166-test
inventory. The preserved facts-200/facts-800 timing and immutable binary
evidence belongs to the landed
`47ca58bec4e11a5defd67a7ca44db7145ba936ff` checkpoint binary and its immutable
copy, not to the repaired build. The source/workload shape and the smaller hot
fact layout keep that evidence representative by analysis; no new performance
probe is required, and the preserved timings are not claimed as measurements
of the repaired binary.

## Prior checkpoint context

The preceding audit row at `eee242c6` established the shared PA12 semantic-fact
foundation and recorded the 90/166 checkpoint, its 76 residual failures, the
through-PA11 result, and the prior qualification/function-redeclaration
repairs. That residual family remains outside this bounded review.

## Audit ledger

| audit row | findings and ownership trace | evidence | uncertainties / residual exclusions | exact validation |
| --- | --- | --- | --- | --- |
| 2026-08-24 prior PA12 audit at `eee242c6` | Shared typed semantic owner, qualification safety, canonical function redeclaration/definition state, and lexical dump views were recorded before the structured-statement increment. | Prior checkpoint record: PA12 90/166 with 76 residual failures; through-PA11 685/685; performance and file-audit evidence preserved. | Broader PA12 residual families were explicitly excluded. | See the prior committed audit record in history. |
| 2026-08-24 PA12 `checkpointAudit` at `47ca58be` | Complete ownership path reviewed; shared fixed-target promotion, exact converted-case representability, scoped-enum legality, compact 64-bit case payload continuity, empty control substatements, child order, lexical jump validation, deterministic cold rendering, bounded indexes, and final file-audit shape are repaired or confirmed. | Final PA12 **103/166**, **63 failures**, all 166 covered; through-PA11 **685/685**; focused **22/22**; temporary probes **17/17 expected outcomes**; normalized failure paths: 63 baseline, 63 current, 0 current-only, 0 baseline-only; `SemanticFact` layout 144-to-136 bytes. | Future residual-family work requires separate authorization; PA12 is not complete. The two header-division warnings are preserved known findings. No tests, refs, harnesses, grammar, or scripts changed. | `make test-pa12` (exit 2, 103/166); exact through-PA11 command; `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src` (pass, 2 warnings); `git diff --check` (pass); changed-path audit. |
