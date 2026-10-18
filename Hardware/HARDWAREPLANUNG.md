# Hardwareplanung nach den geführten Baugruppentests

Stand: **18. Oktober 2026**. Status: **belegte Teilbefunde übernommen;
elektrische Gesamtfreigabe und Fertigungsfreigabe offen**.
Auftrag: bestätigte Ergebnisse der geführten Tests in die Hardwareplanung übernehmen.
Quellenprüfung am Integrationsstand `d61f77fe753c1a837c13a67edb282d6a79610a30`;
in dieser Sitzung kein neuer Gerätetest und keine elektrische Messung.

Diese Planung führt den [vorläufigen Hardwarevertrag](HARDWAREVERTRAG-V0.1-2026-10-11.md)
fort. Das [Nachweisregister](../handoff/BAUGRUPPEN-NACHWEISE.md) hält die festen
Quellstände und Sicherungsreferenzen; die Berichte bleiben die Originalbelege.
[H0 / #39](https://github.com/Sven401/Larp-Escape/issues/39) führt die
Hardwarezuordnung, [H1 / #54](https://github.com/Sven401/Larp-Escape/issues/54)
die gezielte Matrixnachprüfung. Kein neues Hardwaredesign aus alten Exporten ableiten.

## Übernommene Ergebnisse und Konsequenzen

Alle folgenden Ergebnisse sind **aus den Originalberichten ausgelesen und gegen
das Nachweisregister belegt**; Nutzerangaben sind als solche gekennzeichnet.
Die Datums-/Codestände gelten für den damaligen Versuch, nicht als Aussage über
die heute tatsächlich installierte App/Firmware.

| Ergebnis / Datum / Prüfcode | Anerkannter Befund | Konsequenz für Hardwareplanung und Grenze |
| --- | --- | --- |
| **LED-01**, 15.10., `7d1f45b` | [Sichtprüfung](../docs/led/device-test-2026-10-15.md): vorhandener Streifen mit 23 LEDs an GPIO15; Funkeln, Stop, weißer Lichtimpuls und blauer Ripple mit anschließendem Dunkel bestätigt. | GPIO15 als As-built-Zuordnung erhalten. GPIO4 aus alten Zeichnungen nicht übernehmen. Sichtbare Funktion ist belegt; Chiptyp, tatsächliche Farbfolge aller Pixel, Pegel, Laststrom und Versorgung bleiben offen. |
| **WAKE-01**, 15.10., `ca86b93` | [Vier Positionen](../docs/wake/device-test-2026-10-15.md) funktional zugeordnet: oben links/rechts, unten links/rechts = **2/1/4/3**; untere Positionen teilweise durch dynamische Nutzerbeobachtung. Zwei gleichzeitige Eingänge und gespeicherte Anordnung geprüft. | Funktionalen Vierkanalbefund erhalten. Spätere gespeicherte Anordnung **2/3/4/1** ist eine Bedienkonfiguration, keine neue Verdrahtung. Chipmarking und reale Wake-1/B7-Leitung bleiben offen; keine elektrische Vierkanalfreigabe. |
| **MATRIX-01**, 16.10., Erfassung `20ac696`, Auswertung `724f0bd` | [Erhaltener Sammellauf](../docs/matrix/collection-result-2026-10-16.md): 68 erkannte Koordinaten, 110 registrierte Kontaktfolgen, neun nutzerbestätigt unbestückte Felder, vier Problemstellen. | **C3, G3, H3, A8** einzeln lokalisieren und nachprüfen. **B1, A2, A3, B4, E5, C8, E9, G9, I9** bleiben unbestückt; App-Deaktivierung ist kein Firmwarefilter oder Spielprofil. Zähler sind keine mechanische Betätigungszahl. Raumpositionen und elektrische Fehlerursachen offen. |
| **AUDIO-01**, 16.10., installiert `243f00c`, Produktquellen `958d6dc` | [Hör-/Bedienprüfung](../docs/audio/device-test-2026-10-16.md): nach zusätzlicher Audioversorgung Track 101 hörbar; Pause/Fortsetzen, Lautstärke, Stop/Neustart und R1/R2/R4–R6 bestätigt. BUSY nutzerberichtigt LOW bei Wiedergabe, HIGH bei Stille. [Pinbestätigung](../docs/audio/test-preparation.md): DFPlayer-RX → ESP GPIO26, TX → GPIO25, BUSY → GPIO14. | Drei Leitungen als nutzerbestätigten As-built-Stand übernehmen, ESP-/Modulrichtung ausdrücklich trennen. Zusätzlichen Versorgungszweig in der nächsten Schaltskizze erfassen. Erster stiller Versuch bleibt erhalten; keine vollständige Ursachenklärung für Track 100. GPIO13/WAKE und Verstärker-/Lautsprecherpfad offen. R3 kurzes natürliches Titelende ausdrücklich ausgenommen, keine neue Pflicht dieses abgeschlossenen Laufs. |
| **NFC-01**, 16.10., `d4892bf` | [Vorerkundung](../docs/nfc/device-preparation-2026-10-16.md): zwei PN532-/HCE-Zyklen mit 59 Byte und Flags 1 → 2, Wiederholschutz; Software-SPI SCK16/MISO17/MOSI23/SS4 funktional beobachtet. PN532 meldet Firmware 1.6. | Diese Buskonfiguration als funktionalen Prüfstand erhalten. HCE ist kein physischer Tag; keine Stecker-, Durchgangs- oder Pegelfreigabe. IRQ/Reset wurden nicht genutzt. |
| **NFC-02**, 16.10., `d4892bf` | [Ein echter NTAG213](../docs/nfc/physical-tag-test-2026-10-16.md): App → Tag → Box per USB → Tag → App; neue Handylesung bestätigt Nachherdaten. Erstschreiben erst durch zusätzlichen Lesekontakt bestätigt. | NTAG213 als begrenzten physischen Referenzfall erhalten. 137 Byte Android-NDEF-Kapazität und 144 Byte CC-Datenfläche nicht gleichsetzen. Kein zweiter Rundlauf oder Nachweis für weitere Tagtypen. |
| **NFC-03**, 16.10., App `c0f76ee`, Box Wake unverändert | [Erstschreibkorrektur](../docs/nfc/first-write-readback-2026-10-16.md): weiterer leerer NDEF-Tag am moto g14 nach neuer Erkennung mit gleicher UID und allen 59 Byte bestätigt, ohne zweites Schreiben. | Leere unmittelbare Android-Rücklesung nicht als Hardwaredefekt einstufen. Unklares Schreibergebnis durch frische Lesung klären; Herstellerursache bleibt teilweise abgeleitet. |
| **NFC-04**, 16.10., `dc746ec` | [Vollständiger App-Rundlauf](../docs/nfc/app-device-test-2026-10-16.md): Boxaufträge über echte App; Flags 1 → 2 mit Boxrücklesen, danach neue unabhängige Handylesung derselben UID und vollständigen Nachher-Fixture. | App-/Boxpfad funktional erhalten; Typ dieses Tags nicht gesondert bestimmt. Zweiter Lauf, falscher Tag, Abbrüche sowie Spielintegration bleiben offen. |

## Aktueller Quellenvertrag

Am oben genannten Integrationscommit ausgelesen:

- [`LedOutput.h`](../firmware/led/include/LedOutput.h): GPIO15;
  [`AudioOutput.h`](../firmware/led/include/AudioOutput.h) und
  [`DfPlayerBus.cpp`](../firmware/led/src/DfPlayerBus.cpp): ESP-RX25,
  ESP-TX26, BUSY14; GPIO13 bleibt in diesem Baugruppenpfad unberührt.
- [`WakeInputs.h`](../firmware/led/include/WakeInputs.h) und
  [Implementierung](../firmware/led/src/WakeInputs.cpp): `0x20`, B7/B6/B4/B3,
  HIGH aktiv, Pull-ups. Die Konfiguration ist kein elektrischer Eignungsnachweis.
- [`MatrixInputs.cpp`](../firmware/led/src/MatrixInputs.cpp): Spalten
  `0x20 B1/A0–A6/B5` einzeln LOW, inaktive Spalten hochohmig;
  Zeilen `0x21 B7–B0` und `0x20 B0` mit Pull-up lesen. Die ursprüngliche
  Scanrichtung ist erhalten. [Korrekturbeleg](../docs/matrix/scan-direction-correction-2026-10-16.md).
  Das explizite historische IO7-Opt-in in
  [`MatrixInputs.h`](../firmware/led/include/MatrixInputs.h) löst den Chipkonflikt nicht.
- [`Pn532Spi.cpp`](../firmware/nfc-probe/src/Pn532Spi.cpp): SCK16/MISO17/MOSI23/SS4;
  IRQ/Reset bleiben unbenutzt. GPIO34 wird nicht als Ausgang betrieben.

Die alten GPIO4-LED- und GPIO16/17-Audiozeichnungen beschreiben damit nicht
den bestätigten As-built-/Funktionsstand. Sie bleiben Herkunft, kein Anlass zum
Umpinnen. Ein neuer CAD-Vertrag muss diese Unterschiede ausdrücklich auflösen.

## Verbleibende Hardwarearbeit

Die folgende Reihenfolge konkretisiert #39/#54; **geplant, nicht ausgeführt**.
Bestandene Normalfälle werden nur bei einer Änderung wiederholt, die ihre Aussage berührt.

| Reihenfolge / Zuständigkeit | Nächster konkreter Schritt | Abschlussbeleg |
| --- | --- | --- |
| 1 · H0 / #39, Hardware | Vorhandene Boxrevision, ESP-/MCP-/LED-/Audiomodulmarkierungen, Steckerorientierung und Versorgungszweige zuordnen. ESP32-D0WD-V3 Rev. 3 / 4 MiB ist historisch dokumentiert; heutigen Aufbau nicht daraus voraussetzen. | Zuordnungsblatt mit Fotos/Markings und Quellbezug. Zwei B7-Fragen getrennt: Wake 1 an `0x20 B7`, Matrixzeile 1 an `0x21 B7`. Falls identifizierter Chip diese Eingänge nicht zulässt: gemeinsam mit Firmware neuen Pinvertrag festlegen, vor Netzliste/Layout. Kein Pinwechsel allein aus funktionaler Reaktion. |
| 2 · H1 / #54, Hardware + nutzergeführte Nachprüfung | C3/G3/H3/A8 räumlich lokalisieren, Kontakt-/Kabelbefund vor/nach gezielter Reparatur dokumentieren. Vor Prüfung tatsächliches App-/Firmwarepaar und gespeicherten Lauf zuordnen. | Bestehenden Lauf fortsetzen; jeden Kontakt einzeln mehrfach erkennen und nach Loslassen erneut prüfen. 68 Treffer samt bisherigen Zählern/Zeiten sowie neun unbestückte Felder erhalten. Neue Ergebnisse ergänzen; Problemstatus nur mit Nachweis auflösen, Export und App-Neustart prüfen. Kein pauschaler Vollrundgang. |
| 3 · H0 / #39, Hardware | Einspeisung einschließlich zusätzlicher Audioversorgung, gemeinsame Masse, Rückleiter, USB-Rückspeisung, Schutz und Lastreserve erfassen; reale Rails und Signalpegel gegen die identifizierten Teile prüfen. | Benannte Messpunkte/Testmittel, Leerlauf-/Lastwerte und Herstellergrenzen. Softwarebudgets, Powerindicator, BUSY und hörbare/sichtbare Funktion ersetzen keine elektrische Messung. |
| 4 · H0 / #39, CAD | Zugeordnete editierbare Revision und tatsächlichen Kabelbaum nachführen; Audio-RX/TX/BUSY, LED15, PN532 und Expanderanschlüsse getrennt von alten Entwürfen abgleichen. | Native CAD-Netzliste, vollständige BOM/Moduldetails, ERC, PCB-Quelle und DRC mit Werkzeug-/Revisionsstand. Bis dahin keine Fertigungsfreigabe. |
| 5 · #31 sowie #61/#63, gemeinsame Betriebsabnahme | Nach lokalem Abgleich gemeinsame Gerätebesitzer, Last, Ressourcen und Dauer-/Ausfallbetrieb prüfen; für verteilten Betrieb konkreten ESP-/SDK-/Antennen-/Versorgungsstand zuordnen. | Eigene Messbelege für lokalen Vollbetrieb und später BLE-/ESP-NOW-/WLAN-Koexistenz. Die im [Architektur-PR #64](https://github.com/Sven401/Larp-Escape/pull/64) dokumentierte dezentrale Architektur ändert die lokalen Teilbefunde nicht. Umsetzung und verteilte Abnahme bleiben offen. |

## Abnahme- und Veröffentlichungsgrenze

Historische Teilabnahme, integrierte V2-Software, gemeinsame Geräteabnahme und
elektrische/CAD-Freigabe bleiben getrennt. Das gemeinsame V2-Paar wurde in dieser
Planungssitzung weder installiert noch getestet. Letzter hier dokumentierter
Gerätenachweis ist NFC-04; vor einem späteren Lauf tatsächlichen Stand feststellen.
Vor gesondert beauftragtem Upload den chipgebundenen Flash-Rückweg nach der
in #39 verlinkten historischen Hardwareanleitung abgleichen.

Prüfweg dieser Übernahme: Originalberichte, Nachweisregister, Matrix-CSV/JSON und
die oben verlinkten Adapterquellen am Basiscommit abgleichen; relative Dokumentlinks
und Diff prüfen. Unveränderte Software wurde hierfür nicht neu gebaut oder am Gerät geprüft.

**Gemessen am 18.10.2026 auf dieser Dokumentfortschreibung:** alle 44 relativen
Linkziele der fünf geänderten Dokumente vorhanden; Matrix-CSV/JSON stimmen mit
81 Feldern, 68 Treffern, 110 Kontaktfolgen, den neun benannten unbestückten
Feldern und den vier benannten Problemstellen überein. Diff-Leerzeichenprüfung
erfolgreich. Der veröffentlichte Dokumentcommit bezeichnet diese Fortschreibung;
er ist kein neuer Hardwareprüfstand.
