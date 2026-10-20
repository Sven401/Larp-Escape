# LED-Effekte interaktiv mit Kamera entwickeln

**Kristallsiegel · 20.10.2026:** Derselbe Host unterstützt mit `--seal` den
[gemeinsamen Kristallsiegel-Kern und seine Liveparameter](../../firmware/led/LIVE-CRYSTALSEAL.md).
`seal-start`, `seal-update` und `seal-stop` nutzen zusätzliche bestätigte
SEALSTATE-Antworten; die folgenden Näheglut-Befehle bleiben erhalten.

Stand: **18. Oktober 2026**. Anleitung für die implementierte Hostroute und
`LARP-LIVE/1`, abgeglichen mit [Hostwerkzeug](../../scripts/led_camera_interactive.py)
und [Laborvertrag](../../firmware/led/LIVE-NEARGLOW.md). Diese Anleitung behauptet
keinen bereits ausgeführten Geräteversuch. Beobachtungen und Änderungen gehören
in den [gesonderten Näheglut-Versuchsbericht](naeheglut-live-results-2026-10-18.md).

Ziel ist ein kurzer Kreislauf: einen kleinen Effekt zeigen, das aktuelle Bild
ansehen, **danach** einen Parameter ändern und erneut vergleichen. Eine Session
hält den seriellen Port offen; ein unabhängiger Prozess erfasst die Kamera.
Die JSONL-Befehlsdatei kann während derselben Aufnahme ergänzt werden. Farbe,
Gain, Nähewert, Gültigkeit, Dauer und Kurve benötigen dafür keinen neuen Build.
Algorithmusänderungen am gemeinsamen [C++-Kern](../../firmware/led/src/NearGlow.cpp)
benötigen weiterhin Prüfung, Build und einen beauftragten Upload.

## Voraussetzungen und Zuständigkeit

- Bekannte, geprüfte Box: 23 LEDs an GPIO15, maximal 32 je Kanal; Chipidentität,
  aktueller Flashzustand und chipgebundener Rückweg müssen abgeglichen sein.
  Installation und Wiederherstellung folgen dem [bestehenden Kamera-Prüfweg](camera-measurement.md).
  Alte Sicherungen oder frühere erfolgreiche Läufe bestätigen den heutigen Chip
  nicht. Private vollständige Flashabbilder und Zugangsdaten bleiben außerhalb Git.
- Genau ein Besitzer öffnet Port und Kamera. Andere serielle Monitore und
  Kameraanwendungen vorher schließen; während des Versuchs keine Builds,
  Regressionen oder weiteren Kamerazugriffe parallel ausführen.
- Windows/DirectShow, Python mit den Paketen aus
  [requirements-led-camera.txt](../../scripts/requirements-led-camera.txt).
  Einen dafür vorbereiteten Python-Interpreter verwenden. Das Skript installiert
  nichts, lädt keine Firmware hoch und verbindet nach einem Fehler nicht erneut.
- Kamera und Ring fixieren. Lichtflächen müssen erkennbar sein. Für erstes
  gestalterisches Feedback genügen Gesamtbilder. Ein vorhandenes Regionslayout
  darf nur bei unveränderter Kamera, Auflösung, Position und Optik verwendet werden.

Den Laborbuild bei Bedarf **vor** dem Geräteversuch mit der vorhandenen
PlatformIO-Umgebung erstellen:

```powershell
# Aus dem Repository-Stamm; Interpreter passend zur lokalen Umgebung auswählen.
$repo = (Get-Location).Path
$buildPython = (Get-Command python).Source
& $buildPython -m platformio run -d (Join-Path $repo 'firmware/led') -e larp-led-camera-lab -j 2
```

Das erzeugt `.pio/led-product/larp-led-camera-lab/firmware.bin`. Build allein
installiert nichts. Erst nach der getrennt ausgeführten, abgesicherten Installation
mit der tatsächlich installierten Binärdatei fortfahren. Der Host verlangt deren
exakte Größe und MD5 aus `HELLO`, protokolliert zusätzlich SHA-256 und prüft
`LARP-LIVE/1 2 23 32`. Ein anderer Sketch wird nicht gestartet. Das ersetzt nicht
den vorherigen Chip- und Rückwegabgleich.

## Eine Session öffnen

Im ersten PowerShell-Terminal, aus dem Repository-Stamm:

```powershell
$repo = (Get-Location).Path
$cameraPython = (Get-Command python).Source # vorbereitete Kamera-Umgebung wählen
$port = Read-Host 'Bereits geprüfter serieller Port der bekannten Box'
$firmware = Join-Path $repo '.pio/led-product/larp-led-camera-lab/firmware.bin'
$privateRoot = Join-Path $repo 'private/led-live'
$runId = 'trial-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
New-Item -ItemType Directory -Path $privateRoot -Force | Out-Null
$commands = Join-Path $privateRoot ($runId + '.jsonl')
$output = Join-Path $privateRoot $runId
New-Item -ItemType File -Path $commands -ErrorAction Stop | Out-Null
Write-Output "Befehlsdatei: $commands"
Write-Output "Aufnahme: $output"
& $cameraPython (Join-Path $repo 'scripts/led_camera_interactive.py') `
    --port $port --firmware $firmware --commands $commands --output $output `
    --seconds 60 --camera 0 --width 640 --height 480 --exposure -7
```

Der Ausgabeordner darf noch nicht existieren. Auf `"status": "live"` warten.
Bis dahin werden Identität, anfängliches STOP/DONE, Kameraöffnung und Aufwärmung
abgeschlossen. LIVE beginnt schwarz. Die Kamera wird fortlaufend gelesen;
`latest.png` wird ungefähr alle 250 ms erneuert. Der Aufnahmeordner und die
Befehlsdatei bleiben nach dem Versuch erhalten.

Alle Argumente des Skripts:

| Argument | Bedeutung |
| --- | --- |
| `--port`, `--firmware`, `--commands`, `--output` | Pflicht; geprüfter Port, installierte Binärdatei, ergänzbare JSONL-Datei, neuer Aufnahmeordner |
| `--seconds` | Sessiondauer ab LIVE, 1–80 s; Standard 60 s |
| `--camera` | Kameraindex; Standard 0, nach Umstecken neu zuordnen |
| `--width`, `--height` | Angeforderte Auflösung; Standard 640×480 |
| `--exposure` | Angeforderter DirectShow-Wert; Standard −7 |
| `--rois` | Optionaler Pfad zu einem passenden vorhandenen Aufnahme-Regionslayout |

Angeforderte und zurückgelesene Kameraeinstellungen werden gespeichert. Eine
Treiberbestätigung beweist keine tatsächliche feste Belichtung; insbesondere
unverfügbare Getter nicht als gültigen Automatikzustand deuten.

## Während der Aufnahme beobachten und ändern

In einem zweiten Terminal denselben Befehlsdateipfad setzen:

```powershell
$commands = Read-Host 'Befehlsdateipfad aus dem ersten Terminal'
$output = Join-Path (Split-Path -Parent $commands) ([IO.Path]::GetFileNameWithoutExtension($commands))
```

**1. Einen kleinen Ausgangsentwurf starten.** Eine vollständige JSON-Zeile
anhängen; `valid` ist die Zahl 0 oder 1, kein JSON-Bool:

```powershell
'{"op":"start","slot":0,"gen":1,"p":1000,"valid":1,"rgb":[24,12,2],"gain":1000,"duration_ms":1000,"curve":0,"label":"Ausgangsidee Gold linear"}' |
    Add-Content -LiteralPath $commands -Encoding utf8
```

`active.json` zeigt den von der Firmware bestätigten Zustand. `latest.png` öffnen
und nach dem Übergang erneut ansehen. Notieren: Ist die Helligkeitsänderung
verständlich, wirkt die Farbe warmgolden, ist der Übergang ruhig, sind dunkle
Stufen sichtbar? Beobachtung und Bildzeit festhalten, bevor Werte verändert werden.

**2. Genau eine begründete Variante ausprobieren.** Beispiel für eine Farbänderung:

```powershell
'{"op":"update","slot":0,"rgb":[24,18,4],"label":"Mehr Gruen und Blau fuer waermeres Gelb pruefen"}' |
    Add-Content -LiteralPath $commands -Encoding utf8
```

Das Beispiel ist ein Prüfwert, keine zugesagte Farbverbesserung. Der Host ergänzt
fehlende Werte ausschließlich aus dem letzten bestätigten `STATE`; ein Update
vor bestätigtem Start wird nicht erraten. Er sendet stets einen vollständigen
Auftrag. Nach `APPLIED` das erneuerte Bild ansehen, Wirkung mit der Ausgangsidee
vergleichen und die nächste Änderung daraus ableiten.

**3. Verlauf und Eingangsverhalten getrennt ansehen.** Jede folgende Zeile ist
für einen eigenen Beobachtungsschritt gedacht:

```jsonl
{"op":"update","slot":0,"p":500,"label":"Halbe Naehe"}
{"op":"update","slot":0,"curve":1,"duration_ms":1500,"p":1000,"label":"Smoothstep vergleichen"}
{"op":"update","slot":0,"valid":0,"label":"Messwert fehlt"}
{"op":"update","slot":0,"valid":1,"label":"Messwert wieder gueltig"}
```

`curve=0` ist linear, `curve=1` Smoothstep. `p` und `gain` liegen in 0–1000,
RGB jeweils in 0–32, `duration_ms` in 50–5000. Farbe, Gain, Dauer und Kurve
werden ab dem aktuellen Beitrag übernommen. Wiederholte identische Ziele
starten den Übergang nicht neu. Kontinuierlich wechselnde Ziele erreichen ihr
letztes Ziel erst nach dessen anschließender voller Übergangsdauer.

**4. Überlagerung oder gezielten Stop prüfen.** Für eine zweite Instanz steht
`slot=1` mit eigenem vollständigem Start zur Verfügung. Ein Hintergrund ist
sofort wirksam und lässt sich beispielsweise mit folgender Zeile einschalten:

```powershell
'{"op":"background","rgb":[4,4,4],"label":"Hintergrund"}' | Add-Content -LiteralPath $commands -Encoding utf8
'{"op":"stop","slot":0,"label":"Nur Naeheglut ausblenden"}' | Add-Content -LiteralPath $commands -Encoding utf8
```

Stop wiederholt intern die zuletzt bestätigten vollständigen Parameter und
verwendet deren Dauer/Kurve. Er ist kein gleichzeitiges Umkonfigurieren. Nach dem
Ausblenden gegebenenfalls `{"op":"status"}` anhängen; Zustand 3 bedeutet beendet.
Ein neuer vollständiger Start desselben Slots braucht eine höhere `gen`.
Hintergrund und zweiter Beitrag bleiben bei einem gezielten Stop erhalten.

**5. Versuch ausdrücklich beenden:**

```powershell
'{"op":"finish","label":"Variante beobachtet"}' | Add-Content -LiteralPath $commands -Encoding utf8
```

Der Host sendet globales STOP, verlangt einen **neuen** schwarzen `L`-Frame und
`DONE` für den Sessiontoken und erfasst noch ungefähr 0,5 s Kameranachlauf.
`final_black_confirmed` bezeichnet bestätigte übertragene schwarze Kanalwerte;
das Kamerabild bleibt ein separater optischer Nachweis. Fehlen Schwarz oder DONE,
ist der Abschluss unbestätigt und die Aufnahme `incomplete`. Beim Ende noch
ausstehende Aufträge werden im STOP-Nachlauf weiter zugeordnet. Fehlt danach eine
vollständige Bestätigung mit STATE oder eine ausdrückliche Ablehnung, steht der
Auftrag als `UNRESOLVED` im Bericht und der Lauf bleibt `incomplete`. Ein hart beendeter
Hostprozess kann seinen Abschlussbericht nicht mehr schreiben.

Die Befehlsdatei nur ergänzen, nicht überschreiben oder verkürzen. Ein unvollständiges
Teilstück am Dateiende wartet auf die abschließende neue Zeile. Fehlerhafte JSON-
oder Parameterzeilen werden als `HOST_REJECT` protokolliert. Der Host sendet
höchstens einen noch unbestätigten Änderungsauftrag, wiederholt abgewiesene
Befehle nicht automatisch und vergibt die Anfragenummern selbst.

## Belege und Antwortzeiten richtig lesen

| Datei | Aussage |
| --- | --- |
| `latest.png`, `latest.json` | Laufende Bildvorschau und zugehöriger Empfangszeitpunkt; Vorschau wird ersetzt |
| `active.json` | Letzter bestätigter Zustand; natürlicher Abschluss wird erst mit neuer STATE-Auskunft sichtbar |
| `session.jsonl` | Während des Laufs fortgeschriebenes Sende-/Empfangsjournal |
| `frame-*.png`, `camera.json` | Gespeicherte Kamerabilder, Hashes, Einstellungen, Empfangslücken, globale Bildmittelwerte und Sättigungsanteil |
| `capture.json` | Befehle, bestätigte Parameter, Revisionen, tatsächliche TX_DONE-Frames, Firmwarehash und Abschlussstatus |

Die Vorschau ist bewusst kein unveränderlicher Beleg. Ein Windows-Bildbetrachter
kann `latest.png` vorübergehend sperren. Der Host erfasst dafür
`preview_warning_count` und begrenzt aufbewahrte `preview_warnings`; Aufzeichnung
und Kamera-Heartbeat laufen weiter, ein späterer Versuch erneuert die Vorschau.
Bild und JSON werden jeweils über eine temporäre Datei atomar ersetzt. Bei einer
Metadaten-Dateisperre kann die JSON-Angabe vorübergehend älter als das Bild sein.
Für nachvollziehbare Beobachtungen deshalb die unveränderten `frame-*.png` und
den abgeschlossenen `camera.json`-Bericht verwenden.

`host_to_ack_ms` misst Host-Sendebeginn bis Empfang der Übernahmebestätigung.
`applied_to_submitted_ms` und `applied_to_completed_ms` verwenden die lokale
Geräteuhr bis zum ersten passenden Frame. `host_to_tx_event_ms` enthält zusätzlich
seriellen Empfang und Hostverarbeitung. Eine vor dem nächsten Frame ersetzte
Konfiguration hat möglicherweise keinen eigenen sichtbaren Frame; einen fehlenden
Messwert nicht als null Verzögerung behandeln.

Kamerazeiten sind Host-Empfangszeiten, keine Sensor-Belichtungszeitpunkte. Die
angeforderten 30 Bilder/s, Belichtung, Treiberpuffer und USB-/Hostlast begrenzen
die zeitliche Zuordnung. Die Vorschau mit ungefähr 4 Bildern/s ist langsamer als
die Aufzeichnung. Empfangslücken, Bilderverlust durch vollen Schreibpuffer und
Sättigung aus `camera.json` mitberichten. Globale Bildmittelwerte hängen stark
vom schwarzen Hintergrund ab und sind keine LED-Leuchtdichte oder Farbkalibrierung.

Der Host verlangt Befehlsbestätigung binnen 2 s und bricht bei 2,5 s ohne neue
Kamerameldung ab. Er wartet beim Schlussstop höchstens 3 s auf Schwarz/DONE;
ein hängender Kameraprozess wird nach begrenztem Warten beendet. Firmwareseitig
führen mehr als 1500 ms ohne KEEP oder 90 s Versuchsdauer zum Stop. Diese Grenzen
sind Fehlerreaktionen, keine zugesagte Echtzeit-Latenz. Ein Software-Stop beweist
keine elektrische Sicherheit bei defektem Controller oder Bus.

## Drei getrennte Evidenzniveaus

| Niveau | Geeignete Aussage und nächster Schritt |
| --- | --- |
| Gestalterisches Feedback | Sichtbare Richtung, Tempo, Ruhe und Verständlichkeit anhand früher Bilder beurteilen; Nutzerurteil und Kameraunsicherheit festhalten. Hier beginnt die Iteration. |
| Begrenzter optischer Vergleich | Varianten bei gleichem Aufbau, festen Einstellungen, erhaltenen Frames und synchronisierten Befehlen vergleichen; Sättigung und Zeitlücken ausweisen. Das gilt nur für diesen Aufbau. |
| Technische/optische Abnahme | Nach tragfähiger Variante: gemeinsamer Kern und Adapter prüfen, passende neue Referenzen/Indexzuordnung, unabhängige Zeitmarker, festgelegte Kriterien und Produkt-/Spielpfad gemäß [Kamera-Prüfweg](camera-measurement.md) abnehmen. Frühes Feedback ersetzt diesen Nachweis nicht. |

Die [ursprüngliche Näheglut-Entwurfsprobe](probes/naeheglut/README.md) bleibt ein
historischer Rechenbeleg. Der interaktive C++-Kern interpoliert auch bei Farb- und
Kurvenwechseln den aktuellen Beitrag; die alte Pythonprobe ist keine zweite
Kameraberechnung und kein Test dieser erweiterten Implementation. Tatsächliche
Varianten, Beobachtungen, gemessene Zeiten und verbleibende Entscheidungen werden
im [Versuchsbericht](naeheglut-live-results-2026-10-18.md) belegt.
