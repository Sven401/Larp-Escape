# Kristallsiegel im bestehenden LED-Labor

Stand: **20. Oktober 2026**. Gemeinsamer ausführbarer C++11-Effektkern und
USB-Laboradapter. Der [Entwurf](../../docs/led/kristallsiegel-design.md) liefert
die sichtbare Absicht und festen Sollpunkte. [CrystalSeal.cpp](src/CrystalSeal.cpp)
wird sowohl von den Hosttests als auch von der Laborfirmware verwendet.
Das feste Spiel, Profile und BLE-V2 rufen den neuen Kern noch nicht auf.

## Kern und Berechnung

Eine Instanz mit 23 unabhängig zeitgesteuerten Pixelrampen, aufgeteilt in
8/7/8 LEDs. Keine Heap-Allokation, Hardwarezugriffe oder eigene Uhr im Kern.
Der Besitzer übernimmt vollständige Wertkopien und liefert lokale
`uint32`-Millisekunden; weniger als 2^31 ms zwischen Serviceaufrufen.
Abgeschlossene Rampen und Pulse verlieren ihren Zeitanker. Ausgelassene Frames
verlängern den Ablauf nicht und spielen keinen bereits beendeten Puls nach.

Default: Grundfarbe `(0,8,12)`, Pulsmaximum `(4,18,24)`, Abschnittsaufbau
600 ms, einzelne LED-Rampe 150 ms, Snapshot/Stop 300 ms, Puls 800 ms.
Q=1.000.000, Smoothstep und Mischung wie im Entwurf; Zwischenprodukte uint64,
erst beim Rendern auf RGB8 reduzieren. Die neue konfigurierbare Obergrenze
5000 ms ergibt im Smoothstep höchstens 1,25e17 und in der Mischung höchstens
3,2e13, jeweils innerhalb uint64. Das ist keine photometrische Kalibrierung.

| Parameter | Bereich |
| --- | --- |
| count / valid / cause | 0–3 / 0 oder 1 / 0 Snapshot, 1 Fortschritt |
| colour und peak | Je RGB-Kanal 0–32 |
| sweepMs | pixelMs bis 5000 |
| pixelMs | 50–1000 |
| transitionMs | 50–5000 |
| pulseMs | Gerade Zahl 200–5000; zwei gleich lange Hälften |

Ein +1-Fortschritt startet nur den neuen Abschnitt. Verzögerung der LED j:
`floor((sweepMs-pixelMs)*j/(n-1))`. Start, Wiederherstellung, Korrektur und
Farb-/Zeitänderung übernehmen das aktuelle Bild und blenden zum Snapshot.
Ein erstmaliger Wechsel von Fortschritt zu Snapshot tut dies auch bei gleichem
count und beendet einen vorgemerkten/laufenden Puls. Wiederholte identische
Updates verlängern nichts. Während `valid=false` bereits ausgeblendet wird,
ändern neue Farb-/Zeitparameter nur künftige Übergänge; die laufende
Invalid-Ausblendung behält ihre Zeitbasis. Gültige Wiederkehr ist ein Snapshot.

Stop übernimmt einschließlich Puls die aktuellen internen Werte und blendet
aus. Stop muss die bestätigten Parameter wiederholen; Updates während Stop
werden abgewiesen. Wiederholstop verlängert nicht. Eine dunkle aktive Instanz
und eine beendete Instanz bleiben unterscheidbar.

## Runde und einmaliger Puls

Aufträge tragen `round`, `generation`, `sequence`, Operation, vollständige
Parameter und zwei getrennte Pulsfelder. Alle Identitäten sind positiv.
Eine höhere Runde darf nur mit Start beginnen und entfernt den alten eigenen
Beitrag sofort. Ein Start derselben Runde braucht eine höhere Generation und
eine beendete Instanz. Alte Runden/Generationen bleiben gesperrt.

`inheritedConsumed` wird nur bei Start akzeptiert und mit dem bereits bekannten
Verbrauch derselben Runde verknüpft. Ein Start kann den Verbrauch nicht löschen.
`authorizePulse` ist ausschließlich bei Update zulässig und wird nicht für
Folgeaufträge vererbt. Es erlaubt den Puls nur bei einem gültigen Live-Schritt
2→3 ohne gleichzeitige Konfigurationsänderung und ohne bisherigen Verbrauch.
Jedes erste gültige 3/3 verbraucht die Berechtigung, auch ein Snapshot ohne Puls.
Stop/Invalid/Korrektur brechen ab. 3→2→3 und Stop/Neustart derselben Runde
feiern nicht erneut. Der Puls beginnt nach dem letzten noch laufenden Aufbau.

`Scene::reset()` entfernt den Beitrag, behält jedoch Runde/Generation/Sequenz
und Pulsverbrauch als Sperrvermerk. Ein neuer Laborlauf konstruiert eine neue
Scene und wird durch einen neuen LIVE-Token abgegrenzt. Ein künftiger
Produktadapter muss die übergeordnete Besitzerepoche, Profilbarriere und
gegebenenfalls wiederhergestellten Pulsverbrauch selbst liefern; der Kern
implementiert weder dauerhafte Speicherung noch das lokale Spiel.

## Laborprotokoll

Bestehender [LARP-LIVE/1-Vertrag](LIVE-NEARGLOW.md), 192-Byte-Zeilenpuffer,
KEEP-Wächter, maximale Laufzeit und STOP/Schwarz/DONE bleiben erhalten.
HELLO meldet zusätzlich `LARP-SEAL/1 1 23 32`. Der Laborbesitzer mischt einen
Kristallsiegelbeitrag mit den beiden bestehenden Näheglut-Slots und Hintergrund:
breite Addition, Begrenzung auf 32. Es gibt nur einen RMT-Ausgabebesitzer.

```text
SEAL token req round generation start|update|stop count valid cause inheritedConsumed authorizePulse r g b peakR peakG peakB sweepMs pixelMs transitionMs pulseMs
SEALSTATE token revision deviceMs state round generation sequence count valid cause consumed pending r g b peakR peakG peakB sweepMs pixelMs transitionMs pulseMs
```

SEAL hat 21 Felder, SEALSTATE 23. Boolesche Felder sind 0/1, Dezimalfelder
erlauben nur Ziffern. req/round/generation liegen in 1…uint32_max. `state`:
0 leer, 1 aktiv, 2 ausblendend, 3 beendet. `pending` umfasst vorgemerkten und
laufenden Puls. Die inaktive Instanz meldet gültige Defaultparameter sowie
Identitäten null. Der Controller prüft Grenzen vor jeder schmalen Typwandlung.

SEAL, GLOW und BG teilen die steigende req-Folge und eine gemeinsame Revision.
Nur der letzte angenommene Auftrag wird für exakte Wiederholung gespeichert;
gleiche req mit anderen Daten ist Konflikt, ältere req ist veraltet.
Ablehnung verbraucht keine req. APPLIED ist weiterhin Parameterübernahme;
die spätere L-Zeile bestätigt erst den abgeschlossenen Frame-Transfer.
Token, Revision und Pixelwerte werden gemeinsam beim Submit eingefroren.

Nach LIVE, APPLIED, DUPLICATE und STATUS folgen der unverändert 29-feldrige
STATE sowie SEALSTATE derselben Revision. Alte Hostclients können die Zusatzzeile
ignorieren. Neue Seal-Aufträge gelten hostseitig erst mit passendem SEALSTATE
als vollständig bestätigt; STATE allein genügt nicht. Globale STOP-Ausgabe
enthält beide Zustände und verlangt einen neuen bestätigten Schwarzframe.
Gezielter `SEAL … stop` lässt Hintergrund und Näheglut bestehen.

## Interaktiver Lichtversuch

Die [vorhandene Anleitung](../../docs/led/interactive-development.md) gilt mit
dem zusätzlichen Hostargument `--seal`. Dieses verlangt die exakte Fähigkeit
vor LIVE. Seal-Befehle ohne passende Fähigkeit werden nicht gesendet. Ein
Geräteupload ist ein eigener Auftrag; der chipgebundene Flash-Rückweg bleibt
Voraussetzung. Dieser Quellstand lädt nichts automatisch auf ein Gerät.

Die nachstehenden Objekte jeweils als vollständige Zeile an die laufende
JSONL-Befehlsdatei anhängen. `active.json` enthält die bestätigten Zustände;
`latest.png` zeigt die unabhängige Kameraaufnahme. Die ursprünglichen
Serienbilder und das Abschlussmanifest bleiben die Belege.

```jsonl
{"op":"seal-start","round":1,"gen":1,"count":0,"valid":1,"cause":0,"consumed":0,"label":"Neue Runde dunkel"}
{"op":"seal-update","count":1,"cause":1,"label":"Erster Kristall"}
{"op":"seal-update","count":2,"cause":1,"label":"Zweiter Kristall"}
{"op":"seal-update","count":3,"cause":1,"authorize_pulse":1,"label":"Dritter Kristall mit einmaligem Puls"}
{"op":"seal-update","rgb":[0,10,16],"label":"Ruhefarbe etwas heller pruefen"}
{"op":"seal-update","valid":0,"label":"Quelle voruebergehend ungueltig"}
{"op":"seal-update","valid":1,"label":"Stand ohne neue Feier wiederherstellen"}
{"op":"seal-stop","label":"Nur Kristallsiegel ausblenden"}
{"op":"finish","label":"Gesamtaus und Aufnahme abschliessen"}
```

Start verlangt round/gen/count/valid/cause/consumed ausdrücklich. RGB-, Peak-
und Zeitparameter haben die oben genannten Defaults. Update/Stop erben nur
bestätigte SEALSTATE-Parameter. `authorize_pulse` ist ohne explizite Angabe 0;
`consumed` ist nur Startinput, nicht die Freigabe für einen neuen Puls.
Stop darf nicht gleichzeitig umparametrieren. Für eine neue Runde ausdrücklich
`seal-start` mit höherer round senden. Sie startet aus dem angegebenen Snapshot.

Erster Sichtlauf: Einzeleffekt vor dunklem Hintergrund; normale 0→1→2→3-Folge,
schnelle Folge, Stop während Aufbau/Puls, gültige Wiederkehr und neuer Rundensnapshot.
Farbe, Lesbarkeit der Abschnitte, Bewegung und Ruhephase beurteilen. Erst danach
gezielt mit Näheglut/Funkeln vergleichen. RGB-Summen allein belegen nicht, dass
die Fortschrittsanzeige unter anderen Beiträgen noch ablesbar ist.

## Softwareprüfung und Grenzen

```powershell
python firmware/led/tests/run_camera_tests.py
python -m unittest discover -s scripts/tests -p 'test_led_camera*.py'
python -m platformio run -d firmware/led -e larp-led-camera-lab -e larp-led-camera-rssi-lab -e larp-led-product -j 2
```

Vorbereitete Interpreter/Abhängigkeiten gemäß Projektprüfwegen verwenden.
Der C++-Runner prüft Kern, Parser, echten Laborloop mit asynchronem FakeBus,
gemeinsame Revisionen, Komposition, gezielten Stop, Watchdog und einen hängenden
Transfer beim Gesamtstop. Die bestehenden Näheglut-/Kalibrier-/RSSI-Fälle bleiben
Teil desselben Laufs. Python prüft Hostkodierung, Capability und Antwortzuordnung.
Der tatsächliche Ergebnisstand wird im PR am veröffentlichten Commit geführt.

**Lokal ausgeführt am 20.10.2026:**

| Prüfung | Ergebnis |
| --- | --- |
| Kristallsiegel-Kern | 11 unabhängige Gruppen bestanden; feste Sollpunkte, Grenzen, Unterbrechungen, Runden-/Generationswechsel und Zeitüberlauf |
| Gemeinsamer C++-Laborrunner | 12 Laborloopfälle, 7 Näheglut-Gruppen einschließlich 2002 Pegel-/Kurvenfällen sowie 10 RSSI-Fälle bestanden |
| Kamera-Hosttests | 42 bestanden, darunter 21 Interactive-Fälle mit fehlender Capability und unvollständiger/falsch zugeordneter Antwort |
| ESP32-Builds | Produkt, Kameralabor und Kameralabor mit RSSI erfolgreich; Frameworkwarnung in unverändertem `esp32-hal-uart.c` |
| Prüfung der Quellen | C++11 mit Warnungen als Fehler; zusätzliche unabhängige Kern-/Controllerprüfung, Hostaufruf und Dokumentlinks erfolgreich |

Das erzeugte Kameralabor-Binary umfasst 287328 Bytes, SHA-256
`577d0b5a97e28f5a2c267aee0daf5a7bf34169a139b8baafbbafc27cd8c20ea3`.
Es liegt lokal unter `.pio/led-product/larp-led-camera-lab/firmware.bin` zusammen
mit dem ELF. Ein erneuter Build kann einen anderen Binaryhash erzeugen;
vor jedem späteren Versuch die tatsächlich verwendete Datei erneut zuordnen.

Ein erfolgreicher Softwarelauf ist keine Geräteübertragung. Frühes Kamerafeedback,
optische Abnahme, Spielbindung, Bedienung in Dev/GM und elektrischer Nachweis
bleiben eigene Schritte. Es wurde keine neue universelle Misch- oder Spielregel
eingeführt.
