# DG-Step v1: Quellen und Byteherkunft

Stand 2026-10-10. Androidbasis `7e5acb56ebf49b2c3a83a631063fa302b1aa8da0`.
Eigener Branch `plan/nutzergefuehrte-hardwarediagnose-android-step`.
Wireursprung `04c145f6775b98761a8b7b018020566f9f3fa70b` auf
`plan/nutzergefuehrte-hardwarediagnose-schrittransport`; korrigierte Semantik
`b5a8c430fcf5cf37731c18ba3ac261fce3cc13f7` auf
`plan/nutzergefuehrte-hardwarediagnose-wirezeitgrenzen`.
Keine fremde Firmwareabstammung wurde in den Androidbranch übernommen.

SHA256 der ursprünglichen Gitblobs und der maßgeblichen Nachfolgebasis:

| Quelle | `04c145f` | `b5a8c430` |
| --- | --- | --- |
| `docs/companion/diagnostic-step-transport-host.md` | `2019c1edc26b38734ed96a2115465a284f4aecfe46eca8e7c8fccbe7883095e4` | `b99ecd25fdecd2f73927203f18acc160e51c9cc22b36373ca0a572661d129f03` |
| `include/CompanionIntegration/DiagnosticStepTransport.h` | `ca7ee7021886c64ffedacdf7641eca7de1318cf82163f74cab4ba7436291724a` | `ca7ee7021886c64ffedacdf7641eca7de1318cf82163f74cab4ba7436291724a` |
| `src/CompanionIntegration/DiagnosticStepTransport.cpp` | `7b556534806edd68a9af463452dfc5dbe0df7923bd552a10d7b2afc7b7f525bb` | `6d0b86836cab3659037d60207fa8e1242b280f05acf10cfd3a95d9b285f85548` |
| `test/test_diagnostic_step_transport/test_main.cpp` | `03cf7d8f314ec2ae5b033a94d13a2d255543c95ae7a64d461479b3ecedc253bb` | `f76a1963e104ee746ca00cee5584af1cd05e3b4724d28ae871898d829cb241e4` |
| `scripts/check_diagnostic_step_transport.py` | `6a4ae61e5bdb45de34ddca6f2b82efdbf4558736fad750cb62c6db190d9da362` | `886b8a813c6422d24e4336ac04ac255610bf5d90bfa0a9e5495d148b6d508bc6` |

Der unabhängige Python-Generator erzeugt Anfrage, vollständige Antwort und
13-/20-/127-/128-Byte-Segmente mit `struct` und UUIDs. Die gültigen 744-Wake-/
104-LED-Bytes sind vollständige tatsächliche C++-Goldenbytes. Die beiden früheren
Zeitfehler werden unverändert als negative Daten aufbewahrt.

Testressource `companion/android/core/protocol/src/test/resources/diagnostic-step-v1.json`:
SHA256 `22eaded864c8034ea89c6857ab1210d06f782e7ebc1393053a5831ae3ab975ab`.
LF ist durch `.gitattributes` festgelegt. Regeneration/Bytevergleich:
`python companion/contract/diagnostic-step-v1/generate_vectors.py --check`.
Rein lesende C++-Gegenprüfungen am 2026-10-10: 756 Assertions auf `04c145f`,
955 auf `b5a8c430`, gültige vollständige Goldenbytes unverändert; zusätzlich
zehn unabhängige korrigierte Zeitgrenzen durch C++-Decoder und Reassembly geprüft.
