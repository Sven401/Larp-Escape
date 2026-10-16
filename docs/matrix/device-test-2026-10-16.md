# Tastenmatrix: erster gemeinsamer Gerätetest

Stand: **16. Oktober 2026, Europe/Berlin**. Geprüftes App-/Firmwarepaar:
`c613acebfb07805d1c363b52a325721f2b6f8077`, Matrixumgebung `larp-matrix-test`.
**Installation belegt; kein Matrixkontakt erkannt und keine physische Zuordnung
bestätigt. Ursache offen.** Der Nutzer verschiebt den Klemmenabgleich in eine neue
Sitzung. Der [Testplan](test-preparation.md) bleibt die Ablaufgrundlage; die unten
beschriebene IO7-Einschränkung begrenzt seine bisherige Hardwareannahme.

## Installation und aktueller Gerätestand

Aus Installationsprotokollen ausgelesen: bisheriges APK gesichert; vollständiger
4-MiB-Flashabzug erstellt und am identifizierten ESP verifiziert. Bisherige
Wake-Anwendung, Partitionstabelle und Schlüsselpartition mit dem Rückweg
abgeglichen. Zuerst APK mit Datenerhalt ersetzt und installiertes APK
zurückgelesen/hashgeprüft; danach ausschließlich die Anwendung ab `0x10000`
geschrieben. Anwendung, Partitionstabelle und Schlüssel anschließend separat
verifiziert. Der vorhandene Boxzugang funktionierte ohne erneuten Schlüsselimport.

| Artefakt | SHA-256 |
|---|---|
| Installiertes Matrix-APK, 23.264.146 Byte | `fbdb0439aef6eb950b39c9eaf1b042865654e0865e46cce36ae91c5dc6910cd6` |
| Installierte Matrix-Firmware, 1.139.840 Byte | `50accb8ddb3e14a30e196a7f7dc6f479206d6e3a22e7f32337c8ac2a0c2f60ce` |
| Verifizierter Vorab-Vollflash, 4.194.304 Byte | `d82af581f2d09d50f3a0645f8b159bfadc77ce3f336897dd906cb40925788aa8` |

**Installiert bleibt dieses Matrixpaar. Es wurde kein Rückspielen der
Wake-Firmware durchgeführt oder nachgewiesen.** Der gesicherte Rückweg ist
vorhanden; seine Existenz ist kein Restore-Nachweis. Die beobachtete Wake-Anordnung
war oben **2 / 3**, unten **4 / 1**; sie wurde nicht geändert.

## Getrennte Nutzer- und Appbeobachtungen

| Schritt | Nutzerangabe | Frische Appbeobachtung / Ergebnis |
|---|---|---|
| M1, Ruhe | Alle Matrixkontakte frei | „Live · elektrische Kontaktkoordinaten“, „Beobachtete Kontakte: keine“. Nur eine leere Ruhemomentaufnahme; ohne positive Kontrolle kein Funktionsnachweis. |
| M2, unten links | Ein Kontakt unten links gehalten | Zwei UI-Abzüge jeweils Live und keine Kontakte; keine Koordinate zugeordnet. |
| M2, unten links wiederholt | Magnetischer Matrixkontakt; Magnet direkt am Kontakt unten links | Nach Rückkehr zur App zunächst getrennt/unbekannt, dann authentifiziert wieder verbunden: erneut Live und keine Kontakte. |
| Zweiter Kontakt, oben rechts | Magnet oben rechts; Matrix besteht aus Reedkontakten und funktionierte laut Nutzer im Ursprungsprojekt | Erneut Live und keine Kontakte. Erfolgreicher historischer Binär-/Quellstand nicht bestimmt; kein physischer Defekt nachgewiesen. |

Die erste Wake-Gegenprüfung nach M2 war nicht auswertbar, weil eine andere App im
Vordergrund lag. Spätere Abzüge zeigen alle vier Wake-Eingänge frisch LOW. Das ist
keine erneute Wake-Betätigungsabnahme. M3–M6, die vollständigen M7/M8-Prüfungen und
der LED-Start/Stop-Gegentest M9 sind offen; eine Wiederverbindung wurde lediglich
im Diagnoseablauf beobachtet. Keine der 81 logischen Positionen wurde zugeordnet.

## Codebefunde und offene Ursache

**Belegt durch Quellvergleich:** Der ursprüngliche Scanner, unter anderem
`61ef6c6` vom 19.02.2025, treibt `COLMAP` LOW, liest `ROWMAP` mit Pull-up und
treibt die Spalte anschließend HIGH. `c613ace` treibt dagegen `ROWMAP` LOW und
liest `COLMAP` mit Pull-up. Beispiel A9: früher `0x20 B1` LOW / `0x20 B0` lesen;
jetzt `0x20 B0` LOW / `0x20 B1` lesen. Der Strompfad ist umgekehrt. In `0472a9e`
nennt ein Kommentar den `colMapping`-Ausgang irreführend „row pin“. Der Nutzer
bestätigte zunächst Dioden mit Ring an der „Zeilenleitung“; deren konkrete
MCP-Klemme ist weiterhin unbekannt. Eine Unverträglichkeit der neuen
Abfragerichtung ist **vermutet, nicht physisch bestätigt**.

Ebenfalls belegt: `61ef6c65206b1fcd5656ac64825d22a518396f99` änderte Spalte I
von `0x20 A7` auf `0x20 B5`. Die gegenwärtige Zuordnung folgt B5. Dieser separate
historische Unterschied erklärt ohne Klemmenabgleich keinen generellen Ausfall.

**Belegter Registerkonflikt, Korrektur offen:** Das
[Microchip-Datenblatt DS20001952D, Abschnitt 3.5.1, Seite 18](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-Data-Sheet-DS20001952.pdf)
verlangt für MCP23017 IO7 die Richtungsstellung Ausgang. `MatrixInputs` gibt
`0x21 B7` jedoch über IODIR=Eingang frei. B7 lediglich nicht als GPIO-Eingang
auszulesen reicht somit nicht als regelkonforme Lösung. Die gegenteilige Annahme
aus der Vorbereitung ist unzureichend. Tatsächliche Chipidentität und Einfluss
auf den beobachteten Fehler bleiben offen; dieser Code-/Datenblattkonflikt ist
kein Nachweis der physischen Fehlerursache.

## Nächster konkreter Schritt und Belegorte

In der neuen Sitzung an einem geschlossenen Reedzweig beide tatsächlichen
Expanderklemmen und die Kathoden-/Ringseite bestimmen: Adresse `0x20`/`0x21`
plus A-/B-Pin, nicht nur den geometrischen Namen „Zeile“. Falls eine Messung
nötig ist, spannungsfrei und möglichst vom Expander getrennt prüfen;
Parallelpfade können In-Circuit-Ergebnisse verfälschen. Danach Scanrichtung,
Pinvertrag und IO7-Richtung gemeinsam klären, bevor eine Korrektur getestet wird.
Ein nur teilweise geprüfter Acht-Reihen-Scan darf nicht als global gültiger
81-Positionen-Status ausgegeben werden.

Bereinigte Nachweise und Quellenzuordnung stehen in
[evidence/device-test-2026-10-16.json](evidence/device-test-2026-10-16.json).
Originalprotokolle liegen sitzungsgebunden in `work/evidence/matrix-01`,
Flash-/APK-Sicherungen, Installationslogs und XML-Abzüge in
`work/private/matrix-01`, außerhalb des Projekt-Repositories. Der Bericht wurde
aus diesen vorhandenen Belegen und dem Quellvergleich erstellt; dafür fand kein
weiterer Gerätezugriff statt.
