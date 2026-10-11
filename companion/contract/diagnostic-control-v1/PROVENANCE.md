# DG-Control v1: Android-Hostcodec

Stand: 2026-10-10. Firmwarequelle `3209275136f96d3a6a0013c485e70d692e428cf4` auf
`plan/nutzergefuehrte-hardwarediagnose-transport`. Androidbasis `45d3e79`.
Eigener Branch `plan/nutzergefuehrte-hardwarediagnose-android-control`.
Kein fremder Firmwarebranch wurde in die Android-Abstammung übernommen.
Gelesene Vertragsquellen und SHA256 der originalen Git-Blobs:

| Quelle | SHA-256 |
| --- | --- |
| `docs/companion/diagnostic-transport-host.md` | `a9b51b5e8cce19b30290f9db5ee61672dbd941f5009829b032cef76e82ff8a43` |
| `include/CompanionIntegration/DiagnosticTransport.h` | `40a8ea18d45c7ccb522b9c78e75f2db2a6791dc517b9c5199acb51a92ee46d55` |
| `src/CompanionIntegration/DiagnosticTransport.cpp` | `ad538308784024dc62335a0b3338bd99a10b1581cf85c51bfcb76672d9d516cf` |
| `test/test_diagnostic_transport/test_main.cpp` | `e695bba9ca53ef7f57293387df97f997cd7b7f80ad542f6a6ea5ac2dc19f6747` |
| `scripts/check_diagnostic_transport.py` | `c62e163c4ceddb4729dece47a1c82d5d1af3af62f018ad5c0f8f5ee9cdbefa0d` |

Unabhängig erzeugte Testressource:
`companion/android/core/protocol/src/test/resources/diagnostic-control-v1.json`
SHA-256 `1caef396ccb7af90ba33191f945fff7702ed2b6c0382e63862145bbc11e45dc2`.
Regeneration und Vergleich: `python companion/contract/diagnostic-control-v1/generate_vectors.py --check`.
Die tatsächliche C++-Grantantwort wurde am 2026-10-10 auf sauberem
Quellstand `3209275` erneut durch dessen Runner bestätigt: 350 Assertions
und sämtliche 88 Grantbytes bestanden, ausschließlich synthetische Ports.
