# Näheglut: Entwurf und Rollenprobe

Datum: **18. Oktober 2026**. Status: **Nutzerabsicht bestätigt; ausführbarer
Laborprototyp mit gemeinsamem C++-Effektkern**. Der ursprüngliche skalare
Entwurf 1.0 und sein Pythonbeleg bleiben unten als historische Rechenreferenz
erhalten. Ein Produkt-/Spieladapter ist noch nicht implementiert. Die praktisch ausgewählte
Arbeitsvariante ist RGB `(24,8,0)` mit Smoothstep über 1000 ms.
Das optische Nutzerurteil bleibt offen.

Implementierter [Kern- und Laborvertrag](../../firmware/led/LIVE-NEARGLOW.md).
Tatsächlich ausgeführte Hardware-/Kameraläufe werden getrennt in
[Näheglut: praktische Ergebnisse](naeheglut-live-results-2026-10-18.md)
fortgeschrieben; dieser Verweis behauptet für sich keinen Gerätebeleg.
Anwendung der [LED-Effekt-Architektenrolle](rollen/led-effekt-architekt.md);
übergreifende Grenzen im [Auftrags-/Mischvertrag](effect-command-contract.md).
Ausgangsbasis des Entwurfs 1.0: `dead12040c196ff74ab4be17ba71451a9351a676`.
Quelle der Antworten: Codex-Sitzung `01a14f9a-35fb-72e1-9acb-132b0d16599d`,
zwei Fragerunden mit jeweils drei Antworten am 18.10.2026.

## Bestätigte Absicht und Entwurfsvorschläge

**Vom Nutzer bestätigt:** Mehr Nähe lässt das Licht heller werden. Alle
23 LEDs leuchten gemeinsam in warmem Gold, bei 0 dunkel und bei 100 am
hellsten, mit etwa einer Sekunde weichem Übergang. Ein veränderlicher Spielwert
steuert den Effekt bis zum ausdrücklichen Stop. Ungültige Werte blenden nur
diesen Beitrag aus; gültige Werte lassen ihn zurückkehren. Nach Stop ist ein
neuer Start erforderlich.

**Technische Ausgangswerte des Entwurfs 1.0:** normalisierter ganzzahliger Eingang
0–1000, lineare Überblendung über 1000 ms, Goldmaximum RGB `(24,12,2)`,
ADD32-Mischung und ebenfalls 1000 ms weicher Stop. Diese Details sind
prüfbare Arbeitswerte, keine optisch bestätigte Farbrezeptur. Konkrete
Spielsituation, Nähemessquelle und deren Kalibrierung bleiben offen;
„Nähe“ ist keine zugesagte Entfernung in Metern.

Der Laborprototyp erlaubt zusätzlich Laufzeitänderungen von Farbe, Gain,
Dauer und Hüllkurve innerhalb des unten beschriebenen Vertrags. Dies dient
der praktischen Iteration; die unten verlinkte Arbeitsvariante ist ausgewählt,
die Produktfreigabe durch den Nutzer bleibt offen.
Die Ausgangswerte bleiben erhalten und werden nicht still durch eine andere
Produktabsicht ersetzt.

## Eingang und Verantwortung des Spiels

Der lokale Spielbesitzer liefert `p ∈ [0,1000]` (Promille), `valid`,
Geltungsbereich, Instanzgeneration und Auftragsfolge. 0 bedeutet fern,
1000 nah. Ein unbekannter Messwert ist nicht ein gültiger Wert 0.

Frische/Veraltung wird **vor** der Effektgrenze beurteilt. Ein stabiler
lokaler Spielwert darf unverändert gültig bleiben; ein ausfallender Sensor
muss ausdrücklich zu `valid=false` führen. Keine frei erfundene Timeoutdauer
im Effekt. Wiederkehr gültiger Daten aktualisiert nur einen bereits gestarteten
Effekt; sie startet keinen gestoppten. Fehlerhafte Parameter außerhalb des
Wertebereichs werden vollständig abgewiesen, ohne Phase oder Ziel zu ändern.
Auch bei `valid=false` muss das übergebene p formal im Bereich liegen;
sein Wert wird dann für die Zielberechnung ignoriert.

Bestehende [RSSI-Regelung](../espnow/rssi-test.md) und
[Gehversuch](../espnow/distance-camera-result-2026-10-18.md) sind
Quellen für Signalaufbereitung, keine passende fertige Nähekalibrierung:
Der Bestand verwendet bei veralteten Daten 20 % Funkelintensität.
Näheglut soll dagegen ausblenden. Keine Übernahme dieses Fallbacks und
keine Änderung der bestehenden RSSI-/Funkelregelung durch diesen Entwurf.

## Lebenszyklus und sichtbarer Ablauf

Die Zeitangaben dieser Tabelle beschreiben die unveränderte Ausgangseinstellung
mit D=1000 ms und linearer Kurve. Der Laborprototyp verwendet dieselben
Lebenszyklen mit der jeweils vollständig bestätigten Dauer/Kurve; Parameter-
änderungen übernehmen den aktuellen RGB-Beitrag nach der folgenden Berechnung.

| Eingang/Ereignis | Zustand und sichtbarer Beitrag |
| --- | --- |
| Boot / vor Start | Keine Instanz, Beitrag null |
| Start mit neuer Generation | Aktiv; Anfangswert null, Ziel aus p/valid; alle 23 LEDs steigen gemeinsam an |
| Update mit neuem Ziel | Alten Verlauf zuerst zum Übernahmezeitpunkt auswerten; von genau dort 1000 ms zum neuen Ziel |
| Weiter identischer Zielwert | Laufenden Übergang unverändert fortsetzen; kein Neustart des Timers |
| Ungültig | Aktiv bleiben, in 1000 ms zu null; wiederholtes Ungültig setzt den Timer nicht zurück |
| Wieder gültig | Aus aktuellem Wert in 1000 ms zum Ziel; kein Sprung |
| Stop der Instanz | Zustand ausblendend; in 1000 ms zu null, dann beendet; Updates auch während Ausblenden abweisen |
| Stop erneut | Keine neue Wirkung, keine Verlängerung |
| Start während aktiv/ausblendend | Ohne Teilwirkung abweisen; keine neue Farbe oder Phase |
| Neuer Start nach beendet | Höhere Startgeneration erforderlich, wieder bei null beginnen |
| Gesamt-Aus/Reset | Sofort alle Beiträge entfernen; Produkt-Soll: neue Besitzerepoche sperrt alte Aufträge; Laborabgrenzung siehe unten |
| Pause/Zustandswechsel | Kein vom Effekt erfundener Stop; Spiel muss ausdrücklich weiterführen oder stoppen |
| BLE-Abbruch | Kein impliziter Stop eines autonomen Spielbeitrags; manuelle Ausgaben haben eigenen Besitzvertrag |

Der erste Stop startet bewusst eine eigene 1000-ms-Ausblendung ab dem
aktuellen Wert, auch während eines schon laufenden Ungültig-Übergangs oder
bei bereits dunklem Beitrag. Nach dieser Sekunde ist die Instanz beendet.
Die allgemeine Regel für gleiche Ziele gilt für Updates, nicht diesen ersten
Stop; wiederholte Stops verlängern ihn nicht.

Bei ständig wechselndem Ziel beginnt jede echte Änderung einen neuen
Übergang ab dem aktuellen Wert. Das ist keine feste Verzögerung von einer
Sekunde. Erst wenn das Ziel 1000 ms unverändert bleibt, ist es erreicht.

Es gibt kein natürliches Ende bei p=0 oder p=1000. Bei gleichbleibendem Wert
wird das Licht nach dem Übergang ruhig; kein Atmen, Puls oder Lauflicht.

## Aktueller RGB-Festkommakern und Laufzeitparameter

Der implementierte [NearGlow-Kern](../../firmware/led/src/NearGlow.cpp) und seine
[Schnittstelle](../../firmware/led/include/NearGlow.h) berechnen drei unabhängige
RGB-Kanäle in Festkomma. Der vollständige [Laborvertrag](../../firmware/led/LIVE-NEARGLOW.md)
führt Parametergrenzen, Rechenweg, Befehle, Zustandsantworten und die Softwareprüfung.
Alle 23 LEDs erhalten denselben Beitrag; zwei feste Slots sind die Kapazität
dieses Prototyps, keine unbegrenzte Produktzusage.

| Laufzeitparameter | Zulässiger Bereich / Ausgangswert |
| --- | --- |
| p / valid | 0–1000 / bool; Ausgangsszenario p=1000, gültig |
| Ziel-Farbcode C | Je Kanal 0–32; Ausgangswert (24,12,2) |
| Gain | 0–1000; Ausgangswert 1000 |
| Übergangsdauer D | 50–5000 ms; Ausgangswert 1000 ms |
| Kurve | 0 linear; 1 Smoothstep; Ausgangswert 0 |

Für jeden Kanal c gilt mit Q=1.000.000:

```text
z[c] = valid ? floor(C[c] * Q * p * gain / 1.000.000) : 0
d = min(uint32(now - t0), D)

Kurve 0: n = d;                 m = D
Kurve 1: n = d*d*(3*D - 2*d);   m = D*D*D

v[c](now) = floor((a[c]*(m-n) + z[c]*n) / m)
E[i,c] = floor(v[c](now) / Q), für i=0..22
```

Bei einer Änderung des RGB-Ziels, der Dauer oder Kurve zuerst alle alten
`v[c](now)` auswerten, als neue `a[c]` übernehmen und erst dann Ziel, D, Kurve und
t0 ersetzen. Damit springen auch aktive Farb-/Gainänderungen nicht unmittelbar.
Wenn RGB-Ziel, D und Kurve gleich bleiben, bleibt die Rampe erhalten; trotzdem
wird der vollständige neue Parametersatz gespeichert. Ein geändertes ignoriertes
p bei valid=false verlängert somit die Ausblendung nicht.

Zielbildung und Interpolation verwenden uint64. Der größte Interpolationszähler
beträgt höchstens 32.000.000 * 5000³ = 4.000.000.000.000.000.000 und passt hinein.
D ist strikt positiv. Erst nach der Interpolation auf RGB8 reduzieren; danach
frischen Hintergrund und Beiträge breit addieren und auf 32 begrenzen. Fertige
Rampen werden abgeschlossen; der Zeitvertrag bleibt unter 2^31 ms zwischen
Serviceaufrufen. Das ist kein Nachweis der wahrgenommenen Helligkeitslinearität.

Zusätzliche feste Sollpunkte für Kurve 1 bei Start mit den Ausgangswerten:
bei 250 ms ist v=(3.750.000,1.875.000,312.500), also RGB (3,1,0);
bei 500 ms RGB (12,6,1); bei 750 ms v=(20.250.000,10.125.000,1.687.500),
also RGB (20,10,1). Nach D gilt exakt das jeweilige Ziel. Die linearen
Standard-Sollpunkte des historischen Entwurfs bleiben mit dem C++-Kern vereinbar.

Konkreter Umparametrierungsfall im [C++-Test](../../firmware/led/tests/test_near_glow.cpp):
Standardstart, bei 333 ms Wechsel auf C=(0,32,16), Gain=500, D=2000 und Kurve 1.
Am Wechsel bleibt v=(7.992.000,3.996.000,666.000) unverändert; bei 1333 ms
v=(3.996.000,9.998.000,4.333.000), nach 2333 ms RGB (0,16,8).
Das sind nachvollziehbare Rechenpunkte, keine vorweggenommene Farbentscheidung.

**Reset-/Epochengrenze:** Der gemeinsame Kern verwaltet Slotgenerationen und
Sequenzen, aber keine produktive Besitzerepoche. Scene::reset() entfernt Beiträge
und Identitätsnachweise. Im Labor übernimmt der äußere LIVE-Token die Abgrenzung:
Nach Gesamtstop sind weitere Effektaufträge inaktiv; ein neuer LIVE-Lauf benötigt
einen neuen Token. Ein späterer Spieladapter muss alte Aufträge über eigene
Besitzerepochen, Profil-/Durchlaufbindung und begrenzte Identitätsnachweise sperren.
Ein nackter Kernreset ist keine vollständige produktive Wiederholungssperre.

## Historischer Entwurf 1.0: skalare Berechnung und Pythonbeleg

Die folgende skalare Formel ist die erhaltene Ausgangsfassung. Sie wurde mit
[Python](probes/naeheglut/model.py) geprüft und ist keine Beschreibung der heutigen
aktiven Farb-/Gainumparametrierung. Der RGB-Kern rundet intern pro Kanal;
Pixelgleichheit beliebiger langer Updatefolgen mit der Skalarprobe ist deshalb
nicht zugesagt. Beide Fassungen und ihre Prüfungen bleiben unterscheidbar.

Alle Indizes `i=0..22` erhalten denselben Beitrag; keine Ringarithmetik nötig.
Jede Instanz hat unabhängig `a,b,t0,state`. Zeit stammt vom lokalen
LED-Besitzer in Millisekunden, nicht aus Frames oder einer entfernten Uhr.

```text
D = 1000 ms; Q = 1 000 000
Ziel b = valid ? 1000 * p : 0
d = min(uint32(now - t0), D)
x(now) = floor((a*(D-d) + b*d) / D)
E[i,c] = floor(C[c] * x(now) / Q), C = (24,12,2), i=0..22

bei Änderung:
  aktuell = x(now)          // noch mit altem a,b,t0
  wenn neues Ziel != b:
      a = aktuell; b = neues Ziel; t0 = now
  sonst Verlauf erhalten
```

Nach Erreichen des Ziels den Tween abschließen (`a=b`, keine offene
Zeitdifferenz über viele Wochen). Beim neuen Auftrag zuerst auswerten.
Zwischen Serviceaufrufen weniger als `2^31` ms voraussetzen. Der modellierte
32-Bit-Überlauf ist damit eindeutig; beliebig lange Strom-/Servicepausen sind
kein zugesicherter Uhrenvertrag. Verpasste Frames werden nicht nachgeholt:
der nächste Frame verwendet die aktuelle Zeit. Ein langer Ausfall kann somit
einen sichtbaren Sprung erzeugen; er verlängert die Übergangsdauer nicht.

Zwischenprodukte in mindestens 64 Bit, keine vorzeichenlose Subtraktion
`b-a`. Formel hat nur nichtnegative Terme. Erst am definierten Schritt
abrunden; beim Abblenden nicht vorzeitig auf RGB8 reduzieren. Die resultierenden
Codes liegen bei einer Instanz in R=0–24, G=0–12, B=0–2. Kleine Werte können
durch Quantisierung rotbetont oder ganz dunkel wirken: optisch noch zu prüfen.

## Historische unabhängige Sollpunkte vor der Rechnerprobe 1.0

Start bei t=0, p=1000; Zeiten in ms, RGB jeweils für **alle** 23 LEDs:

| Zeitpunkt / Ereignis | Interner x-Wert | RGB |
| --- | ---: | --- |
| 0 Start | 0 | (0,0,0) |
| 250 | 250000 | (6,3,0) |
| 500 | 500000 | (12,6,1) |
| 750 | 750000 | (18,9,1) |
| 1000, Update p=500 | 1000000 | (24,12,2) |
| 1500 | 750000 | (18,9,1) |
| 2000, ungültig | 500000 | (12,6,1) |
| 2500 | 250000 | (6,3,0) |
| 3000 | 0 | (0,0,0), weiterhin aktiv |
| 3500, gültig p=1000 | 0 | (0,0,0) |
| 4000 | 500000 | (12,6,1) |
| 4500 | 1000000 | (24,12,2) |
| 5000, Stop | 1000000 | (24,12,2), ausblendend |
| 5500 | 500000 | (12,6,1) |
| 6000 | 0 | (0,0,0), beendet |
| 6500, Update p=1000 | 0 | abgewiesen, bleibt null |
| 7000, neuer Start p=500 | 0 | (0,0,0) |
| 7500 / 8000 | 250000 / 500000 | (6,3,0) / (12,6,1) |

Beispielrechnung t=1500: `floor((1000000*500+500000*500)/1000)=750000`;
Blau `floor(2*750000/1000000)=1`. Die Tabelle ist eine manuelle
Sollvorgabe, kein aus dem Prüfling generierter Erwartungswert.

## Überlagerung und gezielter Stop

Die folgenden Zahlen bleiben die synthetischen Mischbeispiele des Entwurfs 1.0;
der RGB-Kern verwendet ebenfalls ADD32. Seine frei wählbaren Parameter erweitern
diese Beispiele, ohne daraus optisch bestätigte Mischungen zu machen.

Für diese Probe liegen alle Beiträge bereits im **finalen Kanalcoderaum
0–32**. Keine zusätzliche Gammafunktion, globale Skalierung oder
FastLED-Korrektur im Modell. Dies ist keine photometrisch lineare Helligkeit
und keine pixelgleiche Rekonstruktion der Originalausgabe.

`F[i,c] = min(32, B[i,c] + Summe aktiver E[k,i,c])`, in breiter Arithmetik.
Frischer Hintergrund pro Frame, dann Addition. Nur ADD32 ist hier erprobt;
Alpha, MAX und Ersetzen sind nicht Gegenstand des Näheglut-Modells.

| Synthetische Komposition pro LED | Ergebnis |
| --- | --- |
| Hintergrund (4,4,4) + halbe Näheglut (12,6,1) + blauer Testbeitrag (0,0,8) | (16,10,13) |
| Derselbe Aufbau mit voller Näheglut (24,12,2) | (28,16,14) |
| Näheglut gezielt fertig gestoppt | (4,4,12); Hintergrund und Blau bleiben |
| Zwei volle Näheglut-Instanzen + Hintergrund | (32,28,8); Rot sättigt |
| Gesamt-Aus | (0,0,0), unabhängig vom Hintergrund |

Blauer Testbeitrag und konstanter Hintergrund sind Prüffixtures, keine
neu beauftragten Spieleffekte. Unterschiedliche Starts/Updates erzeugen
unabhängige Übergänge. Sättigung kann Näheunterschiede verdecken; die
tatsächliche Spielkomposition braucht ein Helligkeitsbudget und optische Abnahme.
Die aktuelle RMT-Grenze 32 bleibt gewahrt, eine elektrische Freigabe folgt nicht.

## Historische Python-Rechnerprobe 1.0: Umfang und Prüfplan

Editierbare [Probe und Herkunft](probes/naeheglut/README.md),
[Modell](probes/naeheglut/model.py), [Ergebnis](probes/naeheglut/result.json).
Historisches Rechenmodell, kein Test des C++-Kerns oder Firmwareadapters.
Modellkapazität zwei demonstriert begrenzte
Annahme, legt keine Produktkapazität fest. Eine Besitzerepoche, Instanz-ID,
Generation und Sequenz modellieren veraltete Aufträge. Exakte Wiederholung
des zuletzt angenommenen Auftrags wirkt nicht erneut; andere Daten bei gleicher
Sequenz sind Konflikt, ältere Sequenzen veraltet. Kein Netzwerkprotokolltest.

| Fall | Eingaben / vorab bestimmtes Soll |
| --- | --- |
| N1 Kurve | Alle Sollpunkte oben exakt; alle 23 Pixel gleich |
| N2 Grenzen | Alle 1001 p-Werte; Endpunkte und Kanalbereiche; monotone Helligkeit |
| N3 Änderungen | Neues Ziel mitten im Übergang ohne Sprung; gleiche Ziele/invalid mehrfach ohne Timerneustart |
| N4 Zeit | 17/33/67-ms-Abtastung und ausgelassene Frames stimmen an gemeinsamen Zeitpunkten überein; Start 0xffffff00 über Überlauf |
| N5 Lebenszyklus | Ungültig/gültig erholt sich; Stop/Stop/Update/Neustart; keine Wiederbelebung durch Update |
| N6 Annahme | Alte Generation/Sequenz, Duplikat, geänderter Duplikatinhalt, fehlerhafter p, volle Kapazität: definierte Ablehnung ohne Teiländerung |
| N7 Komposition | Zwei unabhängige Instanzen, Hintergrund, Blaufixture, ADD32-Sättigung; gezielter Stop erhält andere Beiträge |
| N8 Reset | Sofort null; alte Epoche und alte Starts bleiben gesperrt |
| N9 Laufender Eingang | Start p=0 bei 0; alle 100 ms p um 100 bis 1000 erhöhen: x(1000)=348677, x(1500)=674338, x(2000)=1000000. RGB (8,4,0), (16,8,1), (24,12,2). Stop während invalid-Ausblendung beginnt genau einmal eine neue Sekunde. |

Nur tatsächlich ausgeführte Fälle als gemessen berichten. Das
[Erprobungsprotokoll](../../handoff/LED-EFFEKT-ARCHITEKT-ERPROBUNG-2026-10-18.md)
ordnet Ergebnisse, Rollenänderungen und offene Nachweise zu.

## C++-Softwareprüfung und offener Produktanschluss

Der aktuelle [Kern-/Laborprüfweg](../../firmware/led/LIVE-NEARGLOW.md#softwareprüfung)
führt die tatsächlichen C++-Quellen aus: Berechnung, aktive Umparametrierung,
Parallelslots, Parser, Lebenszyklus, In-flight-Revisionszuordnung und begrenzter
Schwarzstop werden mit einem simulierten asynchronen Bus geprüft. Dies ist
Softwareprüfung des ausführbaren Prototyps, kein Geräte- oder Sichtnachweis.

Für den noch fehlenden Produktadapter bleiben Besitzer-/Queueannahme, Überlast,
Stopzustellung, veraltete Tickets, Besitzerepochen, Profilbarrieren und der reale
Ausgabepfad gesondert zu prüfen.
Eine verspätete alte Übertragung darf keinen neuen Start/Stop bestätigen.
Nominale 33 ms sind keine gemessene Latenz. Ressourcen- und Latenzbudget erst
an Zieladapter und Parallelbelegung binden. Passende bestehende Prüfwege:
[Quality](../quality-checks.md) und [LED-Firmware](../../firmware/led/README.md).
Die historische Pythonprobe prüft weder C++-Arithmetik noch diese Adapter;
die getrennten C++-Tests ersetzen ihrerseits keine Prüfung des künftigen
Produktanschlusses.

## Optische Abnahme — offen, praktische Iteration getrennt dokumentieren

Praktische Laborläufe und visuelle Rückmeldungen werden im
[Geräte-/Kamerabericht](naeheglut-live-results-2026-10-18.md) mit tatsächlich
verwendeter Farbe, Gain, Dauer/Kurve, Firmwareidentität und Framezuordnung erfasst.
Eine endgültige Farb-/Kurvenvariante wird hier nicht vorweggenommen. Die folgenden
Kriterien stammen vom Standardentwurf; für eine bewusst geänderte Variante vor
deren Abnahmelauf passende Referenzen und Zeitwerte festlegen.

Voraussetzungen: eigener Geräteauftrag und abgeglichener chipgebundener
Rückweg, dokumentierter Firmwarehash, feste Kamera/Belichtung, tatsächliche
Zuordnung aller 23 LEDs durch unabhängige Einzellichter. Eigene direkte
Referenzen für Schwarz, Goldcodes der Solltabelle, Hintergrund und Blau;
nicht alte Weiß-/Cyan-Grenzwerte übernehmen. Übertragene Frame-/Eingangsspur
und synchronisiertes Kamerabild getrennt erfassen.

Vorgeschlagene Kriterien **vor** einem späteren Messlauf festhalten; bei
ungeeigneter Kalibrierung den Aufbau ändern, nicht nach Ergebnis lockern:

- Drei Wiederholungen 0→500→1000→0, je Ziel 3 s halten; invalid/valid und
  Stop/Neustart separat. Goldmaximum und Halbstufe müssen an jeder LED im Gesamt-/Luminanzsignal
  über dem dreifachen Dunkelrauschen liegen; keine gesättigten Kamerapixel.
- Stabile Farbstufen: je Kanal Abweichung höchstens
  `max(3*Kalibrier-Standardabweichung, 10 % des Gold-Maximalsignals)`.
  10 % ist ein neuer vorläufiger Entwurfswert für erkennbare Stufen, kein
  übernommener Resonanzkreis-Nachweis. Referenzfamilie je Mischfall wählen.
- Übergang nach abgezogener unabhängig gemessener Kameraverzögerung in
  1000±100 ms beendet; maximale Bildlücke 50 ms. Die Toleranz lässt zwei
  Bildabstände zu. Verpasste/lückenhafte Aufnahme wiederholen.
- Zielstand ohne unbeabsichtigte Oszillation halten; monotoner Verlauf
  innerhalb dreifachen Referenzrauschens. Bei Stop nur Nähebetrag entfernen;
  isolierte Dunkelheit gegen Schwarzreferenz prüfen, mit Hintergrund dessen
  erwartetes Restlicht erhalten. Gesamt-Aus gesondert prüfen.
- Menschliche Abnahme: 0/500/1000 ohne App richtig als fern/mittel/nah ordnen;
  warmgoldener Eindruck, ruhiger Übergang und Unterscheidbarkeit in der
  vorgesehenen Spielkomposition vom Nutzer beurteilen lassen. Raumlicht,
  Distanz und abweichende Wahrnehmungen notieren.

RGB-Codes, Bildschirmvorschau, Kamera und menschlicher Farbeindruck bleiben
unterschiedliche Nachweise. Der Effekt kann vor dieser Abnahme rechnerisch
richtig und trotzdem zu dunkel, rötlich oder in Überlagerungen unverständlich sein.

## Offene Anschlussentscheidungen

- Konkreter Spieleinsatz/Start- und Stopzustand, Nähemessquelle und normierte
  Abbildung; Definition der Messwertfrische beim Spielbesitzer.
- Zielpfad, Kapazität, Helligkeitsbudget konkurrierender Effekte, Verhalten bei
  Pause/Profilwechsel; fachlicher Vorschlag steht im Auftragsvertrag.
- Ausgewählte RGB-Arbeitswerte, Smoothstep über 1000 ms und weichen Stop
  optisch durch den Nutzer bestätigen oder ändern; keine stillschweigende Übernahme als fertiges Produkt.
- Der C++-Kern und Laboradapter sind ausführbar; der Produkt-/Spieladapter,
  dessen Integrationstests und Epochenvertrag bleiben offen. Tatsächliche
  Geräte-/Kamerabelege werden separat fortgeschrieben; elektrische Freigabe
  und gemeinsame Spielabnahme folgen daraus nicht automatisch.

## Praktisch ausgewählte Arbeitsvariante (18.10.2026)

Der [reale Iterationslauf](naeheglut-live-results-2026-10-18.md) führt von den
obigen Ausgangswerten zu RGB `(24,8,0)`, Gain 1000, Smoothstep und D=1000 ms.
Weniger Grün/Blau wirkte im Kamerabild wärmer; B=0 entfernt den bei B=1 erst am
vollen Endpunkt auftretenden Blau-Schritt. Der weichere Beginn passt als Vorschlag
zur ruhigen Absicht. Das ist eine begründete Arbeitsauswahl, keine optische
Nutzerfreigabe. Nahwertquelle und Produktadapter bleiben offen.
