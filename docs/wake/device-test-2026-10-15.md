# Wake-Inputs: Installation und gemeinsamer Gerätetest

Stand: 15. Oktober 2026. **Installation und gemeinsamer Funktionstest abgeschlossen;
weiterführende elektrische und Bedienprüfungen bleiben offen.** Auftrag: neue App und Firmware installieren,
anschließend vier Wake-Inputs und Kacheltausch gemeinsam testen.

## Installiertes Paar

Quellstand: `ca86b93d410fa4c79d43e718a7eedd30989c3398`,
[PR #46](https://github.com/Sven401/Larp-Escape/pull/46).
Der [Testplan](test-preparation.md) beschreibt Bedienung und elektrische Vorbehalte.

| Artefakt | SHA-256 |
|---|---|
| Installiertes Debug-APK, 22.961.512 Byte | `eb0f1751c3abc9c23450d0a31b9bcf0d302d2a08f60f9e16d9c6190dc7029318` |
| Firmware, 1.137.664 Byte | `316661b6daa41ff449ab944839dd18863f962f413ea1db0f35be2ba49c395378` |
| Vollständiger Flash-Rückweg vor Wake-Upload, 4 MiB | `b21c57303c1fd084c8fc65d215638070e8cc1e61bfb5427fda8552e007df91b8` |

Zuerst App auf dem moto g14 mit Datenerhalt aktualisiert; vorherige APK privat
gesichert. Die anschließend vom Handy zurückgelesene installierte APK stimmt
per SHA-256 exakt mit dem vorbereiteten Testartefakt überein.

Box vor Upload als derselbe ESP32-D0WD-V3, Revision 3, 4 MiB an COM3 identifiziert.
Chipzuordnung und Hash des bisherigen Rückwegs gegen das private Manifest der
[LED-Installation](../led/device-test-2026-10-15.md) abgeglichen. Neuer vollständiger
Flashabzug erstellt und am Chip mit `verify_flash` bestätigt. Die bisherige
Anwendung im Abzug stimmt mit dem dokumentierten LED-Stand überein; vorhandene
und vorbereitete Partitionstabelle sind bytegleich.

Nur die Anwendung ab `0x10000` geschrieben. Upload-Hashprüfung und nachfolgende
separate Verifikation der Anwendung erfolgreich. Die Schlüsselpartition
`0x310000`/`0x6000` stimmt anschließend weiterhin exakt mit dem Vorab-Abzug überein.
In einem 30-Sekunden-Startmitschnitt erschien genau eine Bereitmeldung und kein
beobachteter Panic-, Abort-, Brownout- oder eigener Fehler-Marker.

Die echte App hat die Box gefunden und sich mit dem erhaltenen Zugang gegenseitig
authentifiziert. Kein erneuter Schlüsselimport war nötig. Die App zeigt Live-Wake-
Status und LEDs aus. Diese Beobachtung ist kein Dauerlauf- oder SMP-Bondnachweis.

## Gemeinsame Beobachtungen

| Schritt | Geräte-/Appbeobachtung | Nutzerbeobachtung / Ergebnis |
|---|---|---|
| Ruhezustand | Frischer Status; Input 1, 2, 3, 4 jeweils Inaktiv / LOW, Maske 0 | Nutzer: „Ja, alle vier sind unbetätigt.“ |
| Realer Eingang oben links | Nur Input 2 Aktiv / HIGH; übrige LOW, frischer Status | Nutzer hält nur oben links; Zuordnung für diese Betätigung beobachtet |
| Realer Eingang oben rechts | Nur Input 1 Aktiv / HIGH; übrige LOW, frischer Status | Nutzer hält nur oben rechts; Zuordnung für diese Betätigung beobachtet |
| Realer Eingang unten links | Erste und spätere Momentaufnahme jeweils alle LOW bei Live | Nutzer bestätigt bei Wiederholung: Input 4 HIGH/LOW korrespondiert korrekt mit dem Statusindikator des realen Sensors unten links |
| Realer Eingang unten rechts | Momentaufnahme alle LOW bei Live | Nutzer bestätigt: Input 3 folgt dem Sensor unten rechts |
| Tauschvorschau | Screenshot: Input 2 angehoben, Input 1 oben rechts am Ausgangsplatz als „Vorschau“ | Nutzer bestätigt die Vorschau während des Haltens |
| Endgültiger Tausch | UI-Abzug: oben 2 / 1, unten 4 / 3 | Nutzer lässt Input 2 oben links und anschließend Input 4 unten links fallen; bestätigt Reihenfolge |
| Zwei Eingänge nach Umordnung | Drei aufeinanderfolgende Abzüge: Input 2 und 3 HIGH, Input 1 und 4 LOW | Nutzer hält oben links und unten rechts; bestätigt genau diese beiden aktiven Positionen |
| Alle sechs Platzpaare | Automatisierte Touchgesten am Handy; nach jedem Tausch erwartete Reihenfolge im UI bestätigt | Anschließend 2 / 1 / 4 / 3 wiederhergestellt |
| Abbruch außerhalb und in Rasterlücke | Jeweils unveränderte Reihenfolge 2 / 1 / 4 / 3 nach Loslassen | Automatisierter Touchtest |
| App-Neustart | Vor Verbindung viermal Unbekannt; nach erneuter Authentifizierung Live, Reihenfolge 2 / 1 / 4 / 3 | Gespeicherte Anordnung wiederhergestellt |
| Bewusstes Trennen/Wiederverbinden | Getrennt viermal Unbekannt; nach Anmeldung wieder Live und 2 / 1 / 4 / 3 | Abschließend verbunden, alle vier LOW |

Bei den beiden oberen realen Eingängen wurde jeweils HIGH bei Betätigung und
LOW nach Loslassen beobachtet. Das bestätigt noch keine elektrische
Vierkanalfunktion. Die Reaktion von Input 1 ist beobachtet; Chipidentität und
Leitungszuordnung Wake 1/B7 bleiben bis zur Hardwareprüfung vorläufig.
Für die gemeinsam beobachtete räumliche Zuordnung ist die Reihenfolge
oben links/rechts und unten links/rechts **2, 1, 4, 3**. Für die unteren Sensoren
ist der dynamische Gleichlauf durch den Nutzer bestätigt; die verzögerten
UI-Abzüge der Einzelprüfungen zeigen jeweils LOW und belegen allein keine HIGH-Phase.
Input 3 ist zusätzlich im späteren Zweikanaltest als HIGH erfasst. Aus der ersten
LOW-Momentaufnahme unten links folgt daher kein bestätigter Defekt.

## Umfang und verbleibende Prüfungen

Der begrenzte Funktionstest auf dieser Box ist erfolgreich: alle vier realen
Sensorpositionen sind zugeordnet, die gewünschte Vorschau und beide vom Nutzer
bedienten Tausche sind beobachtet. Zusätzlich sind alle sechs Platzpaare,
Abbruch außerhalb/in der Lücke, zwei gleichzeitige Eingänge, App-Neustart und
bewusstes Trennen/Wiederverbinden geprüft. Keine Codeänderung war dafür nötig;
installiert bleibt `ca86b93`.

Nicht geprüft: Chipmarking und reale B7-Leitung, elektrische Eignung aller Pins,
TalkBack, Rotation/große Systemschrift am Gerät, Ziele überspringen/zurückziehen
und systemseitiger Gestenabbruch, Livewechsel während einer Kachelbewegung,
gezielter Funk-/MCP-Fehler sowie Fristmessung am Gerät. Keine Impulserfassung
oder Dauerlaufabnahme. Die LED-Anzeige blieb bei den Kacheltests aus; ein
neuer physischer LED-Start/Stop-Test gehört nicht zu diesem Wake-Nachweis.

## Belege und Grenzen

Private Rohbelege liegen unter
`%LOCALAPPDATA%/LarpEscape/LabRuns/Wake-20261015-ca86b93`: Installationsmanifest,
vorherige und zurückgelesene APK, Flashabzug, Verifikations-/Upload-/Startlogs,
App-UI-Abzüge und getrennte Beobachtungen. Flashabzug und Schlüsselpartitionskopie
enthalten Zugangsdaten und bleiben außerhalb Git. Kein Merge oder Release.

Konkrete UI-Belege: `top-left-held.xml`, `top-right-held.xml`,
`bottom-left-repeat.xml`, `bottom-right.xml`, `top-swap-preview.png`,
`arranged.xml`, `two-active-repeat-1.xml` bis `-3.xml`, `touch-swap-*.xml`,
`touch-cancel-outside.xml`, `touch-cancel-gap.xml`, `restart-fresh.xml`,
`restart-connected.xml`, `disconnected.xml`, `final-connected.xml`.
`restart-start.xml` ist ausdrücklich kein Neustartbeleg: Der erste Dump scheiterte
während des Appstarts und der anschließend kopierte Abzug war noch der vorige.
Der frische Abzug `restart-fresh.xml` ersetzt ihn.
