# PR #103: Mergevorbereitung der TLS-/BLE-Quellbausteine

Stand: 30.10.2026. Nutzerauftrag: nach #101 den PR #103 vorbereiten.
Federführung Plattform/Systemintegration: Hauptagent `/root`.
Anderer Plattform-/Security-/Qualitätsgegenprüfer: `/root/pr103_review`.
Varianten: nativer CMake-Hostkern, TLS-Worker, NimBLE-Quelladapter und zwei
isolierte ESP32-Qualifikationsimages; keine Aktivierung im Produktstart.

## Herkunft und Konfliktauflösung

Alter PR-Head `b1410a08138e974947c859371950e7914151ed84`.
Integrationsbasis `eb3f4507369b12ea11cd4097195325b73b1d8f0b`, einschließlich
#100/#101/#102/#104. #101-Merge-CI frisch geprüft: Quality 3/3 und native
Qualifikation 1/1 erfolgreich. #102 ist bereits bei `25e75f0` integriert;
Nutzer präzisierte deshalb die anschließende Arbeit auf #103.

Zusammenführung `ba2945c1bd3ff1c4f027dc555eeabaae69a4b717` löst zwei
überlappende Testkonfigurationen: ein einziger native-empty-ecc-Eintrag,
zusätzlich die TLS-/BLE-Tests; beide nativen Testsuiten bleiben ausschließlich
im CMake-Prüfweg und werden vom Legacy-host-regression nicht mitgebaut.
SDK-Pin/ECC-Regression sind bereits über #101 integriert und bleiben identisch.
Direkter Dateivergleich bestätigt: lib/PinnedPeerTls, lib/NimblePeerSource und
firmware/idf-ble gegenüber dem unabhängig geprüften alten Head unverändert.

## Gegenprüfung und Umfang

Anderer Bearbeiter prüfte TLS-/NimBLE-Quellen, 29 Modellfälle und 18 echte
TLS-Gegenstellenfälle, CMake, SDK-Komponenten und beide Firmwareeinstiege.
Keine konkreten P1/P2-Codebefunde im begrenzten Vorbereitungspaket.
Keine eigenen Testläufe des Gegenprüfers. Dokumentbefunde korrigiert:
TLS-Anleitung nennt Mbed TLS 4.1.1; Bereitschaftseinstieg datiert bereits
integrierte Voraussetzungen und historische Buildprobleme getrennt.

Originalepoche/Lifetime, exakter SPKI-Pin mit übrigen X509-Flags, begrenzte
kopierte Puffer und unveränderte ausstehende TLS-Schreibwerte bleiben erhalten.
NimBLE-Hostbesitz, SC-only, Schlüsselgröße16/Bond, bewusste Numeric Comparison,
volle Mailboxen, Heartbeat-Ausfall und MTU23/20-Byte-Notify sind abgedeckt.
TLS-Erfolg und Bond erteilen keine Spielrolle. Die finalen Verbraucher müssen
ursprüngliche Verbindung, aktuelle Autorisierung und Profilfreigabe prüfen.
Die SDK-Einstiege starten weder Radio noch Enrollment oder Produktberechtigung.

S1–S6, tatsächliche SMP-/HCI-/ACL-Ordnung, BLE-/Androidkopplung, reale
Profilgeräte, Datenkompatibilität sowie Ressourcen-/Geräteabnahme bleiben offen.
Ein Merge dieser Bibliotheks-/Quelladapterbasis schließt diese Tore nicht.
Keine Geräteinstallation, kein Upload, kein Release und kein Zielbranch-Merge.
Neue vollständige CI und Review der Konfliktauflösung vor Ready-Status auswerten.

Lokal sieben SDK-Guardfälle erfolgreich, Exit 0. Gegenprüfung der Mergekonfiguration bestätigt genau einen ECC-Eintrag und beide nativen Testausnahmen. Ein beim Dokumentedit entstandener Kodierungsfehler wurde nach Gegenprüfung mit explizitem UTF-8 korrigiert; bestehende Produktquellen bleiben unverändert.
