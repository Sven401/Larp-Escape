# Audio: Ergebnisbogen für den gemeinsamen Test

**Vorlage vom 16. Oktober 2026. Nicht durchgeführt. Alle Ergebnisse offen.**
Den ausgefüllten Bogen als eigenen datierten Gerätebericht erhalten.
Sollwerte und Rückweg stehen im [Testplan](test-preparation.md).

## Identität und Vorbereitung

| Feld | Tatsächlicher Wert / Beleg |
|---|---|
| Datum und Uhrzeit, Europe/Berlin | offen |
| Quellcommit / Branch | offen |
| APK-Größe und SHA-256 | offen |
| Installierte APK zurückgelesen und gleich | offen |
| Firmware-Größe und SHA-256 | offen |
| Partitionstabelle SHA-256 / unverändert | offen |
| Chip gegen privaten Rückweg abgeglichen | offen; Kennung nicht veröffentlichen |
| Neuer vollständiger Flashabzug, Hash und Chipverifikation | offen; Abzug privat |
| Anwendung nach Upload verifiziert | offen |
| Schlüsselpartition zum Vorab-Abzug bytegleich | offen |
| Startprotokoll / beobachtete Laufzeit | offen |
| App authentifiziert, Status frisch | offen |
| UART-/BUSY-/WAKE-Leitungsstand und Quelle | offen |
| microSD / ausgewählter Titel / erwarteter Inhalt | Nutzer bestätigt Con-Audiostand; Dateiliste und Inhalt offen, erster Kandidat 100 |
| Lautsprecher oder Verstärker / Versorgung | offen |
| Anfangslautstärke / höchste verwendete Stufe | offen; Soll 5 / höchstens 10 |

## Gemeinsame Beobachtungen

Beobachtungen wörtlich beziehungsweise mit konkreter Meldung erfassen.
Ein gesendeter UART-Befehl und ein BUSY-Pegel ersetzen keinen Hörbeleg.
Mögliche Ergebnisse: **bestanden**, **fehlgeschlagen**, **unklar**,
**nicht durchgeführt**. Einzelne Quellen dürfen unterschiedlich ausfallen.

| ID / Uhrzeit | App-/Boxauftrag und Belegdatei | BUSY-Rohpegel / Verlauf und Beleg | Nutzer-Hörbeobachtung | Ergebnis / offene Frage |
|---|---|---|---|---|
| A0 Verbindung ohne Start | offen | offen | offen | nicht durchgeführt |
| A1 Auswahl ohne Start | offen | offen | offen | nicht durchgeführt |
| A2 Erster bewusster Start | offen | offen | offen | nicht durchgeführt |
| A3 Stop während Wiedergabe | offen | offen | offen | nicht durchgeführt |
| A4 Bewusster erneuter Start | offen | offen | offen | nicht durchgeführt |
| A5 Neustart während Wiedergabe | offen | offen | offen | nicht durchgeführt |
| A6 Natürliches Ende oder Stopauftrag nach 30 s | offen | offen | offen | nicht durchgeführt |
| A7 Trennen während Wiedergabe | offen | offen | offen | nicht durchgeführt |
| A8 Wiederverbinden / App-Neustart | offen | offen | offen | nicht durchgeführt |
| A9 Abschließender Stop | offen | offen | offen | nicht durchgeführt |
| Optional zweiter Titel | offen | offen | offen | nicht durchgeführt |

Eine bewusste Wiederholung als neue Zeile mit neuer Uhrzeit aufnehmen.
Ein verpasster kurzer Titel ist kein bestandener Hörnachweis. Unklaren
Ausgang nicht durch stilles Wiederholen aus dem Bericht entfernen.

## Abschluss und Rückweg

| Nachweis | Ergebnis / Beleg |
|---|---|
| Abschließender Stopauftrag | offen |
| Nutzer bestätigt Stille | offen |
| Tatsächlich verbleibender App-/Firmwarestand | offen |
| Falls wiederhergestellt: Anwendung am Chip verifiziert | offen |
| Falls wiederhergestellt: Schlüsselpartition unverändert | offen |
| Falls wiederhergestellt: normaler Start und Anmeldung | offen |
| Falls wiederhergestellt: frische Wake-Werte | offen |
| Private Rohbelegorte | offen; keine Zugangsdaten veröffentlichen |

## Bewertung

Bestätigter Umfang: **noch keiner**.

Offene Punkte: physische UART-/BUSY-Zuordnung und tatsächlicher Titelinhalt,
Hörbarkeit, Stop-/Neustartverhalten, Verlust der Verbindung und Rückweg.
GPIO13 bleibt im Prüfpfad unberührt; die Funktion seiner externen
Beschaltung ist weiterhin unbekannt.
Weitere Einschränkungen nach dem Lauf konkret ergänzen. Keine vollständige
Hardware-, Lautstärke-, Dauerlauf- oder Spielintegrationsabnahme ableiten.

## Ergänzung: Pause/Fortsetzen und direkte Lautstärke

Pinzuordnung GPIO26/25/14 wurde vom Nutzer am 16.10.2026 bestätigt;
Messmittel und elektrische Messung bleiben gesonderte Nachweise.

| Versuch | App-/UART-Beleg | BUSY-Rohwert | Hörbeobachtung | Ergebnis |
|---|---|---|---|---|
| Pause an benannter Stelle, 3 s warten | offen | offen | Stille? | offen |
| Fortsetzen | offen | offen | Richtige Fortsetzungsposition? | offen |
| Lautstärke 5 → 8 → 3 → 5 jeweils übernehmen | offen | offen | Hörbarer Unterschied? | offen |
| Pause, Lautstärke ändern, Fortsetzen | offen | offen | Pause erhalten, neue Lautstärke? | offen |
| Stop, gleicher Titel mit Play | offen | offen | Neuer Anfang? | offen |
| Pause, anderen Titel wählen, Play | offen | offen | Neu ausgewählter Inhalt? | offen |

Die ursprüngliche 30-s-Frist bleibt während Pause/Resume/Regelung bestehen;
ein daraus resultierender Stop ist kein selbständiger Wiedergabefehler.
