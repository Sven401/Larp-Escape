# Interaktive Näheglut im isolierten Kameralabor

Stand: 18.10.2026. Implementierter C++-Kern und Laboradapter, keine Einbindung in
Spielcontroller oder BLE-V2. Geräteausgabe, Kamera und optische Freigabe benötigen
separate tatsächlich ausgeführte Nachweise. `NearGlow.cpp` ist derselbe Kern für
Rechnerprüfung und Laborfirmware; die produktive Spielanbindung ist noch offen.

## Rechenvertrag

Zwei feste Instanzplätze, alle 23 Pixel; jede Instanz besitzt unabhängige
RGB-Endpunkte in Festkomma mit Q=1.000.000. Ein vollständiger Auftrag enthält
p=0..1000, valid, RGB jeweils 0..32, Gain=0..1000, D=50..5000 ms und Kurve.
Ziel pro Kanal: `valid ? C * Q * p * gain / 1.000.000 : 0`.

Lineare Kurve: Gewicht `d/D`. Smoothstep: `d²*(3D-2d)/D³`, jeweils d=min(elapsed,D).
Interpoliert wird `(from*(denominator-numerator)+to*numerator)/denominator`
in uint64, danach einmal abgerundet. Maximaler Zähler 32e6*5000³=4e18 passt in
uint64. RGB8 entsteht erst beim Rendern durch Division durch Q. Frischer
Hintergrund plus beide RGB8-Beiträge wird breit summiert und auf 32 begrenzt.
Das ist ein Kanalcodevertrag ohne Gamma-/Farbkorrektur, keine optische Kalibrierung.

Jede echte Ziel-/Dauer-/Kurvenänderung übernimmt den derzeitigen unquantisierten
RGB-Beitrag. Auch Farbe und Gain lassen sich daher ohne unmittelbaren RGB-Sprung
ändern. Gleiches RGB-Ziel bei gleicher Dauer/Kurve setzt den Timer nicht zurück;
der vollständige Parameterschnappschuss wird trotzdem übernommen. Insbesondere
verlängert ein anderes ignoriertes p bei valid=0 das Ausblenden nicht.

Dies erweitert die frühere skalare Python-Entwurfsformel zu drei unabhängigen
Festkomma-Kanälen. Die ursprünglichen benannten Standard-Sollpunkte passen;
Pixelgleichheit für beliebige lange Umparametrierungsfolgen wird nicht behauptet.
Die Original-Pythonprobe ist kein Test dieses C++-Kerns.

Vor Befehlsübernahme wird service(now) ausgeführt. Zeit ist die lokale uint32-Uhr;
Service-Abstände bleiben kleiner als 2^31 ms. Fertige Rampen werden abgeschlossen.
Stop übernimmt den aktuellen Beitrag und blendet mit gespeicherter Dauer/Kurve
zu null. Wiederholstop verlängert nicht. Updates während Stop sind gesperrt;
neuer Start benötigt nach Abschluss eine höhere Generation. Gesamtstop entfernt
auch den Hintergrund und verlangt einen frischen bestätigten Schwarztransfer.

## Laborprotokoll LARP-LIVE/1

Ausschließlich `larp-led-camera-lab`; Produkt-USB und BLE werden nicht erweitert.
HELLO liefert unveränderte LARP-CAMERA/1-Identität mit Firmwarehash sowie
`LARP-LIVE/1 2 23 32`. Token: genau 16 kleine Hexzeichen. Dezimalzahlen nur Ziffern,
kein Vorzeichen/Fließkomma, keine Überläufe. Zeilenpuffer 192 Byte.

```text
LIVE token
GLOW token req slot generation start|update|stop p valid r g b gain D curve
BG token req r g b
STATUS token
KEEP token
STOP
```

slot=0/1, generation und req=1..uint32_max, valid=0/1, curve=0 linear/1 smoothstep.
GLOW benötigt immer den vollständigen Parametersatz. Stop wiederholt die zuletzt
bestätigten Parameter unverändert; er konfiguriert nicht gleichzeitig um.
BG setzt den Hintergrund sofort. LIVE ist nur im bereiten Leerlauf möglich,
startet schwarz und benötigt einen neuen Token. Ein laufender Versuch endet
spätestens nach 90 s oder nach mehr als 1500 ms ohne passendes KEEP.

req steigt global innerhalb einer LIVE-Sitzung. Nur der letzte angenommene
Auftrag wird für Wiederholung gespeichert: gleicher Inhalt ergibt DUPLICATE
und keine neue Wirkung, anderer Inhalt bei gleicher req ergibt conflict;
ältere req ergeben stale. Ablehnung verbraucht keine req. Alte Token werden
abgewiesen. Das ist kein Netzwerk- oder Berechtigungsprotokoll.

```text
LIVE token revision deviceMs
APPLIED token req revision deviceMs
DUPLICATE token req revision deviceMs
REJECT token req reason revision deviceMs
STATE token revision deviceMs bgR bgG bgB [state gen seq p valid r g b gain D curve]slot0 [..]slot1
L token revision frameNumber submittedMs completedMs rgbHex
```

STATE hat 29 Felder und folgt auf LIVE/APPLIED/DUPLICATE sowie STATUS. Zustand:
0 leer, 1 aktiv, 2 ausblendend, 3 beendet. revision ordnet die angenommene
Konfiguration zu, nicht jeden Zeitschritt; natürlicher Abschluss erhöht sie nicht.
STATUS zeigt den aktuellen Lebenszyklus. RGB-Hex enthält 23*3 Bytes (138 Zeichen).
L hat 7 Felder. Token, Revision und RGB werden beim Submit gemeinsam eingefroren;
L wird erst nach TX_DONE ausgegeben. Alter Transfer bestätigt keine neue Einstellung.

Globales STOP liefert STOP token Grund, dann einen neuen schwarzen L-Frame und
DONE token. Bei einer hängenden Übertragung nach 2000 ms zuerst Reset, danach
frisches Schwarz; Reset allein bestätigt kein Licht. Der Host muss Schwarz und
DONE beobachten und sonst das Abschaltergebnis als unbestätigt behandeln.
Bestehende CAL/RUN-Befehle und ihr F-Trace bleiben unverändert.

## Softwareprüfung

`python firmware/led/tests/run_camera_tests.py` prüft neun Laborloopfälle mit
asynchronem FakeBus (inklusive LIVE, Watchdog, In-flight-Revisionsbindung und
hängendem Transfer beim Stop) sowie sieben Kernprüfgruppen, darunter 2002
Pegel/Kurven-Unterfälle. Der Runner baut C++11 mit Warnungen als Fehler.
Das belegt keine echte Geräteübertragung oder optische Wirkung.
