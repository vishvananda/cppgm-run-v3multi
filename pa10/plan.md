# PA10 Checkpoint Plan and Evidence

## Spec alignment and ownership

```text
typed phase-7 tokens
    -> build_indexes: delimiter/template/RShift facts plus one typed
       PA10ParenthesizedGroupKind per parenthesized group
    -> declaration_start / parse_declarator / parameter and new routing
    -> canonical PA10 AST
    -> observation-only renderer
```

PA10 remains a syntax-only AST stage.  The relevant grammar path is
`declaration` -> `simple-declaration`/`function-definition`,
`declarator`/`abstract-declarator`, `parameter-clause`, and `new-expression`.
This satisfies `spec.md` §1's single forward pipeline, §2's typed-fact
continuity and single owner, §4's bounded work, and §7's conformance and
measurement rules.  N3485 §6.8/§8.2 declaration preference is implemented by
indexed group facts and bounded followers, not by semantic lookup.

`PA10ParserSupport::build_indexes` is the sole producer of the typed
`PA10ParenthesizedGroupKind` vector.  The parser consumes that fact for
declaration-start, canonical nested declarators, root-only parameter-clause
preference, abstract declarators, and new-expression type-ids.  A same-path
Phase A correction removes the unnecessary `*`-only gate for named
member-pointer groups; reference-led nested groups retain the checked PA10
expression-safe boundary.  `parenthesized_declaration_start_at` additionally
applies a bounded exact-single-identifier and declaration-follower decision;
that contextual routing is not a duplicate implementation of the support-owned
multi-shape group classification.  No source-text downgrade/reparse, trial
AST, backtracking, lookup, host/reference shortcut, duplicate parser, or
renderer path is present.

## Exact turn-start failure map

The authoritative start evidence is **152/159 passing; 159 discovered**.  The
seven failures are all residual families and remain out of scope:

```text
pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t
pa10/tests/general/200-lambda-capture-forms.t
pa10/tests/general/200-member-template-parameter-value-vs-template-name.t
pa10/tests/general/200-qualified-enumerator-call-argument.t
pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t
pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t
pa10/tests/general/200-trailing-parameter-vendor-attribute.t
```

The Phase B identity rule is exact: a failure test is judged by exit status;
a success test requires exact AST output and exit status.  New passes cannot
offset a new or replaced failure identity.

## Affected invariants and repair

- Every parenthesized group has one typed support-owned fact, with reset side
  indexes and token-count sentinels for missing delimiters.
- `declaration_start` scans only the current direct pointer/reference/cv
  spine or indexed parenthesized shape and checks a finite declaration
  follower set.
- `parse_declarator` uses the existing nested AST path; only the root of
  `parse_parameter_declaration` gets parameter-clause preference.
- `parse_abstract_declarator` and `new` reuse the same group fact, preserving
  the distinction between abstract declarators, parameter clauses, and
  parenthesized initializers.
- Named raw-pointer and member-pointer groups are declaration candidates;
  direct reference spines remain declaration-routed.  `T(&x);` is
  syntactically a valid reference declarator and general §6.8 preference would
  select a declaration, but PA10 deliberately preserves the checked
  `function(&spawned_thread);` expression AST because this syntax-only stage
  does not perform lookup; this is a fixture-bound exception.
- Malformed/truncated groups fail closed through sentinels and the existing
  parser work, recursion, angle, non-angle, and renderer limits.

## Focused evidence

```text
make -C dev cppgm++
  exit 0

make -C pa10 check [21 focused declaration/declarator/parameter/new/operator
                    tests, including malformed and member-pointer siblings]
  exit 0; pa10 check: PASS (21/21)

g++ ... /tmp/pa10_group_index_harness.cpp dev/src/pa10_parser_support.cpp ...
/tmp/pa10_group_index_harness
  pa10 group index harness: PASS
```

The temporary `/tmp/pa10_phase_a_decl_ambig_probe.t` exited 0 and its AST
showed direct pointer/reference/cv declarations, named and abstract
member-pointer groups, the four §6.8 declaration forms, the three expression
followers, and the preserved `T(&x)` reference-led expression boundary.  The
checked malformed parameter-list test returned its expected failure status.
The corrected external harness includes `(C::*p)` and asserts its exact
`NamedDeclarator` group value, in addition to reset, sentinel, and truncation
checks.

## Bounded performance evidence

No timing claim is made.  Source inspection establishes one forward
token/delimiter pass and one reverse token visit; each individual pointer,
member-pointer, or mock-name scan is bounded by its current delimiter owner
and indexed template closes.  `build_indexes` returns its instrumented
predicate/scan counter and the parser charges that returned count against
`96 * token_count + 2048`.  This is a finite accounting ceiling, not a proof
of a tight aggregate-O(n) bound when nested group spans overlap.  Parser
recursion and AST/renderer nesting limits remain in force.  No source is
reparsed and no trial tree is built.  The focused matrix and truncated-group
harness are the bounded evidence; no larger stress result is claimed.

## Phase B broad evidence

Fresh Phase B results, with the exact turn-start failure identities above,
are:

```text
make test-pa10
  exit 2; TEST SUMMARY: 152 / 159 TESTS PASSED; 159 discovered

n=10; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
  exit 0; ===== ALL TESTS PASSED SUCCESSFULLY! (457 / 457) =====

perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src
  exit 0; File audit passed for pa10 with 1 warning(s).
  [warning][bad-division] dev/src/cpp_semantic_core.h:1: header contains substantial implementation body; prefer .cpp ownership

git diff --check
  exit 0; no output
```

The fresh PA10 run is `152/159` with exactly the seven turn-start identities,
so there is no coverage reduction or new/replaced failure identity.  The
through-PA9 report is `457/457`.  The only file-audit warning is the known
pre-existing `dev/src/cpp_semantic_core.h:1 [bad-division]` warning; no new
warning was introduced.  No timing claim is made.

## Next checkpoint

The next checkpoint is the separately assigned residual-family audit.  The
seven residual families remain untouched.  Do not edit tests, references,
grammar, harnesses, build files, or other parser surfaces.

## Checkpoint ledger

| checkpoint | status | compact evidence/state |
| --- | --- | --- |
| `a2b82dcb` template/angle ownership | landed historical | typed components/sidecars and bounded close ownership; historical 106/157 |
| `27623d64` declarator/member boundary | landed historical | unified declarator/member path and bounded shape |
| `b9b58b9c` declarator audit | landed historical | historical 123/157; through-PA9 457/457; one pre-existing audit warning |
| `08c38115` structured names/special members | landed historical | removed 12 prior residuals; retained course boundary fixture |
| `017eb658` structured-name audit | historical starting point | clean at 158/136 with 22 failures |
| `25f784873f2a852fd825316b2188d9f157f8eae5` typed postfix checkpoint | landed historical | historical 142/159 with exact original 17 failures; prior focused postfix evidence |
| `d24f8e1689130b0449e19654ffd9e9f3dfc3b853` structured new expressions | landed historical | bounded indexed abstract-declarator correction; final gates and evidence retained in history |
| `c16e04ef82e93bb0c628d2f495cc7132d47dd749` elaborated-type boundary | completed historical; residuals remain | focused 3/3 + 16/16; historical PA10 148/159 with exact 11 residuals; through-PA9 457/457; file audit exit 0 with one known warning |
| `90e9687c759a39d9b4844cdc01deed3ab1e80250` declaration/declarator ambiguity | completed; seven residuals remain | focused 21/21; corrected member-pointer harness PASS; §6.8 probe PASS; PA10 152/159 with exactly the seven turn-start identities; through-PA9 457/457; file audit exit 0 with one known warning; diff check exit 0 |
