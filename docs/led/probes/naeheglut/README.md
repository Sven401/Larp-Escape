# Näheglut: ausführbare Entwurfs-Rechnerprobe

**Status:** Historische skalare Entwurfsprobe 1.0 vom 18. Oktober 2026.
Der aktuelle gemeinsame C++-Kern und die reale Iteration sind im
[Livebericht](../../naeheglut-live-results-2026-10-18.md) belegt.
`collect_live_evidence.py` erzeugt die Auszüge unter `live/` aus privaten
Rohaufnahmen. Die folgende Python-Modellprobe prüft nicht den C++-Kern.

Die Probe wendet die [LED-Effekt-Architektenrolle](../../rollen/led-effekt-architekt.md)
auf den [Näheglut-Entwurf](../../naeheglut-design.md) an. Verbindlich für die
Zahlenvergleiche sind die vor dem Modell festgelegten Sollvektoren im Entwurf,
insbesondere Start, Halbierung, ungültiger Wert, Wiederkehr und gezielter Stop.
Der Auftraggeber bestätigte Nähe, gemeinsames warmgoldenes Licht aller 23 LEDs,
etwa eine Sekunde Übergang, expliziten Stop und Wiederkehr nach ungültigem Eingang.
Die Kanalcodes, lineare Rampe, zwei Plätze, Sättigung und Befehlssemantik sind
nachvollziehbare Entwurfsannahmen; sie ersetzen keine optische Abstimmung.

## Ausführen und Ergebnisse

Im Repository-Stamm, mit Python 3.10 oder neuer und ohne Zusatzpakete:

```text
python docs/led/probes/naeheglut/model.py
```

Der Aufruf prüft das Modell und schreibt nur `result.json` und `preview.svg`
in dieses Verzeichnis. Exitcode 0 bedeutet bestandene Rechnerprobe, 1 einen
Fehlschlag. Der Ergebnisbericht nennt Python-Version, UTC-Prüfzeit, Zahl der
benannten Tests, Fehler, ausgelassene Tests und SHA-256 der tatsächlich gelesenen
`model.py`. Der Hash ordnet den Lauf der editierbaren Modellquelle zu, auch wenn
der spätere Dokumentationscommit erst nach dem Lauf entsteht. Die gezielte
LF-Festlegung für model.py in .gitattributes erhält diesen Fingerabdruck
auch in Windows-Checkouts.

Geprüfte Repository-Ausgangsbasis: `dead12040c196ff74ab4be17ba71451a9351a676`.
Das ist der Integrationsstand, auf dem das neue Modell verfasst wurde; es ist
kein behaupteter Commit mit bereits eingechecktem Modell und kein
Firmware-Prüfnachweis. Historische Fachquellen und ihre getrennten Commitstände
sind im Rollenprofil und im Entwurf verlinkt. Kein Produktmodul wird importiert.

`preview.svg` ist ein numerisches Ablaufdiagramm mit unveränderten RGB-Kanalcodes
als Farbfeldern. Bildschirm, LED, Diffusor und Kamera bilden diese Werte
unterschiedlich ab. Die dunklen Farbfelder sind deshalb keine Aussage darüber,
wie hell oder wie golden der reale Ring wirkt. Der Verlauf zeigt den berechneten
Pegel, keine gemessene wahrgenommene Helligkeit.

## Modellgrenzen und Übernahmevertrag

- `level` ist entweder ein ganzzahliger Promillewert 0–1000 oder `None`.
  `None` repräsentiert `valid=false` und löst bewusst Ausblenden aus.
  Ein ungültiger Zahlenbereich, Bool, Fließkomma oder Text wird atomar abgewiesen.
  Ein zusätzliches fehlerhaftes `p` bei `valid=false` ist hier nicht darstellbar;
  die spätere Eingangsvalidierung muss beide Felder des echten Auftrags prüfen.
- Das Modell führt die Übernahme vollständig und nacheinander aus. Ein Besitzer
  ruft zum gemeinsamen Zeitpunkt zuerst `service(now)` auf, übernimmt seine
  geordneten Befehle und berechnet den frischen Frame. Dieser tatsächliche
  Übernahmezeitpunkt bestimmt die Rampe. Laufzeit bis zur Übernahme, Warteschlange,
  Threadzugriff, Priorisierung, Verlust und Transportquittung sind nicht simuliert.
- `K=(instance,generation)` und der getrennte Besitzer-`epoch` trennen
  Instanzleben und Gesamt-Reset. Die Sequenz gilt innerhalb eines K. Nur der
  zuletzt angenommene Befehl wird zum Duplikatvergleich gespeichert:
  gleiche Sequenz und gleicher Typ/Inhalt = `duplicate` ohne neue Wirkung;
  gleicher Zähler mit anderem Inhalt = `reject:conflict`; kleinerer Zähler =
  `reject:stale`. Übernahmezeit gehört nicht zum Payloadvergleich. Ablehnungen
  erhöhen keinen Zähler. Das ist keine Exactly-once-Garantie für einen Transport.
- Eine neue Generation ist erst nach `stopped` möglich und muss größer sein.
  Alle Generationen starten mit Pegel null. Zwei aktive oder ausblendende
  Effektinstanzen belegen die beiden Plätze; auch ein dunkler aktiver Beitrag
  belegt weiterhin einen Platz. Die gespeicherten Identitätsnachweise sind im
  Python-Modell nicht speicherbegrenzt. Ein produktiver Besitzer braucht eine
  begrenzte, dokumentierte Strategie für IDs, Epochen und Zählerende.
- Die Zeit ist uint32 in Millisekunden. Service-Abstände bleiben unter `2**31` ms.
  Fertige Rampen werden auf `a=b` gesetzt und ihr Zeitanker wird erneuert; dadurch
  beginnt die alte Rampe nach einer vollständigen Uhrumdrehung nicht erneut.
  Lange Standzeiten ohne Service oberhalb dieser Grenze sind nicht abgedeckt.
- Python verwendet breite Ganzzahlen. Der Entwurf fordert uint64-Zwischenwerte
  für Rampen und breite Kanalsummen vor Sättigung. Diese Probe belegt die Formel,
  nicht die korrekte Typwahl oder Überlaufsicherheit einer künftigen C++-Fassung.
- Normale Frames beginnen frisch mit Hintergrund `(4,4,4)` und optional der blauen
  Testschicht `(0,0,8)`. Näheglut gibt nur seinen eigenen Beitrag ab. Nach einem
  gezielten Stop bleibt der Rest sichtbar. Ein harter Gesamt-Reset löscht dagegen
  alle Beiträge, erhöht den Besitzer-Epoch und sperrt die Gesamtausgabe auf
  23 schwarze Pixel. Alte Aufträge können sie nicht wieder freigeben. Erst ein
  angenommener Start mit aktuellem Epoch gibt in dieser Probe den Hintergrund
  und die optionale blaue Testschicht wieder frei. Der unabhängige Hintergrund
  hat im Modell keine eigene Befehls- oder Zustandsmaschine.

## Tatsächlich geprüfter Umfang

23 benannte Unit-Tests, null Fehler und null ausgelassene Tests beim dokumentierten
Lauf. Darin enthalten sind 1001 Pegelwerte als Unterfälle eines Tests, nicht 1001
zusätzliche unabhängige Tests. Der Ergebnisbericht ist für den jeweiligen Lauf
maßgeblich.

| Prüfgruppe | Konkretes Soll |
| --- | --- |
| Lebenszyklus | Vorab festgelegte RGB-Zahlen für Start, Update, Invalid, Wiederkehr, Stop und neue Generation |
| Berechnung | Alle 1001 Pegel, 23 gleiche Pixel, Kanalgrenzen, monotone Auf- und Abfahrt, exakte Endpunkte |
| Änderungen | Aktuellen skalaren Pegel exakt übernehmen; identische Ziele und wiederholtes Invalid setzen Rampe nicht zurück |
| Zeit | 17/33/67-ms-Renderfolgen und ausgelassene Frames stimmen an gemeinsamen Zeitpunkten überein; uint32-Übergang und langer servierter Betrieb |
| Befehle | Fehlerhafte Werte atomar abweisen; aktuelle Duplikate, Konflikt, alte Sequenz/Generation und Reset-Epoch unterscheiden |
| Überlagerung | Zwei parallele Beiträge; eigener Stop; frischer Frame; breite Summe mit Begrenzung auf 32 |
| Grenzen | Volle Kapazität und Start während aktiv/stopping ohne Teilwirkung ablehnen; schwarzer Gesamt-Reset und Schutz vor altem Start |

Ohne blaue Testschicht ergibt Hintergrund plus zweimal Vollgold `(32,28,8)`.
Mit der zusätzlichen blauen Testschicht ergibt derselbe Fall `(32,28,16)`.
Halbgold plus Hintergrund und Blau ergibt `(16,10,13)`; nach Stop von Halbgold
bleibt `(4,4,12)`. Diese Festlegung vermeidet ein mehrdeutiges „B“ für Hintergrund
und blaue Schicht.

## Weitere Ablauf-Sollwerte aus dem Review

Kontinuierlich wechselnde Eingänge haben keine feste Latenz von einer Sekunde.
Jede echte Zieländerung setzt einen neuen 1000-ms-Übergang **ab dem aktuellen
skalaren Pegel** auf. Start p=0 bei t=0, danach bei t=100,200,…,1000 jeweils
p=100,200,…,1000 ergibt mit ganzzahligem Abrunden an jeder Übergabe:

```text
x_neu = floor((9*x_alt + altes_Ziel) / 10)
x vor/nach Änderung bei t=100…1000:
0, 10000, 29000, 56100, 90490, 131441, 178296, 230466, 287419, 348677
x(1500) = floor((348677+1000000)/2) = 674338
x(2000) = 1000000
RGB bei 1000/1500/2000: (8,4,0) / (16,8,1) / (24,12,2)
```

Erst 1000 ms nach der **letzten** Zieländerung ist das Ziel garantiert erreicht.
Eine Rekurrenz ohne das vorgeschriebene Zwischenabrunden würde hier um einen
Skalarschritt abweichen; die festen Zahlen im Test verwenden die Ganzzahlregel.

Der erste explizite Stop erhält auch während einer Invalid-Ausblendung seine
eigene volle 1000-ms-Dauer. Beispiel: Vollgold bei t=1000 wird ungültig;
bei t=1500 ist x=500000. Stop bei t=1500 übernimmt genau diesen Wert, bei t=2000
ist x=250000 und bei t=2500 endet die Instanz. Ein weiterer Stop bei t=1750
verlängert den Verlauf nicht. Der ursprüngliche Invalid-Timer hätte sonst
schon bei t=2000 null erreicht; dieser Unterschied ist ausdrücklich geprüft.

## Aussage und offene Prüfungen

Die Probe trägt den **Entwurf** und seine Rechenregeln. Sie prüft keine echte
Firmware, reale Warteschlange, Profilbarriere, RMT-Ausgabe, Originalpixelgleichheit,
Stromversorgung oder Sichtwirkung. Produkt-Hosttests, C++-Typprüfung,
Geräteübertragung, Kamera und menschliche optische Abnahme sind getrennte,
noch offene Nachweise gemäß Prüfplan im Entwurf. Ein bestandener Rechnerlauf
gibt keinen dieser Schritte frei.
