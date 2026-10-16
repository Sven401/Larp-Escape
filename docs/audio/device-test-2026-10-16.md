# Audio: Installation und gemeinsamer Test – 16. Oktober 2026

**Status: App-/Firmwarepaar installiert und technisch verifiziert;
erste Hör-/Bedienprüfung nutzerbestätigt, weitere Schritte offen.** Nutzerauftrag in dieser Sitzung:
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
Zu diesem Installationszeitpunkt waren Titel 100 und Lautstärke 5 nur vorausgewählt; noch kein Play gesendet. Die späteren Versuche stehen unten.
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
| A0: Verbunden, kein Start gedrückt | Authentifiziert, Audio bereit, kein aktiver Startauftrag, BUSY HIGH | Nutzer: „Ja, die Audio-Baugruppe ist still.“ | bestanden für diesen Ruhezustand |
| Erster Play-Versuch: Titel 100, Lautstärke 5 | Genau ein Play-Tap; App meldet UART-Übertragung, beim anschließenden Auslesen kein aktiver Startauftrag und BUSY HIGH; private Belege `play-before.xml`, `play-first-result.xml` | Nutzer: „Es bleibt still, ich höre nichts.“ | Hörversuch fehlgeschlagen; UART-Ausgabe separat bestätigt |
| Zusätzliche Audio-Versorgung manuell hergestellt | Keine neue Spannungs-/Strommessung durch Codex | Nutzer: Powerindicator leuchtete anschließend | Versorgungshandlung und Anzeige nutzerbestätigt; elektrische Werte offen |
| Play Titel 101 nach Versorgungshandlung | Vom Nutzer am Handy durchgeführt; kein zeitgleicher Codex-Mitschnitt | Track 101 abgespielt und gehört | hörbare Wiedergabe für diesen Versuch nutzerbestätigt |
| Pause / Fortsetzen | Vom Nutzer durchgeführt | Beides funktional | nutzerbestätigt; genaue Zeit-/Fortsetzungsposition nicht protokolliert |
| Lautstärkeänderung während Wiedergabe | Vom Nutzer durchgeführt; konkrete Stellwerte nicht mitgeteilt | Funktional während der Track spielt | nutzerbestätigt; getestete Stellwerte offen |
| Pause-Verfügbarkeit | Nutzer beobachtet die App | Nur während Track spielte verfügbar; nach Trackende nicht mehr verfügbar | Bedienbeobachtung nutzerbestätigt; Ursache der Zustandsänderung nicht separat gemessen |
| BUSY beim erfolgreichen Versuch | Kein zeitgleicher unabhängiger Pegelmitschnitt | LOW während Audio spielte, HIGH bei Stille (vom Nutzer unmittelbar berichtigt) | Verlauf nutzerbestätigt; keine unabhängige elektrische Messung |
| Manueller Stop während Track 101 | Vom Nutzer durchgeführt; kein zeitgleicher Codex-Mitschnitt | Nutzer bestätigt: Ton endet sofort, BUSY wechselt auf HIGH | nutzerbestätigt bestanden; keine gemessene Abschaltlatenz |
| Bewusster Neustart von Track 101 nach Stop | Vom Nutzer durchgeführt | Derselbe Titel beginnt wieder am Anfang; BUSY wechselt auf LOW | nutzerbestätigt bestanden |
| Play während Track 101 bereits spielt | Vom Nutzer durchgeführt | BUSY kurz HIGH, dann wieder LOW; Titel beginnt erneut am Anfang | nutzerbestätigt bestanden; keine gemessene Umschaltzeit |
| Titelauswahl zwischen zwei hörbar erkannten Titeln | 100 stumm vor Versorgung, 101 hörbar danach | Keine zwei hörbar erkannten Inhalte unter gleicher Versorgung verglichen | offen |

Quelle aller nachträglichen Hör-/Bedienbeobachtungen: Nutzerbericht am
16. Oktober 2026 in dieser Sitzung. Sie werden nicht als eigener Geräte-
oder Messmitschnitt ausgegeben. Nach dem Zuschalten der Zusatzversorgung
war Track 101 hörbar; daraus allein folgt kein Nachweis des tatsächlichen
Dateiinhalts von Track 100 oder einer vollständigen elektrischen Fehlerursache.

**Berichtigung des Nutzers am 16. Oktober 2026:** BUSY ist bei Stille
HIGH; die unmittelbar zuvor gemeldete Angabe LOW bei Stille war falsch
wiedergegeben. Maßgeblich ist der berichtigte Verlauf **LOW bei Wiedergabe,
HIGH bei Stille**. Er passt zur konfigurierten aktiven LOW-Interpretation.
Die Korrektur ist eine Nutzerbeobachtung, keine nachträgliche eigene Messung.
Pause war während der Wiedergabe verfügbar und nach dem wahrgenommenen
Trackende nicht mehr verfügbar. Natürliches Titelende und automatische
30-Sekunden-Begrenzung wurden zeitlich noch nicht getrennt protokolliert.

**Nächster gemeinsamer Schritt:** Wiedergabe stoppen, Titelnummer 102
als nächsten Kandidaten auswählen und einmal Play drücken. Prüfen, ob ein
anderer hörbarer Inhalt als bei 101 beginnt; BUSY getrennt beobachten.
Der tatsächliche Karteninhalt von 102 ist noch nicht bestätigt. Nach diesem
Schritt auf die Nutzerbeobachtung warten. [Ablauf und Erweiterungen](test-preparation.md).
