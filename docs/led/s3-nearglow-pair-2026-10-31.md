# Zwei ESP32-S3: Sicherung und autonome Näheglut

Datum: **31.10.2026**. Auftrag: beide S3 auf COM4 und COM7 sichern und
mit der vorhandenen Näheglut-Testfirmware einrichten. Status: beide installiert,
gepaart und dauerhaft freigegeben; beidseitige frische Funkantworten,
sichtbarer Quellenverlust mit autonomer Rückkehr und STOP nach Neustart bestanden.
Räumliche Annäherung bei größerem Abstand bleibt gesondert zu prüfen.

Produktionscode `9f233e41e5c0517add3080f1bea078d3ca01db6f`, native Firmware
`s3-proximity-bench-v1`. Zusätzliche echte PSA-Qualifikation `f5914bb`,
Hosthelferkorrektur `d38a2c5`. Keine Änderung am Produktionscode beim Geräteauftrag.
[Entwurfs-PR #118](https://github.com/Sven401/Larp-Escape/pull/118).
Firmware/Plattform: `s3_firmware`; Hosthelfer: `proximity_concept`;
Geräteausführung: Hauptinstanz; unabhängige Software-/Rückwegprüfung: `s3_review`.
Der andere Reviewer hat keine Geräteports oder privaten Schlüssel geöffnet.

## Identität und Rückweg

| Anschluss dieses Laufs | Chip/MAC | Vollbackup SHA-256 |
| --- | --- | --- |
| COM4 | ESP32-S3 Rev. 0.1; `34:B7:DA:59:77:E8` | `e5ae08a8be2e021724d6b1f0959420d2efcd9ed5956ad2f6044c3e3e0e7a5d0e` |
| COM7 | ESP32-S3 Rev. 0.2; `E4:B3:23:F8:0F:0C` | `77cdaae04bfb12967a483124c23283af44199fb85555875ad16d129b1e1c80b8` |

Beide frisch ausgelesen: 4 MiB eingebauter Flash, 2 MiB PSRAM, Sicherheitsflags
0, Secure Boot und Flashverschlüsselung aus. Jeder Vollabzug enthält exakt
4.194.304 Byte und wurde vor dem ersten Schreiben mit dem vollständigen Flash
desselben Chips erfolgreich verglichen. COM4 enthält den aktuellen Handyteststand,
nicht nur das ältere Originalabbild. COM3 blieb unberührt.

Private Belege: `private/s3-pair-20261031/`, pro MAC Vollabbild, Read-/Verify-/
Installationslogs und Rückweganweisung. Zusätzliche hashgleiche Vollkopien:
`%LOCALAPPDATA%/LarpEscape/ESP32-Backups/s3-nearglow-pair-20261031/`.
Originale und Paargeheimnis werden nicht ins Repository aufgenommen. Die neue
Paarungsdatei und Rückweganweisung liegen zusätzlich privat im genannten
dauerhaften Sicherungsverzeichnis; die Dateikopie wurde hashverglichen.

Der vorbereitete, nicht ausgeführte Vollrückweg schreibt nur das zu diesem
erneut identifizierten Chip gehörende Abbild ab Adresse 0, mit Flashparametern
`keep`, und verifiziert anschließend das gesamte Abbild. Er stellt auch die
jeweilige alte Partitionierung und Konfiguration wieder her.

COM4 hatte bereits die LED-Partitionstabelle. COM7 hatte zuvor ein Dual-OTA-/UF2-
und FAT-Layout. Der Standard-NVS-Bereich blieb bei beiden `0x9000/0x5000`.
Es gab weder pauschales Flashlöschen noch NVS-/SPIFFS-/Schlüsselpartitions-Erase
oder eFuse-Änderungen. Die alte Arduino-Initialisierung von `larpkeys` wurde
ausdrücklich nicht übernommen; die neue Firmware verwendet dort keine Daten.

## Tatsächliche Installation

Der native Build am qualifizierten SDK-Pin
`9a97f6c54ec638111ce55cd36581b3c192f15207` wurde erneut erfolgreich ausgeführt.
Die tatsächlichen `flash_args` und `flasher_args.json` stimmen überein:

| Adresse | Bereich | Byte |
| --- | --- | ---: |
| `0x0` | Bootloader | 20.688 |
| `0x8000` | Partitionstabelle | 3.072 |
| `0xe000` | initiale OTA-Metadaten | 8.192 |
| `0x10000` | Anwendung | 783.280 |

DIO, 80 MHz, 4 MiB. Anwendung SHA-256:
`daec0ad69cda5e3da088612d4742bc26525df5f3e67e2a4e6ccd9ee466bdf8ed`.
Alle Bereiche wurden beim Schreiben digestverifiziert. COM7 erhielt zusätzlich
den unabhängigen Vergleich aller vier Bereiche vor dem ersten Anwendungsboot.

Auf COM4 waren beim separaten Vergleich nach dem ersten Boot Bootloader,
Partitionstabelle und Anwendung unverändert. Die initial leeren OTA-Metadaten
hatte der Bootloader regulär durch den OTA0-Selektor ersetzt. Ein separater
8-KiB-Abzug und unabhängige Prüfung bestätigten Sequenz 1, Status 2,
CRC `0x4743989a`, leeres Label und alle übrigen Bytes weiterhin `0xff`.
Das ist kein offener Code-/Flashvergleichsfehler; es wurde deswegen nicht neu geschrieben.

COM7 benötigte nach dem Vorbootvergleich einmal RESET ohne BOOT, bevor die
Anwendung antwortete. Anschließend meldeten beide ungepaart, dauerhaft gesperrt,
Fehler 0 und RMT-bestätigtes Schwarz.

## Einrichtung und echte Funkwerte

Ein einziger frischer Paarungslauf erzeugte die private Originaldatei außerhalb
des Repositorys und konfigurierte beide Geräte zunächst gesperrt. Nach den
Neustarts meldeten beide bereits den richtigen Partner, aktiven Funk,
dauerhafte Sperre, bestätigtes Schwarz und Fehler 0; der Hostabschluss war
dennoch abgebrochen und die Datei noch `prepared`.

Die genaue damalige Ausnahme ist rückwirkend nicht belegt. Ein reproduzierbarer
Boottext-Abbruchpfad wurde ausschließlich für die Bereitschaftsprüfung nach
eigenem Neustart begrenzt korrigiert und mit 22 Helfertests unabhängig geprüft.
Kein allgemeines Resume-Kommando und keine gelockerte normale INFO-Prüfung.

Dieser konkrete Lauf wurde kontrolliert mit seiner bekannten Originaldatei,
den ursprünglichen Paarungsbestätigungen und erneut vollständigen öffentlichen
Identitäts-/Partner-/Disarm-/Schwarz-Gates abgeschlossen. Keine neuen Schlüssel,
keine erneute Paarung. Passende MACs allein würden eine beliebige fremde
Paarungsdatei nicht legitimieren; es wird kein Rücklesen des gespeicherten
Geheimnisses behauptet. Privater Provenienzbeleg: `controlled-pair-completion.json`.

Danach bestätigte der Helfer ARM auf beiden Geräten einschließlich Speicherung.
Erster beidseitiger 8-Sekunden-Lauf: je 16 gültige Statusproben, alle frisch,
kein Fehler und keine USB-Lesefehler. COM4 RSSI -47 dBm, Level 860;
COM7 -47 bis -46 dBm, Level 860 bis 880. Bestätigtes RGB zuletzt `(20,6,0)`
beziehungsweise `(21,7,0)`. Dies belegt tatsächliche gegenseitig akzeptierte
Funkantworten und laufende Glutberechnung, keine Meterkalibrierung.

Der Host fragt nur INFO ab. Er erzeugt dabei keine Funkproben oder Effektupdates;
beides führt die autonome Firmware selbst aus.

Weiterer 35-Sekunden-Lauf: je 67 Statusproben, alle frisch und Fehler 0;
RSSI blieb bei -47 bis -46 dBm und Level bei 860 bis 880. Dies ist ein
Stabilitätsbeleg, kein nachgewiesener räumlicher Annäherungsverlauf.

## Sichtprüfung und weitere Gerätetests

Der Nutzer bestätigt: **Beide eingebauten LEDs leuchten warm orange.**
Eine Helligkeitsänderung ist kaum sichtbar; beide Boards hängen über USB am
Rechner, die mögliche Abstandsänderung ist sehr gering. Ein belastbarer
Annäherungs-/Entfernungsverlauf bleibt mit getrennten Stromquellen zu prüfen.

**Echter Quellenverlust und Rückkehr bestanden:** Der Nutzer zog COM7 vom USB
ab und bestätigte das sichtbare Ausblenden von COM4 nach kurzer Verzögerung.
Die nur lesende COM4-Aufzeichnung zeigt bei t=35,937 s den Wechsel zu
`fresh=false`, Level 0; bei t=36,953 s RGB0 und bestätigtes Schwarz.
Diese Zeiten beziehen sich auf die Aufzeichnung, nicht auf eine synchron
gemessene Abziehzeit. Die Ausblenddauer im beobachteten Übergang beträgt etwa eine Sekunde.

Nach Wiedereinstecken von COM7 erschien ohne ARM/Handy/Startbefehl bei
t=58,937 s wieder eine frische Antwort; bei t=59,453 s war RGB bereits `(5,1,0)`
und die Glut erholte sich weich. Der Nutzer bestätigt, dass beide LEDs von
selbst wieder leuchten. COM7 hat damit seinen gespeicherten ARM-Zustand über
einen tatsächlichen Stromverlust beibehalten; COM4 erholt seinen laufenden Effekt.
Quellverlust und Rückkehr blieben ohne gemeldeten Fehler. Privater Verlauf:
`source-loss-return.jsonl`.

Der vollständige Verlust-/Rückkehrmitschnitt enthält 235 COM4-Proben ohne
USB-Lesefehler oder Gerätefehler, davon 190 mit frischer Quelle. Während der
Unterbrechung bleibt die Freigabe bestehen, der ungültige Eingang steuert null.

**STOP und Neustart bestanden:** Der Helfer bestätigte auf beiden Geräten
dauerhaften Disarm und RMT-bestätigtes Schwarz. Beide wurden danach über den
disarmed-geschützten REBOOT-Befehl neu gestartet. Frische öffentliche INFOs
bestätigen auf beiden weiterhin `armed=false`, `persistedArmed=false`,
`durableKnown=true`, Fehler 0 und RGB0 bei vorhandenem Funk. Nach den ACKs
wurde 1,5 Sekunden gewartet; zusätzlich wurden `level=0`, `fresh=false` und
`ageMs=null` als zurückgesetzte Messwerte verlangt. Privater Beleg:
`stopped-reboot-confirmed.json`. Die erste zu frühe Probe `stopped-reboot.json`
wurde vom unabhängigen Reviewer verworfen und zählt nicht als Neustartnachweis.
Das ist ein Anwendungsneustart, kein Stromabbruch mitten im Speicherschreiben.

Anschließend wurden beide bewusst erneut freigegeben. Abschlusslauf:
je 10 von 10 Proben frisch, keine Lesefehler, Fehler 0 und dauerhaftes ARM.
COM4 zuletzt -80 dBm/Level 200/RGB `(4,1,0)`, COM7 -79 dBm/Level 220/RGB `(5,1,0)`.
**Endzustand: beide aktiv und testbereit.**
Privater Beleg nach korrigierter Neustartprüfung: `final-armed-confirmed.jsonl`.

Status `black` bestätigt RMT-abgeschlossenes RGB0; er ist keine unabhängige
optische Beobachtung. Elektrische Wellenform-/Last-/Fehlerinjektionsabnahme,
Stromausfall während eines NVS-Commits und Produkt-/Rätselintegration bleiben offen.

## Android-Kandidat

Vorhandene Basis: Companion **Larp Dev 0.2 / Code 2**, Paket
`de.larpescape.companion`. Lokal statisch geprüfte APK SHA-256:
`c81456450b0ff68cd675113f65b728db3dc017da95aaa9942e2132ad7e9d5520`.
Worktree `ble-rollen-profile` bei `0c8ced2`; eine exakte APK-Commitbindung
ist damit allein nicht bewiesen. Der RSSI-Anzeigecode stammt aus `6e6f093`.

Die App besitzt echte RSSI-/Glättungs-/Peeranzeigen, erwartet jedoch aktiv
angeforderte authentisierte BLE-V2-Binärantworten. Dieser native Prüfstand
startet kein BLE und liefert USB-INFO-JSON. Es existiert deshalb **noch kein
direkt kompatibler Android-Liveviewer** für das autonome Paar. Die APK wurde
für diesen Auftrag nicht installiert oder ersetzt.

Ein rein lesender USB-Monitor kann bereits die vorhandenen Statusfelder
abfragen. Eine Android-Anbindung braucht einen eigenen USB-OTG-Adapter oder
spätere passive BLE-Telemetrie mit Koexistenzprüfung. Sie ist kein Bestandteil
dieses Geräteuploads und darf die autonome Effektsteuerung nicht übernehmen.
