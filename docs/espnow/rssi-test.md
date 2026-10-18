# RSSI-Anzeige und Funkelintensität

Stand: 18. Oktober 2026. Beauftragt: gemeinsame Abstandsversuche mit RSSI und
Funkelsteuerung. Basis `3ff7d11`; Arduino-ESP32 bleibt 2.0.14 / ESP-IDF 4.4.
Die [Frameworkmigration](../../handoff/ARDUINO-ESP32-3-MIGRATION.md) bleibt
separate zukünftige Arbeit. Neue Geräteabnahme noch offen.

## Bedienung

- **Signalstärke messen:** wiederkehrende, einzeln nummerierte Messaufträge,
  keine LED-Startanweisung.
- **Messen und Funkeln steuern:** authentifizierter Funkeln-Start, danach
  Messaufträge mit Intensitätssteuerung.
- **Messung beenden:** sofort keine weiteren Proben; bei gesteuertem Funkeln
  nach Abschluss eines laufenden Auftrags LED-Stop anfordern.

Die App zeigt Rückantwort-RSSI **zweiter ESP → Box** in dBm, geglätteten Wert,
Alter seit BLE-Ergebnisempfang am Handy, Zielintensität, Auftrag und
Empfängerzähler. Keine Meterumrechnung. Körper, Wände, Gehäuse und Ausrichtung
beeinflussen den Wert; die angegebenen Schwellen sind keine Entfernungsschwellen.

Alle 500 ms prüft eine Vordergrundmessung frischen Status und freien gemeinsamen
BLE-Slot. Jede Probe erhält neue Auftragsnummer und zufällige Funkkennung;
keine parallelen Anfragen oder Wiederholung einer bereits gesendeten Nummer.
Bei Funkverlust kann ein Versuch zwei Sekunden dauern. LED/Audio/NFC teilen
weiterhin den Slot. Die ursprüngliche Einzelsendetaste bleibt getrennt und
ist während der Messung gesperrt. BLE-Abbruch stoppt wie bisher die Ausgaben;
Wiederverbindung oder App-Rückkehr startet keine Messung. Kein Betrieb ohne
Handy beauftragt.

## Zuordnung mit bestehendem SDK

Gemäß [Espressifs Verfahren für ältere SDKs](https://docs.espressif.com/projects/esp-faq/en/latest/application-solution/esp-now.html#how-do-i-obtain-the-corresponding-rssi-when-transferring-data-using-esp-now)
erfasst die Box Managementframes auf Kanal 1 im Promiscuous-Modus. Der normale
ESP-NOW-Callback enthält keinen RSSI und darf RSSI-Aufträge nicht abschließen.

Der begrenzte Parser verlangt exakt 107 Byte einschließlich SDK-FCS-Länge:
24 Byte MAC-Header, Actionheader, Vendor-IE mit 64 Byte Rückantwort und FCS.
Kategorie, beide Espressif-OUI, Typ/Version und vollständige LENT-Hülle werden
geprüft; Zieladresse muss eigene STA-MAC sein. FCS-Prüfung verbleibt beim SDK.
RSSI, Quelle und Paket werden zusammen in die begrenzte Queue kopiert und
ausschließlich im Anwendungslauf ausgewertet.

Nur ausgewählter Peer, frische 16-Byte-Kennung, exakte Nutzlast und Zähler > 0
liefern einen Messwert. Alte, fremde und verkürzte Frames liefern keinen.
Queuedrops sperren weitere Funkaufträge bis zum Neustart. Korrelation ist
weiterhin keine kryptografische Funkanmeldung. Empfängerformat/-firmware
bleiben unverändert.

## Regelung und Grenzen

Nur Funkeln wird skaliert; Phasen laufen ohne Neustart weiter. Burst/Ripple
bleiben unverändert. Erster gültiger Wert bzw. erster nach Veraltung setzt den
Filter; danach Q8-Glättung mit 25 % neuem Anteil. −90 dBm und schwächer ergeben
20 %, −40 dBm und stärker 100 %, dazwischen linear. Nach drei Sekunden ohne
frische Antwort: unbekannt und Rückfallziel 20 %. Ausgabe nähert sich dem Ziel
um höchstens drei Prozentpunkte je 33 ms. Prozentwerte beziehen sich auf die
bisherige Grenze 32/255; Strombudget bleibt erhalten. Manuelle LED-Befehle
setzen Modulation zurück. Neue bewusste Messproben können wieder modulieren,
starten selbst aber keine LEDs.

## Additiver V2-Vertrag

Status bleibt 91 Byte; Fähigkeitsbyte 38 zusätzlich Bit 3 = RSSI, setzt Bit 1
(ESP-NOW) voraus. Bits 4–7 null. Neue App unterstützt ältere Firmware mit
deaktivierten RSSI-Starts. Ältere Apps lehnen das neue Bit ab: App/Box gemeinsam
aktualisieren.

**0x07, 70 Byte:** Aufbau wie 0x06, Body 38, HMAC
`LARP-LED/2/rssi-command`. Operation an 30: 1 messen ohne Modulation,
2 mit Modulation; Kanal an 31 = 1, Peer an 32–37. Session, gemeinsamer Zähler,
Frist, Slot und identischer Duplikatcache bleiben erhalten.

**0x88, 112 Byte:** erste 76 Byte wie 0x87; Byte 76 gültig 0/1,
77 roher und 78 geglätteter RSSI als signed int8 (−127..−1), 79 Zielintensität
20..100. HMAC an 80–111: `LARP-LED/2/rssi-result` über Body 80.
RSSI-Gültigkeit verlangt bestätigte passende Rückantwort. Ohne diese sind
alle vier Zusatzbytes null; kein früherer Erfolg wird kopiert. 0x87/0x88
werden in der App getrennt geprüft und angezeigt.

## Offener gemeinsamer Gerätetest

Neue App und Boxvariante `larp-espnow-test` gezielt gemeinsam installieren;
vorhandener `espnow-app-receiver` bleibt passend. Zuvor chipgebundenen Rückweg
und Schlüssel gegen die aktuelle Installation abgleichen.

1. Reine Messung: frische negative Werte, neue Aufträge/Zähler, kein LED-Start.
2. Bekannte Abstände (z. B. 0,5 / 1 / 2 / 5 m) je einige Sekunden beobachten;
   Ausrichtung, Hindernisse und Gehäuse mit notieren.
3. Gesteuertes Funkeln bewusst starten; Rohwert, Glättung, Ziel und tatsächlichen
   Lichteindruck getrennt beobachten.
4. Empfänger stromlos: unbekannt und sanfter Rückfall; nach Neustart nur frische
   Antworten verwenden.
5. Während einer Anfrage stoppen, neu starten und BLE trennen/wiederverbinden:
   keine doppelten Pollschleifen oder automatischer Wiederanlauf.
6. Stop sowie normale LED-/Audio-Bedienung anschließend gezielt prüfen.

Dies ist ein Abnahmeplan; keine neue Geräteprüfung behauptet.
