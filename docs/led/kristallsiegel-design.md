# Kristallsiegel als sichtbarer Kristallfortschritt

**Ausführbarer Folgestand · 20.10.2026:** Auf den Auftrag „Als Led Architekt
arbeite an diesem Effekt“ folgen ein gemeinsamer C++-Kern und die Erweiterung
des bestehenden USB-/Kameralabors. [Kern, Liveparameter, Rundenvertrag und
Prüfweg](../../firmware/led/LIVE-CRYSTALSEAL.md). Das folgende Soll bleibt die
gestalterische Grundlage. [Reale Lichtversuche, Parametervergleich und verifizierter
Rückweg](kristallsiegel-live-results-2026-10-20.md) ergänzen den ausführbaren Stand.
Spieladapter und abschließendes Nutzerurteil sind offen.

Stand: **20. Oktober 2026**. Status: **am Gerät erprobter Gestaltungsvorschlag**.
Auftrag: „entwerfe den nächsten sinnvollen LED Effekt“. Geprüfte Projektbasis:
`5b8596591cf6657c65d99fc20a70e5334264b368` nach frischem Fetch.
PR-Basis nach parallel erfolgter Dokumentationsintegration #88:
`97a2584cc108ebb9c0f667fc46f898d163e00144`; Produktquellen unverändert.
Der Nutzer hat die Weiterentwicklung dieses Effekts beauftragt; die endgültige
Farbwirkung, Spielbindung und Überlagerungspriorität bleiben offen. Der ursprüngliche
Entwurfsumfang war Bildschirmvorschau ohne Firmwareänderung; den ausführbaren
Folgestand führt der verlinkte Laborvertrag.

## Idee und Nutzen

Jeder vom lokalen Spiel bestätigte Kristall lässt einen weiteren Abschnitt
des LED-Kreises entstehen. Das Licht wandert weich durch den neuen Abschnitt
und bleibt anschließend ruhig stehen. Beim dritten Kristall schließt sich
der Kreis, hebt sich einmal sanft in der Helligkeit und bleibt geschlossen.
Die Anzeige sagt **„0, 1, 2 oder 3 Kristalle erkannt“**. Sie erklärt weder
den Spieler zum Sieger noch beendet sie die Runde.

Heute geben kurze Bursts, zunehmende Funkeldichte und globale Helligkeit
den Fortschritt indirekt wieder. `PuzzleProgress` führt bereits drei bestätigte,
nach Position deduplizierte Einträge. Daraus lässt sich eine räumlich ablesbare
Anzeige ableiten, ohne eine neue Sensor- oder Funkquelle vorauszusetzen.
[Quellen: Fortschrittsmodell](../../src/GameDomain/PuzzleProgress.cpp),
[Anzahl](../../include/GameDomain/PuzzleProgress.h),
[Spielphasen und Rückmeldungen](../../lib/GameState/GameState.cpp).

[Näheglut](naeheglut-design.md) bleibt eine andere Aussage: ein kontinuierlicher
Nähewert verändert alle LEDs gemeinsam. Kristallsiegel zeigt diskreten lokalen
Fortschritt. Die historischen Wake-/Ripple-Effekte sind ebenfalls keine
gespeicherte Fortschrittsanzeige. Der aktuelle Integrationsstand hat im manuellen
`larp::led::Effect` nur Off/Twinkle/Burst/Ripple; der Resonanzkreis aus dem
separaten Quellstand `e7ec0f9` wird hier nicht als integrierte Auswahl behauptet.

## Sichtbarer Ablauf und Arbeitswerte

Die Skizze nutzt einen **schematischen logischen Kreis** mit 23 LEDs. Sie ist
keine neu vermessene Hardwareanordnung. Vor einem Lichtversuch muss eine feste,
vollständige Abbildung der logischen Indizes auf die tatsächlichen LEDs stehen.
Die Abschnittsnummer bezeichnet den Fortschritt, keine Kristallidentität,
Rolle, Sensorposition oder Himmelsrichtung.

**Realer Prüfstand vom 20.10.:** Alle 23 Indizes wurden vor dem Effektlauf
frisch zugeordnet und in Gegenfolge bestätigt. Die vorhandene Kette bildet drei
räumliche Gruppen mit unregelmäßiger, teils zickzackförmiger Indexfolge. Die
Kreisvorschau bleibt schematisch; der [Versuchsbericht](kristallsiegel-live-results-2026-10-20.md)
zeigt die echte Geometrie sowie die langsamere Arbeitsvariante 900/250 ms.

| Eigenschaft | Vorschlag |
| --- | --- |
| Abschnitt 1 / 2 / 3 | Logische Indizes 0–7 / 8–14 / 15–22, also 8 / 7 / 8 LEDs |
| Richtung | Ab Index 0 in aufsteigender logischer Reihenfolge |
| Ruhefarbe | Kühles Türkis, RGB-Kanalcodes `(0,8,12)` im begrenzten Raum 0–32 |
| Neuer Abschnitt | Aufbau über 600 ms; jede LED blendet innerhalb 150 ms weich ein |
| Dritter Abschnitt | Nach vollständigem Aufbau aller drei Abschnitte ein Puls von 800 ms |
| Pulsmaximum | `(4,18,24)`; 400 ms ansteigen, 400 ms zurück zur Ruhefarbe |
| Start aus bekanntem Stand | Alle benötigten LEDs gemeinsam in 300 ms einblenden; kein Nachspielen |
| Ungültig / gezielter Stop | Aktuellen eigenen Beitrag in 300 ms ausblenden |
| Stillstand | Kein laufender Schweif, kein zyklisches Atmen; bestätigte Abschnitte bleiben ruhig |

Farbcodes und Zeiten sind erste Arbeitswerte. Die Bildschirmvorschau stellt
Farbcharakter und Ablauf schematisch dar; sie simuliert weder reale Lichtleistung,
Diffusor, Gamma noch die Kamera. Der verstärkte Leuchthof im Bildschirm-Puls
ist eine zusätzliche Anzeigehilfe und kein gemessener optischer Effekt.

Beispiel: Start bei 0/3. Bei t=0 kommt der erste bestätigte Kristall, bei
t=2000 der zweite und bei t=4000 der dritte. Die Abschnitte stehen bei
600, 2600 und 4600 ms vollständig. Der gemeinsame Puls beginnt bei 4600 ms,
erreicht bei 5000 ms sein Maximum und endet bei 5400 ms. Danach bleibt 3/3
ruhig sichtbar, bis das Spiel die Anzeige ausdrücklich beendet oder erneuert.

## Eingang und Lebenszyklus

Der lokale Spielbesitzer liefert vollständige, serialisierte Aufträge nach
dem [bestehenden Auftrags-/Mischentwurf](effect-command-contract.md):
Besitzerepoche, Rundenkennung, Instanzgeneration, Sequenz, Operation, `count`
als Ganzzahl 0–3, `valid` und Anlass **Fortschritt** oder **Bestandsübernahme**.
Dies ist ein semantischer Vorschlag, keine bestehende Wire- oder C++-API.

Beim Start übergibt der Rundenbesitzer außerdem den bereits verbrauchten
Pulszustand. Ein Live-Update 2→3 benötigt seine einmalige, an Runde und
Auftragssequenz gebundene Pulsfreigabe. Der Besitzer verbucht den Verbrauch
bei gültiger Übernahme; ohne Freigabe entsteht kein Puls. Der Effekt kann
diese Berechtigung weder durch Stop noch durch einen neuen Start erneuern.

`Fortschritt` entsteht erst nach `PuzzleProgress::record() == Added`;
`Duplicate`, `Full` und fachlich abgewiesene Kristalle erzeugen keinen solchen
Auftrag. `count()` ist maßgeblich, nicht die Nummer der aktuellen Spielphase:
Die Erfassung kann fortschreiten, während die Audioausgabe den Phasenwechsel
noch aufhält. Frische, Persistenz und Gültigkeit bewertet das Spiel. Ein gültiger
lokaler Stand veraltet nicht allein durch BLE-/WLAN-Unterbrechung.

| Ereignis | Festgelegtes Verhalten dieses Vorschlags |
| --- | --- |
| Boot ohne Start | Keine Instanz, kein Beitrag |
| Start mit gültigem Eingang | Neue zulässige Generation; von null zum vollständigen Snapshot in 300 ms, auch bei 3/3 ohne Puls |
| Start mit `valid=false` | Aktive, dunkle Instanz; erster gültiger Wert als 300-ms-Bestandsübernahme, ohne Puls |
| Gültiger Fortschritt um genau +1 | Nur neuer Abschnitt startet seinen Aufbau; bereits laufende Abschnitte behalten ihre Timer |
| Gleicher Stand, weiterhin gültig | Kein Neustart und kein neuer Puls; Sequenz darf trotzdem bestätigt werden |
| Sprung um mehr als +1 oder ausdrücklicher Snapshot | Aktuelles Bild in 300 ms zum Ziel überblenden; keine versäumten Ereignisse nachspielen |
| Kleinerer gültiger Stand | Als autoritative Korrektur in 300 ms zum Ziel, ohne rückwärts wandernde Erfolgserzählung |
| `valid=false` | Instanz bleibt aktiv, eigene Ausgabe in 300 ms zu null; Puls verwerfen, Wiederholung verlängert nichts |
| Gültigkeit kehrt zurück | In 300 ms zum Snapshot, ohne Puls; auch bei unverändertem count kein vorzeitiges No-op |
| Gezielter Stop | Eintritt in Ausblenden, Updates abweisen, nach 300 ms beendet; Stop-Wiederholung verlängert nichts |
| Start während aktiv/ausblendend | Vollständig abweisen, ohne Farb-/Timeränderung |
| Neuer Start nach Stop | Ausdrücklich höhere Generation erforderlich; Update allein startet nichts |
| Neue Runde / Spielreset / Profilbarriere | Alte Identität sperren und eigenen alten Beitrag an der Übernahmegrenze sofort entfernen; neue Instanz bewusst aus aktuellem Snapshot starten |
| Gesamt-Aus | Alle Beiträge entfernen; Erfolg erst nach zugeordneter Schwarzübertragung melden |
| Pause oder Phasenwechsel | Vorschlag: lokalen Stand weiter zeigen, bis ausdrücklich gestoppt/reset; konkrete Spielbindung vor Integration bestätigen |

Fehlerhafte Parameter, unpassende Epoche/Runde/Generation und alte Sequenzen
werden ohne Teilwirkung abgewiesen. Ein exaktes gespeichertes Duplikat erhält
dieselbe Antwort ohne neue Wirkung; gleiche Sequenz mit anderem Inhalt ist
ein Konflikt. Start und jedes Update prüfen auch bei `valid=false` den formalen
Wertebereich. Alle angenommenen Aufträge werden in Reihenfolge ausgewertet.

**Rundenkennung neu vorsehen:** `roundCounter` ist hierfür ungeeignet: Er wird
erst beim Abschluss erhöht und beim Spielreset zurückgesetzt. `resetRound()`
löscht den Kristallstand, besitzt aber keine monotone Instanzgeneration. Der
spätere Adapter benötigt eine eigene Runden-/Besitzerepoche und eine definierte
Neustartgrenze; die Anzeige darf alte Meldungen nicht einer neuen Runde zuordnen.

**Puls höchstens einmal pro Runde:** Beim ersten gültigen Erreichen von 3/3
wird seine Berechtigung verbraucht. Nur ein lückenloser Live-Fortschritt 2→3
darf ihn dabei vormerken. Snapshot/Start/Wiederherstellung bei 3 verbrauchen
die Berechtigung ohne Puls. Invalid, Korrektur, Snapshot, Stop und Reset
verwerfen einen vorgemerkten/laufenden Puls. Die Folge 3→2→3 erzeugt keinen
zweiten Puls. Eine neue Instanz derselben Runde erbt den Verbrauch; deshalb
liegt diese Sperre beim Rundenbesitzer und nicht allein im Effektobjekt.

Ein reines gleiches gültiges Fortschrittsupdate oder identisches wiederholtes
Snapshot erzeugt kein neues Ziel und unterbricht auch keinen laufenden Verlauf.
Ein erster tatsächlicher Wechsel in Snapshot-/Korrekturbehandlung verwirft den
Puls; dessen wiederholte Übernahme startet die 300-ms-Rampe nicht neu.

Bei schnellen Meldungen 1/3 bei 0, 2/3 bei 100 und 3/3 bei 200 ms enden die
Aufbauten bei 600, 700 und 800 ms. Der Puls läuft genau einmal von 800 bis
1600 ms. Es gibt keine Warteschlange abzuspielender Feieranimationen.

## Berechnung des Beitrags

Zeitbasis: lokale monotone Millisekunden, keine Framezählung. Für einen
32-Bit-Zähler `elapsed = uint32(now - start)`; zwischen Serviceaufrufen weniger
als 2^31 ms. Fertige Übergänge abschließen und Zeitanker verwerfen. Verpasste
Frames nicht nachholen, sondern den Verlauf zur aktuellen Zeit auswerten.

Für Q=1.000.000 und 0≤d≤D definiere Smoothstep in Festkomma:

```text
s(d,D) = floor(Q * d*d * (3*D - 2*d) / (D*D*D))
mix(a,b,s) = floor((a*(Q-s) + b*s) / Q)
```

`a` und `b` sind interne Kanalwerte in Millionsteln eines Ausgabecodes.
Alle Zwischenprodukte mindestens uint64; Differenzen wie `now-start` nur nach
dem angegebenen Uhrenvertrag, keine vorzeichenlose Farbsubtraktion. D ist 150,
300 oder 400 ms; der größte Smoothstep-Zähler ist 64.000.000.000.000, der
größte Mischzähler bei maximal 32Q ist 32.000.000.000.000. Beides passt in uint64.
Den Kanal erst ganz am Ende mit `floor(v/Q)` in RGB8 umwandeln.

Beim Aufbau eines neuen Abschnitts mit n LEDs, lokaler Position j=0…n−1:

```text
delay[j] = floor(450*j/(n-1))
d[j] = clamp(elapsed - delay[j], 0, 150)  // signed bzw. vor Subtraktion prüfen
v[j,c] = mix(a[j,c], C[c]*Q, s(d[j],150))
C = (0,8,12)
```

Zum Start die aktuellen internen Werte `a` des betroffenen Abschnitts erfassen.
Die beiden anderen Abschnittsverläufe unverändert lassen. Bei regulärem
Fortschritt sind die neuen LEDs vorher dunkel; aus einer noch laufenden
Korrektur dürfen sie auch andere aktuelle Werte haben. Gleiche Eingänge
verändern weder `a` noch den Zeitanker.

Snapshot, Korrektur, Wiederkehr und Ausblendung erfassen **alle 23 aktuellen
internen RGB-Werte einschließlich eines laufenden Pulses**, verwerfen alte
Verläufe und interpolieren gemeinsam über 300 ms. Snapshot-Ziel: C für alle
Abschnitte unterhalb count, sonst null. Invalid/Stop-Ziel: überall null.

Für den vorgemerkten Puls zunächst alle drei Abschnitte vollständig zur
Ruhefarbe kommen lassen. Der Pulsbeginn ist deren spätester Endzeitpunkt,
nicht der Zeitpunkt des nächsten verspäteten Renderaufrufs. Bei Pulsalter u:

```text
0 <= u < 400:   v[i,c] = mix(C[c]*Q, P[c]*Q, s(u,400))
400 <= u < 800: v[i,c] = mix(P[c]*Q, C[c]*Q, s(u-400,400))
u >= 800:      v[i,c] = C[c]*Q; Puls abschließen
P = (4,18,24), für alle i=0..22
```

Ein verspäteter Frame jenseits des Pulsendes zeigt direkt den Ruhezustand.
Er spielt den Puls nicht nach. Sichtbare Wartezeit/Latenz ist damit weiterhin
eine Eigenschaft des späteren Ausgabepfads, keine hier gemessene Echtzeitzusage.

Unabhängige Sollpunkte: Bei einem dunklen neuen 8er-Abschnitt erhält die erste
LED nach 75 ms `(0,4,6)` und nach 150 ms `(0,8,12)`; die letzte bleibt bis
450 ms null, hat bei 525 ms `(0,4,6)` und bei 600 ms `(0,8,12)`.
Für einen fertigen Kreis sind Pulsalter 0/200/400/600/800 ms genau
`(0,8,12)` / `(2,13,18)` / `(4,18,24)` / `(2,13,18)` / `(0,8,12)`.
Ein Stop vom Pulsmaximum hat nach 150 ms `(2,9,12)` und nach 300 ms null.

## Komposition und notwendiger Anschluss

Jede Instanz erzeugt einen eigenen frischen 23-Pixel-Beitrag; außerhalb aktiver
Abschnitte ist er null. Vorschlag für die begrenzte Laborprobe:
`F[i,c] = min(32, B[i,c] + sum(E[k,i,c]))`, breit addiert. Keine zusätzliche
Gammafunktion in dieser Rechenprobe; kein Übernehmen alter Pixelpuffer.
Gezielter Stop entfernt nur Kristallsiegel, nicht Hintergrund oder Näheglut.

Beispiel am Pulsmaximum: Näheglut-Arbeitswert `(24,8,0)` plus Kristallsiegel
`(4,18,24)` ergibt `(28,26,24)`. Mit Hintergrund `(4,4,4)` ergibt sich
`(32,30,28)`; zusätzliches Funkeln kann bereits sättigen. Nach gezieltem Stop
bleibt in diesem Beispiel `(28,12,4)`. Solche Zahlen belegen nur Kanalgrenzen.
Flächiges Licht kann dunkle Abschnitte füllen und die Fortschrittsaussage
unlesbar machen. **Empfohlene erste Sichtprobe: Kristallsiegel allein.**

Für die tatsächliche Spielnutzung sind Maskierung/Priorität oder ein gemeinsames
Helligkeitsbudget mit Funkeln/Näheglut zu entscheiden und optisch zu prüfen.
Der Entwurf entfernt oder dämpft bestehende Effekte nicht stillschweigend.
Die manuelle Baugruppe hat keinen solchen Mixer; ihr vorhandener BLE-Vertrag
wird nicht umgedeutet. Ebenso ist die FastLED-Ausgabe des festen Spiels kein
fertiger Adapter für den beschriebenen Raum 0–32.

Für eine spätere Umsetzung denselben Effektkern im Labor und im gewählten
Produktadapter verwenden. Aufgaben: Rundengeneration/Start/Stop verdrahten,
bestätigte Zählerstände samt Anlass kopieren, begrenzte Kapazität und
Stopzustellung festlegen, Profilbarrieren und Übertragungsrevisionen erhalten.
Zunächst kein neuer BLE-Effektbezeichner, keine Appänderung und kein automatischer
Funkstart nötig. Der Spieladapter ist im Bestand noch nicht vorhanden.

## Prüfplan und Entscheidungsgrenzen

| Prüfung | Erwartung vor der Implementierung festgelegt |
| --- | --- |
| Zuordnung | Genau 23 eindeutige Indizes; Sektoren 8/7/8, keine Lücke/Überlappung/negativer Index |
| Rechenpunkte | Obige Sollwerte, exakte Endpunkte und jeder Kanal 0–32 |
| Zeit | Gemeinsame Zeitpunkte bei 17/33/67-ms-Abtastung gleich; ausgelassene Frames und Start bei 0xffffff00 ohne Verlängerung |
| Schnelle Kristalle | 0/100/200-ms-Fall: unabhängige Aufbauten, Puls 800–1600 ms; währenddessen gleiche Meldungen wirkungslos |
| Wiederherstellung | Start/Snapshot bei 0,1,2,3: 300-ms-Einblendung, kein Nachspielen und kein Puls |
| Unterbrechung | Invalid während Aufbau/Puls, gültig mit gleichem count, Stop während Fade; Updates nach Stop bleiben abgewiesen |
| Korrektur | 3→2→3, Sprung 0→3 und neuer Start derselben Runde: kein zweiter oder nachgeholter Puls |
| Verbrauch über Instanzen | 3 erreicht → Stop → Korrektur auf 2 → neue Instanz mit 2 → Live-3: weiterhin kein weiterer Puls |
| Identität | Alte Runde/Generation/Sequenz und widersprüchliche Duplikate ohne Teilwirkung; nach Reset kein Wiederaufleben |
| Mischung | Obige Summen, Sättigung, gezielter Stop erhält andere Beiträge, Gesamt-Aus fordert Schwarz |
| Aufnahme am Gerät | Tatsächliche Reihenfolge und alle 23 LEDs prüfen; eigene Schwarz-/Türkis-/Pulsreferenzen, konstante Belichtung und Framezuordnung |

Für den ersten gestalterischen Lichtversuch gilt: 1/3, 2/3 und 3/3 müssen nach
Ende der Animation ohne erneutes Ereignis unterscheidbar sein. Der neue Abschnitt
soll eine zusammenhängende Bewegung zeigen, bereits bestätigte Abschnitte sollen
ruhig bleiben. Drei schnelle Bestätigungen sollen sich bis zum vollen Kreis
aufbauen; der Puls soll genau einmal sichtbar sein. Nutzerurteil zu Farbe,
Tempo und Lesbarkeit erfassen, bevor metrische Kameraschwellen festgelegt werden.
Keine alten Weiß-/Gold-Abnahmeschwellen auf Türkis übertragen.

Produktentscheidung vor Umsetzung: Ist diese Fortschrittsaussage gewünscht,
soll sie durch Spielpause/Abschlussphase hindurch stehen bleiben, und welche
Lichtaussage hat bei Überlagerung Vorrang? Der Entwurf empfiehlt Persistenz
bis zum Rundenreset sowie eine erste isolierte Sichtprobe. Die 3/3-Anzeige löst
keinen Audio-, Sensor-, Funk- oder Spielzustandswechsel aus.

## Ergebnisumfang

Die [editierbare Vorschau](probes/kristallsiegel/preview.html) zeigt den regulären
Aufbau, schnelle Bestätigungen, Schluss-Puls, Stop und neue Runde. Sie ist eine
schematische Ablaufprobe, kein zukünftiger Firmwarekern und kein Test von
Auftragsidentitäten, Invalid-/Snapshotbehandlung oder physischer LED-Ausgabe.
Der Entwurf wurde mit der [LED-Architektenrolle](rollen/led-effekt-architekt.md)
und einer unabhängigen Quell-/Randfallprüfung erarbeitet. Dabei wurden die
Audio-unabhängige Erfassung, fehlende Rundengeneration, getrennte Abschnittstimer
und die rundenweite Pulssperre berücksichtigt. Neue Rollenregeln entstehen nicht.

Am 20.10.2026 ausgeführt: unabhängige Formelprobe mit den oben angegebenen
Sollpunkten, 23 eindeutigen Indizes und 9015 Pixelzeitpunkten für die
Abschnittslängen 7/8; Sollpunkte, Grenzen und Aufbaureihenfolge bestanden.
Die Vorschau wurde mit simuliertem DOM und kontrollierter Zeit auf 23 LEDs,
statischen Start, unabhängige Aufbauten, Pulswerte/Endzeit, Begrenzung auf 3,
Stop/Neustart, Pulsabbruch, reduzierte Bewegung, Zustandswiederherstellung und
Ende der Animationsframes geprüft. Dies ist keine Browser-/Layoutprüfung.
Der PR benennt den tatsächlichen Commit dieser Ergebnisse.
Firmwarebuild, Geräte-/Kameralauf und optisches Nutzerurteil bleiben getrennte,
für diesen Entwurfsauftrag nicht ausgeführte Nachweise.
