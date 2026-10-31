# Kompaktgehäuse: erster stromloser Passprototyp

Stand31.10.2026, Basis483c3320 plus bestätigte Modulauswahl. Außenmaß des
Gehäusekörpers **66 × 34 × 38,4 mm**, zuzüglich der gewählten Schraubenköpfe
(bei1,6-mm-Köpfen maximal40 mm Höhe). Das ist ein praktisch gepackter
Erstentwurf, kein mathematisch bewiesenes globales Minimum. Gegenüber R1 ist
das umschreibende Körpervolumen von2238 auf86,17 cm³ reduziert.

**Freigabeumfang: digitale Geometrie eines stromlosen Passprototyps.**
`mechanik/geometry-report.json` prüft sechs zusammenhängende gültige Körper,
geschlossene STL-Netze, STEP-Rückimport und fehlende Kollisionen der angegebenen
Hüllen. Es gibt noch keinen realen Druck, keine Kontakt-/Verpolungs-, Funk-,
Wärme-, Optik- oder Vierstundenabnahme. Diese realen Tests folgen der ersten
Prototypen-Teilebestellung; sie sind keine zirkuläre Voraussetzung für deren Kauf.

## Eingepackte Baugruppen

| Baugruppe | Geometrischer Ansatz |
| --- | --- |
| Keeppower P1835C3 | Maximalhülle Ø18,75 ×39,0 mm; nominal38,7 mm, kleinsteLänge38,4 mm. Zelle liegt quer zur Gehäusehöhe, längs der66-mm-Achse. |
| Mill-Max0906-2-15-20-75-14-11-0, zwei | Freie Kontaktlänge5,0038 mm plus1,7526-mm-Lötstift; FlanschØ1,8288 mm. Kontakte werden durch getrennte gedruckte Klemmen gehalten, nicht an die Zelle gelötet. |
| PowerBoost1000 Basic2030 |29 ×23 mm; konservative4,5-mm-Z-Hülle für SMD/Lötung trotz Herstellerangabe2 mm. Die lose JST-Buchse, USB-A und Klemme **nicht** montieren. Direkte isolierte zugentlastete Litzen gemäß elektrischem Vertrag. |
| QT Py S3 5700 |21,7 ×17,8 ×5,7 mm inklusive USB; ESP-Board erfüllt25 ×20 mm. USB-C-Mund liegt an der vorderen Wand;12 ×8-mm-Steckerraum ist im Bericht reserviert. |
| 5×5 NeoPixel BFF5646 |20,9 ×17,9 ×2,8 mm, separat über QT Py mit isolierendem Abstand.25 physische Pixel; tatsächliche Aktivierung folgt dem Hardware-/Firmwarevertrag. Keine langen Stift-/Buchsenleisten verwenden. |
| E-Switch EG1218 | Körper11,6 ×4 ×5,4 mm, Pins4,7 mm, Aktuator2 mm und2,5-mm-Weg. Körper seitlich montiert, Pins nach innen; damit12,1-mm-Gesamtausdehnung entlang der Betätigungsachse berücksichtigt. Nur EN schalten, kein Akku-Hauptlaststrom. |

Die Modulmodelle sind konservative Hüllen aus Herstellermaßen, keine vollständig
detaillierten Bauteil-STEPs. Gedruckte Anlageflächen sind gegen echte Platinen-
revisionen und Lötstellen zu prüfen. Die flexible Verdrahtung ist räumlich
vorgesehen, aber kein vollständig dreidimensional vermessener Kabelbaum.

## Teile des Gehäuses

Pro Gerät einmal `base`, `tray`, `lid`, `diffuser`, `contact_cap_negative` und
`contact_cap_positive`. STL-Dateien liegen einzeln aufZ0. STEP/FCStd behalten
gemeinsame Montagekoordinaten. Der herausnehmbare isolierende Träger liegt über
dem Akku. Er trägt PowerBoost links sowie QT Py und LED-Modul rechts; für den
Zellwechsel wird er angehoben. Keine Kabel in den Hubweg der Kontakte legen.

Für zwei Geräte: sechsM2×8-Schrauben für Deckel und achtM2×10 für Kontaktklemmen,
je14M2-Sechskantmuttern (AF4 mm, Höhe höchstens1,6 mm), plus kleine Reserve.
Die Aussparungen sind4,3 mm weit; Schrauben nicht mit Kraft in PETG ziehen.
M2×10-Kontaktschrauben enden mindestens2 mm über dem inneren Boden; keine
längeren Schrauben als Ersatz. Gehäuse- und Kontaktkopfformen beim Coupon prüfen.
Ein4 mm breites isolierendes Textilband unter der Zelle dient als Entnahmehilfe;
je Gerät etwa70 mm Länge, keine metallischen Heraushebelwerkzeuge.

Schraubenköpfe der Kontaktklemmen dürfen keine Zellenfolie berühren. Lötenden
der Pogo-Kontakte und alle unbenutzten Schalterpins isolieren. Kabel beidseitig
gegen Zug sichern; keine Lötstelle darf als Zugentlastung dienen. Der Träger
muss ohne Kabelzug geöffnet werden können; Leitungen dafür individuell ablängen.

## Kontakt- und Verpolungsgrenze

Freie Kontaktspitzen stehen nominell37,4 mm auseinander. Bei nominaler Zelle
ergibt das0,65 mm Hub je Kontakt. Mit Zelllängentoleranz±0,3 mm, vorläufiger
gesamter Sitz-/Drucktoleranz±0,4 mm und zweimal±0,1524 mm Kontaktlängentoleranz
ergibt sich rechnerisch etwa0,1476–1,1524 mm je Pin. Das liegt unter dem aus
`.055 ±.005 inch` abgeleiteten kleinsten Vollhub1,27 mm, setzt jedoch korrekte
axiale Aufnahme voraus. Alle diese Maße müssen am Druckcoupon und an echten
Kontakten geprüft werden; die breite Aufnahmekammer ist **kein** zugesicherter
Präzisions-Presssitz. Kontaktlage bei Bedarf durch geeignete isolierende
Passstücke einstellen, danach fest einklemmen und den Hub erneut kontrollieren.

**Der aktuelle CAD-Satz enthält noch keinen verifizierten Pluspol-Guard.**
Die minimal garantierte Buttonhöhe und der Buttondurchmesser der gelieferten
Zelle sind nicht belegt. Das Gehäuse darf daher nicht als verpolungssicher
bezeichnet oder allein aufgrund dieses CAD-Checks bestromt werden. Nach Ankunft
Zell-/Kontaktmaße bestimmen und einen passenden Pluspolanschlag ergänzen oder
eine unabhängig wirksame elektrische Verpolschutzlösung einsetzen. Die
vertauschte Einsetzrichtung vor dem ersten Einschalten stromlos prüfen.

Die Zelle wird von oben entnommen, nachdem Deckel und Träger entfernt wurden.
Kontakte dürfen dabei nicht seitlich verbogen oder vollständig auf Block
gedrückt werden. Keine blanke Minus-Zellenhülle mit einem positiven Lötstift
berühren lassen. Laden ausschließlich im vorgesehenen externen Ladegerät,
nicht über QT-Py-USB oder PowerBoost. USB-C dient dem Service.

## Antenne und Optik

QT Py hat eine Johanson-Chipantenne. Die15-mm-Empfehlung für Espressif-Modul-
PCB-Antennen wird nicht pauschal als Kunststoffwandabstand angesetzt. Die
vorgegebene Platinenorientierung legt das Antennenende an die freie rechte
Endzone; keine zusätzliche Schraube, Magnetplatte oder Kabelschlaufe dort
einbauen. Der Original-QT-Py-Layoutbezug ist dokumentiert, jedoch die endgültige
Funkreichweite im geschlossenen/benutzten Gehäuse ausdrücklich noch nicht geprüft.
Die LED-Platine wird separat verdrahtet, nicht unbesehen über alle Headerpins
aufgesteckt. Die maßliche Packung ersetzt keinen elektrischen Strompfadvertrag.

Der transparente/naturfarbene PETG-Diffusor ist0,8 mm dick. Sein unterer Rand
liegt36,75 mm über dem Gehäuseboden, die LED-Hülle endet33,0 mm: etwa3,75 mm
Lichtmischabstand. Punktgleichmäßigkeit und Goldhelligkeit brauchen die echte
Lichtprobe; keine optische Simulation wird behauptet. Die Vorschaubilder zeigen
nur Bauraum und Teile, die Diffusorfarbe ist illustrativ.

## Reproduzieren und erste Druckprobe

```powershell
& 'C:/Program Files/FreeCAD 1.0/bin/python.exe' Hardware/naehe-glut/kompakt/build_mechanics.py
& 'C:/Program Files/Blender Foundation/Blender 4.0/blender.exe' -b --python Hardware/naehe-glut/kompakt/render_mechanics.py
```

`build_mechanics.py` ist die editierbare Konstruktionsquelle; die nativeFCStd
enthält Körper und eine Maßtabelle, keine automatisch verknüpfte parametrische
Feature-Historie. Änderungen am Quellparametermodell gemeinsam neu exportieren.

Zuerst nur Kontakt-/Mutter-/Schraubensitz und USB-Steckerraum als stromlose
Passprobe prüfen. Ganze Teile mit0,4-mm-Düse,0,2-mm-Schichten, PETG und geeignetem
Materialprofil neu slicen; noch keine eigenen Slicerprojekte oder Druckzeiten
für diesen Kompaktentwurf vorhanden. Unterwanne mit Öffnung oben drucken,
Deckel mit Außenseite auf dem Bett, Träger und Kontaktklemmen auf ihre flache
Unterseite. Die0,8-mm-Diffusorscheibe separat drucken.

## Primärquellen

- [QT Py5700](https://www.adafruit.com/product/5700), [Original-QT-Py-Layout](https://github.com/adafruit/Adafruit-QT-Py-ESP32-S3-PCB).
- [BFF5646](https://www.adafruit.com/product/5646), [BFF-Layout](https://github.com/adafruit/Adafruit-5x5-NeoPixel-Grid-BFF-PCB).
- [PowerBoost2030](https://www.adafruit.com/product/2030): lose Steckverbinder und Maße.
- [Mill-Max0906-2 Maßblatt](https://www.farnell.com/datasheets/3189521.pdf): tatsächlich als Bild geprüft.
- [EG1218 P040040RevC](https://assets.rs-online.com/image/upload/v1760392285/Datasheets/2c2f50e74acdd061328cc429d96b63ce.pdf): tatsächlich als Bild geprüft.
- Zell- und Elektrovertrag: `TECHNIK-ENTWURF.md`/aktuelles Hardwareblatt im selben Verzeichnis.
