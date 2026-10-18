# Näheglut mit echtem Funkwert · 18. Oktober 2026

Status: **Drei Laborläufe ausgewertet; ursprüngliche Boxinstallation vollständig
wiederhergestellt und verifiziert. Optische Abnahme bleibt eingeschränkt/offen.**
[Ergebnisbericht und Diagramm](../docs/espnow/naeheglut-result-2026-10-18.md),
[Prüfplan](../docs/espnow/naeheglut-test.md),
[Papier-Kamerakalibrierung](../docs/led/paper-camera-calibration-2026-10-18.md).

## Auftrag, geprüfter Stand und Ergebnis

Der Nutzer beauftragte Näheglut gegen denselben Powerbank-ESP wie beim früheren
Funkel-Gehversuch und vorher eine neue Kameraprüfung wegen weißem transparentem
Papier auf den LEDs. Der frühere Rollenauftrag sowie Merge #74/#75 bleiben
abgeschlossen. Der Zusatz ist ein Laborversuch ohne Produkt-Spieladapter.

Geprüfter Quellcommit: `e26bd93dc13ffd7aba0918df3c4c2c5246ef73a7`, Branch
`codex/naeheglut-espnow-test`, Basis `42d986ed353277d861df07ae59bd1c06345d3b52`.
Worktree `C:/Users/seide/.codex/worktrees/led-effekt-architekt/Larp Escape`.
Quell-/Binaryfingerabdrücke stehen zusätzlich in den privaten Belegen.
Die fremde `.vscode/extensions.json` im Hauptcheckout bleibt unangetastet.
PR-Ziel ist `plan/projektuebergabe-kuratierter-stand`; kein neuer Mergeauftrag.

- Separate Variante `larp-led-camera-rssi-lab` und Hostadapter
  `scripts/led_camera_rssi.py`. Originalfunkkern, Filter und Näheglut-Kern
  unverändert; normale GLOW-Aufträge koppeln reale Messungen an den Effekt.
- `p=clamp(20*(ganzzahliger Filter-RSSI+90),0,1000)`, Frische unter 3000 ms,
  Ausblenden bei Veraltung; gezielter Stop sperrt weitere automatische Updates.
- Softwareprüfung: 10 neue RSSI-Laborfälle, 9 Camera-Fälle, 7 NearGlow-Gruppen
  mit 2002 Werte-/Kurvenfällen sowie 14 neue RSSI- und 13 bestehende Hostfälle
  bestanden. Ausgaben im Sitzungsprotokoll; keine separate Testlogdatei behauptet.
  RF-Laborbuild erfolgreich, RAM 45336/Flash 710985 Byte.
- `walk01`: 160 gültige Funkresultate; verspätete Chat-Phasensteuerung,
  weitgehend obere Begrenzung und nur globaler Abschluss.
- `walk02`: 142 gültige Funkresultate, Filter −82…−34 dBm, p 160…1000;
  Hinweg und begonnene Rückkehr erfasst. Gezielter Stop trotz weiterer Antworten.
- `outage01`: 116 gültige und 26 Sendefehlerresultate. Nach 3016 ms ohne frischen
  Wert valid=0; neuer Peerboot mit Zähler 1 bringt die aktive Instanz zurück.
  Zwei gezielte Stops mit jeweils 12 weiteren gültigen Antworten ohne Auto-Update;
  dazwischen ausdrücklich Generation 2 gestartet.
- Alle 444 Funkresultate und 6686 Kamerabildhashes geprüft. Nominale Nachrechnung
  der TX-Ausgabe: 6730/6732 Frames exakt; zwei Übergangsframes im Ausfalllauf
  weichen um einen Rotcode ab. Tatsächliche Apply-/Render-Zeitpunkte fehlen;
  diese Zeitstempelgrenze ist eine mögliche Einordnung, keine bewiesene Ursache.
- Kamera-Dunkelkriterium nach gezieltem Stop: walk02 0/23, outage01 je 15/23
  Messflächen bestanden. Hintergrundabweichung bleibt sichtbar; keine optische
  Stopfreigabe aus digitalen Schwarzframes ableiten. Gold-Feinstufen und
  lückenloses Timing/Flimmern bleiben durch den Kalibrierbefund eingeschränkt.

## Geräte und abgeschlossener Rückweg

LED-Box `A8:42:E3:CD:4B:18` an COM3, Powerbank-Peer `A8:42:E3:CE:DE:D8`, Kanal 1.
Peerfirmware unverändert. Die vor dem Versuch frisch am selben Chip verifizierte
4-MiB-Sicherung hat SHA-256
`b703f4b13aaaaa3e8c3bc68d8e8134b1cec0a77df957ea06175ac147d1d03c71`.
Temporäres Laborbinary:
`8c8ee844b9f7f18bfe20846f0a07946352677fdae27a998eab4e306f7fc0d27a`.

Nach allen Aufnahmen wurde die Original-App zurückgeschrieben. `restore.json`
bestätigt Appverifikation, unveränderte Boot-/Schlüsselbereiche, Verifikation
des vollständigen Flashs gegen die Sicherung und beobachteten Originalboot.
**Keine Laborfirmware mehr installiert; kein Aufnahmeprozess offen.**

Private Rohbelege: `C:/Dev/Larp Escape/private/naeheglut-espnow-20261018/`.
Enthalten sind chipgebundene Sicherungs-/Restorebelege, Quellmanifest/Binary/ELF,
Papierkalibrierung und Goldreferenz, drei Laufordner mit Aufträgen/Funk/TX/Kamera,
`walk-analysis.json`, CSVs/Manifeste und editierbare Analyse-/Ablaufquellen.
Öffentliche Diagrammdaten und Plotquelle liegen unter
`docs/espnow/probes/naeheglut/`; Rohbilder und Flashmaterial bleiben privat.

## Offene Punkte und konkrete Fortsetzung

1. Nutzerurteil zu Ausblenden, Wiederkehr, Restlicht, Sprüngen und Flackern wurde
   erfragt und ist noch offen. Nicht als optische Zustimmung auslegen.
2. Vollständige kontinuierliche Rückkehr bis zum Nahplateau ist im begrenzten
   Gehfenster nicht belegt. Eine eventuelle Folgeprüfung benötigt einen zum
   tatsächlichen Weg passenden Ablauf; Chatantwortzeiten sind kein Messtakt.
3. Für quantitative optische Abschaltung die veränderte Schwarzbasis untersuchen;
   Umgebung, Schatten oder Kameraautomatik sind bislang nur mögliche Ursachen.
   Vorab festgelegte Masken/Schwellen nicht nachträglich passend machen.
4. Zwei ±1-Rotabweichungen brauchen bei Bedarf einen separaten Nachweis genauer
   Apply-/Render-Zeitpunkte. Keine nachträgliche Zeitverschiebung zur Ergebnisrettung.
5. Produktadapter, echte Spielregeln, kalibrierte Meterentfernung, Reichweite,
   Dauerbetrieb und Gesamtspiel bleiben außerhalb dieses Zusatzversuchs.

Die Testanbindung und Ergebnisbelege liegen in
[PR #76](https://github.com/Sven401/Larp-Escape/pull/76) gegen den kuratierten
Integrationsbranch vor. Aktuelle automatische Prüfungen am PR nachsehen; kein Merge
vorgenommen. Spätere Nutzerbeobachtungen an diesem Ergebnis ergänzen. Ein weiterer
Upload oder Merge benötigt seinen passenden konkreten Auftrag.
