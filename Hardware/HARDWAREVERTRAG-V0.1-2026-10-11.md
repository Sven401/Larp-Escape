# Vorläufiger Hardwarevertrag v0.1.1 und Messauftrag

**Stand:** 11. Oktober 2026. **Status:** Quellen- und Nutzerbefund, gesperrt für echte
Diagnoseausgänge und Fertigung. **Produktbasis:**
`plan/hardware-revision-schnittstellenabgleich` / `669160f0d05fed4b901b1a2cdbb4549c22b646a4`.
**Änderung v0.1 → v0.1.1:** Der Nutzer hat am 11. Oktober 2026 die
LED-Datenleitung der realen Box an ESP GPIO15 als geprüft gemeldet. Diese
Pinzuordnung ist damit ein nutzerbestätigter As-built-Befund; Prüfmittel,
Messprotokoll und Boxrevision wurden nicht mitgeteilt. Der Vertrag trennt
weiterhin Zeichnung, Firmwarekonfiguration und reale Box. Er bestätigt keine
LED-Funktion oder übrige bestückte Revision.
Er ändert keine Firmware- oder App-Fähigkeit.

## Quellenrang und Schnittstellen

Die editierbaren Eingänge, ihre Hashes und Zuordnung stehen in
[QUELLENINVENTAR.md](QUELLENINVENTAR.md) und
[SCHNITTSTELLENABGLEICH-2026-10-10.md](SCHNITTSTELLENABGLEICH-2026-10-10.md).
Die geometrische Auswertung in `pruefung/quellnetze-2026-10-10.json` ist keine
CAD-generierte Netzliste. F0 `e973818`, serielle Diagnose `db52d7a` und der
App-/Firmware-Plan der getrennten KI-Werkbank wurden erneut verglichen.

| Signal/Funktion | Editierbare Quelle bzw. Export | Firmwarekonfiguration | Vertrag v0.1.1 / Sperre |
| --- | --- | --- | --- |
| LED-DATA | Altes EasyEDA-JSON: `LED-G4:2 DATA` und `U3:26 GPIO4` im selben gezeichneten Netz. V2 zeigt `G4` an DATA und GPIO4, hat aber keine zugeordnete editierbare Quelle. | F0 `src/main.cpp`: WS2811, GRB, GPIO15, 23 LEDs; `src/HardwareAcceptance/main.cpp` bei `db52d7a` ebenfalls GPIO15. | **Reale Box laut Nutzerprüfung vom 11.10.2026: LED-DATA an GPIO15.** Die GPIO4-Zeichnungen widersprechen dem gemeldeten As-built-Stand und bleiben historische Entwürfe. GPIO4 ist PN532-SS im Firmwarepfad. Kein Pinwechsel. LED-Typ, Signalqualität, Versorgung und Funktion offen. |
| LED-Versorgung | Altes JSON zeichnet 5VIN, ESP-V5, DFPlayer-VCC und LED-Klemme Pin 1 auf einem +5-V-Netz, Pin 3 an GND. | FastLED-Grenze 5 V/1000 mA. | Gezeichnete Verbindung und Softwarelimit sind weder Versorgungsspannung noch Netzteilbemessung. Quelle, Rückleiter, Schutz, USB-Rückspeisung und Lastreserve offen. |
| Wake 1–4 | Expanderquelle führt B7/B6/B4/B3 auf dreipolige Klemmen; keine bestätigte Taster- oder MCP-Chip-Verdrahtung. | MCP `0x20` B7/B6/B4/B3, `INPUT_PULLUP`, HIGH als aktiv. | Rohpegel, aktive Polarität und physische Nummerierung getrennt protokollieren. B7 als Eingang bei einem **MCP23017** unzulässig; Chipmarking offen. |
| 9×9-Matrix | Expanderquelle benennt A-/B-Klemmen, enthält aber keinen vollständigen nachgewiesenen Matrixkabelbaum. | Zeilen 1–8 MCP `0x21` B7…B0, Zeile 9 MCP `0x20` B0; Spalten MCP `0x20` B1/A0…A6/B5, während des Scans LOW/HIGH. | MCP `0x21` B7 wäre beim MCP23017 ebenfalls ein unzulässiger Eingang. Richtung, Pull, Ruhepegel, 81 Felder und Mehrfachbetätigung offen. |
| Expanderbus | Modulquelle CN1/CN2: 1 GND, 2 VCC, 3 SDA/SI, 4 SCL/SCK. V2-Export nennt GPIOEXPANDER ohne belegte ESP-Netzzuordnung. | Standardprofil SDA21/SCL22, I²C-Adressen `0x20`/`0x21`. | VCC-Pegel, Pull-ups, Adressjumper, Steckerorientierung und tatsächliche Leitungen offen. Busantwort allein belegt keine Eingangsverdrahtung. |

Das Microchip-Datenblatt [DS20001952D](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-Data-Sheet-DS20001952.pdf),
Seite 1 und Pin-Tabelle 2-1, nennt GPA7/GPB7 beim MCP23017 als reine
Ausgänge. Das gilt nicht pauschal für andere Bauteile mit ähnlichem Namen.
Der PCB-1.0-Plan der KI-Werkbank verlangt deshalb für beide B7-Eingänge einen
neuen, gemeinsam mit Firmware geprüften Pinvertrag vor Netzliste und Layout.

## Abhängigkeit des späteren Diagnose-LED-Schritts

Der Hardwarestatus der Fähigkeit `led_single` bleibt **unbestätigt**, obwohl
LED-DATA an GPIO15 der realen Box nutzerbestätigt ist. Firmware und App dürfen
andere Pins nicht aus einem Dateinamen, Label oder Export wählen. Für den
ersten echten Schritt sind erforderlich: identifizierte LED-Baugruppe;
Versorgung und gemeinsame Masse; passender Pegel gemäß den Datenblättern der
tatsächlich verbauten Teile;
produktiver GATT-/Rollenbesitzer und exklusive Übergabe vom Spiel;
zeitbegrenzte sichtbare Testfreigabe; unabhängiger 500-ms-Aus-Timer und
bestätigter Auszustand; korrelierte Geräteantwort. Erst dann kann die App einen
einzelnen gedimmten LED-Auftrag mit separater Nutzerbeobachtung anbieten.
Ein `show()`-Aufruf, Framezähler oder die Android-Vorschau ist kein Sichtnachweis.

## Auftrag für den späteren nutzergeführten As-built- und App-Lauf

Dieser Auftrag ist **Vorbereitung**, kein Startsignal. Die unterbrochene
Originalboxprüfung bleibt unterbrochen. Der Nutzer führt den Lauf nach
Bereitstellung und Abnahme des sicheren App-/Firmware-Pfads selbst aus und
exportiert den abgeschlossenen Bericht. Elektrische Messungen erfolgen nur an
spannungsfreiem Aufbau bzw. bei offen zugänglichen, berührungssicheren
Messpunkten durch eine dafür befähigte Person; vor jeder Durchgangsmessung
Versorgung und USB trennen. Kein Codex-gesteuerter Test, Upload oder Restore.

1. **Identität:** Fotos der Box, Platinenrückseiten, LED-Erstbaustein-Markierung,
   ESP-, MCP-, Netzteil- und Steckermarkings, Kabelrichtung und Pin 1 dem
   Bericht zuordnen. Firmware- und App-Version sowie Boxkennung erfassen.
   Die Nutzerbestätigung GPIO15 mit Datum und Herkunft übernehmen; keine
   erneute Durchgangsmessung als Startbedingung verlangen. Die Zuordnung
   anderer Pins und die genaue Boxrevision bleiben offen.
2. **Versorgung vor Last:** Einspeisung, GND-Rückweg, 5-V- und 3,3-V-Rails und
   mögliche USB-Rückspeisung als Schaltskizze erfassen. Mit den Markings der
   tatsächlich verbauten Module zulässige Spannungs-, Eingangspegel- und
   Stromgrenzen festlegen. Erst nach bestandenem, strombegrenztem Einschalt-
   und Leerlaufcheck die Spannungen am ESP-VIN, an der LED-Klemme und am
   ersten LED-Modul messen. Ein Messwert außerhalb einer belegten
   Herstellergrenze, Einbruch beim kurzen Lastschritt, unerwartete
   Rückspeisung oder nicht erklärter Stromanstieg führt zu **Stopp/Fehler**.
   Mangels identifiziertem Modul gibt es in v0.1.1 keine erfundene absolute
   Spannungs- oder Stromschwelle. FastLEDs 1-A-Limit ist keine solche Schwelle.
3. **Datenpegel erst nach Gates:** Falls der Signalpegel gemessen wird,
   während eines einzigen von der App ausgelösten, gedimmten und auf
   höchstens 500 ms begrenzten LED-0-Schritts mit geeigneter hochohmiger
   Sonde GPIO15 und LED-DIN samt GND-Bezug und LED-Versorgung erfassen.
   Der High-Pegel an DIN muss die im Datenblatt des identifizierten LED-Typs
   geforderte `VIH(min)` erreichen und `VDD`/absolute Grenzwerte einhalten;
   sonst **Fehler**, ohne passende Teileangabe oder Messung **unklar**. Bleibt an GPIO15
   Aktivität aus, ist der Firmware-/Gatepfad zu prüfen; liegt sie am ESP an,
   aber nicht an DIN, ist der Signalweg zu prüfen. Dieser optionale Pegeltest
   wiederholt die bereits bestätigte Pinzuordnung nicht und beweist keine
   sichtbare LED-Funktion.
4. **Sicht und Aus:** Die App fragt Farbe/Position von LED 0 und den sichtbaren
   Auszustand nach Fristablauf separat ab. Nur korrelierte Geräteantwort,
   belegte elektrische Grenzen und passende Nutzerbeobachtung können diesen
   Schritt bestehen lassen. Fehlende Beobachtung, fehlende Messung oder
   uneindeutige LED-Zuordnung bleiben **unklar**; keine sichtbare Änderung
   ist **fehlgeschlagen**. Bericht samt Rohwerten, Zeit, Testmitteln, Fotos
   und Fehlerstatus exportieren; erst eine spätere Sitzung wertet ihn aus.

**Offen für v0.2:** genaue Bestückungs- und Kabelbaumrevision, echte Versorgungswerte,
LED-Modul und Pegelanforderung, beide MCP-B7-Neuzuordnungen, Matrix-/Wake-
Polarität, CAD-Netzliste, ERC, PCB-Quelle und DRC. Dieser Quellenabgleich
enthält keine CAD-Designregelprüfung und keine Fertigungsfreigabe.
