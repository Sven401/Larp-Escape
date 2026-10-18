# Optische LED-Kalibrierung und Gerätevergleich · 18.10.2026

Status: **belegt für den unten bezeichneten Laborstand und Prüfbereich**.
Quelle: vollständiger automatischer Lauf `suite-03`, USB-Ausgabespur der Box
und getrennt erfasste Kamerapixel. Basis `fcb9121` plus die Umsetzung in PR #68.
[Maschinenlesbare Befunde, Indexkarte und Quellfingerabdrücke](fixtures/camera-results-2026-10-18.json).
[Wiederholung und Kriterien](camera-measurement.md).

## Ergebnis

Alle 23 Firmwareindizes 0–22 sind durch einzeln adressiertes Licht zugeordnet
und durch die umgekehrte Folge bei anderer Helligkeit unabhängig bestätigt.
Für jede LED wurden nutzbare, nicht gesättigte Messpixel und Ansprechkurven
für die beiden tatsächlichen Effektfarbfamilien bestimmt. Alle Grundfarben
bestehen die Prüfung auf den erwarteten Kamera-RGB-Kanal. Ein zusätzlicher
optischer Filter war nicht nötig. Die Abnahmegrenzen blieben unverändert.

| Effekt | LEDs bestanden | kleinste Korrelation | größter mittlerer RGB-Fehler | größte geschätzte Markerunsicherheit je LED | finales Aus |
| --- | --- | --- | --- | --- | --- |
| Funkeln | 23/23 | 0.9787 | 4.67 % | 9.20 ms | bestätigt |
| Lichtimpuls | 23/23 | 0.9990 | 1.86 % | 9.12 ms | bestätigt |
| Ripple | 23/23 | 0.9303 | 1.48 % | 9.58 ms | bestätigt |

Fehler beziehen sich auf die jeweilige Kamera-Weiß-32-Referenz, nicht auf
absolute Leuchtdichte. Keine Sättigung innerhalb der festen Messflächen in
allen drei Effektläufen. Dunkel wird mit dem dokumentierten 5-%-Kriterium
bewertet; es ist keine Aussage über exakt null Kameracodes oder Strom.

1280×720 MJPG, rund 30 empfangene Messbilder/s. Die Kalibrierung enthält 1309
Messbilder und 134 vollständige TX_DONE-Ereignisse; größte Empfangslücke
56,05 ms. Aufnahmebereiche werden als verlustfreie PNG-Kacheln mit regelmäßigen
Gesamtbildern gespeichert. Der komplette Lauf belegt lokal rund 311 MiB.
Die reine Pilotübersicht wird absichtlich nur mit etwa 5 gespeicherten Bildern/s
aufbewahrt und wird nicht als zeitliche Effektmessung bewertet.

## Installations- und Rückwegbeleg

Temporäre Firmware: ausschließlich die eigene Umgebung `larp-led-camera-lab`,
mit echter `LedOutput`-Berechnung und tatsächlichem SDK-RMT-Adapter. Keine
Appinstallation und kein BLE-Produktumbau. Firmwarebinärdatei und ELF bleiben
als private Belege erhalten; Laufzeit-Imagegröße und -MD5 sowie esptool-
Verifikation binden die Messung an genau diese Anwendung.

Firmware-SHA-256: `cd3f3d0fb67a0b3f92fd340f24fe6378161c57a839d136c5b0db2a9b9d2bf9e0`.

Vor dem Upload wurden die dokumentierte Box und ihr Partitionslayout geprüft,
ein frischer vollständiger 4-MiB-Flashabzug gelesen und am Chip verifiziert.
Nur die Anwendung bei `0x10000` wurde temporär ersetzt. **Nach dem Lauf ist die
bisherige Anwendung wiederhergestellt und der gesamte 4-MiB-Flash am Chip
bytegleich gegen die Sicherung verifiziert.** Bootloader, Partitionen,
Zugangsdaten und alle anderen Flashbereiche stimmen mit dem Vorzustand überein.

Vorher-/Nachher-Snapshot-SHA-256: `b703f4b13aaaaa3e8c3bc68d8e8134b1cec0a77df957ea06175ac147d1d03c71`.

## Prüfweg und Erkenntnisse

Die ursprüngliche USB-Empfangszeit war durch das Kameralesen gerastert.
Beim ersten vollständigen Versuch lag Ripple an Index 8 als Grenzfall
unter der festen Korrelationsgrenze (0,8991 bei mindestens 0,9000); das ist
kein festgestellter Hardwaredefekt. Dieser frühere negative Lauf bleibt unter
`suite-02` erhalten und wird nicht als bestanden umgedeutet.

Der finale Vergleich nutzt die tatsächlichen relativen TX_DONE-Gerätezeiten.
Optische Marker mit unterschiedlichen Phasendauern bestimmen anschließend
**je LED** die Zeitzuordnung und ihre Intervallgrenzen. Die unabhängigen Marker
werden vor dem Effekt gezeigt; der Effekt selbst wird nicht zum zeitlichen
Nachjustieren verwendet. Die neue vollständig automatische Suite bestätigt
Zuordnung und alle drei Effekte mit unveränderten Fehlergrenzen.

Frühere Vollbild-/Kompressionsversuche und falsch gewählte Aufnahmebereiche
wurden korrekt verworfen. Die finale Regionsaufnahme erhält relevante
Kamerapixel, vermeidet doppelt abgebildete Nachbarlichter und bindet Ergebnisse
über Prüfsummen an Bilder, Ereignisse, Kalibrierung und tatsächliche Quellen.
Ein Fehler des ersten Laboradapters beim Übergang von Anfangsdunkelheit zum
Effekt wurde vor der finalen Messung korrigiert; echte Steuerschleifen-Tests
prüfen seitdem die natürliche Laufzeit aller drei Effekte.

Softwareprüfung: sechs Fälle der tatsächlichen USB-/Engine-Steuerschleife
(vollständige Sequenz, Verbindungswächter, beide Stop-Wege und drei Effekte).
Alle sieben optischen Python-Fälle sowie die sechs Steuerschleifenfälle
bestanden lokal. Die vorhandenen 16 LED-Engine- und vier SDK-Fälle sowie
Wake-/Matrix-/I2C-Prüfungen bestanden ebenfalls am geänderten Arbeitsbaum.
Der Labor-Firmwarebuild war erfolgreich. Optische Tests prüfen zusätzlich
native Geräteintervalle, unabhängige Index- und Pixelzuordnung,
fehlende LEDs, falsche Farbe, ausbleibendes Aus, falsches
Effekttiming sowie reine Kamera- und unterschiedliche LED-Abtastverzögerung.
Der genaue abschließende Softwareprüfstand steht im PR; ältere grüne Läufe
werden nicht auf spätere Quelländerungen übertragen.

Private Rohbelege: `private/led-camera-calibration` im Hauptcheckout, insbesondere
`suite-03`, `rollback.json`, Upload-/Verify-/Restoreprotokolle, finale Binärdatei
und ELF. Unvollständige große HD-Rohsequenzen wurden entfernt; ihre
Fehlermanifeste sind erhalten und sie dienen nicht als Abnahmebeleg.

## Verbleibende Grenzen

Dies bestätigt die optische Wirkung der drei Effektberechnungen im isolierten
Laborbuild. Gemeinsames BLE-App-/Firmwarepaar, Spielintegration, Eingangs- und
Audiobetrieb, Strom-/Signalqualität, PWM und kurze Aussetzer sind damit nicht
abgenommen. Kameracodes sind keine absolute Farb-/Lichtkalibrierung; gesättigte
Kerne bleiben ausdrücklich außerhalb der Messflächen. Andere Hardware, Optik,
Kameraposition oder Farbfamilien brauchen passende neue Kalibrierung.
