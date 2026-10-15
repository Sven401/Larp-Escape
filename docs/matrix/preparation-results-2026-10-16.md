# Prüfstand der Matrix-Testvorbereitung

Stand: **16. Oktober 2026**. Status: **Software geprüft; physische Abnahme offen**.
Dieser Bericht hält die ursprüngliche Softwarevorbereitung fest. Der spätere
[Gerätetest](device-test-2026-10-16.md) erkannte zwei betätigte Reedkontakte
nicht. Scanrichtung und B7-Freigabe bleiben zu korrigieren beziehungsweise
physisch abzugleichen; die grünen Hosttests belegen deren Eignung nicht.

Basis `c0f76ee5123ef51252341b21c18a636abb6f6bc2`, eigener Branch
`codex/matrix-test-preparation`. Quelle: lokale Prüfläufe der unveränderten
Quellen dieses Beitrags und unabhängige Durchsicht in derselben Sitzung.
Die Commit- und Binärzuordnung steht im ausgelieferten Paketmanifest; zusätzliche
Buildresultate und GitHub-Prüfläufe stehen am zugehörigen Pull Request.

## Gemessene Softwareprüfungen

| Prüfung | Ergebnis |
|---|---|
| Native BLE-Vertragsprüfung | **4572 Assertions bestanden**, echte HMAC mit unabhängigen Python-Referenzbytes |
| LED-/SDK-/Wake-/Matrix-Hostsuite | **bestanden**; 16 LED-Fälle, 4 SDK-Fälle, Wake/I²C und neuer Matrixscanner |
| Android-Protokollkern | **68 Tests**, 0 Fehler, 0 ausgelassen; darunter 20 LED-/Matrix-Vertragstests |
| Android-App | **52 Tests**, 0 Fehler, 0 ausgelassen; darunter 8 neue Matrixfälle |
| Android-Build | Debug und unsigniertes Release **erfolgreich** |
| ESP32-Builds | `larp-led-product` und `larp-matrix-test` **erfolgreich** |
| Debug-Lint | **0 Fehler**, 3 bestehende Warnungen und 1 bestehende Information |
| Architekturprüfung | **bestanden**, einschließlich 7 Guard-Selbsttests; 145 Dateien / 302 direkte Includes |
| Diff-Prüfung | `git diff --check` **bestanden** |

Debug-APK SHA-256:
`fbdb0439aef6eb950b39c9eaf1b042865654e0865e46cce36ae91c5dc6910cd6`.
Unsigniertes Release-APK SHA-256:
`bc534f35a15cc164081a8935de19a25dac0d9d3b81acd6ee8e52935bfe3c81a3`.

Matrix-Firmware SHA-256:
`50accb8ddb3e14a30e196a7f7dc6f479206d6e3a22e7f32337c8ac2a0c2f60ce`.
RAM 42.472/532.480 Byte; Programm 1.133.537/3.145.728 Byte. Standardfirmware:
RAM 42.424 Byte, Programm 1.131.449 Byte. Die Partitionstabelle entspricht
bytegleich dem bestehenden Wake-Rückweg. Die NFC-Basisaktualisierung ändert
keine Firmwarequelle; der Dateivergleich gegen den gebauten Stand ist leer.

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

Die inzwischen veröffentlichte NFC-Rücklesekorrektur `c0f76ee` wurde vor dem
Abschluss als Basis übernommen. Die kombinierte App wurde erneut gebaut und
geprüft; die oben genannten 120 Tests und Prüfsummen gehören zu diesem Stand.
