# Eigenständiger LED-Zugang über BLE, Version 1

> Historischer V1-Vertrag der Einzelzweige. Für den konsolidierten Stand gilt
> ausschließlich der [gemeinsame V2-Vertrag](ble-wire-v2.md).

Status: implementierter begrenzter Produktvertrag, 15. Oktober 2026. Dieser Zugang
steuert die eigene LED-Funktion und überträgt lesend die vier Wake-Rohpegel. Er übernimmt weder den optionalen
F1/P-256-Vertrag noch dessen Spielleiter-, Bond- oder Spielstatusversprechen.
Die Einrichtung erfolgt nach ausdrücklicher Nutzerentscheidung einmal über einen
vertrauenswürdigen USB-Weg mit einem zufälligen Schlüssel pro Box. Das öffentliche
Device-ID-Label allein ist kein Authentisierungsnachweis.

## Einrichtung und Grenze

Der lokale Einrichtungsdatensatz enthält `schemaVersion:1`, `deviceId` als
32 Hexzeichen und `secret` als 64 Hexzeichen; `name` ist optional. Die Box-ID hat
16 nicht ausschließlich null Bytes, der zufällige geheime Schlüssel 32 Bytes.
Keine fest eingebauten Testschlüssel, Geheimnisse in Repository, Logs oder
Bluetooth-Advertising. Android speichert den Zugang mit einem nicht exportierbaren AES-Schlüssel aus dem Android Keystore verschlüsselt;
USB-Import und Aufbewahrung des ursprünglichen Einrichtungsdokuments gehören
zur vertrauenswürdigen Einrichtung. Besitzer desselben Schlüssels haben denselben
begrenzten LED-Zugang. Es gibt hier keine Fernvergabe weiterer Rollen.

BLE-Verschlüsselung ersetzt keine Anwendungsauthentisierung. Der gegenseitige
HMAC-Nachweis und die Nachrichtentags liefern den Befehlsintegritätsnachweis.
Dieser Vertrag behauptet keine authentisierte BLE-Paarung, F1-Identität,
SMP-/Bondabnahme oder Vertraulichkeit gegen einen aktiven BLE-Relay. Ein
ordnungsgemäß authentisierter Live-Relay kann nicht allein durch diese
Anwendungsschicht als räumlich entfernte Verbindung erkannt werden.

Der Plattformadapter besitzt genau einen aktiven BLE-Link, lehnt weitere
Verbindungen ab und erzeugt pro Boot sowie **jedem** Link jeweils neue
kryptografisch zufällige 16-Byte-Nonces. Die Android-App erzeugt ihre eigene
frische 16-Byte-Nonce mit SecureRandom. Kein Reconnect übernimmt eine Sitzung,
eine offene Operation oder automatisch gestartetes Licht.

## Dienst und Transport

| Element | UUID |
|---|---|
| Dienst | `6c790001-2f50-4d5a-a7f4-35a478c4b101` |
| Eingang, WRITE mit ATT-Antwort | `6c790002-2f50-4d5a-a7f4-35a478c4b101` |
| Ausgang, NOTIFY mit CCCD | `6c790003-2f50-4d5a-a7f4-35a478c4b101` |

Kein separater Long-Read: Nach bestätigter CCCD-Anmeldung sendet die App Hello.
Alle Nachrichten werden in Werte von höchstens 20 Bytes fragmentiert; MTU 23
genügt. Jedes Fragment beginnt mit acht Bytes:

| Offset | Inhalt |
|---:|---|
| 0 | Version 1 |
| 1 | Frametyp 0x4C |
| 2..3 | Transfer-ID, LE16, 1..65535 |
| 4..5 | Gesamtlänge, LE16, 1..128 |
| 6..7 | Byteoffset, LE16 |
| 8..19 | 1..12 Nutzbytes |

Pro Richtung steigt die Transfer-ID innerhalb eines Links strikt, ohne Umlauf.
Alle Fragmente einer unveränderlichen Nachricht werden vollständig nacheinander
gesendet. Ein neuer Transfer beginnt bei Offset 0. Lücken, Überlappungen,
abweichende Gesamtlängen oder gemischte IDs sind Fehler; nur die exakte
Wiederholung des unmittelbar vorigen noch nicht abschließenden Fragments ist
wirkungslos zulässig. Ein vollständig bearbeiteter Transfer wird nicht erneut
zusammengesetzt. Die absolute Frist ab dem ersten Fragment beträgt 2000 ms;
Duplikate verlängern sie nicht. Nach einem Framingfehler trennt der Adapter den
Link. Bei ausgeschöpfter Transfer-ID wird getrennt, nicht auf 1 umgebrochen.

BLE-Callbacks kopieren nur begrenzte Werte und ursprüngliche Verbindungsgeneration
in eine begrenzte Queue. Der alleinige LED/Mainloop-Besitzer bearbeitet Framing,
Session und LED-Anfragen. Veraltete Generationen werden vor der Verarbeitung
verworfen. Ein ATT-Write-ACK bestätigt lediglich Transportaufnahme.

## Gegenseitiger Nachweis

Alle Mehrbytezahlen sind little endian. HMAC ist SHA-256 mit vollständigem
32-Byte-Tag. Die Domainstrings sind ASCII **ohne** abschließendes NUL. Es wird
niemals ein C++-Structdump übertragen. Kontext:

`DeviceID16 || BootNonce16 || ServerConnectionNonce16 || ClientNonce16`

| Nachricht | Bytes und Offsets |
|---|---|
| Hello, 18 Bytes | Version 1 @0, Typ 1 @1, ClientNonce @2 |
| Challenge, 98 Bytes | 1 @0, 0x81 @1, DeviceID @2, BootNonce @18, ServerNonce @34, ClientNonce @50, Tag @66 |
| Proof, 34 Bytes | 1 @0, Typ 2 @1, Tag @2 |

Challenge-Tag:
`HMAC(key, "LARP-LED/1/challenge" || erste 66 Challengebytes)`.

Android verlangt seine eingerichtete DeviceID und eigene ClientNonce, bevor es
die Challenge-MAC prüft. Erst danach signiert es Proof:
`HMAC(key, "LARP-LED/1/proof" || Kontext)`.

Session-ID: erste 16 Bytes von
`HMAC(key, "LARP-LED/1/session" || Kontext)`.

Die Box stellt nach genau passendem Proof einen authentisierten Granted-Status
bereit. Der einmalige Nachweis muss innerhalb von 10000 ms seit Linköffnung
abschließen. Ein identisches wiederholtes Hello liefert nur dieselbe Challenge;
es verlängert diese Frist nicht. Falsche MAC, falsche Sitzung, unerwartete
Nachweisnachricht oder Protokollfehler schließen den Link und stoppen dessen
manuelle LED-Ausgabe. Die Plattform muss beim Disconnect zusätzlich ausstehende
Startaufträge sperren und die LEDs über den alleinigen Ausgabebesitzer abschalten.

## Status und Spielgrenze

Granted (Typ 0x82) und laufender Status (Typ 0x84) haben ohne Matrix 70 Bytes;
der separate Matrix-Prüfmodus erweitert beide auf 82 Bytes (siehe unten).

| Offset | Inhalt |
|---:|---|
| 0..1 | Version 1, Nachrichtentyp |
| 2..17 | Session-ID |
| 18..21 | aktuelle LED-Revision, LE32; 0 beim Start zulässig |
| 22..25 | nächste Requestnummer, LE32, beginnt bei 1 |
| 26..29 | Geräte-Millisekunden, LE32 |
| 30..33 | Statussequenz, LE32, beginnt bei 1, kein Umlauf |
| 34 | aktuell ausgegebener Effekt |
| 35 | Statusbits |
| 36 | Modus |
| 37 | Wake: Bit 7 unterstützt, Bit 6 gültige Lesung, Bits 0–3 HIGH-Maske Wake 1–4; Bits 4–5 = 0 |
| 38..69 | HMAC über Domain `LARP-LED/1/status` und erste 38 Bytes |

Effekte: 0 Aus, 1 Funkeln, 2 Lichtimpuls/Burst, 3 Ripple. Statusbits:
Bit 0 bereit, Bit 1 Änderung ausstehend, Bit 2 aus, Bit 3 Einmaleffekt beendet,
Bit 4 Ausgabefehler. Andere Bits sind null. Effekt 0 gilt genau bei gesetztem
Aus-Bit. Bereit und Fehler gleichzeitig sind ungültig.

| Modus | Bedeutung und Handbedienung |
|---:|---|
| 0 | LED_ONLY: ausdrücklich kein Spielmodul; Handbedienung erlaubt |
| 1 | bestätigter gestoppter Spielowner; Handbedienung erlaubt |
| 2 | Spiel läuft; Handbedienung gesperrt |
| 3 | Spiel pausiert; Handbedienung gesperrt |
| 4 | Spielstatus unbekannt; Handbedienung gesperrt |

Die aktuelle LED-Produktfunktion veröffentlicht **LED_ONLY**, keine fingierte
Messung „Spiel gestoppt“. Eine spätere Spielintegration muss einen echten
Laufstatus und den alleinigen LED-Besitzer anschließen; Standby/Idle des alten
Spielcontrollers sind keine Belege für ein gestopptes Spiel.

Der Server veröffentlicht alle 250 ms einen authentisierten Status. Android
übernimmt Zustandsdaten nur aus der aktuellen Sitzung mit gültiger MAC und
streng neuer Statussequenz. Ergebnis und Status teilen diese Sequenz. Ist die
letzte neue Nachricht lokal älter als 1000 ms, wird Bedienung bis zu einer
frischen Nachricht gesperrt.

### Wake-Erweiterung für den gemeinsamen Eingangstest

Byte 37 ist Bestandteil derselben Status-MAC. `0` bedeutet keine Unterstützung
(alte Firmware), `0x80` Lesefehler/unbekannt und `0xC0 | mask` eine gültige gemeinsame
GPIOB-Lesung. Ohne Unterstützung müssen alle Bits null sein; ohne gültige Lesung
muss die Maske null sein. Bits 4–5 sind unzulässig. Bit 0 der Maske gehört zu B7,
Bit 1 zu B6, Bit 2 zu B4, Bit 3 zu B3. HIGH wird vorläufig als aktiv interpretiert;
Polarität und B7-Eignung sind keine bestätigten Hardwareeigenschaften.

Nur Granted und laufender Status enthalten Wake. Results behalten ihr Format
und aktualisieren weder Wake-Wert noch dessen Empfangszeit. Die App führt die
höchste gemeinsame Statussequenz, verwirft ältere Messungen und entwertet Wake
nach 1000 ms ohne neueren vollständigen Wake-Status sowie bei Trennung.
Neue Apps unterstützen das bisherige Nullbyte. Alte Apps lehnen die Erweiterung
ab; App und Firmware sind als zusammengehöriges Testpaar bereitzustellen.
Prüfvorbehalte, Bedienung und Abnahme stehen in der
[Wake-Testvorbereitung](../wake/test-preparation.md).

### Matrix-Erweiterung für den gemeinsamen Kontakttest

Stand 16. Oktober 2026. Die neue App akzeptiert exakt 70 oder 82 Bytes für
Granted/Status. Bei 70 Bytes fehlt die Matrixunterstützung. Im 82-Byte-Format
bleiben Bytes 0–37 unverändert; die bisherige MAC wird nach hinten versetzt:

| Offset | Inhalt |
|---:|---|
| 38 | Bit 7 unterstützt, Bit 6 gültiger vollständiger Scan, Bit 5 mehrdeutiges Rechteckmuster; Bits 0–4 null |
| 39..49 | 81 Kontaktbits, row-major: Index `9 * (Zeile - 1) + Spalte`, Spalte A=0 … I=8; je Byte niederwertiges Bit zuerst |
| 49 | nur Bit 0 für I9 zulässig, Bits 1–7 null |
| 50..81 | HMAC über `LARP-LED/1/status` und erste 50 Bytes |

Ohne Unterstützung sind alle Matrixbits null; ohne gültigen Scan sind
Kontaktmaske und Mehrdeutigkeitsflag null. Mehrdeutigkeit setzt Gültigkeit
voraus. Nicht verwendete Bits oder widersprüchliche Flags werden verworfen.
Die MAC schützt auch sämtliche Matrixdaten; Sitzung und gemeinsame strikt
steigende Statussequenz gelten unverändert.

Die Firmware sendet das erweiterte Format nur mit Matrixunterstützung. Results
bleiben 74 Bytes ohne Matrixprobe. Sie können die gemeinsame Sequenz anheben,
verlängern aber weder Matrixfrische noch Wakefrische. Fragmente und gleiche/
ältere Sequenzen zählen ebenfalls nicht als neue Messung. Android entwertet
Matrixwerte nach 1000 ms ohne neueren vollständigen Messstatus, bei ungültigem
Scan, Sitzungswechsel und Trennung. Eine Teilrunde ist kein gültiger Scan.

Das Mehrdeutigkeitsflag beschreibt ein beobachtetes Rechteckmuster, keine
Diagnose eines einzelnen defekten Kontakts und keine zugesicherte Erkennung
jeder möglichen Ghosting-Situation. Kontaktbits bleiben Rohbeobachtungen.
Der [Matrix-Testplan](../matrix/test-preparation.md) beschreibt die provisorische
Pinzuordnung, umgekehrte Scanrichtung, Hardwarefragen und Abnahmeschritte.
Alte Apps lehnen das verlängerte Format ab: neue App zuerst installieren.

## Start, Stop und korrelierte Ausführung

Ein Command hat 64 Bytes:

| Offset | Inhalt |
|---:|---|
| 0..1 | 1, Typ 3 |
| 2..17 | Session-ID |
| 18..21 | Requestnummer, LE32 |
| 22..25 | erwartete LED-Revision, LE32 |
| 26..29 | expiresAt in Geräte-Millisekunden, LE32 |
| 30 | Operation: 1 Start, 2 Stop |
| 31 | Effekt: Start 1..3, Stop ausschließlich 0 |
| 32..63 | HMAC über Domain `LARP-LED/1/command` und erste 32 Bytes |

Die App verwendet `expiresAt = letzter authentisierter deviceMillis + 3000`
modulo 2^32 und sendet nur bei lokal höchstens 1000 ms altem Status.
Die Box verlangt bei Aufnahme eine verbleibende Frist von 1..3000 ms.
Ein verspäteter oder zu weit vorausdatierter Befehl schaltet nichts.

Die App hält höchstens eine Operation offen. Auswahl alleine sendet nichts;
Start ersetzt den laufenden manuellen Effekt. Funkeln läuft bis Stop;
Burst und Ripple laufen einmal und enden vollständig dunkel. Stop beendet
die manuelle Ausgabe. Wiederverbinden oder Effektauswahl startet nichts.

Der Sessionkern ruft den Ausgabebesitzer nur mit gültiger MAC/Sitzung,
exakt nächster Requestnummer, Frist, Modus, Bereitschaft und erwarteter Revision
auf. Der Besitzer nimmt eine neue, nicht umlaufende Revision entgegen; er darf
bei erschöpfter Revision keine Ausgabe mehr annehmen. Queued/Pending ist noch
kein Ausführungserfolg. Erst wenn der tatsächliche Besitzer die Zielrevision
als `appliedRevision` ohne Pending veröffentlicht, folgt Applied. Fehler,
fremde Revision oder ausbleibende Anwendung nach 2000 ms erzeugen einen
Fehler; beim Timeout wird zusätzlich ausgeschaltet. Applied beschreibt den
bestätigten Software-/Treiberweg, **keine menschlich beobachtete Leuchte**.

Ein Result hat 74 Bytes:

| Offset | Inhalt |
|---:|---|
| 0..1 | 1, 0x83 |
| 2..17 | Session-ID |
| 18..21 | betroffene Requestnummer |
| 22..25 | nächste Requestnummer |
| 26..29 | aktuelle LED-Revision |
| 30..33 | Geräte-Millisekunden |
| 34..37 | gemeinsame Statussequenz |
| 38 | Ergebnis |
| 39..41 | Effekt, Statusbits, Modus |
| 42..73 | HMAC über Domain `LARP-LED/1/result` und erste 42 Bytes |

Ergebnisse: 0 Applied, 1 GameActive, 2 StaleRevision, 3 InvalidCommand,
4 Unavailable, 5 Conflict, 6 OutOfOrder, 7 Exhausted, 8 Expired, 9 Busy,
10 DriverFault, 11 Superseded. Eine authentisierte neue Anfrage wird auch bei
fachlicher Ablehnung verbraucht. Fremde/ausgelassene Nummern werden nicht
verbraucht. Requests laufen nicht um; bei UINT32_MAX werden keine neuen
Aktionen angenommen.

Die letzte verbrauchte Anfrage samt exakter Antwort bleibt für diese Sitzung
gespeichert. Exakt dieselbe Anfrage wiederholt nur die Antwort; dieselbe Nummer
mit anderem Inhalt ergibt Conflict. Eine identische noch ausstehende Anfrage
löst keine zweite Ausgabe aus. Der Client wiederholt nach unklarer Zustellung
nicht automatisch: Er trennt die Verbindung und zeigt den unklaren Ausgang.
Ein gültiges älteres Result darf eine passende offene Requestkorrelation
abschließen, überschreibt aber keinen bereits neueren Status.

## Implementierung und Nachweise

Transportfreier Kern: `firmware/led/include/LedWire.h` und `firmware/led/src/LedWire.cpp`.
`Session` und `Assembler` gehören genau einem Mainloop-/Workerbesitzer.
Die Plattform injiziert HMAC, Zufall, Uhrzeit und `IControl`; das ist kein
Ersatz für diese tatsächlichen Adapter. `disconnect()` muss auf diesem Besitzer
auch bei Queueüberlauf, Authentisierungsfehler oder BLE-Verlust erreicht werden.

Öffentliche bekannte Testschlüssel und Crosslanguage-Nachrichten stehen in
[fixtures/wire-v1.json](fixtures/wire-v1.json). **Nie auf Geräte provisionieren.**
Der Generator nutzt Python HMAC/SHA256; der native Test BCrypt auf Windows
bzw. OpenSSL auf anderen Hosts. Reproduzierbar:

```text
python test/test_led_ble_wire/run.py
```

Der Test prüft Goldenbytes, gegenseitige Bindung, falsche MACs/Sitzungen,
Fristen, Rollen-/Modussperre, drei Effekte, Wiederhol-/Revisionsschutz,
verspätete tatsächliche Anwendung, Fehler, Framing und Millisekundenumlauf.
Hosttests beweisen keine BLE-Funkübertragung, SMP-Eigenschaft oder sichtbare LED.
