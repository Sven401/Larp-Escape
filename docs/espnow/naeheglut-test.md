# Näheglut mit ESP-NOW: kurzer Geh- und Ausfallversuch

Stand: **18. Oktober 2026**. Status: **Vorabplan; drei Laboraufnahmen inzwischen abgeschlossen**.
Auftrag: Näheglut gegen den bereits bekannten Powerbank-ESP
`A8:42:E3:CE:DE:D8` erproben. Der [Ergebnisbericht](naeheglut-result-2026-10-18.md)
führt reale Funk-/Stopnachweise, zwei offene Rechenabweichungen, negative optische
Stopkriterien und den verifizierten Rückweg. Nutzerurteil und vollständige
optische Abnahme bleiben offen. Der folgende Vorabplan bleibt als Maßstab erhalten;
abweichender tatsächlicher Ablauf und Ergebnisgrenzen stehen im Bericht.

## Umfang und wiederverwendete Grundlage

Zwei getrennte Läufe von jeweils höchstens 80 Sekunden: zuerst nah–fern–zurück,
danach Funkausfall, Wiederkehr und gezielter Stop. Box und Kamera bleiben stehen;
der zweite ESP bewegt sich mit der Powerbank. RSSI beschreibt diese Funkstrecke,
keine Entfernung in Metern. Die Versuchsanbindung entscheidet keine Spielregeln.

Der [frühere Gehversuch](distance-camera-result-2026-10-18.md) liefert Aufbau,
Funk-/Kameragrenzen und erhaltene Rohbelege. [RSSI-Verfahren](rssi-test.md) und
[Installationsnachweis](rssi-result-2026-10-18.md) beschreiben die vorhandene
Signalaufbereitung und den unveränderten Empfänger. [Näheglut-Livebericht](../led/naeheglut-live-results-2026-10-18.md),
[Entwurf](../led/naeheglut-design.md) und [Rolle 2.0](../led/rollen/led-effekt-architekt.md)
liefern Effektkern, Lebenszyklus und Kameraerfahrung; deren Ergebnisse werden
nicht als Nachweise dieses neuen Versuchs gezählt. Für die inzwischen neu
aufgelegte Papierabdeckung gilt der eigene [Papier-Kalibrierbeleg](../led/paper-camera-calibration-2026-10-18.md):
Voll-CAL bei −8 bestanden; Goldprüfung abgeschlossen, Dynamikkriterium an 19/23 LEDs
nicht erreicht. Grobe Helligkeitsänderung und Ausblenden bleiben beobachtbar;
Feinstufen und volle quantitative Goldfreigabe bleiben offen.

Geplanter Anschluss: separate RF-Variante des bestehenden Kameralabors.
Der [interaktive Host](../../scripts/led_camera_interactive.py) erhält
`MEASURE` und sendet normale `GLOW`-Aufträge an denselben Näheglut-Kern;
`--rssi-peer` wählt den bekannten Peer. Messwerte starten keine Instanz.
Nach gezieltem Stop unterbindet der Host weitere Updates bis zu einem bewussten
Neustart. Keine Erweiterung des Produkt-BLE-Vertrags oder der Handy-App.

## Vereinbarte Arbeitswerte

- Alle 23 LEDs, ein Goldbeitrag, zunächst kein Hintergrund/zweiter Beitrag:
  RGB `(24,8,0)`, Gain 1000, Smoothstep und Übergangsdauer 1000 ms.
- Vorhandener Q8-Filter mit 25 % neuem Anteil; erster gültiger Wert sowie erster
  nach Veraltung setzen den Filter neu. Nur passende frische Peerantworten zählen.
- Der Filter rechnet intern in Q8; `Filter.value()` liefert ganzzahlige dBm
  durch C++-Trunkierung Richtung null. Der Host verwendet genau diesen Wert:
  `p = clamp(20 * (filtered_integer + 90), 0, 1000)`.
  Die Ausgabe besitzt keine zusätzliche Q8-Auflösung; p ändert sich in 20er-Schritten.
- 3000 ms ohne gültige Antwort bedeuten `valid=false`; Fehler/alte Antworten
  erneuern diese Frist nicht. Der aktive Beitrag blendet dann in 1000 ms aus.
  Bei Wiederkehr darf nur eine noch aktive Instanz zurückkehren.
- Die alte Funkeluntergrenze von 20 % wird **nicht** übernommen. Unbekannt ist
  kein gültiger Wert null. Die Schwellen sind Versuchsparameter, keine Produktkalibrierung.

Überprüfbare Sollpunkte nach unverändertem Ziel über mindestens 1000 ms:

| Gefilterter RSSI | p | RGB je LED |
| --- | ---: | --- |
| −90 dBm oder schwächer | 0 | (0,0,0) |
| −65 dBm | 500 | (12,4,0) |
| −40 dBm oder stärker | 1000 | (24,8,0) |

Bei ständig wechselndem Ziel setzt der Kern vom aktuellen Beitrag neu an.
Eine Sekunde ist die Übergangsdauer zum konstanten Ziel, keine garantierte
Verzögerung zwischen Ortswechsel und sichtbarer Reaktion.

## Mindestnachweise vor dem ersten Lauf

- Aktuellen Quellstand, RF-Laborbinary und Hash festhalten; gezielte Prüfungen
  für Peer-/Frischezuordnung, p-Grenzen, Stop ohne Wiederanlauf und den passenden
  Laborbuild abschließen. Keine umfangreiche Regression als Vorbedingung.
- Bei temporärer Installation Box `A8:42:E3:CD:4B:18`, aktuelle Partitionen und
  frische vollständige 4-MiB-Sicherung am selben Chip verifizieren. Nur passende
  Anwendung ab `0x10000` ersetzen; Boot-/Schlüsselbereiche erhalten.
  [Rückwegverfahren](../led/camera-measurement.md), jüngste Belege unter
  `private/led-live-20261018/`. Historische Sicherung ist kein heutiger Chipnachweis.
- Peerfirmware bleibt unverändert; Kanal 1 und Ziel-MAC abgleichen.
  Historische COM-Nummern sind keine Geräteidentität; USB-Zuordnung siehe
  [ESP-NOW-Übergabe](../../handoff/ESPNOW-UEBERGABE-2026-10-18.md).
- Am Messplatz zuerst Steuerbarkeit und frischen Schwarz-/Stopabschluss prüfen.
  Kamerabild mit allen Lichtflächen, freie Speicherung und Zuordnung zum aktuellen
  Lauf kontrollieren. Für den Papieraufbau sind neue 23 Aufnahmebereiche und
  die vollständige CAL bei Belichtung −8, 1280×720 MJPG bereits belegt;
  die früheren Versuche bei −7 bleiben als negative Prüfbelege erhalten.
- Die separate Goldprüfung ist ausgewertet: Monotonie innerhalb Toleranz,
  fehlende Sättigung und finales Dunkel sind belegt; die vorab festgelegte
  20-Code-Dynamikgrenze verfehlen 19/23 LEDs. Den Gehversuch auf deutliche
  Helligkeitsänderung und Ausblenden begrenzen, keine Feinstufen-/Farb- oder
  Timingabnahme ableiten. Papierlage, Box, Kamera, ROI und Einstellungen
  unverändert übernehmen; Details und Grenzen im verlinkten Kalibrierbericht.
- Keine Builds oder Bildtests parallel zur Aufnahme. Die hier auf Nutzerauftrag
  ausgeführte neue Kalibrierung ist kein allgemeines Tor vor frühem Gestaltungsfeedback;
  quantitative Aussagen brauchen zum konkreten Aufbau passende Referenzen.

## Zwei kurze Läufe

| Lauf / Phase | Ablauf und zu beobachtender Unterschied |
| --- | --- |
| 1: Vorlauf / nah | Etwa 5 s dunkel aufnehmen; bewusst starten und etwa 10 s nahe halten. Frische Messungen und ruhiges gemeinsames Licht erfassen. |
| 1: fern / Rückkehr | Bis etwa 15 s entfernen, etwa 10 s halten, bis etwa 15 s zurückkehren und etwa 10 s nahe halten. Ausrichtung/Hindernisse notieren. Gezielt stoppen, Ausblendung abwarten und mindestens 5 s dunkel beobachten; erst danach globales STOP. Insgesamt höchstens 80 s. |
| 2: nah / Ausfall | Neuer Lauf mit dunklem Vorlauf und bewusstem Start; etwa 10 s nahe halten, dann Peer für 8 s ausschalten. Letzte gültige Antwort, Veraltung und Ausblendung zuordnen. |
| 2: Wiederkehr | Peer wieder einschalten und etwa 10 s nahe halten. Nur frische Antworten dürfen den noch aktiven Beitrag zurückholen; ein neuer Empfängerboot darf einen neuen Zähler beginnen. |
| 2: gezielter Stop | Instanz bewusst stoppen, Ausblendung abwarten und mindestens 5 s dunkel mit weiter eintreffenden gültigen Messungen beobachten: keine Updates und kein Wiederanlauf. |
| 2: bewusster Neustart / Ende | Höhere Generation bewusst starten und Wiederkehr kurz beobachten. Erneut gezielt stoppen, Ausblendung abwarten und mindestens 5 s dunkel beobachten; danach globales STOP. Auch dieser Lauf bleibt unter 80 s. |

Nach globalem STOP nutzt der Host seinen bestehenden Kameranachlauf von 0,5 s.
Die längeren Dunkelphasen liegen davor; sie erfordern keinen Hostumbau.

Wenn der Gehweg keine ausreichend unterschiedlichen RSSI-Werte erzeugt, ist die
Nah-/Fernwirkung mit diesem Lauf nicht beurteilbar. Keine gewünschte Funkkurve
voraussetzen oder weitere Wiederholungen stillschweigend anhängen.

## Kriterien, Belege und getrennte Urteile

| Ebene | Kriterium und erforderlicher Beleg |
| --- | --- |
| Funk und Eingang | Jede Messanfrage eindeutig zuordnen; gültige Antworten, Sendefehler, Timeout und offene Ergebnisse erhalten. Peer, Kennung, Roh-/Filter-RSSI, Alter/Gültigkeit, Empfängerboot/-zähler und normalisiertes p protokollieren. Alte oder fremde Antwort darf keinen neuen gültigen Eingang erzeugen. |
| Effekt und Ausgabe | Angenommene GLOW-Parameter, Slot/Generation/Revision, STATE sowie zugeordnete TX_DONE-Spur erhalten. Wertebereich und obige Sollpunkte bei passenden stabilen Eingängen prüfen. Stop sperrt Updates auch während des Ausblendens; neue Messung allein startet nicht. |
| Ausfall und Abschluss | Veraltung ab letzter gültiger Antwort nach 3000 ms, danach Ausblenden gemäß D dokumentieren. Globales STOP verlangt frischen schwarzen Transfer und DONE. Fehlende Bestätigung bleibt offen; noch ungelöste Aufträge dürfen nicht als vollständig aufgezeichnet gelten. |
| Kamera | Unveränderte Bilder, Zeitstempel, Einstellungen, Sättigung und Empfangslücken erhalten. Grobe Abdunklung/Erholung und sichtbares Ende unabhängig von TX beurteilen. Kameraempfang ist kein Belichtungszeitpunkt; keine harte Latenz, Lux-, Farb- oder Flimmerabnahme. |
| Nutzerurteil | Erkennbarkeit, warmgoldene Wirkung, ruhiger Übergang und störende Helligkeitsstufen getrennt festhalten. `recorded` heißt aufgezeichnet, nicht optisch bestanden. |

Je Lauf einen neuen Belegordner verwenden. Quell-/Binaryhash, MEASURE-/Befehlsspur,
Kameramanifest und Phasenmarkierungen gemeinsam referenzieren; Uhrenzuordnung und
deren Grenzen ausdrücklich nennen. Frühere Webcam-Korrelationen, Weiß-/Blau-
Kalibrierungen oder Funk-Erfolgsquoten sind keine Bestehensschwellen für Gold.

Nach temporärer Installation bisherigen Boxzustand gemäß vereinbartem Rückweg
wiederherstellen und vollständigen Flash verifizieren, sofern keine andere
Installation beauftragt ist. Ergebnisbericht trennt Beobachtung, begründete
Parameteränderung, Softwareprüfung und optisches Urteil. Spielintegration,
Produktadapter, dauerhafte Funkzuverlässigkeit, elektrische Prüfung und
Gesamtspielabnahme bleiben außerhalb dieses begrenzten Versuchs.