# Hardwareabnahme der Originalbox – laufender Prüfbericht

Stand: 10. Oktober 2026. Produktzweig `codex/reichstagsbox-diagnostics` auf
`5d3779eb99516bcf6c73ae519d7da36e480636dc` zu Beginn. Die eigenständigen
Diagnosequellen sind noch Arbeitsbaumänderungen. Der Bericht wird nach den
physischen Prüfungen ergänzt; offene Zeilen sind keine bestandene Abnahme.

## Geräte und Rückwege

| Gerät | Kennung, Flash | Diagnose | Vor Upload gesicherter Rückweg |
| --- | --- | --- | --- |
| Originalbox, COM3 nach USB-Wechsel | ESP32-D0WD-V3, `a8:42:e3:cd:4b:18`, 4 MB | `reichstagsbox-acceptance`, 1.208.313 Byte Flashbelegung von 1.310.720 Byte (92,2 %) | `C:/Users/seide/AppData/Local/LarpEscape/ESP32-Backups/COM3-20261010-Reichstagsbox/original-flash.bin`, 4.194.304 Byte, SHA-256 `a502ea122c9c81e02637f593a2c8c03ddf5c2963fa5f4b878eb291e271c3f020` |
| Testempfänger, COM3 vor USB-Wechsel | ESP32-D0WD-V3, `a8:42:e3:ce:de:d8`, 4 MB | `espnow-acceptance-receiver`, 730.893 Byte Flashbelegung von 1.310.720 Byte (55,8 %) | `C:/Users/seide/AppData/Local/LarpEscape/ESP32-Backups/COM3-20261010-ReceiverCandidate/pre-receiver-flash.bin`, 4.194.304 Byte, SHA-256 `c09b2951daae7212d5a8ce037c92c4944591e2d1bf9235cc50991afb0153c7c3` |

Beide Vollsicherungen liegen außerhalb von Git und OneDrive. Größe und Hash
wurden vor dem jeweiligen Upload erneut geprüft; die jeweilige Chip-MAC und
4-MB-Flashgröße wurden am tatsächlich angeschlossenen Gerät neu gelesen.
Alle vier von PlatformIO geschriebenen Bereiche hatten bestätigte Schreib-Hashes.
Ein Restore wurde nicht ausgeführt. Ein Image des anderen ESP32 darf nie als
Rückweg verwendet werden.

## Start- und Leerlaufbefund

Die bisherige Spiel-Firmware lieferte direkt vor dem Diagnose-Upload im frischen
45-Sekunden-Lauf einen Boot und vier Laufzeitberichte: `PASS`, beide MCP23017,
PN532, `Runtime ready`, maximale Loopzeit 39.123 µs, freie LED-Taskstackreserve
mindestens 3.224 Byte, maximale LED-Renderdauer 2.790 µs, null verpasste
LED-Deadlines und null LED-/ESP-NOW-Queueverluste. Rohlog:
`C:/Users/seide/AppData/Local/LarpEscape/ESP32-Backups/COM3-20261010-Reichstagsbox/acceptance-idle-20261010.txt`.
FastLED meldete erneut den generischen Clockless-Fallback.

Die unabhängige Box-Diagnose startete danach auf derselben Box: MCP 0x20/0x21,
PN532 und ESP-NOW bereit; alle vier Wake-Eingänge im Leerlauf inaktiv. Der
Empfänger meldete am USB `RECEIVER READY`, Kanal 1, MAC
`A8:42:E3:CE:DE:D8`, gespeicherte Datensätze 0. Auf der Powerbank leuchtet
nur die feste Power-LED. Eine steuerbare Bereitschafts-LED steht am Board
nicht zur Verfügung; der Powerbank-Start ist optisch nicht separat bestätigt.

## Baugruppenstand

| Baugruppe | Bisheriger Ist-Befund | Status / noch fehlender Nachweis |
| --- | --- | --- |
| Vier Aufweckeingänge | Vier inaktive Startpegel. Nach dem Funklauf erschienen Wechsel an Eingang 1 und 3. Der Nutzer könnte beide beim Anschluss der zusätzlichen Versorgung berührt haben; Zuordnung daher unklar. | Dreimalige Einzelbetätigung, Freigabe und Kombinationen offen; keine Zuordnungsfreigabe. |
| 9×9-Matrix | Diagnose scannt 81 Koordinaten und protokolliert Down/Up. | Tatsächliche Betätigung und Abgleich mit physischer Koordinate aller erreichbaren Felder offen. |
| PN532 Lesen/Schreiben | Schreibpfad ist auf erkannte NTAG213/215/216, identische zuvor gelesene 7-Byte-UID und Nutzdatenseite 4 beschränkt. A=`4C413031`, B=`4C423032`; pro explizitem Auftrag START und VERIFIZIERT/UNKLAR, kein automatischer Schreibversuch. Beim ersten Start meldete PN532 Firmware 1.6 und war bereit; nach weiteren Versuchen schwankte der Start zwischen `firmware=32010607 sam=1 ready=1` und `firmware=00000000 sam=0 ready=0`, auch ohne Audio-Versorgung. Zwei Tags lieferten zunächst keine UID. Nach Korrektur der Lesefolge: mit Tag 2 `detection_frame_failed`, beim zweiten Versuch `detection_start_failed`. Nach vollständiger USB-Stromtrennung ohne Tag meldete PN532 erneut `ready=1`; Tag 2 danach wieder `detection_frame_failed` und `detection_start_failed`. Ein Statusbefehl zeigt nur den beim Boot gespeicherten Bereitschaftswert und ist nach einem Laufzeitfehler kein neuer Hardwaretest. | NFC-Leseantwort genauer instrumentieren und Hardware-/Versorgungslage klären. A/B-Rücklesen, Entfernen/Wiederauflegen und Neustart offen. Kein Tag wurde beschrieben. |
| 23 LEDs | Erster Lauf: 69 serielle Einzelkommandos 0–22 je RGB, maximal 2.066 µs `FastLED.show()`. Gruppen 0–7 rot, 8–15 grün, 16–22 blau und Helligkeiten 16/32/48/64/80 kommandiert. Wiederholung bei laufender Handykamera: erneut 69 Einzelkommandos sowie Gruppen und Helligkeiten, maximal 2.047 µs, 5 V/1 A Leistungslimit aktiv. | Aufnahme/Sichtbefund des Nutzers noch nicht ausgewertet; dadurch Nummern, Reihenfolge und Farben optisch offen. Versorgungsmessung offen. Die Diagnose zählt ausgeführte Frames, keine feste Frame-Deadline. |
| DFPlayer/SD/Lautsprecher | Track 100 zweimal ohne Modulversorgung angefordert; BUSY wurde beide Male für rund 504 ms aktiv. Der Nutzer stellte anschließend die fehlende Versorgung fest. Nach bestätigter Versorgung und Neustart: zweimal Track 100 angefordert, BUSY blieb inaktiv. Nutzer hörte keinen Ton; SD-Karte sitzt, DFPlayer-Power-LED leuchtet. | Kein positiver Audiotest. UART/Playerstatus, Dateibestand und Versorgung unter Last klären. |
| ESP-NOW | Empfänger USB-Start und leerer Speicherbericht bestätigt. Lauf `0000202610100001`: 10/10 korrelierte ACKs, null lokale Sendefehler. Lauf `0000202610100002` an ca. 2 m ohne Hindernisse: 10/10 korrelierte ACKs, null lokale Sendefehler. ACK wird im Empfänger erst nach bestätigter NVS-Speicherung erzeugt. | Empfänger nach Funklauf wieder per USB auslesen und gespeicherte Nummern/Dubletten abgleichen. Powerbank-Bereitschafts-LED fehlt. |

Rohlogs bleiben privat neben den Sicherungen:
`radio-near-20261010.txt`, `radio-site-20261010.txt`,
`led-sweep-20261010.txt`, `led-groups-20261010.txt` und
`audio-100-twice-20261010.txt` im Originalbox-Sicherungsordner.
Die LED-Wiederholung bei laufender Handykamera steht in
`led-sweep-video-20261010.txt` und `led-groups-video-20261010.txt`.
Der erste Tagversuch steht dort in `tag1-scan-20261010.txt`.
Die weiteren NFC-/Startproben stehen in `tag1-rescan-20261010.txt`,
`tag1-diagnostic-scan-20261010.txt` und `boot-after-supply-20261010.txt`;
der Audiolauf nach Versorgung in `audio-100-powered-20261010.txt`.

Beim Versuch, die Box ohne Tag erneut zu starten, war COM3 vorübergehend nicht
vorhanden. Nach Abschalten der separaten Audio-Versorgung und erneutem
Verbinden des Box-USB erschien der CP210x wieder. Der Nutzer sah den Box-Start
und die ausgeschaltete DFPlayer-Power-LED. Die Chipkennung wurde erneut als
`a8:42:e3:cd:4b:18` geprüft. Die erweiterte Diagnoseversion wurde erfolgreich
hochgeladen und meldete beim Neustart PN532-Transport, Firmwareantwort und
SAM-Initialisierung als bereit. Eine lesende SD-Trackanzahlabfrage ist nun
verfügbar. Rohlogs: `boot-no-audio-no-tag-20261010.txt`,
`boot-nfc-stages-audio-off-20261010.txt`, `tag1-probe-audio-off-20261010.txt`.
Die Versorgungskorrelation ist ein Hinweis, keine geklärte Ursache.
Ein zusätzlicher rein lesender Rohpaket-Befehl (`tag raw`) ist im Quellstand
gebaut (1.209.021 Byte), aber noch nicht auf die Box geladen. Die aktuell
geladene Fassung umfasst die IRQ-Leseprobe (1.208.313 Byte).

## Ausstehender Abschluss

Wake- und Matrixbetätigungen, NFC-A/B-Abfolge, hör- und sichtbare Beobachtung,
Empfänger-USB-Bericht, drei Kaltstarts und gemeinsamer längerer Lauf. Nicht
gemessene Versorgungsspannung und fehlende Videoaufnahme bleiben ausdrücklich
**nicht geprüft**. Die Diagnose-Firmware bleibt bis zu einer ausdrücklichen
Restore-Aufforderung auf der Box.

NFC-Seitengrenze und GET_VERSION-Auswertung beruhen auf dem
[NXP-Datenblatt NTAG213/215/216](https://www.nxp.com/docs/en/data-sheet/NTAG213_215_216.pdf)
(Nutzdaten ab Seite 4, 144/504/888 Byte; Konfigurationsseiten getrennt).
