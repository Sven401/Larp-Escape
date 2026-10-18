# Gemeinsamer Baugruppenvertrag, Version 2

**Architekturanschluss · 18.10.2026:** [VA-01](../veranstaltungsarchitektur.md) erhält diesen lokalen Baugruppenvertrag einschließlich Bytes, Fristen und manueller Abbruchregeln. BLE-Abbruch manueller Ausgaben ist kein pauschaler Stop des autonomen Spiels. Boxübergreifende Ereignisse bekommen einen eigenen Vertrag; siehe [Änderungsmatrix](../veranstaltungsarchitektur-umsetzung.md).

Stand: **18. Oktober 2026; ESP-NOW-Erweiterung implementiert, begrenzter App-/Geräte-Gehversuch abgeschlossen.**
[Ergebnis und Grenzen](../espnow/distance-camera-result-2026-10-18.md): keine gemeinsame Gesamtspielabnahme.
Quellen: [Firmware-Session](../../firmware/led/src/LedWire.cpp),
[NFC-Session](../../firmware/led/src/NfcBleWire.cpp),
[Android-Decoder](../../companion/android/core/protocol/src/main/kotlin/de/larpescape/protocol/LedProtocol.kt),
[BLE-Sitzung](../../companion/android/app/src/main/java/de/larpescape/companion/platform/ble/LedBleSession.kt).

## Version und Paarung

Jede innere Nachricht beginnt mit Version **2**. Die unveränderte
Fragmenthülle trägt weiterhin Version 1 und Kennung `0x4c`, höchstens 128 Byte
Nachrichteninhalt, 12 Byte Fragmentnutzlast und verbindungsgebundene Transfer-IDs.
Ein Fragment ist keine vollständige Beobachtung. Die Größen unten enthalten HMAC.

Die gegenseitige Anmeldung verwendet dieselben Nonces und dieselbe
Identität wie [Version 1](ble-wire-v1.md), aber ausschließlich die HMAC-Domänen
`LARP-LED/2/challenge`, `/proof` und `/session`. Challenges bleiben 98 Byte,
Hello 18 Byte und Proof 34 Byte. Session-ID sind die ersten 16 Byte des
Session-HMAC über Device-, Boot-, Connection- und Client-Nonce. Jedes neue
BLE-Link bekommt eine neue Connection-Nonce. Keine Protokollherabstufung.

| App | Firmware | Verhalten |
| --- | --- | --- |
| V2 | V2 | Gegenseitige Anmeldung, explizite Fähigkeiten, begrenzte Bedienung |
| V2 | V1 aus #45–#52 | Versionsfehler vor Grant; keine Bedienung |
| V1 | V2 | Firmware verwirft das Hello und beendet die Sitzung; keine Bedienung |
| V1 | passende historische V1 | Nur der ursprüngliche jeweilige Baugruppenvertrag |

V1-Apps/Firmwares mit kollidierenden Audio-/NFC-Typen werden nicht nach
Nachrichtenlänge erraten. Ein passendes neues App-/Firmwarepaar muss später
gemeinsam installiert werden; dieser PR installiert nichts. Das Zugangsformat
`schemaVersion:1`, die NVS-Partition `larpkeys`/`access-v1`, Wake-Anordnungen,
Matrixlauf-Dateien und NFC-Tagdaten ändern sich nicht. Labor-v2 ist ein eigener,
unveränderter Statuspfad und nicht diese Baugruppen-Protokollversion.

## Eindeutige Nachrichtentypen

| Typ | Richtung/Inhalt | Bytes | HMAC-Domäne |
| --- | --- | ---: | --- |
| `0x01` | Hello | 18 | keine, anschließend Challenge/Proof |
| `0x02` | Proof | 34 | `LARP-LED/2/proof` über den Nonce-Kontext |
| `0x03` | LED-Auftrag | 64 | `LARP-LED/2/command`, Body 32 |
| `0x04` | Audio-Auftrag | 66 | `LARP-LED/2/audio-command`, Body 34 |
| `0x05` | NFC-Auftrag | 70 | `LARP-NFC/2/command`, Body 38 |
| `0x06` | ESP-NOW-Testauftrag | 70 | `LARP-LED/2/espnow-command`, Body 38 |
| `0x07` | RSSI-Messauftrag | 70 | `LARP-LED/2/rssi-command`, Body 38 |
| `0x81` | Challenge | 98 | `LARP-LED/2/challenge`, Body 66 |
| `0x82` | Granted/Gesamtstatus | 91 | `LARP-LED/2/status`, Body 59 |
| `0x83` | LED-Ergebnis | 74 | `LARP-LED/2/result`, Body 42 |
| `0x84` | laufender Gesamtstatus | 91 | `LARP-LED/2/status`, Body 59 |
| `0x85` | Audio-Ergebnis | 82 | `LARP-LED/2/audio-result`, Body 50 |
| `0x86` | NFC-Ergebnis | 105 | `LARP-NFC/2/result`, Body 73 |
| `0x87` | ESP-NOW-Testergebnis | 108 | `LARP-LED/2/espnow-result`, Body 76 |
| `0x88` | RSSI-Messergebnis | 112 | `LARP-LED/2/rssi-result`, Body 80 |

Mehrbytezahlen sind little endian. Außer Hello/Proof enthalten Nachrichten
nach dem Header die 16-Byte-Session-ID. Typ, exakte Länge, Sitzung und
vollständiger HMAC werden vor Ausführung geprüft. Unbekannte Versionen,
Nachrichtentypen, reservierte Bits oder widersprüchliche Zustände führen zu
keiner Bedienung. Die App zeigt einen Fehler und beendet ihren Sitzungsweg;
die Firmware beendet bei einem Protokollfehler den Link und fordert Stop an.

## Ein Statusschema für alle Varianten

Granted und laufender Status haben **immer 91 Byte**, auch bei fehlenden
Baugruppen. Fähigkeiten werden nicht mehr aus der Nachrichtenlänge abgeleitet.

| Offset | Bedeutung |
| ---: | --- |
| 0–1 | Version 2 und Typ `0x82`/`0x84` |
| 2–17 | Session-ID |
| 18–21 | LED-Revision |
| 22–25 | nächste gemeinsame Auftragsnummer |
| 26–29 | Geräte-Millisekunden |
| 30–33 | gemeinsame LED-/Audio-/Gesamtstatussequenz |
| 34–36 | LED-Effekt, LED-Flags, Modus; Semantik aus V1 |
| 37 | Wake: Bit 7 unterstützt, Bit 6 gültig, Bits 0–3 Rohmaske; NFC: Bit 5 unterstützt; Bit 4 null |
| 38 | Fähigkeiten: Bit 0 Audio unterstützt, Bit 1 ESP-NOW unterstützt, Bit 2 ESP-NOW bereit, Bit 3 RSSI unterstützt; Bits 4–7 null; bereit/RSSI verlangen ESP-NOW-Unterstützung |
| 39–46 | Audio-Snapshot: LE32 Revision, LE16 Titel, Lautstärke, Flags |
| 47 | Matrix: Bit 7 unterstützt, Bit 6 gültig, Bit 5 mehrdeutig; Bits 0–4 null |
| 48–58 | Matrixkontakte 0–80, zeilenweise; nur Bit 0 des letzten Bytes verwendet |
| 59–90 | HMAC-SHA-256 über Domäne und Body 0–58 |

Nicht unterstütztes Audio hat acht Nullbytes. Nicht unterstützte Matrix hat
keine Gültigkeit, Mehrdeutigkeit oder Kontakte. Ungültige Matrix hat keine
Kontakte/Mehrdeutigkeit; ungültiges Wake keine Rohmaske. Audioflags und
Bediensemantik bleiben gemäß [Audiovertrag](../audio/ble-wire.md) erhalten.
Unbekannte Fähigkeiten werden nicht als Bereitschaft interpretiert.

| Firmware | LED | Wake | Audio | NFC | Matrix |
| --- | --- | --- | --- | --- | --- |
| `larp-led-product` | ja | ja | ja | nein | nein |
| `larp-nfc-test` | ja | ja | ja | ja | nein |
| `larp-matrix-test` | ja | ja | ja | nein | ja |

Ergänzung vom 18. Oktober: `larp-espnow-test` enthält LED/Wake/Audio und den
bewusst auslösbaren Funkversuch. Nur diese Variante startet zusätzlich WiFi.
Die neue App zeigt ESP-NOW bei bisherigen V2-Firmwares als nicht unterstützt;
alte V2-Apps lehnen die neuen Fähigkeitsbits ab und benötigen ein Update.
[ESP-NOW-Vertrag und Testablauf](../espnow/app-test.md) definieren die neuen
Typen, Korrelation, Fristen und das erforderliche Empfängerpaar. Der folgende
ursprüngliche Umfang wird um ESP-NOW als vierte gemeinsame Auftragsart ergänzt.
Die [RSSI-Erweiterung](../espnow/rssi-test.md) ergänzt Messwerte und optionale
Funkelmodulation mit eigenen Typen/Domänen; kein automatischer Einzelteststart.

NFC und Matrix bleiben bewusst getrennte Prüfvarianten. Gleichzeitige
Testmakros sind ein Buildfehler. Der gemeinsame Decoder kann alle Felder
gleichzeitig verarbeiten; die entsprechende Goldenfixture ist ein
synthetischer Softwaretest und keine neue Hardwarevariante. Modus `LED_ONLY`
behauptet weiterhin keinen gestoppten realen Spielcontroller.

## Gemeinsame Auftragsfolge und getrennte Beobachtungen

LED, Audio, NFC und ESP-NOW verwenden eine gemeinsame, nicht umlaufende Folge ab 1.
Ein angenommener Auftrag reserviert den Slot bis zur korrelierten Antwort.
Andere Baugruppen erhalten Busy und führen nichts aus. Verbrauchte
Aufträge zählen auch bei fachlicher Ablehnung weiter; Busy, OutOfOrder und
konfligierende Duplikate verbrauchen keinen Slot. NFC hat weiterhin eine
eigene Ergebnis-Sequenz innerhalb derselben authentifizierten Sitzung.

Nur die identische Wiederholung des zuletzt abgeschlossenen Auftrags darf
die gespeicherte Antwort liefern. Eine laufende identische Wiederholung
erzeugt keinen weiteren Besitzerauftrag. Ein altes NFC-Ergebnis kann nach
einem später abgeschlossenen LED-/Audioauftrag nicht erneut den gemeinsamen
Fortschritt behaupten. Nach unklarem Ausgang keine automatische Wiederholung,
insbesondere kein NFC-Schreibretry. Verbindungswechsel setzt Auftragszähler,
Korrelationen, Fragmente, Duplikatcaches und Beobachtungen zurück.

LED und Audio behalten getrennte Revisionen und physische Ausgabebesitzer.
NFC behält frische UID-Bindung, doppelte Vorher-Prüfung, einmaliges Schreiben
und vollständiges Nachher-Rücklesen. Während NFC läuft, wird kein neuer
LED-/Audioauftrag angenommen. NFC benötigt wie bisher gestoppte LED-Ausgabe.
Die bestehenden Audio-Stopregeln laufen unabhängig weiter.

Ergebnisse tragen nur ihre tatsächlich vorhandenen Beobachtungen: LED keine
Wake-/Matrix-/Audiomessung, Audio seinen Snapshot und LED-Zustand, NFC UID und
Payload. Keine Aussage über hörbaren Ton, sichtbare LED-Wirkung oder elektrisch
richtige Kontakte aus einem Transport-ACK ableiten. Die Geräteberichte bleiben
eigenständige Nachweise.

## Fristen und Prüfungen

App: Anmeldung 10 s, unvollständige Fragmentierung 2 s, LED-/Audioantwort 5 s,
NFCantwort 45 s. LED-Zustand und jede tatsächlich enthaltene Wake-/Audio-/
Matrixbeobachtung sind 1000 ms gültig (Alter 0–999 ms). Gleiche/ältere
Statussequenzen erneuern keine Frist. Eine ältere passende Antwort darf
ihre Korrelation schließen, aber neuere Beobachtungen nicht ersetzen.
NFC-Ergebnisse erneuern keine Eingangs-/Audiobeobachtung.

Firmware: Auftragsgültigkeit maximal 3 s, LED-/Audio-Ausgabeziel maximal 2 s.
NFC-Transaktionsgrenzen und unklare Schreibausgänge folgen dem vorhandenen
Workervertrag. Nach Abbruch kann ein bereits gesendetes Transaktionskommando
nicht rückwirkend zurückgenommen werden. Die historischen Laborgrenzen
31/40 s gelten ausschließlich im Laborstatuspfad (#33).

[Python-Goldenbytes](../../test/test_led_ble_wire/generate_vectors.py),
[V2-Fixtures](fixtures/wire-v2.json), native C++-Sessiontests und Kotlin-Tests
prüfen echte HMACs, alle Fähigkeiten, gemeinsame Befehlsfolge, überlappende
Aufträge, alte Sitzungen, Fehlrouting, falsche Längen und gemischte Versionen.
Die historischen [V1-Fixtures](fixtures/wire-v1.json) bleiben als Quelle erhalten.
Gemeinsamer Prüfcommit und Restabnahme: [Konsolidierungsbericht](../../handoff/KONSOLIDIERUNG-2026-10-17.md).
