# Ursprüngliche Wake-Ripples: Analyse und Vorbereitung der Kameraprüfung

Stand: **18. Oktober 2026**. Status: **Quellcodeanalyse und gezielte
Rechnercharakterisierung belegt; ursprünglicher Ablauf noch nicht mit Kamera geprüft.**
Referenz ist der vom Nutzer als letzter Live-Spielstand benannte Commit
`a40365d715b2055ee23f767fa53b55ac25243437` auf SvensBranch, Merge von PR #24
vom 10. Oktober 2026. Damit ist der Quellstand festgelegt; ein historisches
Flashabbild des damaligen Live-Spiels wurde hier nicht zusätzlich nachgewiesen.
Keine Änderung oder Installation von Produktfirmware in dieser Analyse.

## Ergebnis: vier Pulse im Standby und danach Spielstart

Das ursprüngliche Verhalten besteht aus vier voneinander unabhängigen,
überlagerten Pulsen im **Standby**. Ein HIGH-Eingang fordert seinen Puls an;
Halten wiederholt ihn nach seinem Ende. Loslassen lässt einen gestarteten
Puls auslaufen. Sobald die Übergangsprüfung alle vier HIGH-Werte erhält,
wechselt das Spiel zu **Idle**. Es gibt keine 800-ms-Zusammenführung zu einem
gemeinsamen Dauerlicht und keine Mindesthaltedauer.

Der aktuelle manuelle Resonanzkreis ist eine andere Variante. Auch der
aktuelle manuelle Einmaleffekt „Ripple“ ist keine Nachbildung dieses alten
Standby-Ablaufs. Ihre bisherigen Kameraergebnisse ersetzen dessen Prüfung nicht.

## Steuerkette im Referenzstand

1. `StateMachine` startet bei Zustand 0; `setup()` legt Standby zuerst an.
2. Beim Eintritt setzt `stateStandByState()` die **Zieldichte** des Funkelns
   auf 0 und setzt Spiel-/Rundenfortschritt zurück. Helligkeit, Palette und
   Ripple-Instanzen werden dort nicht unmittelbar gelöscht oder zurückgesetzt.
3. `KeyMatrix::readWakeSignals()` liest MCP1 `0x20`, B7/B6/B4/B3, direkt als
   vier boolesche HIGH-Werte. Jeder Leseaufruf setzt den Pin auf `INPUT_PULLUP`.
4. Für jeden HIGH-Wert ruft der Zustand `fireRipple(index,60,origin)` auf.
   `GameLightEffects` reicht dies unmittelbar an die jeweilige Instanz weiter.
5. Die LED-Task zeichnet pro Durchlauf erst Funkeln, dann Burst, danach die
   vier Ripples, ruft `FastLED.show()` und wartet anschließend nominal 33 ms.
6. Nach der Zustandsfunktion prüft `State::execute()` die Übergänge. Die
   Standby-Übergangsprüfung liest die vier Wake-Werte dafür **erneut**.

Es gibt hier keine Flankenerkennung, Entprellung oder ausdrücklich gültig/
ungültig markierte Wake-Maske. Die vier GPIO-Lesungen und die zweite
Übergangslesung sind kein atomarer gemeinsamer Schnappschuss. Ein späterer
Test muss Eingangsfolgen und tatsächliche Zustandswechsel getrennt protokollieren.

Quellen am historischen Commit:
[Standby und Aufrufe, Zeilen 323–352](https://github.com/Sven401/Larp-Escape/blob/a40365d715b2055ee23f767fa53b55ac25243437/lib/GameState/GameState.cpp#L323),
[Übergang, ab Zeile 529](https://github.com/Sven401/Larp-Escape/blob/a40365d715b2055ee23f767fa53b55ac25243437/lib/GameState/GameState.cpp#L529),
[Wake-Lesen, ab Zeile 100](https://github.com/Sven401/Larp-Escape/blob/a40365d715b2055ee23f767fa53b55ac25243437/lib/Keymatrixwrapper/Keymatrix.cpp#L100),
[Zeichenreihenfolge, ab Zeile 63](https://github.com/Sven401/Larp-Escape/blob/a40365d715b2055ee23f767fa53b55ac25243437/src/main.cpp#L63).

## Was ein ursprünglicher Ripple zeichnet

`Ripple::fire()` startet ausschließlich eine bereits beendete Instanz.
Ein weiterer Aufruf während ihrer Laufzeit verändert weder Dauer noch Zentrum
noch Phase. Vier Instanzen können dadurch unabhängig und zeitversetzt laufen.
Die Konstruktoren starten inaktiv (`step=frames`); erst `fire()` aktiviert sie.

Bei 23 LEDs durchläuft die räumliche Schleife ausschließlich `i=0,1,2,3`.
Der Puls verändert seine räumliche Ausdehnung während der Laufzeit nicht:
**ein blau betontes Zentrum mit drei gleichzeitig leuchtenden Nachbarn je Seite**.
Die Helligkeit aller dieser Stellen steigt beziehungsweise fällt gemeinsam.
Es wandert keine Wellenfront entlang des Streifens.

| Wake | MCP1-Pin | aktives Zentrum, nullbasiert | adressierte LED-Stellen im Original |
| --- | --- | ---: | --- |
| 1 | B7 | 22 | 19,20,21,22,0,1,2 |
| 2 | B6 | 2 | **−1**,0,1,2,3,4,5; −1 ist ein Fehler |
| 3 | B4 | 16 | 13–19 |
| 4 | B3 | 10 | 7–13 |

Die Bereiche überlappen an 0/1/2, 13 und 19. LED 6 erhält von keinem der
vier ursprünglichen Ripples einen Beitrag. Andere Schichten können sie
trotzdem beleuchten. Räumliche Sensorpositionen am Gehäuse sind damit nicht
erneut physisch nachgewiesen.

Für den durch Standby angeforderten Puls gilt bei Frame `s=0..59`:

```text
B(s) = floor(255*(s+1)/10)       für s < 10
B(s) = 255 - (s-10)*5           für s >= 10
Zentrum:          (floor(B/2), floor(B/2), B)
Abstand d=1..3:   (floor(B/(2d)), floor(B/(2d)), floor(B/(2d)))
```

| Frame, nullbasiert | 0 | 1 | 8 | 9 | 10 | 11 | 30 | 59 | ab 60 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| B | 25 | 51 | 229 | 255 | 255 | 250 | 155 | 10 | kein neuer Beitrag |

Am Maximum ist das Zentrum `(127,127,255)`, seine Nachbarn erhalten jeweils
127, 63 und 42 auf allen drei Kanälen. Alle Beiträge werden mit FastLEDs
saturierender Addition auf den vorhandenen Bildpuffer addiert. Der Ripple
selbst löscht den Puffer weder vor dem Zeichnen noch an seinem Ende.
Das Funkeln überschreibt den Puffer im normalen LED-Task vorher vollständig.
Erst dieser Kompositionsablauf macht ein ausgelaufenes Signal bei schwarzem
Hintergrund wieder dunkel.

60 Schritte mit nominal 33 ms Wartezeit ergeben ungefähr zwei Sekunden;
die ersten zehn Schritte umfassen ungefähr 0,33 s. Das sind abgeleitete
Nennwerte: Rechenzeit, `show()`, Taskwechsel und die seriellen Ausgaben kommen
hinzu. Die Task verwendet `vTaskDelay`, keine feste absolute Framefrist.
Es liegt hier keine gemessene historische Bildrate vor.

Quelle: [unveränderte historische Ripple-Datei](https://github.com/Sven401/Larp-Escape/blob/a40365d715b2055ee23f767fa53b55ac25243437/lib/LedFx/ripple.cpp#L7).
Ihr Git-Blob `867723937ad3c3540f716d14495b2c68510176d6` ist identisch zum
Einführungsstand `101fc6d3106b8000d389d8b977380e67cb02b852`.

## Halten, Loslassen und alle vier Eingänge

- **Kurz betätigen:** Ein angenommener Start läuft alle 60 Frames durch,
  auch wenn der Eingang inzwischen LOW ist. Ein erneut gedrückter Eingang
  startet währenddessen keinen zweiten Puls und setzt den ersten nicht zurück.
- **Gehalten lassen:** Die Zustandsfunktion fordert ständig neue Starts an.
  Bis zum Ende werden sie ignoriert; anschließend wird die Instanz erneut
  gestartet. Der Wiederholungszeitpunkt hängt vom Spiel-/LED-Taskablauf ab.
- **Mehrere Eingänge:** Jede Instanz besitzt ihre eigene Phase. Ihre Beiträge
  werden addiert; es existieren weder feste Phasenversätze noch ausschließlich
  einem Eingang zugeteilte Segmente.
- **Alle vier bei der Übergangslesung HIGH:** Standby endet unmittelbar im
  Zustandsautomaten. Im nächsten Durchlauf wird Idle ausgeführt. Begonnene
  Ripples werden dabei nicht abgebrochen und können in Idle weiter auslaufen.
  Späteres Loslassen führt aus Idle nicht wieder zu Standby. Ein Rückwechsel
  ist in den dafür vorgesehenen Spielzuständen an das Zehn-Minuten-Timeout
  seit dem letzten entsprechenden Zustandseintritt gebunden.

Eine Besonderheit des tatsächlichen Erfolgsbildes:
`stateIdle()` ruft direkt nacheinander `fireBurst(120,grün)` und
`fireBurst(15)` auf. `Burst::fire()` übernimmt die neue Farbe **vor** seiner
Laufzeitprüfung. Bei zuvor inaktivem Burst entsteht deshalb bei serieller
Ausführung ein **weißer Burst über 120 Frames**, nicht ein grüner Vier-
Sekunden-Burst gefolgt von einem kurzen weißen Burst. Dazwischen kann die
LED-Task eingeplant werden; ein anfänglicher grüner Frame ist daher nicht
deterministisch ausgeschlossen. Zustand und LED-Task greifen ohne gemeinsame
Synchronisierung auf die Effektobjekte zu. Dieser Übergang braucht eine eigene
Kameraaufnahme statt eines aus den Aufrufnamen abgeleiteten Sollbilds.

Idle setzt außerdem die Funkel-Zieldichte auf 1 und fährt die globale Helligkeit
mit einem Schritt je frühestens 150 ms Richtung 20. Der Zeitpunkt richtet sich
nach dem Zustand des vorhandenen Rampentimers. Beim allerersten Absenken von
100 auf 20 sind nominell rund zwölf Sekunden nötig.
Quellen: [Idle ab Zeile 140](https://github.com/Sven401/Larp-Escape/blob/a40365d715b2055ee23f767fa53b55ac25243437/lib/GameState/GameState.cpp#L140),
[Burst::fire](https://github.com/Sven401/Larp-Escape/blob/a40365d715b2055ee23f767fa53b55ac25243437/lib/LedFx/burst.cpp#L32),
[Helligkeitsrampe](https://github.com/Sven401/Larp-Escape/blob/a40365d715b2055ee23f767fa53b55ac25243437/lib/LedFx/GameLightEffects.cpp#L58).

## Hintergrund und Ausgabe gehören zum Originalverhalten

Standby setzt die Funkeldichte nicht sofort auf null, sondern deren Zielwert.
`MyTwinkleFox::draw()` nähert die aktuelle Dichte einmal pro Sekunde um einen
Schritt an. Nach dem Boot kann zunächst die Dichte 1 bestehen; beim Rückweg
aus dem Spiel kann die Beruhigung mehrere Sekunden dauern. Auch Palette und
eventueller automatisch gewählter Hintergrund sind bestehender Zustand.
Ein isolierter Puls auf schwarzem Puffer und ein echter Standby-Eintritt sind
deshalb zwei verschiedene Prüfbereiche.

Der Originalstart setzt WS2811/GRB, GPIO15, 23 LEDs, `TypicalLEDStrip`, globale
Helligkeit **100/255** und FastLEDs Leistungsbudget **5 V / 1000 mA**.
Standby setzt die Helligkeit nicht selbst fest; beim Wiedereintritt übernimmt
er den vorherigen Wert. Die oben berechneten RGB-Werte sind **vor** globaler
Helligkeit, Farbkorrektur und möglicher Leistungsbegrenzung. Sie sind keine
gemessenen Drahtbytes und keine Lichtmesswerte.

Der heutige `firmware/led/src/RmtLedBus.cpp` lehnt dagegen jeden ausgegebenen
Kanal über 32 ab. Der neue Resonanzkreis begrenzt selbst auf 24; der aktuelle
manuelle Ripple ist eine einmalige wandernde Wellenfront ab LED 11 über 1400 ms.
Ein späterer Originaltest benötigt seine eigene nachgewiesene Ausgabeskalierung
und Farbreferenz. Einfaches Begrenzen auf 32 würde die historische Ausgabe
verändern. Die bisherige Blau-/Cyan-Kalibrierung passt auch farblich nicht zu
diesem blauen Zentrum mit weißen Nachbarn.

## Bestätigte Indexfalle und Vergleichsgrenze

Im historischen Schreibzugriff steht `(centerLED-i)%NUM_LEDS`. Bei Wake 2,
Zentrum 2 und Abstand 3, ergibt C++ **−1**, nicht 22. Es wird vor den
23-Pixel-Puffer geschrieben. Eine deterministische historische Wirkung dieser
Speicherbeschädigung lässt sich aus dem Code nicht ableiten. Der Kommentar-/
Debugteil berechnet zusätzlich einen normalisierten Index, verwendet diesen
aber nicht für den Schreibzugriff.

Die Rechnerprobe fängt diese Adresse innerhalb zusätzlich reservierter
Wächterpixel ab: jeder Puls an Ursprung 2 enthält den falschen logischen
Zugriff. Die aktuelle kuratierte `lib/LedFx/ripple.cpp` enthält dafür bereits
`tryWrapLedIndex` und Ursprungsprüfungen aus `46ab2be`; sie ist daher nicht
bytegleich mit der hier analysierten Referenz.

Eine weitere Initialisierung setzt den vierten Konstruktorursprung auf 23
(`NUM_LEDS*4/4`). Da alle Instanzen zunächst inaktiv sind und der erste
Standby-Start den Ursprung auf 10 setzt, ist dies im üblichen Aufrufpfad kein
zusätzlicher aktiver Zugriff auf LED 23.

Vor einem Gerätevergleich muss die Behandlung des negativen Index ausdrücklich
festgelegt werden. Eine sichere Ringkorrektur nach 22 muss als begrenzte
Abweichung dokumentiert werden; eine unveränderte Speicherbeschädigung kann
kein definiertes optisches Sollbild liefern. Die übrige Pulsarithmetik,
Start-/Wiederholungslogik und Komposition bleiben unabhängig davon prüfbar.

## Vorbereiteter Prüfplan

| Fall | Ausführung | erwartetes Originalverhalten / getrenntes Prüftor |
| --- | --- | --- |
| O1 | Vier Ursprünge einzeln; ein kurzer HIGH-Impuls | 60 Frames; festes räumliches Profil; blaues Zentrum/weiße Nachbarn; Auslaufen nach Loslassen |
| O2 | Einzelnen Eingang mindestens drei Pulszyklen halten | Wiederholungen nach jeweils abgeschlossenem Puls; keine Verlängerung oder Rücksetzung durch Zwischenaufrufe |
| O3 | Während eines Pulses lösen und erneut betätigen; nach Ende erneut betätigen | Laufender Puls bleibt unverändert; erst nach Ende ist ein neuer Start möglich |
| O4 | Masken 0–14 und zeitversetzte Eingänge | Unabhängige Phasen, additive Überlagerungen, keine Zusammenführung; LED 6 ohne Ripple-Beitrag |
| O5 | Maske 15 nur im isolierten primitiven Renderer | Vier überlagerte Pulse als Diagnose; ausdrücklich kein fortgesetzter realer Standby-Zustand |
| O6 | Aus echtem Standby den vierten HIGH-Wert hinzunehmen | Wechsel zu Idle ohne 800-ms-Wartephase; Restpulse, Burst und Funkeln getrennt erfassen |
| O7 | Nach O6 einen Eingang loslassen | Kein durch Loslassen ausgelöster Rückwechsel nach Standby |
| O8 | Standby ab Boot und nach Spielzustand betreten | Übergang der Funkeldichte und übernommene globale Helligkeit/Palette dokumentieren; frischen Eintritt von beruhigtem Hintergrund unterscheiden |
| O9 | Idle-Erfolgssequenz aufnehmen | Tatsächliche 120-Frame-/Farbfolge und zeitabhängige Helligkeitsrampe beobachten; möglicher grüner Anfang separat |
| O10 | Ursprung 2 / negative Adresse | Hostprobe muss den Fehler erkennen; vor Kameralauf festgelegte Behandlung und deren Abweichung prüfen |
| O11 | Originale Ausgabekette und Messmittel | Frische Index-/Farbreferenz, tatsächliche ausgegebene Frames nach Skalierung, Belichtung, Clipping, Bildlücken und Leistungsbudget belegen |

Der zukünftige Labor-Stop und USB-Watchdog dienen der sicheren Messsteuerung.
Sie sind keine historischen Funktionen dieser Spielversion. Ungültige
I2C-Beobachtungen benötigen ebenfalls einen eigenen Adapter-/Fehlernachweis;
der alte boolesche Wake-Vertrag bietet kein Gegenstück zur neuen Gültigkeitsmaske.

## Bisheriger Nachweis und nächster Schritt

Die unveränderten historischen `ripple.cpp/.h` und `burst.cpp/.h` wurden mit
einem begrenzten CRGB-/Serial-Hostshim ausgeführt. Bestätigt sind alle vier
60-Frame-Hüllkurven und Adressmuster, alle 16 primitiven Überlagerungsmasken,
die negative Adresse, Wiederholung bei Halten, ignorierte Zwischenstarts,
Neustart nach Ende, die erforderliche externe Pufferlöschung und die serielle
Idle-Doppel-Burst-Folge. Kein echter FastLED-Ausgabetreiber, keine vollständige
Spielzustandsausführung und keine ESP32-Nebenläufigkeit wurden damit geprüft.

[Rechnerergebnis und Quellfingerabdrücke](fixtures/legacy-wake-ripple-characterization-2026-10-18.json).
Editierbare einmalige Probe und Rohspur im Testcheckout unter
`private/legacy-wake-ripple-analysis`: `HERKUNFT.md`, `run.py`, `probe.cpp`,
`FastLED.h`, exportierte Originaldateien, `trace.csv`, `build.log` und `result.log`.
Diese Probe ist kein neuer dauerhafter Projektstandard.

**Nächster Schritt:** Behandlung der Indexstelle festlegen und darauf einen
getrennten, zunächst hostgeprüften Original-Laborrenderer samt ausgegebener
RGB-Spur und passender Kamerakalibrierung vorbereiten. Erst anschließend den
beauftragten Kameralauf mit chipgebundenem Rückweg durchführen. Die Analyse
ändert den installierten RSSI-Boxstand nicht.
