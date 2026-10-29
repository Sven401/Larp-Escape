# PR #101: native Build-/PSA-Basis zur Integration vorbereiten

Stand: 29.10.2026. Auftrag: #101 mergefähig vorbereiten.
Federführung Plattform/Systemintegration: Hauptagent `/root`.
Anderer Plattform-/Security-/Qualitätsgegenprüfer: `/root/pr101_review`.
Varianten: nativer CMake-Hostkern, Profilwerkzeug, echte PSA-Prüfung und
isolierte native ESP32-Prüffirmware; Legacy-/Baugruppenvarianten bleiben erhalten.

## Herkunft und Umfang

Ursprünglicher PR-Head `f26e87efc1d6ed9d1e73bceb3a8a2aab2009ca04`.
Aktuelle Integrationsbasis `4f37317dc2295b2236121f5872756c387bda025b`
enthält #100/#102/#104. Zusammenführung `a2cce5c`: einziger Konflikt im
Maintainer-Einstieg, beide Einträge erhalten. #100 ist damit Voraussetzung
im Zielbranch und kein eigener unintegrierter Bestandteil dieses Pakets mehr.

Gezielte Übernahme aus #103, Head `b1410a08138e974947c859371950e7914151ed84`:
exakter SDK-Wächter und öffentliche ECC-Parserregression unverändert;
Herstellercheckout/rekursive Submodule/Werkzeuginstallation in der CI.
Keine TLS-Worker-, NimBLE-, BLE-Image- oder Produktaktivierung aus #103.

Neuer Hersteller-Wartungspin: `9a97f6c54ec638111ce55cd36581b3c192f15207`.
Er ersetzt den früheren SDK-Pin `fff9895c82d744c7237be8847347bdd1b07c6643`;
Mbed TLS 4.1.1/TF-PSA 1.1.1 statt 4.1.0/1.1.0. Herkunft und weitergehende
Sicherheitsbewertung verbleiben im getrennten #103. Kein behaupteter 6.1.1-Tag.
[Herstellerwarnung CVE-2026-50583](https://mbed-tls.readthedocs.io/en/latest/security-advisories/mbedtls-security-advisory-2026-07-zero-length-ecc-public-key-oob-read/)
am 29.10.2026 erneut gelesen: betroffene alte Versionen und korrigierte
1.1.1-/4.1.1-Reihen bestätigt. Der neue Test platziert einen leeren öffentlichen
ECC-Schlüssel an einer Guardpage und prüft zuvor einen gültigen P256-Schlüssel.
SDK-Guards, rekursive Quellenbindung und Legacy-ABI-Pins bleiben aktiv.

## Prüftore und Grenzen

Die neue CI qualifiziert den aktualisierten SDK-Pin, SDK-Ablehnungsfälle,
fünf native CTest-Einträge einschließlich echter PSA, Journal-Reopen und
ECC-Parserregression sowie das tatsächliche ESP32-Kernimage. Alte grüne Läufe
werden nicht auf den neuen Stand übertragen. Vollständige aktuelle PR-CI
und unabhängige Gegenprüfung sind vor Ready-Status auszuwerten.

Dies ist eine native Build-/PSA-Basis, keine abgeschlossene Produktmigration.
Originalverbindung/SMP, geschützter Produkttransport, reale Geräteadapter,
Datenmigration, Ressourcen-/Lastnachweise und S1–S6 bleiben offen.
Die Prüffirmware startet kein Radio und vergibt keine Autorität.
Keine Geräteinstallation, kein Upload, kein Release und kein Zielbranch-Merge.
Historische Angaben im ursprünglichen Qualifikationsbericht gelten weiterhin
für ihre dort benannten alten Commits und Versionen.

Frisch lokal: sieben SDK-Guardfälle erfolgreich, Exit 0. Diffprüfung bestanden. Unabhängige Quellengegenprüfung des #101-Schnitts und der gezielten SDK-/ECC-Übernahme ohne P1/P2-Befund. Der andere Bearbeiter konnte seine Guardtests wegen lokaler Temp-/Git-Schreibrechte nicht ausführen; daraus wird kein bestandener Test abgeleitet. Finale native CI bleibt eigener Nachweis.

Erster neuer CI-Lauf: native Host-/ECC-Prüfung bestand; Legacy-host-regression versuchte jedoch den CMake-only ECC-Test ohne SDK-Header mitzubauen. platformio.ini ordnet test_native_tls deshalb explizit allein dem nativen CMake-Prüfweg zu; dessen verpflichtende ECC-Ausführung bleibt erhalten. Neuer CI-Lauf erforderlich.
