# PA4 architecture-audit final record

Scope is PA4 only: phases 1--6 and the tokenization/decoding part of phase 7,
ending at typed posttokens and the `macro` CLI token dump.  PA4 has no object,
LowIR, ABI, relocation, or native-emission evidence; those later-stage facts
are intentionally out of scope.

## Ownership and fact continuity

The production path is one forward flow:

```text
source bytes
  -> pp_tokenizer phase 1--3 facts
  -> PPTokenBuffer (typed kind, fixed identity, spelling IDs)
  -> MacroProcessor directives/definitions/substitution/rescan
  -> typed PPTokenBuffer
  -> posttokenize_cpp_tokens
  -> PostTokenStream phase 5--7 facts
  -> macro CLI presentation
```

- `dev/src/pp_tokenizer.cpp` owns phases 1--3.  Its fixed-token tables emit
  `PPTokenFixedIdentity` through the typed `IPPTokenStream` seam.  Digraphs
  share their semantic enum with their primary spelling (`%:` is `Hash`,
  `%:%:` is `HashHash`, `<:` is `LeftBracket`, and so on), and the 13
  identifier-like operator words are classified there as phase-3 identifiers
  with their fixed posttoken identity.  Exact spelling remains payload.
- `dev/src/IPPTokenStream.h` owns the shared phase-3 transport.  `PPToken`
  contains kind, one compact fixed identity field, and a `PPSpellingId`;
  `PPTokenBuffer` owns the spelling table and token vector.  The
  `IdentifierAsPreprocessingOpOrPunc` kind preserves phase-3 identifier
  origin while its fixed identity survives transport.  `PaintedToken` copies
  only the compact token and paint root, not an owning string.  Macro and
  parameter identities are also interned IDs; arbitrary identifier spelling
  is still preserved exactly in the stage table.
- `dev/src/macro.cpp` owns line-start `#define`/`#undef` parsing,
  `MacroDefinition`, raw/expanded/stringized arguments, placemarkers, paste,
  token-local recursion paint, and deque rescan.  Macro lookup and parameter
  lookup use IDs.  The fixed directive names and `__VA_ARGS__` are classified
  once to interned IDs rather than repeatedly compared as token text.
- `dev/src/posttoken.cpp` owns the PA2 phase-5--7 decoder.  The typed fixed
  identity path maps punctuators and all 13 identifier-like operator words
  directly to `SimpleTokenType`, preserving the exact source/digraph payload;
  the identifier-like route uses `emit_simple_identifier` so downstream
  posttoken consumers retain the phase-3 identifier-origin distinction.
  Its existing spelling lookup is limited to ordinary identifiers and the
  standalone textual posttoken entry point: that is the one-time phase-7
  keyword/operator conversion boundary, not recovery of an already-classified
  macro token.
  `dev/macro.cpp` only formats `IPostTokenOutput` facts.

## Textual boundary accounting

Within macro replacement, paste and stringization are the two text
synthesis/reclassification boundaries:

1. `##` concatenates the exact left/right token payloads, calls the PA1
   tokenizer once on the pasted spelling, requires one non-whitespace token,
   stores the resulting typed kind/fixed identity and payload ID, and rescans
   it.
2. `#` constructs one string-literal spelling from raw argument payloads,
   with required whitespace collapse and escaping, and stores it directly as a
   typed `StringLiteral`.  It is not retokenized, because phase-1 treatment
   of raw-string payload text would change the required stringized result.

The variadic separator inserted for `__VA_ARGS__` is a typed comma plus a
whitespace event.  Separately, phase-7 decoding consumes retained literal,
numeric, and ordinary-identifier spelling to decode values or map the
ordinary fixed vocabulary; CLI output renders posttoken facts for the
requested presentation.  Those are phase-7 decoding/presentation uses, not
macro-replacement semantic recovery.  No final output is rendered and
reparsed; the expanded vector goes directly to `posttokenize_cpp_tokens`.

## Findings and resolutions

### Fixed token identity recovery — repaired

The HEAD implementation used one `PreprocessingOpOrPunc` kind and recovered
`#`, `%:`, `##`, `%:%:`, parentheses, commas, and every other fixed
punctuator by spelling comparisons in the macro owner.  It also classified
the 13 identifier-like operator words in the tokenizer, then discarded that
identity and recovered them from spelling in posttokenization.  Both violated
spec section 2.  The canonical producer now emits one `PPTokenFixedIdentity`
enum through the typed seam; `PPToken` carries it alongside the phase-3-origin
kind, the collector preserves it, and posttoken maps it directly.  The old
string callbacks remain compatibility adapters for legacy PA1 streams; the
production tokenizer, collector, macro owner, and typed posttoken path do not
use spelling lookup for fixed-token semantics.  No typed side index was added
beside the old semantic path.

### Per-token owning spellings — repaired

The old `PPToken` and `PaintedToken` records owned `std::string` payloads.
The full path now carries IDs into a stage-owned `PPSpellingTable`, and macro
definitions carry ID names/parameters.  Exact spellings remain available for
the two macro-replacement boundaries, phase-7 decoding, and final
presentation, but are not copied into each hot token record.  A measured
layout probe reports `sizeof(PPToken)=16`
bytes versus `sizeof(std::string)=32` on this build; this is structural
evidence for the scoped repair, not an allocation-count claim.  The table's
index and values are stage-owned storage and are retained only with the PA4
token buffer until posttoken consumption.

### Deep active paint — repaired

The old `map<vector<macro-id>, paint-id>` copied and sorted every active prefix.
The supervisor’s clean-HEAD deep object chain measured approximately 0.03 s /
14.3 MiB at n=1000 and 0.13 s / 41.5 MiB at n=2000, exposing superlinear
retained prefix storage.  Paint is now a persistent fixed-width binary trie:
an add copies one 64-bit key path, membership is a path walk, and union and
difference preserve sharing at untouched subtrees.  The blue-paint semantics,
including maximum inherited ID at the parameter boundary, are unchanged.  No
global recursion cutoff was introduced.

### Other audited properties

- Rescan is a bounded deque process driven by token-local unavailable/deferred/
  blocked state; it has no whole-stream retry loop or arbitrary recursion cap.
- The ordinary-argument prescan cache is local to one invocation and indexed
  by complete parameter position; raw, stringized, and paste-adjacent uses do
  not incorrectly reuse an expanded value.
- Unordered maps are used only for lookup.  Macro IDs, token order, spelling
  IDs, and output order are assigned/traversed deterministically; paint tries
  are never enumerated through hash iteration.
- Source audit of the changed production path found no shell, host-compiler,
  reference-binary, previous-solution, assembler, or linker invocation and no
  second macro/posttoken production model.
- Rejected approach: retaining generic punctuator spelling plus a typed side
  index would leave the prohibited semantic recovery path in place; the enum
  is therefore carried by the canonical token itself.

## Performance evidence

The prior shallow `ID`/`CAT`/`CAT_I` family remains useful historical evidence:
the checked-in HEAD runs produced n+1 output lines with five-run ranges of
0.00 s / 3,756--3,792 KiB RSS at n=100 (1,760-byte input), 0.02 s /
5,180--5,444 KiB at n=1,000 (17,960 bytes), and 0.17--0.19 s /
19,964--20,460 KiB at n=10,000 (188,960 bytes).  Those runs did not test deep
active paint and are not used as proof for it.

After the trie and spelling transport repair, the deep chain
`#define M0 M1` through `#define Mn 1; M0` was run three times in interleaved
order for each size.  All runs exited successfully:

| n | real-time range | max-RSS range |
|---:|---:|---:|
| 100 | 0.00 s | 4,020--4,032 KiB |
| 300 | 0.00--0.01 s | 4,796--4,836 KiB |
| 1,000 | 0.01--0.02 s | 5,628--5,768 KiB |
| 2,000 | 0.03--0.04 s | 9,040--9,288 KiB |

Supplemental single runs at n=4,000 and n=8,000 also exited 0 at 0.04 s /
14,512 KiB and 0.09 s / 25,716 KiB respectively; they are not treated as
repeated medians.

The fixed-width trie structure and this scaling-sensitive sample establish
that copied active paint prefixes are gone.  They do not constitute an
asymptotic theorem for all macro expansion shapes; paste, argument copying,
retokenization, and literal spelling length remain semantic costs.

## Final validation state

- `make -C dev pptoken posttoken macro` passed after the repair.
- Five focused checked-in PA4 tests passed: deep recursion, deferred helper
  prescan, GNU variadic comma paste, raw-string paste, and unavailable paint
  through function replacement (`5/5`).
- Handcrafted typed seam cases passed for all 13 identifier-like operators
  through `pptoken`/`posttoken`, macro transport/rescan including a pasted
  alternative word, and legacy callback preservation, plus the prior
  canonical punctuator/digraph, paste, stringization, and variadic comma
  cases; the `pptoken` and `posttoken` digraph probes also preserved their
  expected payloads.
- The direct operator map was observed as `new=KW_NEW`, `delete=KW_DELETE`,
  `and=OP_LAND`, `and_eq=OP_BANDASS`, `bitand=OP_AMP`, `bitor=OP_BOR`,
  `compl=OP_COMPL`, `not=OP_LNOT`, `not_eq=OP_NE`, `or=OP_LOR`,
  `or_eq=OP_BORASS`, `xor=OP_XOR`, and `xor_eq=OP_XORASS`.  `pptoken`
  continued to emit the legacy `preprocessing-op-or-punc` event for each.
  `CAT(a,nd)` produced a typed `and` token, and with `#define and 7` it
  rescanned to `7`; macro-name treatment therefore remains spelling-ID based
  and unchanged by the new fixed identity.
- The focused layout probe still reports `sizeof(PPToken)=16` and
  `sizeof(std::string)=32`; no generated `.check*` or similar artifacts remain.
- `git diff --check` passed.  Focused test-generated `.check*` files were
  removed.  No tests or `.ref` fixtures were modified.
- Final required gates passed after the reviewed repairs: `perl scripts/cppgm_file_audit.pl --stage pa4 --paths dev/src` checked 18 files; `make test-report-through-pa4` passed all PA1--PA4 stages (`173 / 173`).
  The final commit is the single intended audit commit described below.

## Checkpoint ledger

- 2026-08-22 — HEAD `74261f00`: PA4 implementation baseline; inherited ledger
  recorded 173/173 through PA4, 4/4 stages, and a clean file audit.  Those
  results are historical and are not being relabeled as post-repair results.
- 2026-08-22 — pre-implementation stub was 0/72 on PA4 and 0/3 on the initial
  focused sample; the first post-milestone implementation sample was 60/72.
  Historical repairs then covered object-like paste, GNU empty variadic comma
  provenance, raw-string stringization, pasted/helper rescans, and recursive
  paint boundaries, reaching the checked-in 72/72 result.  The typed transport
  milestone separately recorded 3/3 focused tests.
- 2026-08-22 — supervisor independently identified deep paint scaling on
  clean HEAD; digraph CAT/stringize and deferred-helper samples were correct.
- 2026-08-22 — uncommitted first final-audit milestone: typed punctuator
  identity, interned PP token spellings/ID macro identity, persistent paint
  trie, direct typed posttoken buffer handoff, and this consolidated audit
  record.
- 2026-08-22 — uncommitted second final-audit milestone: one carried
  `PPTokenFixedIdentity` field now preserves the producer's phase-3 identity
  for all fixed punctuators and the 13 identifier-like operator words through
  macro transport and direct posttoken mapping.  Focused proof covers the
  complete operator set, pasted/rescanned alternative spelling, legacy
-  callback behavior, prior regressions, and diff check; it proceeded to the
  final gates recorded above.
- 2026-08-22 — final audit validation: file audit passed for 18 files,
  through-PA4 passed `173 / 173`, and no tests, references, or generated
  check artifacts changed.  The reviewed eight-file scope is committed under
  the final PA4 architecture-audit title.

## Uncertainties and limitations

The current table retains both an ID lookup index and stage-owned spelling
values; allocation counts and whole-program performance were not measured.
The new public PA4 handoff is `PPTokenBuffer` rather than a bare token vector,
so any future consumer must retain the matching spelling table.  The current
audit stops at typed PA4 posttokens/CLI emission and makes no claim about later
object production.
