# Papierauflage: neue Kamerakalibrierung · 18.10.2026

Status: **Vollkalibrierung bei Belichtung −8 bestanden; Goldprüfung mit begrenzten Teilbefunden abgeschlossen**.
Anlass ist die vom Nutzer neu vor die LEDs gelegte weiße, transparente
Papierauflage vor dem [Näheglut-Gehversuch](../espnow/naeheglut-test.md).
Sie verändert die Optik. Die frühere Kalibrierung ohne Papier wird deshalb
nicht als Nachweis dieses Aufbaus übernommen.

## Aufbau, Herkunft und Stand

Die neue Pilotaufnahme `paper-pilot-e7` lieferte 23 Aufnahmebereiche.
`paper-rois-e7.json` und die für −8 benannte `paper-rois-e8.json` sind
bytegleich: gleiche Pilotkarte, keine zweite Regionssuche. Aufnahmebereiche
allein sind noch keine Firmwareindexzuordnung; diese wurde anschließend
durch die CAL-Einzelfolgen und ihre umgekehrte Gegenfolge geprüft.

Quellbasis: `42d986ed353277d861df07ae59bd1c06345d3b52` plus RF-Laborarbeitsbaum
auf `codex/naeheglut-espnow-test`, durch `source-manifest.json` gebunden.
Alle hier genannten Aufnahmen verwenden dasselbe RF-Laborbinary.
[Laborhost](../../scripts/led_camera_lab.py), [Regionssuche](../../scripts/led_camera_rois.py)
und [Assessor](../../scripts/led_camera_assess.py) stimmen beim Dokumentationsabgleich
mit den gespeicherten Quellfingerabdrücken überein. Die bestehenden
[Kalibrierkriterien](camera-measurement.md) wurden nicht gelockert.

## Tatsächliche Versuche

| Aufnahme | größte Empfangslücke | Ergebnis |
| --- | ---: | --- |
| `paper-pilot-e7` | nur Pilotübersicht | 23 neue Aufnahmebereiche; bewusst etwa fünf gespeicherte Bilder/s, kein zeitlicher Effektprüflauf. |
| `paper-cal-e7` | 104,950 ms | Über der unveränderten 80-ms-Grenze; als Kalibrierbeleg verworfen. Aufnahme bleibt erhalten. |
| `paper-cal-e7-repeat` | 59,971 ms | 23 Indizes durch Gegenfolge bestätigt; Voll-CAL nicht bestanden, Grün-Dominanz an LEDs 0, 1, 4, 21 und 22 unzureichend. |
| `paper-cal-e8` | 72,541 ms | Unverändertes `calibrate` vollständig bestanden: Indexzuordnung, feste Messmasken, Weiß-/Blau-Ansprechkurven, Dynamik und Primärkanalprüfung. |

Die Wiederholung bei −7 beseitigte die Zeitlücke, erfüllte aber noch nicht
alle optischen Kriterien. Danach wurde die Belichtung auf −8 geändert und
die vollständige CAL neu aufgenommen. Ein `recorded` im Aufnahmemanifest
bedeutet vollständig aufgezeichnet; es hebt kein negatives CAL-Ergebnis auf.

Der erfolgreiche Lauf enthält **1309 Bilder und 134 TX_DONE-Ereignisse**.
Alle 23 Indizes sind unabhängig bestätigt; die kleinste feste Messmaske
enthält 133 geeignete Pixel. Die unabhängigen Zeitmarker ergeben
Korrelation **0,994105** und geschätzte Unsicherheit **7,378 ms**.
Das ist die Zeitzuordnung dieses Kalibrierlaufs, keine zugesagte Lichtlatenz
und keine auf spätere Aufnahmen übertragene Synchronisation.

## Einstellungen des erfolgreichen Aufbaus

Kameraindex 0; 1280×720 MJPG, 30 Bilder/s angefordert und etwa 30,027
Messbilder/s empfangen. Verlustfreie PNG-Kacheln erhalten die ausgewählten
dekodierten Kamerapixel; die zusammengesetzte Messfläche ist 400×400 Pixel.

Belichtung −8, Gain 0, Auto-Weißabgleich 0, Gegenlichtkorrektur 0,
Schärfung 0 und Gamma 100 wurden angefordert und so zurückgelesen.
Die Bildrate wurde als 30,00003 zurückgelesen. Auto-Exposure wurde mit 0
angefordert und vom Treiber angenommen; der Getter liefert weiterhin −1.
Dies beweist keinen tatsächlichen Automatikzustand. Vollständige Soll-,
Annahme- und Rücklesewerte stehen im Kalibrier- und Aufnahmemanifest.

Papierlage, Box, Kamera, Aufnahmebereiche und Einstellungen für anschließende
Referenzen erhalten. Bewegung, andere Optik oder geänderte Einstellungen
dürfen nicht stillschweigend dieselbe Kalibrierung weiterverwenden.

## Goldreferenz: untersuchter Befund mit begrenzter Aussage

Die separate Aufnahme `paper-gold-e8-complete` ist vollständig aufgezeichnet,
einschließlich zehn bestätigter Aufträge, zugeordneter TX-Spur und abschließendem
Schwarz/DONE. Der eigene Auswerter `analyze_gold.py` verwendet ausschließlich
die festen Masken der neuen CAL; Weiß-/Blau-Ansprechkurven werden nicht auf Gold
übertragen. Papier, ROI, Firmware und zurückgelesene Einstellungen stimmen mit
der CAL überein. LIVE-Metadaten enthalten keine unabhängige Kameraidentität.

Gemessen wurde `p=0,60,140,250,500,750,1000,500,0` mit RGB `(24,8,0)`,
Gain 1000, Smoothstep über 1000 ms und anschließend gezieltem Stop.
Auswertefenster beginnen 1500 ms nach Host-ACK und enden 200 ms vor dem nächsten
Senden; damit werden eingeschwungene Plateaus statt Rampen beurteilt.
Je Plateau liegen 53–54 Bilder, nach gezieltem Stop 99 Bilder vor.
Der Hintergrund stammt aus 71 Bildern vor dem ersten Start.

**Gesamturteil des vorab festgelegten Goldkriteriensatzes: nicht bestanden**
(`status: criteria_failed`). Die Grenze wurde weder abgesenkt noch wurden
Masken nach Goldbeobachtung angepasst.

| Kriterium / Beobachtung | Ergebnis |
| --- | --- |
| Goldmaximum mindestens 20 Kameracodes über Hintergrund | Nur 4/23 LEDs erfüllen das Kriterium; 19/23 nicht. Tatsächlicher Bereich 12,578–21,546 im RGB-Mittel. |
| Ansteigende Plateaus innerhalb `max(3 Codes, 5 % Goldmaximum)` monoton | 23/23 erfüllt. |
| Keine Sättigung ab Kameracode 250 in den festen Masken | 23/23 erfüllt; maximaler gesättigter Anteil null. |
| Rückkehr zu p=0 und gezielter Stop höchstens 5 % des jeweiligen Goldmaximums über Hintergrund | Jeweils 23/23 erfüllt; kein Nachweis exakt null Licht oder Strom. |
| Wiederholtes p=500 | Differenz −0,220 bis +0,058 Kameracodes; keine Driftwarnung. |
| Drei unterste Schritte 0→60→140→250 | Bei allen 23 LEDs unter der verwendeten 3-Code-Auflösungsschwelle; feine Helligkeitsstufen bleiben unaufgelöst. Höhere Schritte liegen darüber. |

Die größte Empfangslücke des Goldlaufs beträgt **219,210 ms** und liegt außerhalb
der Plateau-Auswertefenster. Deren größte Lücken liegen zwischen 41,03 und
53,92 ms. Keine Bilder wegen Schreibstau verworfen. Der Auswerter prüfte insgesamt
2553 Bild-/Kontextdateien gegen ihre gespeicherten Hashes: 1353 aus der CAL und
1200 aus LIVE. Der Goldlauf erfüllt damit **keine lückenlose Timing- oder
Flimmerabnahme**; die erfolgreiche CAL hebt diese spätere Lücke nicht auf.

Deutliche Helligkeitsänderung und Ausblenden sind mit diesem Aufbau für den
beauftragten Gehversuch beobachtbar. Feinstufen, volle quantitative Goldfreigabe
und Nutzerurteil zur Lichtwirkung bleiben offen. Relative Kamerahelligkeit
derselben Goldmischung ist keine Luxmessung oder exakte Farbmessung; auch die
erfolgreiche Primärkanalprüfung ist keine vollständige farbmetrische Kalibrierung.
Der vorhandene automatische Effektassessor unterstützt weiterhin ausschließlich
Funkeln, Lichtimpuls und Ripple mit den dafür kalibrierten Farbfamilien.

## Erhaltene Goldfehlversuche und Korrektur des Ablaufs

| Aufnahme | Befund und Behandlung |
| --- | --- |
| `paper-gold-e8` | `incomplete`: serieller Zugriff wurde im Sandboxaufruf vor dem Öffnen von COM3 abgewiesen; null Effektaufträge, kein gestarteter Lichtversuch. |
| `paper-gold-e8-retry` | `incomplete`: Firmwarewächter beendete den Lauf nach acht Aufträgen; abschließendes Schwarz bestätigt. Die Durchführung führte den blockierten Ablauf auf eine nicht laufend geleerte stdout-PIPE des privaten Orchestrators zurück. |
| `paper-gold-e8-complete` | Privater Orchestrator schreibt stdout/stderr direkt in eine Logdatei; danach vollständige Aufnahme und obige Auswertung. |

Die Ablaufkorrektur betraf `gold_reference.py` im privaten Belegordner.
Laborhost- und Firmwarehash sind in allen drei Versuchen identisch.
Daraus wird kein Fehler des Laborhosts oder der Firmware abgeleitet;
die negativen Läufe bleiben als solche erhalten.

Dieser Bericht enthält reale optische Prüfbelege und ihre Grenzen. Er behauptet
keine neuen Softwaretests, keinen abgeschlossenen ESP-NOW-Gehversuch und
keine Produkt-/Gesamtspiel- oder elektrische Abnahme. Ausgabespur,
Kamerabeobachtung und späteres Nutzerurteil bleiben getrennt.

## Belegorte und Fingerabdrücke

Private Rohdaten: `C:/Dev/Larp Escape/private/naeheglut-espnow-20261018/`.
Dort liegen die genannten Aufnahmeordner, beide ROI-Dateien,
`paper-calibration-e8.json`, `source-manifest.json`, `gold_reference.py`,
`analyze_gold.py` sowie `paper-gold-e8-complete/gold-analysis.json`.
Rohbilder, Flashabbilder und Installationsdaten werden hier nicht kopiert.

| Artefakt | SHA-256 |
| --- | --- |
| RF-Laborbinary laut Aufnahmen | `8c8ee844b9f7f18bfe20846f0a07946352677fdae27a998eab4e306f7fc0d27a` |
| `paper-calibration-e8.json` | `b01ea101ce9bc6774f84ef25a642f6f992036d5df56e2c157d8eff7ad9cd769e` |
| `paper-cal-e8/capture.json` | `8922284d0cf6fdf1e89a7588c1356ac3b1d0161702ba96a5d85771c0cad3e0a5` |
| Beide Papier-ROI-Dateien | `6748ce683a169e2e01d45e04e6c7fd3b23a922d9ed4b2407a33a305b8d8af7fa` |
| `source-manifest.json` | `27abbc464538136ae50adc43aeb9139f41de60f3bd78ff502320021a71516742` |
| Unveränderter Assessor | `4e37a9f087709f1efe3374baf8f66fdecdc2a738d4698fdaf86ffd67256dab6a` |
| `paper-gold-e8-complete/gold-analysis.json` | `b2e90412e589753b9841e423dac99de9761811acd66eb0f1fdbbce3d0117d144` |
| `analyze_gold.py` | `b5ad78337b2351abc33866a825031826176c2f6cea269c3ba645bad10bb9fa46` |
| `paper-gold-e8-complete/capture.json` | `5fcd9fc777ec65f0100513d1c23733fe47f159c40ef73ba93516892cd68af4e4` |

Dokumentationsprüfung: gespeicherte JSON-Metadaten, Verweisziele und
Quell-/Artefakthashes gelesen und abgeglichen. Keine erneute Bildauswertung,
Tests, Builds oder Gerätehandlung für diese Dokumentation ausgeführt.