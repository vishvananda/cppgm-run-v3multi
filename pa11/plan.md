# Stage Design

- Turn-start baseline: clean `f6ad1eb8`, unchanged PA11 discovery of 68
  tests, and `0/68` passing because `--emit-types` always returned
  `EXIT_NOT_IMPLEMENTED`.
- One owner consumes the typed `PA10Ast` after the existing preprocessing and
  parsing path: `PPPreprocessingSession` -> `parse_pa10_ast` ->
  `PA11SemanticModel::analyze` -> deterministic dump. It owns scopes,
  declarations/bindings, lookup, named identities, and hash-consed type IDs;
  presentation strings are created only while dumping.
- The owner uses lexical vectors for deterministic order and hash indexes for
  direct lookup. Namespace reopening and qualified targets reuse the same
  scope/record identity. Parent lookup is bounded by scope depth; a using
  directive lookup may traverse each nominated scope reachable from that
  lookup, with a visited set, rather than claiming unconditional O(1) or O(n).
- The driver still emits the existing translation-unit wrapper and preserves
  PA10 preprocessing/parsing, multi-TU ordering, and failure propagation. The
  semantic walk follows the PA11 contract and N3485 3.3.6, 3.3.7, 3.4.1,
  3.4.3, 7.1.3, 7.3.1--7.3.4, 8.3.5, and the clauses named by the checked-in
  PA11 tests. It never reparses rendered PA10 text.

# Failure Map

The following disjoint lists account for the unchanged 68-test stage set
exactly once: 51 expected-success tests and 17 expected-failure tests.

Expected success — foundational scopes and declarators (15):

- `spec/100-alias-and-function`, `spec/100-class-scope`, `spec/100-empty`,
  `spec/100-global`, `spec/100-namespace-alias`, `spec/100-namespace`,
  `spec/100-qualified-type-lookup`, `spec/100-using-declaration`,
  `spec/100-using-directive`.
- `general/100-class-forward`, `general/100-function-pointer-void-parameter`,
  `general/100-namespace-class`, `general/100-namespace-reopen`,
  `general/100-nested-class`, `general/100-variadic-function-declaration`.

Expected success — named classes, anonymous records, aliases, and qualified
targets (8):

- `spec/200-anonymous-type-typedefs`,
  `spec/200-class-key-compatible-redeclaration`,
  `spec/200-namespace-anonymous-class-array-object`,
  `spec/200-namespace-anonymous-union-injected-members`.
- `general/200-alias-qualified-class-lookup`,
  `general/200-class-qualified-lookup`,
  `general/200-elaborated-type-hidden-by-function`,
  `general/200-qualified-namespace-function-definition-parameter-type`.

Expected success — enums and enumerator lookup (9):

- `spec/200-elaborated-enum-type-specifier`, `spec/200-enum-scoped`,
  `spec/200-enum-unscoped`, `spec/200-opaque-scoped-enum`,
  `spec/200-parenthesized-enumerator-decltype`.
- `general/200-qualified-member-scoped-enum-definition`,
  `general/200-scoped-enum-qualified-enumerator-collision`,
  `general/200-scoped-enum-qualified-enumerator`,
  `general/200-using-imported-scoped-enum`.

Expected success — constants, `decltype`, bounds, and object facts (10):

- `spec/200-const-int-static-assert`, `spec/200-constant-short-circuit`,
  `spec/200-decltype`, `spec/200-sizeof-alignof-bounds`.
- `general/200-class-constants-and-using`,
  `general/200-constexpr-object-vs-function-types`,
  `general/200-constexpr-variable`, `general/200-qualified-decltype`,
  `general/200-sizeof-qualified-type-idexpr`,
  `general/200-sizeof-type-like-id`.

Expected success — template scopes, using values, and namespace reachability
(7):

- `spec/200-template-parameter-scope`, `spec/200-using-directive-values`,
  `general/200-inline-namespace-qualified-lookup`,
  `general/200-namespace-alias-qualified-using-directive-target`,
  `general/200-template-template-parameter`,
  `general/200-using-declaration-values`,
  `general/300-noexcept-function-pointer-declarator`.

Expected success — remaining declarator normalization (2):

- `general/200-void-parameter-normalization`,
  `general/300-scoped-enum-cast-constant`.

Expected failure — unknown names and namespace/binding conflicts (4):

- `spec/100-bad-unknown-type`, `general/100-bad-using-target`,
  `spec/300-binding-after-namespace-bad`,
  `spec/300-namespace-after-binding-bad`.

Expected failure — invalid enum declarations and initializers (4):

- `spec/300-elaborated-undeclared-enum-bad`,
  `spec/300-enumerator-invalid-initializer-bad`,
  `spec/300-opaque-enum-redecl-underlying-bad`,
  `general/200-bad-opaque-unscoped-enum`.

Expected failure — namespace aliases, references, and bounds (4):

- `spec/300-namespace-alias-non-namespace-bad`,
  `general/200-bad-pointer-to-reference-alias`,
  `general/200-bad-sizeof-incomplete-class`,
  `general/300-block-zero-array-bound-bad`.

Expected failure — constant evaluation (3):

- `spec/300-signed-constant-overflow-bad`, `general/200-bad-static-assert`,
  `general/200-class-scope-bad-static-assert`.

Expected failure — template and using restrictions (2):

- `spec/300-template-template-inner-parameter-scope-bad`,
  `spec/300-using-declaration-template-id-bad`.

# Active Checkpoint

- The approved first stop was an uncommitted typed-AST owner with all 15
  successful 100-family cases passing (`make -C pa11 check TEST='tests/spec/100-*.t tests/general/100-*.t'`, 17 discovered including the two expected failures).
- Review corrections and the full expansion now live in that same owner: a
  canonical type binding records compatible class-key declaration facts;
  namespace/type conflicts are checked in both orders; same-union
  redeclarations remain compatible; qualified targets reuse scope/record
  identity; and qualified enum definitions retain a canonical member record
  while exposing the required qualified dump view.
- The owner now covers all listed success cohorts: named/anonymous records,
  elaborated enums, scoped/unscoped enumerators and values, constants and
  short-circuit evaluation, constexpr objects, decltype, sizeof/alignof and
  completeness, template/template-template parameter scopes, inline
  namespaces, using values, qualified function/member definitions, noexcept
  syntax, and the listed diagnostics. The qualified-enum parser extension
  preserves the typed PA10 boundary and does not introduce a second parser.
- Exact broad result: `make test-pa11` discovered and passed `68 / 68`
  unchanged. No tests, fixtures, or references were edited.

# Performance Evidence

- Structural complexity target: one walk over the typed AST, one stored scope
  edge per entered scope, and hash-consed type construction. Direct lookup is
  hash-based plus the lexical parent chain; using-directive lookup is bounded
  by the nominated-scope graph actually visited for that lookup and guarded by
  a visited set. Dump order is lexical-vector order, independent of hash order.
- Temporary `/tmp` stress inputs passed `--emit-types` at both sizes. The
  small input had 64 namespace/class declaration pairs plus one using scope:
  5,462 input bytes, 130 emitted scopes, 192 emitted bindings, 8,768 output
  bytes, and `0.00s` elapsed. The 4x declaration input had 22,910 input bytes,
  514 scopes, 768 bindings, 35,972 output bytes, and `0.02s` elapsed. These
  measurements are representative structural evidence, not a benchmark
  guarantee; the observed scale is approximately linear. The earlier focused
  100-family incremental run was `0.19s` after compilation.

# Checkpoint Ledger

- `f6ad1eb8`: clean turn start; PA11 `0/68`; stub driver only.
- First uncommitted milestone: typed PA10-AST owner implemented; focused
  100-family result `17/17` (15 expected successes and 2 expected failures).
- Supervisor review: architecture approved; requested canonical declaration,
  conflict, union, plan, performance, and remaining-cohort expansion accepted.
- Post-review expansion: all 51 expected-success and 17 expected-failure
  tests pass with unchanged discovery (`make test-pa11`: `68 / 68`).
- Required earlier-stage gate: `make test-report-through-pa10` passed all
  `617 / 617` tests.
- Required audit: `perl scripts/cppgm_file_audit.pl --stage pa11 --paths dev/src`
  passed, with one pre-existing `cpp_semantic_core.h` substantial-header-body
  warning.
- Coherent implementation commit: `1154916b` (`Implement PA11 semantic type
  owner`); all seven intended files were the only staged paths.
- Final plan-ledger update is the only follow-up mutation; verify `git status
  --short` is empty after it.
