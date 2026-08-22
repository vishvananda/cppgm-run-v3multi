# Stage Design

- Baseline: PA4 was 0/72; the pre-change focused sample
  (`100-empty`, `100-nodefs`, `200-onedef`) was 0/3 with
  `EXIT_NOT_IMPLEMENTED`.
- Data flow: PA1 phase 1--3 tokenizer -> owning typed `PPToken` capture ->
  line-start directive parser and one PA4 macro owner -> painted/rescanned
  typed pp-tokens -> existing PA2 typed posttoken consumer.  Paste and
  stringization are the only text/classification boundaries; final output is
  never rendered and reparsed.
- Owners: `pp_tokenizer` owns phase 1--3 facts; `dev/src/macro.cpp` owns
  directives, definitions, arguments, substitution, paste, rescan, and
  token-local unavailable state; `posttoken.cpp` owns phase 5--7 decoding;
  `dev/macro.cpp` is only the CLI/output adapter.
- Spec alignment: line-start `#define`/`#undef`, object/function distinction,
  raw/expanded/stringized parameters, placemarkers, checked-in GNU comma paste,
  retokenized pasted identifiers, and the handout's parameter-substitution
  nesting break are implemented without a global recursion cutoff.

# Failure Map

- Directive grammar/failures and redefinition equivalence: `macro.cpp`
  (`parse_*`, `validate_replacement`, `same_replacement`).
- Expansion and recursion: `macro.cpp` (`expand_tokens`, raw argument
  collection, `PaintedToken` paint/deferred/blocked state).
- Paste/stringization/variadics: `macro.cpp` (`apply_pastes`, placemarker
  provenance, `stringize`, `variadic_argument`).
- Typed seam and final classification: `IPPTokenStream.h`, `posttoken.h/cpp`;
  CLI formatting remains in `dev/macro.cpp`.
- The first full post-milestone sample was 60/72; the resolved clusters were
  object-like `##`, GNU empty `, ##__VA_ARGS__`, raw-string stringization,
  pasted/helper rescans, and recursive paint boundaries.

# Active Checkpoint

- Final implementation is coherent and committed as one typed PA4 engine,
  direct typed posttoken handoff, hashed parameter lookup, linear paste
  reduction, cached ordinary-argument prescans, and token-local paint.
- `make test-pa4` passes 72/72.  Focused milestone validation was 3/3; no
  tests or references were changed.  The required through-PA3, audit,
  through-PA4, and diff checks are now recorded below.
- Hot tokens intentionally own exact `std::string` spellings at this
  short-lived boundary.  This preserves arbitrary source spelling at true
  paste/stringize boundaries and keeps the seam typed; interning all spellings
  is deferred because it would add global lifetime/table complexity without a
  measured checkpoint benefit.

# Performance Evidence

- `apply_pastes` reduces a replacement unit vector in one forward pass; the
  rescan uses a deque, macro and parameter lookup are average O(1), and paint
  membership is binary search over interned sorted sets.  Paint interning and
  token spelling copies are O(k log k) / O(spelling length) for active paint
  depth `k`, not whole-stream rescans; replacement copying and explicit paste
  retokenization remain semantic costs.
- Checked-in `pa4/tests/150-max.t` (94 bytes), five runs of
  `/usr/bin/time dev/macro`: all exited 0, rounded real time 0.00s, max RSS
  3492--3552 KiB.
- Repeatable synthetic `ID`/`CAT`/`CAT_I` paste family produced `n+1` output
  lines.  Five-run ranges were: n=100, 1,760-byte input, 0.00s and
  3,756--3,792 KiB RSS; n=1,000, 17,960 bytes, 0.02s and 5,180--5,444 KiB;
  n=10,000, 188,960 bytes, 0.17--0.19s and 19,964--20,460 KiB.  The small
  case is startup-dominated; the 1,000-to-10,000 timing and linear reducer
  structure are evidence, not an asymptotic claim.

# Checkpoint Ledger

- 2026-08-22: stub baseline 0/72; focused baseline 0/3.
- 2026-08-22: typed transport, PA4 engine/CLI wiring, and direct posttoken
  vector entry; focused milestone 3/3 and supervisor review accepted.
- 2026-08-22: fixed object-like paste, GNU variadic comma provenance,
  stringized raw spelling, recursive/helper paint boundaries, linear paste
  reduction, parameter lookup/prescan caching, and checked-in full suite:
  72/72.
- 2026-08-22 final gates: `n=4` through-PA3 101/101; file audit passed,
  18 files; through-PA4 173/173; `git diff --check` passed; generated
  `*.check*` artifacts removed.
- 2026-08-22: committed as `pa4: implement macro preprocessing stage`;
  final object hash and clean-tree check are in the handoff.
