# D0 Fact writer: actual signed roundtrips

Primary implementation and this initial driver: root, 2026-10-31. Independent
quality review is assigned to `quality_p1`, targeted security review to
`module_digest_review`. This initial driver is implementation validation, not
its author's independent approval.

The runner uses the 22 positive original payloads already preserved by the
independent D0 decoder audit. It creates actual new signatures using Windows CNG
or, with `D0_FACT_REVIEW_PSA`, the actual PsaD0FactSigner and its public PSA key.
Python independently checks every payload byte, SHA-256, original public-key
copy, 77-byte COSE_Key kid, protected header, AAD and actual ECDSA signature.
The public synthetic private scalar 1 is test data, never a deployment key.

Executed locally: 22 actual signed payload roundtrips and 14 rejection/reuse
pairs, zero mismatches. Source Fact types1/2/3, full uint64 widths, eight unsorted
dependencies and eight adjacent/nonoverlapping ranges come from the preserved
independent inputs. Rejections cover unavailable signing, wrong signature length,
actual damaged signature and malformed typed Facts; each must clear all published
fields, original key copy and staging bytes. Genuine production-PSA positives
remain a separate mandatory native-CI gate until that target actually runs.

The CNG/PSA result is `SignedUnpublished`, not an authority, journal commit,
loopback admission or transport grant. The actual writer rechecks the generated
COSE through the decoder and verifier against the initially exported public key.
The provided PSA key is borrowed; the production adapter never imports, creates,
persists, privately exports or destroys it. The synthetic key import/destruction
in this driver belongs exclusively to the test harness.

## Runtime and reproduction

Python must have the real `cryptography` verifier. No missing dependency is
silently skipped. Native CI already installs `cryptography==45.0.7` in
`.github/workflows/native-idf.yml` before CMake/CTest; no workflow/pin change is
needed. Local CMake must select the existing bundled Python runtime (50.0.2 in
the checked environment), instead of a system Python lacking this dependency:

```powershell
cmake -S . -B private/p1a3-host -DPython3_EXECUTABLE=C:/Users/seide/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe
```

Or run the strict standalone Windows build:

```powershell
& C:/Users/seide/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe test/fixtures/d0-writer-review/run.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
```

`run.py --binary <executable>` uses an already built CNG or actual PSA target.
Windows needs BCrypt; native PSA needs the real PsaPackageDigest,
PsaD0FactCrypto and PsaD0FactSigner adapters, the public PSA headers and tfpsacrypto.
The separate `../d0-signer-faults` audit uses a deliberately noncryptographic
API seam and must not be counted as a signature-validity proof.

The borrowed-key API follows the public PSA operations
[sign_hash](https://arm-software.github.io/psa-api/crypto/1.2/api/ops/sign.html)
and [key metadata/public export](https://arm-software.github.io/psa-api/crypto/1.2/api/keys/management.html).
It always requests the D0 ES256 algorithm; a compatible broader private-key
usage policy does not negotiate a different wire algorithm.
