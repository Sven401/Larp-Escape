# Nähe Glut kompakt: unabhängige Beschaffungsgegenprüfung

Stand: **31.10.2026**. Reviewer: `/root/supply_review`; Umsetzung:
`/root/compact_hardware`; Koordination/Beschaffung: `/root`. Basiscommit
`483c3320dd2da64cd74e46b45084359d07337ed7`, Branch
`codex/naehe-glut-hardware-r1`; geprüft wurden die unten gebundenen Arbeitsdateien.
Der Reviewer hat diese beiden technischen Entwürfe nicht selbst implementiert.

**Ergebnis: kein technischer Beschaffungsblocker für den begrenzten
Prototyp-Teilesatz aus fertig bestückten Serienmodulen.** Dies ist keine
Bestellautorisierung, Geräte-, Laufzeit-, Firmware- oder Sicherheitsfreigabe.
Endverdrahtung und Montage sind erforderlich; geliefert werden keine bereits
vollständig verbundenen Geräte. Eigene PCB-Fertigung ist keine Voraussetzung
des geprüften Dezemberwegs.

## Geprüfter Umfang und Dateibindung

Je Gerät: Adafruit QT Py 5700, NeoPixel Grid BFF 5646, PowerBoost 2030,
geschützter KeepPower P1835C3, zwei Mill-Max 0906-2-15-20-75-14-11-0,
E-Switch EG1218 ausschließlich für EN und Littelfuse 0251002.MXL nahe Zellplus.
Gemeinsamer externer Nitecore UM2 mit manuell 200 mA / Li-Ion 4,2 V.

| Geprüfte Datei | SHA256 |
| --- | --- |
| [Verdrahtung und Abnahme](TECHNIK/VERDRAHTUNG.md) | `96C786DF05B173597FD48F7890DAC98240AF5D356A00BFE2D64B633918F29FD8` |
| [Technische Voruntersuchung](TECHNIK-ENTWURF.md) | `72E9F66E9A5CA2F8F6D39BD22C056EDEB42E48058F8BCA207CE9105C4BFC8C1F` |

Prüfweg: technische Unterlagen vollständig gelesen, relevante Herstellerquellen
und veröffentlichtes QT-Py-CAD unabhängig abgeglichen, Netzzuordnung aus der
Eagle-Datei ausgelesen, Energie-/Stromrechnung nachgerechnet, Korrekturen erneut
gelesen und SHA256 selbst ermittelt. Zubehördaten zusätzlich durch
`/root/supply_review/assembly_fit` gegengeprüft. Keine neue Firmwareprüfung,
Geräteprüfung, Messung oder Bestellung. Der abschließende Nachtrag unten ergänzt
eine begrenzte digitale Mechanikprüfung sowie Mengen-/Preisrechnung; eine eigene
Liveprüfung von Händlerbestand, Versand oder Liefertermin wurde nicht vorgenommen.

## Befunde und Quellen

- Die gesamte [QT-Py-5700-Platine](https://www.adafruit.com/product/5700) misst
  21,7 × 17,8 × 5,7 mm und erfüllt die ESP-Boardgrenze. Ihr Speicherprofil ist
  4 MB Flash / 2 MB PSRAM; frühere Firmwarequalifikation gilt nicht automatisch.
- Im [offiziellen Familien-CAD](https://github.com/adafruit/Adafruit-QT-Py-ESP32-S3-PCB)
  liegen D1-/D2-Kathoden, LDO-Eingang und JP3/7 gemeinsam an +5V; BAT führt an
  D2-Anode. Boost → BAT, BFF → QT-Py-5V ist damit schlüssig. Die gelieferte
  5700-Revision und die offene Dioden-Lötbrücke müssen noch geprüft werden.
  [Adafruit bestätigt](https://learn.adafruit.com/adafruit-5x5-neopixel-grid-bff/overview)
  3-V-Datenlogik bei 5-V-Versorgung für die BFF.
- [PowerBoost 2030](https://www.adafruit.com/product/2030) mit
  [TPS61030](https://www.ti.com/lit/gpn/tps61030) besitzt EN-Lasttrennung.
  Wandler und 3,2-V-Warnung ersetzen keine automatische 3-V-Akkuabschaltung.
  Die [P1835C3-Revision](https://www.keeppower.com.cn/products_detail.php?id=675)
  bleibt wegen abweichender älterer Kapazitäts-/Ladeangaben zuzuordnen.
- Die Mittelwertrechnung ergibt unter ihren unbestätigten Annahmen rund
  **4,11 Stunden**, bei 150 statt 120 mA ESP-Mittelstrom nur rund 3,43 Stunden.
  24 aktive von 25 physischen LEDs, ESP-NOW/RSSI und sichtbare Helligkeit bleiben
  Teil des zu prüfenden Produktprofils. Gleiche RGB-Zahlen belegen keine gleiche
  Helligkeit nach LED-Wechsel; der genaue SK6805-Grundstrom bleibt offen.
- LED-PWM-Spitzen wurden vom Mittelstrom getrennt. 2,18 A beim angenommenen
  Vollweißfall bei 2,5 V überschreiten die 2-A-Kontaktdauerangabe; die
  [3-A-Peakangabe](https://www.farnell.com/datasheets/3189521.pdf) nennt keine
  belegte Pulsdauer. Auch die [OR-Diode](https://www.onsemi.com/download/data-sheet/pdf/nsr0320mw2t1-d.pdf)
  erlaubt aus ihrer 1-A-Stromangabe keine pauschale thermische Freigabe.
  Deshalb keine Vollweißprobe ohne gesonderte Puls-/Thermikqualifikation.
- [EG1218](https://www.mouser.com/datasheet/2/140/EG1218-3452668.pdf) ist als
  EN-Signalschalter passend. Die
  [2-A-Sicherung](https://www.littelfuse.com/assetdocs/littelfuse-fuse-251-253-datasheet?assetguid=f47a0bb7-8ede-4679-9646-7114c3787688)
  begrenzt nicht präzise auf 2 A und ersetzt keine Kontakt-/Diodenprüfung.

Behobene Dokumentbefunde: widersprüchliche separate BFF-Versorgung entfernt,
2,5-V-Netzteilprüffall ergänzt, pauschale Kurzzeit-Vollweißfreigabe entfernt und
falsche Kombination aus LED-Mittelwert und ESP-Spitze korrigiert.

## Nach Lieferung offen

1. Tatsächliche Modul-/Akkurevisionen, Netze, Polung, offene Lötbrücke und
   USB-/Akkubetrieb einschließlich Rückspeisung prüfen.
2. Halter-Verpolsperre, Federhub, Kontaktbefestigung, Isolation und Zugentlastung
   am realen Akku nachweisen. Der gedruckte Pluspol-Kragen ist noch nicht belegt.
3. Einschaltstrom, PWM-/Funkspitzen und Temperaturen zunächst am strombegrenzten
   Netzteil einschließlich 2,5 V prüfen; keine absichtliche Zell-Tiefentladung.
   1,5 A Startbegrenzung ist kein vollständiger Spitzenlastnachweis.
4. Eigenes 5700-Firmwareprofil und Flash-Rückweg qualifizieren. Anschließend
   Optik, Funk/Distanz und vier Stunden Betrieb **beider** Geräte im fertigen
   Gehäuse nachweisen. Bestellfreigabe nimmt diese Ergebnisse nicht vorweg.

## Planung und historische R1

Die Bestellstopps am Anfang von [R1-BESTELLUNG](../BESTELLUNG.md),
[R1-README](../README.md) und in der [Hardwareplanung](../../HARDWAREPLANUNG.md)
wurden gelesen: R1 ist keine aktuelle Beschaffungsbasis. Ihre früheren Review-
und Fertigungsnachweise werden nicht auf die Kompaktgeräte übertragen.

Der gemeldete Planungsbefund ist nach erneuter Lektüre **geschlossen**:
[PLANUNG.md](PLANUNG.md) führt jetzt 5700/5646/2030/P1835C3 als Dezemberweg;
eine eigene PCB ist optional. Der R1-README stellt ausdrücklich klar, dass auch
eine spätere Autorisierung der Kompaktvariante den R1-Bestellstopp nicht aufhebt.

## Schlussnachtrag: Mechanik und konkreter Warenkorb

Die letzte Änderung im Verdrahtungsvertrag gleicht den freien Kontaktabstand
auf **37,4 mm** an den [stromlosen Passprototyp](MONTAGE-PROTOTYP.md) an.
Nominal ergeben sich rechnerisch 0,65 mm Hub je Kontakt, mit den dokumentierten
Toleranzen 0,1476–1,1524 mm bei symmetrischer Aufteilung. Beide Werte liegen
unter dem angesetzten kleinsten Vollhub von 1,27 mm. Gleichmäßige reale
Hubverteilung, Sitz, Federkraft und der fehlende Pluspol-Guard sind damit nicht
nachgewiesen und bleiben vor Bestromung zu prüfen.

Gezielt geprüft: Montageblatt, relevante Maß-/Kollisionsprüfungen in der
editierbaren Quelle, `geometry-report.json`, montierte Ansicht und Schnitt-
Explosionsansicht. **22/22 Manifestdateien stimmen in SHA256 überein.** Die
sechs binären STL-Dateien wurden unabhängig ausgelesen: alle auf Z0, Außenmaße
jeweils passend zum Bericht. Körpermaß 66 × 34 × 38,4 mm = 86,17 cm³, ohne
Schraubenköpfe. Die im Entwicklerbericht grünen FreeCAD-/STEP-/Kollisionstests
wurden gegen ihren Prüfcode gelesen, aber vom Reviewer nicht neu ausgeführt.
Die Modulhüllen und Renderings sind kein Nachweis realer Passung, RF-Freiräume,
Optik oder Sicherheit. **Nur stromloser Passprototyp; keine Betriebsfreigabe.**

Für den [konkreten Mouser-Vorschlag](BESTELLUNG.md) wurden sieben Positionen
abgeglichen: je zwei 5700/5646/2030, sechs Kontakte (vier plus zwei Reserve),
zwei EG1218, vier 2-A-Sicherungen (zwei plus zwei Reserve) und ein gemeinsames
Amphenol-USBC2USB1MW-C-Servicekabel. Die angegebenen Zeilensummen ergeben
**83,21 EUR netto + 15,81 EUR MwSt. = 99,02 EUR brutto**. Versand 0 EUR,
Express/DDP und „Versand sofort“ sind von Root im Checkout berichtet, nicht
unabhängig vom Reviewer live bestätigt. Kein verbindlicher Ankunftstag ist
belegt; vor Absenden erneut prüfen und keine spätere Nachlieferung stillschweigend
akzeptieren. Akku, Lader, Drucker und Montage-/Druckmaterial sind nicht in diesen
99,02 EUR enthalten. Die Beschaffungseinschätzung bleibt auf den Prototypensatz
begrenzt; tatsächlicher Kauf benötigt die bezeichnete Nutzerautorisierung.

| Zusätzlich nachkontrollierter Stand | SHA256 |
| --- | --- |
| [Planung](PLANUNG.md) | `6E2EF677ED873B6BF1079D3238D1226F6F23626A4935139AC0E91963FE4E5EE0` |
| [Bestellvorschlag](BESTELLUNG.md) | `6820F6E3C2B1C873B12F67BA5F6351B5C183190F0F97D6B0C4A03E652AB646E6` |
| [Mechanikmanifest mit 22 Dateien](mechanik/SHA256.json) | `998193DE5352BCE5D534EF8E8A8D5D6BFD8DC439B04CAAA5346D5DE452966BC9` |
