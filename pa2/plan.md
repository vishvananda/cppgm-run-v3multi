## PA2 Stage Design and Audit

The committed implementation baseline for this audit is `9bd8e3f4`
(`PA2: implement typed posttoken stage`), after the finalized PA1 checkpoint
`7062fca1`.  This record covers the final audit checkpoint; its new commit
hash is reported by the external handoff because this file is part of that
commit and cannot name its own hash.

`posttokenize_cpp_source` owns the PA1 callback adapter and the PA2 stream
state.  It consumes each preprocessing-token once, classifies it into the
typed enums and records in `dev/src/posttoken.h`, and sends those facts through
`IPostTokenOutput`.  `dev/posttoken.cpp` is only the requested UTF-8 dump
adapter.  No rendered output is reparsed and no reference, previous solution,
host compiler, or external process is used by the implementation.

The source-to-output ownership is:

- `and_eq`: PA1's token-kind callback and source presentation ->
  `PostTokenStream::emit_identifier` -> the immutable source-sorted fixed
  vocabulary -> `SimpleTokenType::OP_BANDASS` -> `emit_simple` -> the dump
  adapter's name table.
- `0xffffffffffffffff`: pp-number callback -> `NumericResult` and
  `IntegerSyntax` (base, value, overflow, suffix) -> ABI-ordered candidate
  selection -> `LiteralData(FundamentalType::UnsignedLongInt, 8-byte
  little-endian value)` -> `emit_literal` -> cold textual rendering.
- `"a" u"π"`: one callback per string preprocessing-token -> typed
  `StringPart` elements -> one pending maximal sequence -> final encoding and
  suffix conflict checks -> direct code-unit append into one `LiteralData` ->
  typed emission -> dump rendering of the required joined source.
- Character and UDL facts retain their parsed code point/type, UDL kind,
  suffix, and (for integer/floating UDLs) the required textual prefix through
  `IPostTokenOutput`.  The `operator""sv` boundary is handled as the required
  empty string literal followed by identifier `sv`.

Text remains only at true PA2 boundaries: input, PA1 token presentation,
required requested-dump source strings, UDL integer/floating prefixes, and the
assignment-required `std::istringstream` floating compatibility decoders.
Those decoders are a PA2 bit-pattern boundary in `floating_value`; they are
not a generic downstream textual representation or a compiler pipeline path.
The current CLI requests source presentation, so the boundary carries it;
object-only consumers must not create an unobserved presentation sidecar.

## Correctness and Conformance Review

- Integer suffix matching is longest-first and case-explicit; decimal and
  octal/hex candidate order follows the PA2 rules, with 32/64-bit ABI limits
  and overflow rejected before emission.  Candidate selection now uses a
  six-slot stack array, matching the maximum grammar candidate count.
- PA1's decoded UCN source is consumed as UTF-8 code points.  Simple, octal,
  and hexadecimal escapes are decoded; numeric string escapes remain numeric
  code units and are range-checked after the maximal sequence chooses its
  encoding.  Character Unicode/range and `char16_t` single-unit rules remain
  typed and checked.
- Maximal strings resolve ordinary/u8/u/U/L conflicts and repeated/different
  UDL suffixes before one direct encoding pass.  Raw delimiters are already
  validated by PA1.  The `operator""` reserved suffix boundary is covered by
  the checked-in reserved-suffix test and a focused manual trace.
- Phase 1--3 failures still escape as `EXIT_FAILURE`; conversion failures
  emit `invalid` and continue.  Focused confirmation: `''` emits `invalid ''`
  and exits 0, while an unterminated quote exits 1 with a phase error.
- Fundamental/simple enum values and name tables remain aligned; the complete
  simple vocabulary is retained as 137 typed entries (122 enum/name values,
  with aliases sharing enum values).  Exact dump behavior is covered by the
  PA1/PA2 fixture comparisons.

No correctness, ownership, self-containment, or boundedness blocker was found
in this audit.  The required final gates and clean-tree protocol are recorded
below.

## Audit Repairs in This Checkpoint

- Replaced the function-local `std::unordered_map<std::string, ...>` with a
  deterministic binary search over the canonical source-sorted 137-entry
  table.  This removes node-owned strings, buckets, one-time construction, and
  hash iteration from a fixed vocabulary while preserving enum ownership and
  alias mappings.
- Replaced per-integer `std::vector` candidate construction with a bounded
  six-slot stack array; no semantic selection order changed.
- Moved string byte sizing after final encoding selection and made it account
  for exact UTF-8/UTF-16 code-unit expansion, numeric code-unit width, and the
  terminator.  This closes the late-wide-prefix under-reserve and avoids a
  hidden vector-growth allocation while keeping the two linear element passes
  bounded.
- Centralized UTF-8/UTF-16 code-unit thresholds in one fixed-size
  `encode_codepoint` helper used by both exact sizing and emission, preserving
  the output while removing a second encoding-decision owner.

## Performance and Allocation Evidence

Measurements used the immutable rebuilt executable
`dev/posttoken` (SHA-256
`3dd46060492b7ee2ffba792c45c651574e633a02a433250b457b03ee61266e6e`) on this
Linux x86-64 host.  Runs used the same executable and inputs, seven repeated
runs, and median `/usr/bin/time` wall/user/system time plus peak RSS.  These
are bounded/scaling measurements, not comparative claims against a single
loaded-host sample.

The checked-in hard fixture is 429,984 input bytes and 14,400 lines, with
1,324,500 output bytes and 40,609 output lines.  Post-repair medians for the
prefix series were:

| input bytes / lines | median wall | median user | median RSS |
| --- | ---: | ---: | ---: |
| 24,699 / 900 | 0.00 s | 0.00 s | 4,012 KB |
| 51,798 / 1,800 | 0.01 s | 0.00 s | 4,780 KB |
| 105,396 / 3,600 | 0.01 s | 0.01 s | 6,588 KB |
| 214,992 / 7,200 | 0.03 s | 0.03 s | 9,916 KB |
| 429,984 / 14,400 | 0.07 s | 0.05 s | 16,060 KB |

The wide-prefix-late series contains ordinary `"a"` parts followed by one
`u"π"` part, so final encoding is known only after the early parts.  Its
input/output sizes and medians were:

| ordinary parts | input bytes | output bytes | median wall | median RSS |
| ---: | ---: | ---: | ---: | ---: |
| 1,024 | 4,103 | 8,264 | 0.00 s | 4,056 KB |
| 4,096 | 16,391 | 32,840 | 0.00 s | 5,052 KB |
| 16,384 | 65,543 | 131,145 | 0.02 s | 10,096 KB |
| 65,536 | 262,151 | 524,361 | 0.07 s | 30,632 KB |

For 65,536 parts the final literal payload is 131,076 bytes.  The old
single-pass estimate was about 65,542 bytes because it charged early ordinary
parts at width 1; the repaired estimate is based on the resolved UTF-16
encoding and exact element expansion.  A lookup-heavy generated input of
1,500,015 bytes / 300,004 output lines had a 0.26 s wall, 0.20 s user,
0.06 s system, 82,704 KB RSS median.  It exercises the fixed vocabulary
without constructing a runtime hash index.

Massif profiling of the valid 65,536-part wide case reached 36,528,160 bytes
total heap, 35,991,765 useful heap, and 536,395 bytes allocator overhead.  The
confirmed peak-snapshot stacks above Massif's display threshold included the
`PostTokenStream::emit_string_literal` allocation path (26,738,688 bytes,
73.20%), PA1/tokenizer storage (6,291,600 bytes, 17.22%), another
PA1/tokenizer path (1,048,604 bytes, 2.87%), a second string-literal path
(1,048,304 bytes, 2.87%), and test-runner input/source buffering through
`std::stringbuf::reserve` (524,289 bytes, 1.44%).  The remaining 0.93%
(340,280 bytes) was below Massif's 1% display threshold.  No simple-token
hash-index construction appears, and the profile did not identify an
unexpected superlinear owner.  These observations, together with the scaling
series, support bounded live ranges involving source/input storage, one
pending maximal sequence, typed element buffers, and one emitted byte vector.
Source joining reserves the sum of token presentations plus separators, and
ordinary work is O(source bytes + token bytes + code units); live memory is
bounded by the input, the pending maximal sequence, and the emitted value.

## Focused Validation at Checkpoint

- `make test-pa1`: PASS, 54/54.
- `make test-pa2`: PASS, 26/26.
- `perl scripts/cppgm_file_audit.pl --stage pa2 --paths dev/src`: PASS,
  14 files checked, exit 0.
- `make test-report-through-pa2`: PASS, 80/80, exit 0.
- Manual ownership traces PASS for `and_eq`, the 64-bit integer, concatenated
  wide strings, character/string/numeric UDLs, `operator""sv`, empty-character
  conversion, and phase-error distinction.
- `git diff --check`: PASS.

The final clean-check protocol is to create a new commit without amending
`9bd8e3f4`, inspect its subject and exact changed-file list, and require
`git status --short` to produce no output.  That protocol was applied to this
checkpoint; no tests or `.ref` fixtures were edited.

## Checkpoint Ledger

- `7062fca1` — finalized PA1 checkpoint; clean-tree PA2 baseline was
  26/26 `CPPGM_EXIT_NOT_IMPLEMENTED` failures.
- `9bd8e3f4` — committed typed PA2 implementation, source-set registration,
  thin dump adapter, and narrow empty-character tokenizer boundary.  The prior
  audit log recorded 80/80 through PA2 before this review; that is historical
  evidence, not final evidence for the current diff.
- Final PA2 audit checkpoint — deterministic fixed-vocabulary lookup, bounded
  integer candidates, exact post-encoding string reserve, and one shared
  code-point encoder are recorded here.  The required file audit and 80/80
  through-stage gate passed, then this record and the implementation were
  committed as a new checkpoint without amending `9bd8e3f4`.  The commit hash
  is reported by the external handoff, and the final `git status --short`
  check produced no output.
