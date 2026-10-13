# Prüfstand der lokalen Übergabe

Stand: 13. Oktober 2026, kuratierter Übergabe-Worktree. Die Prüfungen liefen
auf dem zusammengeführten Quellenstand ohne Geräteupload. Frühere
Branchberichte sind dafür kein Ersatz.

| Prüfung | Ergebnis | Aussagegrenze |
| --- | --- | --- |
| `pio test -e host-configured-control` | 509/509 Fälle bestanden | Hostmodell, kein Funk- oder Hardwarebeleg |
| `pio test -e host-regression` | 105/105 Fälle bestanden | Hostregression, kein Originalboxlauf |
| `pio run -e az-delivery-devkit-v4 -j 2` | bestanden; RAM 55.148/532.480 Byte, Flash 1.229.077/1.310.720 Byte (93,8 %) | Kompilierung und Link, kein Upload |
| `pio run -e esp32-gatts-abi-probe -j 2` | bestanden; RAM 50.920/532.480 Byte, Flash 1.092.337/1.310.720 Byte | synthetische ABI-Kompilierung, kein Funklauf |
| Android `:core:protocol:test :app:testDebugUnitTest :app:assembleDebug` | bestanden; 45 JVM-Fälle ohne Fehler, Debug-APK gebaut | keine Bedien-, Berechtigungs- oder Funkprüfung am Handy |
| Android `:app:lintDebug` | bestanden nach normaler Abhängigkeitsauflösung | Offline-Lauf war wegen fehlender `androidx.collection:collection-jvm:1.4.0` blockiert |
| `Hardware/pruefung/extract_source_nets.py` | Ausgabe erzeugt und als JSON geparst | geometrische Quellenauswertung, keine CAD-Netzliste/ERC/DRC |
| Hardware-POC `pruefe_quellen.py` | Strukturprüfung bestanden: 19 Footprints, 116 Pads, 31 Soll-Netze, 0 Leiterbahnen | POC ist ungeroutet und kein fertiges PCB |
| `pio test -e host-peer-proof` | nicht ausführbar: gepinntes offizielles Mbed-TLS-2.28.4-Archiv und Quellpfad nicht bereitgestellt | kein neuer echter Kryptografie-Hostnachweis |
| Quellarchiv-Abgleich | 40 ursprüngliche Worktrees, 41 lokale Quell-Branchspitzen und 387 offene Datei-/Patch-Snapshots geprüft; alle Branchspitzen erreichbar, alle 387 Git-Blobs und ihre noch vorhandenen Originalquellen mit SHA-256 bytegleich | keine fachliche Freigabe der archivierten Entwürfe |

Es wurden weder eine physische Baugruppe beobachtet noch eine Firmware auf die Originalbox
geschrieben oder restauriert. Der Hardwarevertrag bleibt vorläufig. Der
alte Diagnose-App-Pfad hat keine neue Nutzbarkeitsabnahme erhalten.

Offen für eine spätere Umgebung: den offiziellen Mbed-TLS-Quellstand samt
gepinntem Archiv nach `scripts/native_peer_mbedtls.py` bereitstellen und
`pio test -e host-peer-proof` wiederholen; Android-Bedienung und
Originalboxfunktionen nutzergeführt prüfen. Der vorhandene Flash-Rückweg
ist vor jedem künftigen Upload chipbezogen abzugleichen.

Die Archivprüfung verglich zusätzlich den Basis-Commit als Vorfahren aller
sieben Übergabe-Branches und den kuratierten Stand als Vorfahren der sechs
Archiv-Branches. Die ursprünglichen Worktree-HEADs und offenen Dateilisten
waren unverändert. Die Snapshots summieren sich auf 4.620.797 Byte.
Ein gezielter Scan der Übergabe-Branches fand keine privaten Schlüsselblöcke,
GitHub-`ghp_`-Tokens, AWS-`AKIA`-Kennungen oder üblichen lokalen
Zugangsdaten-Dateinamen; das ist keine vollständige Sicherheitsprüfung.
