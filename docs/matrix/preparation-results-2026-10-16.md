# Prüfstand der Matrix-Testvorbereitung

Stand: **16. Oktober 2026**. Status: **Software geprüft; physische Abnahme offen**.
Basis `e00fe437081582727b2308da7f3b0c835c9bda75`, eigener Branch
`codex/matrix-test-preparation`. Quelle: lokale Prüfläufe der unveränderten
Quellen dieses Beitrags und unabhängige Durchsicht in derselben Sitzung.
Die Commit- und Binärzuordnung steht im ausgelieferten Paketmanifest; zusätzliche
Buildresultate und GitHub-Prüfläufe stehen am zugehörigen Pull Request.

## Gemessene Softwareprüfungen

| Prüfung | Ergebnis |
|---|---|
| Native BLE-Vertragsprüfung | **4572 Assertions bestanden**, echte HMAC mit unabhängigen Python-Referenzbytes |
| LED-/SDK-/Wake-/Matrix-Hostsuite | **bestanden**; 16 LED-Fälle, 4 SDK-Fälle, Wake/I²C und neuer Matrixscanner |
| Android-Protokollkern | **63 Tests**, 0 Fehler, 0 ausgelassen; darunter 20 LED-/Matrix-Vertragstests |
| Android-App | **39 Tests**, 0 Fehler, 0 ausgelassen; darunter 8 neue Matrixfälle |
| Android-Build | Debug und unsigniertes Release **erfolgreich** |
| Debug-Lint | **0 Fehler**, 3 bestehende Warnungen und 1 bestehende Information |
| Architekturprüfung | **bestanden**, einschließlich 7 Guard-Selbsttests; 145 Dateien / 302 direkte Includes |
| Diff-Prüfung | `git diff --check` **bestanden** |

Debug-APK SHA-256:
`e4ef2815efacbe1c87d46474313a19e09b78546fa9fdba0bff134ba2e55ae6f3`.
Unsigniertes Release-APK SHA-256:
`ee4297159fb19f54729c5e9296fef1e5db0d984d0e27e70d670ef3c5be857522`.

Compilerstarts innerhalb der Windows-Sandbox schlugen mit WinError 623 fehl;
die genannten erfolgreichen nativen Läufe wurden mit demselben Quellstand
außerhalb der Sandbox wiederholt. Die lokale Formatbaseline konnte mangels
`clang-format 19.1.7` nicht ausgeführt werden; der bestehende Quality-Prüflauf
installiert dessen festgelegte Version. Kein Format-Pass wird daraus abgeleitet.

## Aussagekraft der neuen Prüfungen

Der Registerfake prüft alle 81 Einzelkontakte und die Vollmaske, erhaltene
Wake-/Fremdbits, höchstens eine LOW-Zeile, Freigabe vor Zeilenwechsel,
Einschwingzeit, vollständige Veröffentlichung, Rechteck-/Ghostingmuster,
Fehler an jeder Initialisierungs-/Scantransaktion, ignorierte Schreibzugriffe,
fehlende Geräte, Bankfehler, fehlgeschlagene Freigabe, Wiederanlauf und Uhrüberlauf.
Eine nicht erreichbare Hardware kann trotz Freigabeversuch eine Zeile aktiv
halten; der Test verlangt dann unbekannte Daten, nicht behauptete Freigabe.

Die Protokollprüfung deckt alte 70-Byte-Nachrichten, neue 82-Byte-Nachrichten,
Bit 80, unzulässige Restbits, widersprüchliche Flags, Länge, MAC und
Sitzungsbindung ab. Android prüft alle Koordinaten, Fristgrenzen,
Teilnachrichten, ältere/gleiche Sequenzen, LED-Results ohne Messung,
Lesefehler, Trennung und mehrdeutige Rohkontakte.

## Durchsicht und behobene Befunde

Eine unabhängige Durchsicht fand zwei Gültigkeitslücken im ersten Entwurf:

1. Nach einer Pause in der Startphase konnte eine alte Probe erneut als frisch
   erscheinen. `snapshot(millis())` begrenzt nun ihr tatsächliches Alter auf
   250 ms ab Rundenbeginn, zusätzlich zur App-Frist.
2. Ein Expanderreset während einer Scanrunde konnte zunächst unbemerkte falsche
   Werte erzeugen. Die Konfiguration wird nun auch vor Veröffentlichung geprüft.

Beide auslösenden Fälle sind mit gezielten Regressionen geprüft. Der Reviewer
hat die Änderungen anschließend erneut gelesen. Die Mehrdeutigkeitsanzeige
bewahrt Rohkontakte mit Warnung, statt einen physischen Tastendruck zu behaupten
oder alle Beobachtungen zu verbergen.

## Offene Nachweise

Keine Appinstallation, kein Firmwareupload und keine Geräteinteraktion in dieser
Vorbereitung. Layout und Bedienbarkeit am echten Handy, physische Zuordnung,
Chip-/Diodenfrage, reale Kontaktanzahl, Mehrfachbetätigung und vorhandener
Wake-/LED-Betrieb im Matrixmodus werden gemeinsam nach dem
[Testplan](test-preparation.md) geprüft. Ein Scan erfasst Kontakte nacheinander,
nicht gleichzeitig; kurze Impulse und Entprellung werden nicht als abgenommen
behauptet. Auch alle vorbereiteten Zeilen im Zuordnungsblatt bleiben offen.

Die gleichzeitig bearbeitete NFC-Rücklesekorrektur war in der Basis noch nicht
enthalten. Vor späterer Installation den dann aktuellen Appstand abgleichen,
damit eine inzwischen installierte Korrektur nicht zurückgesetzt wird.
