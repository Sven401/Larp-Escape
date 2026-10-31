# D0 PSA lifecycle fault audit

Independent security audit, 2026-10-31. This target compiles the actual
`lib/PsaD0FactCrypto/PsaD0FactCrypto.cpp` against a deliberately noncryptographic
PSA failure seam. It does not replace a qualified PSA SDK, validate signatures,
or prove authorization. The separate real CNG/PSA COSE vectors supply that
cryptographic evidence.

The audit checks initialization/import/verification failures, attribute and byte
length forwarding, caller-buffer freezing before backend calls, checked cleanup,
retention of a key after uncertain destruction, blocking reimport until cleanup
recovers, destructor retry, and noncopyable ownership. A shim success result only
exercises ownership/control flow; the shim performs no elliptic curve operations.

From repository root:

```powershell
python test/fixtures/d0-crypto-faults/run.py --compiler C:/Users/seide/.platformio/packages/toolchain-gccmingw32/bin/g++.exe
```

Outputs are regenerable under ignored `private/d0-crypto-faults` by default.
