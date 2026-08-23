# PA10 Checkpoint Plan and Evidence

## Spec alignment

The PA10 production flow is:

```text
phase-3 source -> typed posttoken facts/indexes
    -> PA10Parser seed -> one canonical postfix-suffix loop
    -> typed PA10Ast -> cold deterministic renderer
```

`PA10ParserSupport` owns fixed-token classification and indexed delimiter/
template facts.  `PA10Parser` owns the AST.  Builtin function-style casts use
one shared, exact `simple-type-specifier` keyword predicate; `typeid` is a
postfix seed; call/member/subscript/post-inc/dec suffixes have one consumer.
`RShiftPiece1/RShiftPiece2` remain typed facts: the indexed
`rshift_piece1_nested_close` byte records that an adjacent Piece2 closed
another indexed angle, and is consumed by every bounded template-follow
lookahead.  It is a structural/index fact, not a complete semantic
template-id-versus-`<` classification; that ambiguity remains outside PA10.
The renderer validates
the fixed synthetic `IdExpression` and renders its cold spelling once.  There
is no host/reference shortcut, textual downgrade, parser retry, duplicate
postfix path, or unbounded new recursion.

## Exact failure map

The turn-start baseline is reused evidence: **158 discovered, 141 passed, 17
failed**.  The landed checkpoint started from 136/158 with 22 failures and
removed exactly these five identities:

```text
pa10/tests/general/200-builtin-function-style-cast-expression.t
pa10/tests/general/200-builtin-function-style-cast-member-body.t
pa10/tests/general/200-conditional-simple-type-shift-return.t
pa10/tests/general/200-template-conditional-simple-type-shift-return.t
pa10/tests/general/200-typeid-postfix-member-suffix.t
```

The original 158-test universe retains exactly this final residual set:

```text
pa10/tests/general/200-elaborated-enum-member-declarators.t
pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t
pa10/tests/general/200-friend-function-template-declaration.t
pa10/tests/general/200-friend-type-declaration.t
pa10/tests/general/200-global-struct-paren-declaration.t
pa10/tests/general/200-lambda-capture-forms.t
pa10/tests/general/200-local-typedef-paren-declaration.t
pa10/tests/general/200-member-template-parameter-value-vs-template-name.t
pa10/tests/general/200-mock-type-declaration-ambiguity.t
pa10/tests/general/200-parenthesized-new-type-vs-placement.t
pa10/tests/general/200-placement-new-identifier-led-initializer.t
pa10/tests/general/200-placement-new-pack-init.t
pa10/tests/general/200-qualified-enumerator-call-argument.t
pa10/tests/general/200-sizeof-elaborated-class-type-id.t
pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t
pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t
pa10/tests/general/200-trailing-parameter-vendor-attribute.t
```

All original fixtures remain.  The added reduced course regression for invalid
`auto(1)` cast classification passes independently.  Fresh final
`make test-pa10` evidence is **159 discovered, 142 passed, 17 failed**; the
added regression accounts for the extra discovered and passing test.  Sorted
failure identities compare exactly to the turn-start 17 (`diff` exit 0), so no
original identity was replaced or compensated away.

## Checkpoint findings and focused evidence

The bounded audit repaired the exact cast-keyword domain (`KW_AUTO` is a
declaration specifier, not a PA10 simple-type-specifier), made `build_indexes`
size/reset every output array, routed the three older bounded RShift lookaheads
through the typed indexed-angle marker, and made renderer validation reject
invalid synthetic-node identity/default fields.  No residual owner was
entered.

Current focused results:

```text
make -C dev cppgm++ CXX=g++                                  exit 0
make -C pa10 check [14 postfix/RShift/typeid/malformed cases] exit 0; 14/14
make -C pa10 check [12 template/member-pointer lookahead cases] exit 0; 12/12
make -C pa10 check [new cast-domain regression]               exit 0; 1/1
make -C pa10 check [relational/ordinary-shift siblings]       exit 0; 4/4
warning-clean syntax compiles of three affected .cpp files   exit 0 each
RShift empty/reused-vector probe                             exit 0
renderer invalid-node probe                                  exit 0
four malformed/truncated probes                              exit 1 each
make test-report-through-pa9                                  exit 0; 457/457
file audit --stage pa10 --paths dev/src                       exit 0; 1 pre-existing warning
git diff --check                                             exit 0
```

The RShift probe observes two indexed angle closes as `5 3 2 1` and an ordinary
pair as `4 1 0`.  The new regression is a status-only fixture, so its checked-in
`EXIT_FAILURE` sidecar is the complete expected material under the PA10 test
contract.

## Performance evidence

The final focused executable hash is
`e98aa88ab7f577b7b3435db10860e34c100bb2829854170d00800a898e91e863`.
Thirty-two repeated invocations per input, on that immutable executable,
measured:

| input | elapsed | user | sys | peak RSS |
| --- | ---: | ---: | ---: | ---: |
| `200-typeid-postfix-member-suffix.t` | 0.10 s | 0.04 s | 0.05 s | 4428 KB |
| `200-conditional-simple-type-shift-return.t` | 0.10 s | 0.03 s | 0.07 s | 4428 KB |

These are single-executable characterization values, not a comparative claim.
The measurements are reused from the focused audit; the final rebuild retained
the same executable hash, so they apply to the committed executable.
The structural bounds are one indexed O(n) pass, constant-lookahead seed
selection, monotonic suffix consumption, typed bounded lookahead, and the
existing work/recursion/nesting limits.  The new fact is one byte per token;
no AST hot-record field was added.

## Next checkpoint

Supervisor-selected residual-family audit.  Keep placement-new, lambda,
general declaration/declarator, and qualified-name work out of this path unless
the next owner is explicitly selected and evidence proves ownership.

## Checkpoint ledger

| checkpoint | status | compact evidence/state |
| --- | --- | --- |
| `a2b82dcb` template/angle ownership | landed historical | typed components/sidecars and bounded close ownership; 106/157 historical |
| `27623d64` declarator/member boundary | landed historical | unified declarator/member path and bounded shape |
| `b9b58b9c` declarator audit | landed historical | 123/157 historical; through-PA9 457/457; one pre-existing audit warning |
| `08c38115` structured names/special members | landed historical | removed 12 prior residuals; retained course boundary fixture |
| `017eb658` structured-name audit | starting HEAD | clean at 158/136 with 22 failures |
| `25f784873f2a852fd825316b2188d9f157f8eae5` typed postfix checkpoint | audited and committed | fresh 142/159 with exact original 17 failures; prior-through 457/457; file audit passed with one pre-existing warning; focused 14/14 + 12/12 + regression 1/1; immutable performance characterization above |
