# Prüfbelege des A1/A2-Pilots

Datum: **24.10.2026**, Europe/Berlin. Produktcodebasis
`e61d7642d035ccdcf8eca689d6c50a82bcf1560b`, Pilotcheckout zunächst
`200eea6e0d7110b76065b86887b18bdfa1fb25ef`; neue Tests liefen vor dem
Abschlusscommit als benannte Arbeitsbaumänderungen. Die [Eingangshashes](pruefungen.json)
fixieren diese Testquellen. Dokumentations-/Teständerungen befinden sich in
[PR #95](https://github.com/Sven401/Larp-Escape/pull/95); keine Änderung des
Produktcodes oder der Framework-/Bibliothekspins in diesem Pilot.

| Ausgeführte Prüfung | Ergebnis | Aussagegrenze |
| --- | --- | --- |
| `platformio test -e host-regression -v` | **120/120**, darunter **15 neue PN532-Funktionen**; JUnit gespeichert | Parser-/Hostverhalten, kein tatsächlicher Bus-/Kartenlauf |
| `platformio test -e host-configured-control -v` | **509/509**, JUnit gespeichert | Vorhandenes deterministisches Modell, kein SMP-/Latenzbeweis; besteht trotz A1-01 |
| `python -m unittest discover -s scripts/tests -p test_patch_pn532.py -v` | **10/10**, zusätzlich unabhängig gegengeprüft | Synthetische Quellen/Pins, kein allgemeiner atomarer Schreibschutz |
| [Realer PN532-Quell-/Rückwegversuch](pn532-source-result.json) | Echte Original-/Patchhashes stimmen; Verify, Restore, Reapply bestanden; Originalcache bytegleich erhalten | Nur temporäre Kopie des vorhandenen lokalen Quellcaches; kein frischer Download oder Adapterlauf |
| [RTOS-Diagnose](rtos-probe/result.json) | Kompilierung/Ausführung Exit 0; je 1 Send/Receive/Peek unter Authority-Lock, alle Wartezeiten 0 | Erfolgreiche Reproduktion des Befunds, keine bestandene Konformitätsprüfung |
| `python firmware/led/tests/run_output_tests.py` | Exit 0; 17 LED-Engine- und 4 SDK/RMT-Fälle sowie Wake-/I²C-/Matrixfälle erfolgreich | Eigene Baugruppenadapter; entlastet nicht Legacy-Adafruit |
| SDK-Binärpins | Frisch berechnete Hashes von lokalem `libbt.a` und `libmbedcrypto.a` entsprechen den beiden Projektpins | Kein neuer Targetbuild oder vollständiger ABI-Quellaudit |

Werkzeuge: Windows, Python **3.12.6**, PlatformIO Core **6.2.0**, Unity **2.6.1**;
GCC aus dem vorhandenen PlatformIO-Hostpaket. Die CI verwendet wie bisher
Python 3.11. Der erste isolierte Compilerstart scheiterte an Windows-DLL-/
Sandbox-Temporärrechten; der genehmigte Wiederholungslauf außerhalb dieses
Sandboxkontexts bestand. Kein fehlgeschlagener Produkttest als Erfolg gezählt.

Parserwerte sind synthetische, aus dem NXP-Framevertrag abgeleitete Beispiele.
Vier feste gültige Eingänge erzeugen insgesamt 79 abgeschnittene Präfixe;
Negativfälle erhalten bei semantischen Änderungen korrekte äußere Checksummen.
Die ATS-Grenzfälle prüfen begrenzte Parserkapazität, nicht ISO-14443-4-Inhalte.
Die Akzeptanz einer 4-Byte-UID im Parser ändert den 7-Byte-Spielvertrag nicht.

Die beiden datierten Diagnosen bleiben editierbare Versuchsbelege. Sie sind
keine neuen allgemeinen Werkzeuge oder automatischen Compliance-Gates.
Der reale PN532-Versuch kann ausschließlich an einer temporären Kopie
wiederholt werden:

```powershell
python docs/quality/audits/2026-10-24-pilot/pn532_source_probe.py --repo . --library <vorhandene-gepatchte-PN532-1.3.4-Bibliothek-mit-Originalsicherung> --result <neuer-Ergebnispfad.json>
```

Interne Gegenprüfung: getrennte Fachreviews für A1 und A2 sowie
Rollen-/Dokumentationsreview. Korrigiert wurden die mehrdeutige Bezeichnung
„Pin“ im Plattformmandat und die unzutreffende Behauptung unveränderter
Automatisierungen: Der Quality-Workflow wird erweitert, der bestehende
Codex-Heartbeat bleibt unverändert. Der Patchtest wurde um Wiederanwendung
nach Restore und die fremde Quelländerung während Vorbereitung ergänzt.

**Nicht ausgeführt:** neue ESP32-/Androidbuilds, Geräteuploads, tatsächliche
Karten-/Busfehler-/SMP-/Zeitmessungen, Funk-/Exploitprüfungen, vollständige
Normkonformität und zweiter unveränderter Automationsprobelauf. Eine spätere
Remote-CI hat ihren eigenen Commit-/Mergebezug und wird im PR separat genannt.
