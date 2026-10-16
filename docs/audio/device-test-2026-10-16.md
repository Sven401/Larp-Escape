# Audio: Installation und gemeinsamer Test – 16. Oktober 2026

**Status: App-/Firmwarepaar installiert und technisch verifiziert;
Hörtest noch nicht durchgeführt.** Nutzerauftrag in dieser Sitzung:
„Installiere App und Firmware und dann testen wir die Audio-Baugruppe gemeinsam Schritt für Schritt.“

## Installiertes Paar

Quellen: `243f00c75366c1125d9322420a72cbfb4b2d5fe6`, [PR #51](https://github.com/Sven401/Larp-Escape/pull/51).
Die geprüften Produktdateien stehen bei `958d6dc`; der spätere Stand ergänzt nur Dokumentation.
Installationszeit, Europe/Berlin: `2026-10-16T21:45:45.163657+02:00`.
Das angeschlossene moto g14 wurde mit Datenerhalt aktualisiert.
Die Signierzertifikate von vorheriger und neuer APK sind identisch;
die installierte APK wurde zurückgelesen und ist bytegleich zum vorbereiteten Artefakt.

| Artefakt | SHA-256 |
|---|---|
| Installierte APK, 23.303.762 Byte | `34dbeffd6a7c897c3c9d418cb1ad5b3251645fa155e080fb6c0703653ba8772a` |
| Installierte Firmware, 1.141.040 Byte | `1f787f9194a6d6e45493bf0e03c041a527ee53fd8830840c4cd562318e3c8507` |
| Neuer chipgebundener Vollflash-Rückweg, 4 MiB | `dc63856e987662b01b1bf76a1f74d88970c460b0bb7e904ded802e64d48d0fac` |
| Unveränderte Partitionstabelle | `22790624ada38b3379c977dd7fd8452c6a3c86c00c64ec118380c888655e5b91` |

ESP32-D0WD-V3, Revision 3, 4 MiB, COM3: Chipkennung mit dem privaten Wake-Rückweg
abgeglichen. Vorher vollständigen aktuellen Flash gelesen und mit `verify_flash`
am selben Chip bestätigt. Vorherige APK und vorherige vollständige Apppartition
separat privat erhalten. Partitionstabelle stimmt bytegleich mit der vorbereiteten
Tabelle überein; Schlüsselpartition entspricht dem bekannten chipgebundenen Rückweg.
Ausschließlich die Anwendung ab `0x10000` geschrieben; Uploadprüfung und separate
`verify_flash`-Prüfung erfolgreich. Anschließend Schlüsselpartition bei
`0x310000`/`0x6000` erneut gelesen und bytegleich zum frischen Vorab-Abzug bestätigt.
Kein vollständiges Löschen und keine neue Schlüsseleinrichtung.

Im 20-Sekunden-Startmitschnitt erschien genau eine Bereitmeldung;
kein beobachteter Panic-, Abort-, Brownout- oder eigener Fehler-Marker.
Die echte App fand die Box und meldete „LED-Box verbunden · Zugang bestätigt“.
Gespeicherter Zugang und Wake-Anordnung 2 / 3 / 4 / 1 blieben erhalten.
Die Audioanzeige meldet bereit, keinen aktiven Startauftrag und BUSY HIGH.
Titel 100 und Lautstärke 5 sind nur vorausgewählt; **kein Play gesendet**.
Die Buttons Play/Stop/Lautstärke sind freigegeben; Pause/Fortsetzen sind im
Ruhezustand gesperrt. Eine UART-Annahme oder dieses BUSY-Signal beweist keine Stille.

## Prüfstand und Herkunft

Alle drei CI-Läufe am installierten Quellstand wurden als erfolgreich ausgelesen:
[Quality](https://github.com/Sven401/Larp-Escape/actions/runs/38956104059),
[LED product](https://github.com/Sven401/Larp-Escape/actions/runs/38956103933),
[NFC probe](https://github.com/Sven401/Larp-Escape/actions/runs/38956104299).
Lokale Softwareprüfungen und Grenzen: [Vorbereitungsbericht](controls-results-2026-10-16.md).
Nutzerbestätigung der Audiopins GPIO26/25/14 steht im [Testplan](test-preparation.md).

Private Rohbelege: `%LOCALAPPDATA%/LarpEscape/LabRuns/Audio-20261016-194033Z`.
Manifest, alte/neue APK, Vorab-Vollflash, separate Vorab-Anwendung,
Schlüsselabzüge, Signatur-/Upload-/Verifikationslogs, Startprotokoll sowie
`audio-ready.xml`/`audio-ready.png` bleiben außerhalb Git. Gerätekennungen,
Schlüssel und vollständiger Flash werden nicht veröffentlicht.

## Gemeinsame Beobachtungen

| Schritt | Geräte-/Appbeleg | Nutzerbeobachtung | Ergebnis |
|---|---|---|---|
| A0: Verbunden, kein Start gedrückt | Authentifiziert, Audio bereit, kein aktiver Startauftrag, BUSY HIGH | noch ausstehend: hörbare Stille | offen |
| Play Titel 100, Lautstärke 5 | nicht ausgelöst | ausstehend | nicht durchgeführt |
| Pause / Fortsetzen | nicht ausgelöst | ausstehend | nicht durchgeführt |
| Stop / bewusster Neustart | nicht ausgelöst | ausstehend | nicht durchgeführt |
| Titelauswahl / Lautstärke übernehmen | nur 100 / 5 vorausgewählt | ausstehend | nicht durchgeführt |

**Nächster Schritt:** Nutzer bestätigt zunächst den Ruhezustand ohne hörbare
Wiedergabe. Erst anschließend einen einzelnen Play-Versuch gemeinsam auslösen
und dessen Höreindruck abwarten. Nach jedem physischen Schritt auf die
Nutzerbeobachtung warten. [Ablauf und Erweiterungen](test-preparation.md).
