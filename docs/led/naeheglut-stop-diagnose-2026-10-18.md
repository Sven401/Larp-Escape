# Näheglut: Stop-/Dunkelbefund

Stand 18.10.2026. **Bestätigt: digitaler Stop; optische Ursache eingegrenzt,
aber nicht bewiesen. Keine optische Freigabe.** Auftrag: Stopbefund klären und
danach den Spielanschluss konkretisieren. Geprüfte Repositorybasis:
`7e5f4f38e88ed76358e31dc66d267da9cb0fbc1f`; historische Aufnahmequelle `e26bd93`.

## Ergebnis und Konsequenz

Ein weiterlaufender Näheglut-Beitrag ist in den aufgezeichneten Stopfenstern
nicht nachweisbar. Alle **386 protokollierten abgeschlossenen Transfers** dieser
Fenster enthalten 23 schwarze Pixel. Die ursprüngliche Auswertung und ihre
negativen optischen Kriterien sind unabhängig aus den Bildern reproduziert.
Ein erfolgreicher RMT-Abschluss belegt die Ausgabe am Treiber, nicht den
Lichtzustand oder fehlerfreie elektrische Übertragung bis zu jedem LED-Chip.

Der Pegelanstieg betrifft auch benachbarte Papierflächen außerhalb der festen
CAL-Masken. Er ist daher **nicht auf deren LED-Kernpixel begrenzt**. Das stützt
einen flächigen Einfluss von Beleuchtung, Reflexion, Schatten oder Kamera.
Welche dieser Ursachen vorliegt, bleibt offen. Streulicht durch das Papier
kann auch LED-Licht flächig verteilen; der Randvergleich schließt reales
Restlicht ausdrücklich nicht aus. Keine Firmwarekorrektur auf Verdacht.

## Erneute Auswertung

[Auswerter](probes/stop-audit/audit.py) und [Zahlen](probes/stop-audit/result.json).
650 ausgewählte Originalbilder erneut gehasht und dekodiert: 266 aus `walk02`,
384 aus `outage01`, einschließlich der jeweiligen Vorstartbasis. Die
Capture-/Kameramanifeste und ursprünglichen Auswertungen sind durch ihre Hashes
gebunden. CAL-/ROI-Zuordnung sowie Originalbasis, Bildauswahl und 5-%-Goldgrenze
bleiben unverändert. Rechenwerte stimmen mit der ursprünglichen Auswertung
bis auf numerische Rechengenauigkeit überein.

| Stopfenster | Bilder / schwarze TX-Ereignisse im selben Hostfenster | ursprüngliches Dunkelkriterium | mittlerer RGB-Anstieg CAL-Maske | mittlerer RGB-Anstieg Papierrand |
| --- | --- | --- | --- | --- |
| walk02 / 65 | 129 / 129 | 0/23 bestanden | (8,657; 14,662; 0,433) | (8,393; 14,428; 0,462) |
| outage01 / 5 | 116 / 128 | 15/23 bestanden | (2,206; 3,814; 0,089) | (2,248; 3,851; 0,111) |
| outage01 / 8 | 129 / 129 | 15/23 bestanden | (2,670; 4,327; 0,127) | (2,725; 4,362; 0,148) |

Alle Zahlen sind relative Kameracodes, über 23 Bereiche gemittelt; keine Luxwerte.
Unterschiedliche Bild-/TX-Zahlen sind bei unabhängigen Empfangszeiten zulässig.
Es wird keine einzelne Belichtung einem bestimmten Transfer gleichgesetzt.

Der explorative Papierrand umfasst 70–90 % des bereits gespeicherten
Aufnahmeradius, abzüglich sämtlicher CAL-Maskenpixel. Kein schwarzes Padding.
Die räumlichen Korrelationen der 23 mittleren Pegelanstiege betragen
0,993824 / 0,978683 / 0,987669. Das ist eine beschreibende Diagnose, kein neuer
Abnahmeschwellwert. Die Randflächen liegen im selben optischen System und
sind **keine unabhängige Dunkelkontrolle**. Ihre Werte werden nicht von den
CAL-Werten abgezogen, um ein Bestehen herzustellen.

Im früheren separaten Goldreferenzlauf bestanden p=0 und Stop jeweils 23/23
unter demselben Kriterium. Das zeigt einen funktionierenden begrenzten
Vergleichsfall; es erklärt den späteren Aufbau nicht. Die Kamera meldete
Auto-Exposure weiterhin als −1 zurück. Aus dem angenommenen Einstellauftrag
folgt deshalb keine gesicherte konstante Belichtung.

## Code- und Rechnerprüfung

`NearGlow.cpp`: Stop setzt das Ziel der betroffenen Instanz auf null, Updates
während/nach Stop werden abgewiesen; nach Rampenende wird sie `Stopped`.
`CameraLabMain.cpp`: nur der serialisierte Besitzer rendert; ein protokolliertes
L-Ereignis entsteht erst nach `TxResult::Complete`. Der RMT-Treiber enthält
einen expliziten LOW-Latch. Dies ist Quellprüfung, keine elektrische Messung.

Frisch ausgeführt auf obiger Basis: `firmware/led/tests/run_camera_tests.py`:
9 Camera-Fälle einschließlich Stop-in-flight und Stop-Timeout, 7 NearGlow-Gruppen
mit 2002 Werte-/Kurvenfällen sowie 10 RSSI-Laborfälle bestanden. Der erste
Compilerstart scheiterte in der Windows-Sandbox; derselbe Prüfweg außerhalb
der Sandbox bestand vollständig. Kein Produktquellcode geändert.

Die zwei bereits dokumentierten ±1-Rotabweichungen liegen in Übergangsframes.
Sie erklären keine späteren dauerhaft schwarzen TX-Frames und werden durch
diese Untersuchung weder behoben noch nachträglich als exakt eingestuft.

## Noch fehlender Ursachenbeleg: gezielte Kontrollmessung

Die vorhandene Aufnahme enthält keine unabhängig vor Licht geschützte
Kontrollfläche und keinen unabhängigen Nachweis konstanter Belichtung.
Eine abschließende physische Ursache lässt sich daraus nicht zurückrechnen.
Der folgende vorab festgelegte Ablauf ist der kleinste passende Folgeversuch:

1. Kamera/Papier/Box fixieren, Raumlicht und Personenposition konstant halten.
   Geschützte Dunkelfläche und eine LED-unabhängig beleuchtete Referenz neben
   der Box im **vollen Kamerabild** mitaufzeichnen; bisherige Kacheln reichen
   für diese zusätzlichen Kontrollen nicht. Automatikzustand separat prüfen.
2. Mit unveränderten Masken und gültiger Goldreferenz drei Folgen aufnehmen:
   5 s Schwarz → 5 s Gold → gezielter Stop → 5 s Schwarz → explizites
   Gesamt-Schwarz → 5 s halten. Bewusster Neustart nur zwischen Folgen.
   Der Auswerter verwendet weiter die ursprüngliche 5-%-Grenze, ab 1500 ms
   nach Stop-ACK und bis 200 ms vor dem nächsten Auftrag. Referenzstabilität
   und zeitliche Qualität vor dem Lauf anhand der neuen Kontrollaufnahme
   festlegen; ungeeignete Referenz verwirft den Lauf statt Grenzen zu lockern.
3. Beobachtung notieren: sichtbare LED-Kerne, Papieraufhellung, Einfluss von
   Personenbewegung und Unterschied gezielter Stop/Gesamt-Schwarz. Den gleichen
   Aufbau anschließend bei fachgerecht abgeschalteter LED-Versorgung prüfen,
   sofern das ohne Rückspeisung und gemäß Hardwarevertrag möglich ist.
   Dafür ist eine physische Handlung erforderlich; kein automatisches Schalten
   oder Umverdrahten ist Teil dieser Untersuchung.
4. Bleibt der Anstieg ohne LED-Versorgung, ist LED-Emission als dessen Ursache
   in diesem Kontrollfall ausgeschlossen. Bleibt die Referenz stabil, aber
   reales LED-Restlicht nur bei versorgten LEDs bestehen, Daten-/Latchsignal
   und Versorgung elektrisch prüfen. Bei Referenzänderung optischen Aufbau
   korrigieren und unveränderte Kriterien erneut prüfen.

Dieser Kontrolllauf wurde **nicht durchgeführt**. Der aktuelle Befund rechtfertigt
die Ausarbeitung des [Spielanschlusses](naeheglut-spielanschluss.md), aber keine
optische Produktfreigabe. Geräteinstallation wurde nicht verändert.

Reproduktion (OpenCV/NumPy wie im vorhandenen Kameralabor):

```powershell
python docs/led/probes/stop-audit/audit.py --evidence <privater-Belegordner> --output <neue-Ergebnisdatei.json>
```

Der Auswerter überschreibt keine Ergebnisdatei. Ursprung und Hashes:
[Versuchsbericht](../espnow/naeheglut-result-2026-10-18.md) und
[Papierkalibrierung](paper-camera-calibration-2026-10-18.md).
