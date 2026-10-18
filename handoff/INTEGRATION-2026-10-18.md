# Integrationsvorbereitung der offenen PRs · 18. Oktober 2026

Status: **zusammengeführt auf eigenem Vorbereitungsbranch; Zielintegration offen**.
Auftrag: aktuelle Ergebnisse von PR #66 prüfen und offene PRs zur Integration
vorbereiten. Sitzung `01a14e95-521b-7921-adb6-84bee3c776cf`.
Zielbasis nach Fetch: `fcb91216b335db77d07da702056d8b9b33969b9d`,
Branch `plan/projektuebergabe-kuratierter-stand`. Arbeitsbranch:
`codex/integration-offene-prs-2026-10-18`. Kein Zielbranch-/Main-Merge,
Gerätezugriff, Upload oder neue physische Abnahme in dieser Sitzung.

## Eingänge und Erhaltung

| PR | festgehaltener Head | Übernahme |
| --- | --- | --- |
| [#58](https://github.com/Sven401/Larp-Escape/pull/58) | `2f5fba6c574716b0764f680a0e4c06839dc973e3` | Zugangskonzept und Verweise; Soll, keine Implementierung |
| [#64](https://github.com/Sven401/Larp-Escape/pull/64) | `0b58f359d6137bc5e33a99d1a5a30a35ac490422` | VA-01 und Vertrags-/Backlogmatrix; D0–D4 offen |
| [#65](https://github.com/Sven401/Larp-Escape/pull/65) | `49cb1d01abce027222717857f423c55daa366fc6` | Vollständig als Vorfahr von #66 erhalten; nicht doppelt einspielen |
| [#66](https://github.com/Sven401/Larp-Escape/pull/66) | `1dbe2d1ccd4eef73cbb21a1df31ae79da53d4177` | ESP-NOW/RSSI, App-Journal und abgeschlossener Gehversuch |
| [#68](https://github.com/Sven401/Larp-Escape/pull/68) | `1066b1878acaa01c3be3c0ced84c864dfce15b72` | Isolierter Kameramessweg, Quellen und Laborbelege |

Die Heads sind durch echte Merge-Eltern im Vorbereitungsbranch erreichbar.
Originalbranches und PRs werden nicht umgeschrieben oder geschlossen.
Bei späteren neuen Heads oder geänderter Zielbasis Abgleich und Prüfung erneuern.

## Konfliktentscheidungen

1. #58 gegen Zielbasis: `handoff/MAINTAINER.md`. Den aktuellen Einstieg vom
   18.10. samt Hygiene, Hardwareplanung und Grenzen erhalten; ausschließlich
   den fachlichen Zugangskonzeptverweis aus dem älteren Einstieg ergänzen.
2. #66 nach #58/#64: Android-Einstieg und Wire-v2-Dokument. VA-01-Soll und
   tatsächlich implementierte ESP-NOW-Funktion nebeneinander erhalten. Kein
   Rücksetzen der Veranstaltungsarchitektur und keine Änderung von Wirebytes.
3. #68 nach #66: `.github/workflows/led-product.yml`. Alle vier Produktvarianten
   einschließlich `larp-espnow-test` behalten und zusätzlich
   `larp-led-camera-lab` bauen. Separaten Kameratestjob aus #68 erhalten.
   Laborbuild bleibt isoliert; bestehender Frameworkpin unverändert.

Keine manuelle Konfliktlösung an Produktquellen nötig. #68 ergänzt einen
Laborzugriff auf den RMT-Bus ausschließlich unter `LARP_LED_CAMERA_LAB`.
Die automatisch kombinierte PlatformIO-Datei enthält beide neuen Varianten.

## Aktuelle Ergebnisse von #66

**Belegt:** alle vier PR-Workflows am Head `1dbe2d1` erfolgreich:
[Quality](https://github.com/Sven401/Larp-Escape/actions/runs/39128881302),
[Baugruppen](https://github.com/Sven401/Larp-Escape/actions/runs/39128881349),
[NFC](https://github.com/Sven401/Larp-Escape/actions/runs/39128881306),
[ESP-NOW](https://github.com/Sven401/Larp-Escape/actions/runs/39128881370).
Der Quality-Checkoutlog belegt den tatsächlichen Prüfmerge
`49bc70e377cf233463b3414e17a2fe32155083b3` von `1dbe2d1` in `fcb9121`.
Das API-Metadatenfeld `base_sha` nennt noch `d61f77f`; für die geprüfte
Zusammenführung ist hier der ausgelesene Checkoutlog maßgeblich.

**In dieser Sitzung erneut geprüft:** erhaltene private Gehversuchs-CSV gegen
den SHA-256 im [kanonischen Messbericht](../docs/espnow/distance-camera-result-2026-10-18.md)
abgeglichen; vollständige Zeilen, 92 eindeutige Anfragen und 92 eindeutige
Ergebnisse, gleiche Kennungsmengen, 88 Erfolge, drei Sendefehler und ein Timeout,
96,832 s bis Stop. Start, LED-Start, Stopanforderung und LED-Aus vorhanden.
Veröffentlichte Grafik ist bytegleich mit dem erhaltenen Analyseexport.
Kamerazahlen gegen den erhaltenen Analysebericht abgeglichen, keine erneute
Pixelanalyse und kein neuer Versuch in dieser Sitzung.

Die frühere Aussage „Gehversuch/Kameravergleich noch offen“ ist durch den
Messbericht ersetzt. Keine gemessenen Meterabstände, Luxkalibrierung,
allgemeine Reichweiten-/Dauerlastabnahme oder Gesamtspielfreigabe.
Der 25-Punkte-Lauf bleibt ein separater technischer Journalvorversuch.

## Weitere Ausgangsprüfungen und gemeinsamer Prüfstand

Alle ausgelesenen PR-Workflows von #58, #64, #65 und #68 sind erfolgreich.
Für #68: [Quality](https://github.com/Sven401/Larp-Escape/actions/runs/39127093430),
[Baugruppen/Kamera](https://github.com/Sven401/Larp-Escape/actions/runs/39127093401),
[NFC](https://github.com/Sven401/Larp-Escape/actions/runs/39127093403).
Am Ablesestand keine offenen Inline-Reviewthreads in den fünf PRs.
Das ist keine menschliche Reviewfreigabe und ersetzt nicht gemeinsame CI.

Auf gemeinsamem Quellstand `7cc7b55497ea38384fb892fdfa757c4e2cfc3101`
lokal erfolgreich: sechs native Kameraschleifenfälle, 17 LED-Enginefälle,
vier SDK-Fälle, Wake/Matrix/I2C sowie acht Evidence- und zwölf Quellenhelfertests.
Erster Compileraufruf wurde von der Windows-Sandbox blockiert; Wiederholung
außerhalb der Sandbox erfolgreich. Optische Pythonfälle sowie vollständige
Builds/Androidprüfungen werden durch die gemeinsame PR-CI nachgewiesen;
die Standard-Pythonumgebung enthält lokal kein OpenCV.

Die vollständige CI des veröffentlichten Vorbereitungs-PRs muss den neuen Head
und tatsächlichen Prüfmerge prüfen. Einzel-PR-Erfolge werden nicht darauf übertragen.
Prüfresultate und konkrete Lauf-URLs stehen im Vorbereitungs-PR.

## Integrationsentscheidung und verbleibende Arbeit

Empfohlen: den gemeinsamen Vorbereitungs-PR nach erfolgreicher gemeinsamer CI
prüfen und ausdrücklich beauftragt in den kuratierten Branch übernehmen.
Danach Original-PRs mit Übernahmebeleg abgleichen; #65 ist bereits in #66
enthalten. Nicht zusätzlich die alten PRs als unbesehene Mergekette einspielen.
Merges und Schließungen sind in dieser Vorbereitung nicht ausgeführt.

Hardware #39/#54, allgemeiner Zugang #27/#28, Geräteausführung #31 F5a,
Profile #30, F5b und D0–D4 bleiben offen. Der zuletzt dokumentierte installierte
Stand ist App `6e6f093` / RSSI-Firmware `e012314`; vor neuem Geräteauftrag
tatsächliches Paar und chipgebundenen Rückweg erneut feststellen.
Arduino-ESP32-3.x-Migration bleibt [separate Folgearbeit](ARDUINO-ESP32-3-MIGRATION.md).
