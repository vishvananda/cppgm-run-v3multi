# PA16 typed `nullptr_t` carrier checkpoint

## Stage Design

The production path remains one typed flow:

```text
PA11 FundamentalType::NullptrT
  -> PA12 NullIntegerToNullptr conversion fact
  -> PA15 ABI type `ABI_BUILTIN_NULLPTR` / LowIR `i64` carrier
  -> typed LowIR and the existing backend adapters
```

`NullptrT` remains the semantic type and overload-resolution identity.  At the
Linux x86_64 PA15 boundary its existing physical carrier is `i64`; ABI
encoding of the semantic type is the existing `Dn` terminal.  No pointer model,
spelling recovery, test-name check, duplicate path, or fallback is introduced.
The two PA15 mappings are constant-time switch lookups with O(1) temporary
storage per type use; they do not scan, cache, retry, or alter PA12 facts.

## Failure Map

Turn-start authority is `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`224/243` passed, exactly 19 failures, and `243/243` discovered/reference/fresh
identities were covered.  The complete authority map is:

```text
pa16/tests/general/200-elaborated-member-forward-type.t
pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t
pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t
pa16/tests/general/200-local-default-class-array-lifecycle.t
pa16/tests/general/200-nested-braced-member-aggregate-init.t
pa16/tests/general/200-reference-indexed-pointer-member-access.t
pa16/tests/general/200-reference-member-class-init.t
pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t
pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t
pa16/tests/general/300-callable-field-hides-private-base-method.t
pa16/tests/general/300-friend-function-definition-skip.t
pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t
pa16/tests/general/300-operator-nullptr-t-from-zero.t
pa16/tests/general/300-overloaded-deref-user-assignment.t
pa16/tests/general/300-user-defined-string-literal-operator.t
pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t
pa16/tests/general/400-bit-field-prefix-postfix-increment.t
pa16/tests/general/400-signed-bit-field-read.t
pa16/tests/general/400-signed-enum-bit-field-read.t
```

The active owner is only `300-operator-nullptr-t-from-zero.t`; the other 18
authority failures are outside this checkpoint.  The two signed bit-field refs
remain a residual oracle conflict: the README requires represented negative
values while their checked-in refs only mask.  This checkpoint changes neither
that behavior nor those refs.

## Active Checkpoint

`dev/src/pa15_lowering.cpp` maps `FundamentalType::NullptrT` to
`abi_mangle::ABI_BUILTIN_NULLPTR` in `abi_type_nested`, so the existing ABI
encoder emits `Dn` and the target symbol is `_ZneRK3PtrDn`.  The same semantic
fundamental maps to `LowType::TYPE_INTEGER`/`INTEGER_I64` in `low_type`, so a
pass-by-value nullptr parameter and its slot/call operand remain typed as the
existing physical i64 carrier.  `NullIntegerToNullptr` stays owned by PA12;
there is no new conversion or overload rule.  The rejected bit-field source
candidate was reverted completely in all four affected PA15 files.

Focused final evidence:

```text
make -C dev cppgm++ CXX=g++
  status 0
make -C pa16 check TEST='tests/general/300-operator-nullptr-t-from-zero.t'
  PASS (1/1)
make -C pa12 check TEST='tests/spec/300-nullptr-t-from-zero-overload.t tests/general/300-nullptr-equality.t tests/spec/300-nullptr-pointer-conversion.t tests/general/100-nullptr-static-cast-pointer.t'
  PASS (4/4)
make -C pa13 check TEST='tests/spec/100-nullptr-return-lowir.t'
  PASS (1/1)
make -C pa15 check TEST='tests/general/200-global-pointer-array-nullptr-init.t'
  PASS (1/1)
git diff --check
  status 0
```

Generated target LowIR contains `%__param1 : i64`, `store i64 %__param1`,
`object=_ZneRK3PtrDn`, and a pass-by-value call with the zero carrier.  The
PA16 LowIR validator accepts it.  The optional PA13 `lowir2cy86` run rejects
both this target and its checked-in ref with the same pre-existing `return
value type mismatch` caused by PA16's `u8` semantic return carrying an i64
comparison result; it reports no nullptr carrier or ABI mismatch.  PA16
documents this adapter as optional and targets the later PA29 backend.

Broad final evidence:

```text
make test-pa16
  status 2; TEST SUMMARY: 225 / 243 TESTS PASSED
failure/coverage comparison against last-test.log
  failures 19 -> 18; retained 18; authority-only 1
  authority-only: pa16/tests/general/300-operator-nullptr-t-from-zero.t
  fresh-only 0; discovered/reference/fresh 243/243/243
  all missing/unexpected identity counts 0
n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
  status 0; ALL TESTS PASSED SUCCESSFULLY! (1167 / 1167)
perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src
  status 0; five pre-existing header/body warnings only
git diff --check
  status 0
```

## Performance Evidence

Structural O0 counters from generated LowIR are counts, not timing claims:

| input | functions | instructions | loads | stores | calls | comparisons | i64 lines |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| nullptr operator target | 2 | 20 | 3 | 5 | 1 | 1 | 3 |
| PA12 nullptr equality control | 3 | 37 | 6 | 8 | 2 | 4 | 15 |
| PA15 pointer-null control | 1 | 28 | 5 | 4 | 0 | 3 | 5 |

The target has one i64 pass-by-value parameter and one exact `Dn` symbol.  The
changed boundary adds two constant-time type mappings and no per-expression
scan, allocation, cache, or whole-program traversal.  Raw LowIR, structural
counters, validator results, and command logs are under
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-nullptr-carrier-checkpoint-20260830/`.

## Checkpoint Ledger

| checkpoint | result |
| --- | --- |
| `d54e32d1` turn-start authority | `224/243`, exactly 19 failures, `243/243` identities; clean baseline |
| rejected packed-bit-field candidate | reverted completely in all four PA15 source files; no rejected source diff remains |
| typed nullptr carrier replacement | committed replacement above `d54e32d1`; focused controls, PA16 `1/1`, broad `225/243`, exact `19 -> 18` comparison, prior-through `1167/1167`, audit, diff check, and clean-tree verification all pass |
