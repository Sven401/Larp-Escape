# Gemeinsamer NFC-Rundlauf mit einem echten Tag

**16. Oktober 2026, Europe/Berlin. Ergebnis: ein Rundlauf mit NTAG213 bestanden.**
Geprüfte Umsetzung `d4892bf67c330a7899d1249bc769187072bc23ff`,
Dokumentationsbasis `9eb88f603149e80981cd2a64524959abb8471d03`.
Keine Änderung an App oder Prüffirmware für diesen Lauf.

## Tatsächlicher Ablauf

Der Nutzer hat den physischen Tag zwischen Handy und Box bewegt und die
App bedient. Die Boxaufträge wurden einzeln über USB ausgelöst. Die
Abschlussanzeige wurde vom Nutzer bestätigt und zusätzlich direkt aus der
App samt neuem NFC-Leseereignis erfasst.

| Schritt | Beobachtung | Ergebnis |
|---|---|---|
| App liest Ausgangszustand | Leerer, beschreibbarer NDEF-Tag; 137 Byte verfügbare NDEF-Kapazität | geeignet |
| App schreibt Fixture | Um 00:17:39 Schreibaufruf abgeschlossen; unmittelbares Rücklesen protokolliert 0 Byte | zunächst **UNCERTAIN**, kein Erfolg behauptet |
| App liest nach erneutem Anhalten | Um 00:18:25 frische 59 Byte; gleiche UID, vollständige Nachricht gleich dem gespeicherten Schreibziel; Anzeige „Frühere Schreibung durch erneutes Lesen bestätigt“ | Flags 1 bestätigt |
| Box liest | NTAG213, sieben UID-Bytes, dieselbe UID wie in der App, SAK 00, ATQA 0044; beschreibbar; 144 Byte CC-Datenfläche, 59 Byte NDEF | exakte Vorher-Fixture, CRC gültig |
| Box ändert einmal | Um 00:19:36/37 `CHANGE VERIFIED`, `fresh_readback_exact` | Flags 2, nur Flags und CRC geändert |
| Boxfeld aus | `NFC END OFF OK`, anschließend Nutzerwechsel des Tags zum Handy | Feld-Aus-Befehl bestätigt |
| App liest unabhängig | Um 00:20:49 neuer RF-Lesezugriff über 59 Byte, gleiche UID; „Tagdaten gelesen · Prüfsumme bestätigt“ und vollständige Box-Sollwertbestätigung; Nutzer bestätigt Flags 2 | Rundlauf abgeschlossen |

Kennwert blieb `20261015`, Zeitwert blieb `0`, Stationsbesuch blieb
`1, 1, 0`. Die Box-NDEF-Nachrichten vor und nach der Änderung wurden zusätzlich
unabhängig mit Python `struct`/`zlib` aus den Sollfeldern nachgerechnet und
bytegenau verglichen. CRC32 vorher `577B61F4`, nachher `7EB3D506`.
Die App-Sollwertmeldung vergleicht den frisch dekodierten Datensatz mit der
vollständigen Nachher-Fixture; sie stammt nicht aus einer Formulareingabe.
Nach der Boxänderung wurde in der App kein Schreibauftrag vorbereitet.

## Besonderheit beim ersten App-Schreiben

Der App-Schreibaufruf meldete seinen Abschluss, die unmittelbar folgende
Lesung lieferte aber keine NDEF-Daten (0 protokollierte Bytes). Die App hat das
korrekt als unklar behandelt und den Schreibauftrag im Journal erhalten.
Es wurde **nicht erneut geschrieben**. Erst nach Entfernen und erneutem
Anhalten bestätigte eine neue Lesung dieselbe UID und alle erwarteten Bytes;
das Journal wurde dadurch aufgelöst.

Die Ursache dieses ersten leeren Rückleseergebnisses bleibt offen. Ein
Cache-, Tag-, Funk- oder Android-Fehler lässt sich daraus nicht bestimmen.
Der direkte Erfolgsweg ohne zusätzliches Anhalten ist für diesen ersten
Schreibversuch nicht belegt. Der vollständige Rundlauf ist mit der
dokumentierten Wiederaufnahme durch Lesen belegt.

## Installation und Rückweg

Vor dem Upload wurde die Chipkennung mit dem vorhandenen privaten Nachweis
abgeglichen, der aktuelle 4-MiB-Flash vollständig gesichert und am Chip
verifiziert. Wake-Anwendung, Partitionstabelle und Schlüsselpartition
entsprachen dem erwarteten Rückweg. Der neue Abzug war bytegleich zum
Abzug der Vorerkundung; er bleibt privat.

Die Prüffirmware wurde ausschließlich ab `0x10000` installiert; Anwendung
und Schlüsselpartition wurden danach separat am Gerät verifiziert.
Der PN532 meldete Chip 32, Firmware 1.6 und Unterstützung 07. Vor dem
Rückweg wurde `OFF OK` bestätigt; dies ist keine elektrische Feldmessung.

Anschließend wurde die Wake-Anwendung `ca86b93` wiederhergestellt.
Anwendung und unveränderte Schlüsselpartition wurden am Chip verifiziert.
Die App verbindet sich wieder ohne Schlüsselimport, meldet
„LED-Box verbunden · Zugang bestätigt“ und frische Wake-Werte. Die
vorhandene Anordnung 2/3/4/1 blieb erhalten. Der Tag bleibt auf Flags 2.

Artefakte sind identisch zur [Vorerkundung](device-preparation-2026-10-16.md):
NFC-Firmware SHA-256 `1b1d26962ef7a7d6801425a2607bf7f059e7420c6bbc8a68e48229f39b21c48c`,
Wake-Rückweg `316661b6daa41ff449ab944839dd18863f962f413ea1db0f35be2ba49c395378`.
Der Lauf hat keine neue APK installiert.

## Umfang und offene Punkte

Die Nachweise T1–T4 des [Testplans](test-preparation.md) sind für **diesen
einen NTAG213** erfüllt; T1 enthält die oben dokumentierte zusätzliche
Lesung. Wiederherstellung R1 ist ebenfalls erfüllt. T5 (zweiter kompletter
Rundlauf) wurde nicht ausgeführt. NTAG215/216, weitere Exemplare,
Langzeitspeicherung, provozierte Abbrüche und die Spielintegration bleiben
ungeprüft. Die Kapazitäten 137 Byte in Android und 144 Byte in der Box
bezeichnen unterschiedliche Speicherbereiche und sind kein Widerspruch.

Die bestehenden GitHub-Prüfungen auf `9eb88f6` waren bei Abschluss grün:
[NFC probe](https://github.com/Sven401/Larp-Escape/actions/runs/38815375466),
[LED product](https://github.com/Sven401/Larp-Escape/actions/runs/38815375470),
[Quality](https://github.com/Sven401/Larp-Escape/actions/runs/38815375461).
Für diesen reinen Gerätebericht wurden keine unveränderten lokalen
Softwaretests wiederholt. Ein unabhängiges Belegreview bestätigte UID-
Zuordnung, beide NDEF-Nachrichten und die abschließende frische App-Lesung.

Der [bereinigte Maschinenbericht](evidence/physical-tag-2026-10-16.json)
bewahrt die USB-Antworten, relevante App-Ereignisse und Prüfresultate.
Private Rohbelege liegen im Chat-Arbeitsordner unter
`work/evidence/physical-tag-01/`; der vollständige Flashabzug unter
`work/private/physical-tag-01/`. Gerätekennungen, Schlüssel und vollständige
Flashabzüge werden nicht in das Repository übernommen.
