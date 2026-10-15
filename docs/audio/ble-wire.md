# Audio-Erweiterung des vorhandenen BLE-Vertrags

Stand: **16. Oktober 2026. Implementierungsvertrag; kein Hör- oder
Verdrahtungsnachweis.** Grundlage bleibt der
[LED-/Wake-Vertrag Version 1](../led/ble-wire-v1.md): gleiche Dienst-UUIDs,
Einrichtung, gegenseitige HMAC-Anmeldung, Sitzungsbindung und Fragmentierung.
Der vorhandene Box-Zugang wird für den begrenzten Audiotest verwendet;
zusätzliche Spielleiterrechte oder eine Spielintegration entstehen nicht.

Quellen: [`LedWire.h`](../../firmware/led/include/LedWire.h),
[`LedWire.cpp`](../../firmware/led/src/LedWire.cpp),
[`LedProtocol.kt`](../../companion/android/core/protocol/src/main/kotlin/de/larpescape/protocol/LedProtocol.kt),
[`AudioOutput.cpp`](../../firmware/led/src/AudioOutput.cpp) und
[`DfPlayerBus.cpp`](../../firmware/led/src/DfPlayerBus.cpp).

## Gemeinsame Regeln

Mehrbytezahlen sind little endian; HMAC-SHA-256 verwendet vollständige
32-Byte-Tags und ASCII-Domänen ohne NUL. Audio und LEDs teilen eine
steigende Requestnummer und die gemeinsame Statussequenz. Ihre
Ausgaberevisionen sind getrennt. Es darf insgesamt nur ein manueller
Auftrag offen sein.

Für Audio gelten dieselben Sitzungs-/Frist-/Requestprüfungen wie für LEDs:
erwartete Audio-Revision, gültiger Zugang, genau nächste Requestnummer,
verbleibende Gerätefrist 1–3000 ms und zulässiger Modus 0/1. Die Anwendung
verwendet nur frische Beobachtungen. Eine exakte Wiederholung des letzten
verbrauchten Befehls liefert die gespeicherte Antwort; sie spielt nichts
erneut ab. Eine identische noch ausstehende Anfrage erzeugt keinen zweiten
Ausgabeauftrag. Nach unklarer Zustellung erfolgt kein automatischer Retry.

Modus 0 (`LED_ONLY`) bezeichnet weiterhin den eigenständigen manuellen
Produktpfad ohne Spielcontroller. Er wird für diese Audio-Erweiterung
beibehalten und behauptet keinen gemessenen gestoppten Spielzustand.

## Audio-Snapshot: acht Bytes

| Relativer Offset | Inhalt |
|---:|---|
| 0–3 | Audio-Revision, LE32 |
| 4–5 | zuletzt ausgegebener Titel, LE16; 0 bei Stop |
| 6 | zuletzt ausgegebener Lautstärkestellwert, 0–10 |
| 7 | Audioflags |

Audioflags: Bit 0 `ready`, Bit 1 `pending`, Bit 2 `active`, Bit 3
`busyValid`, Bit 4 `busyLow`, Bit 5 `fault`; Bits 6/7 sind null.
BUSY LOW ohne gültige Abtastung ist unzulässig. Bereit und Fehler sind
nicht zugleich gültig; aktiver Auftrag verlangt Titel und Lautstärke > 0.

`ready` bedeutet initialisierter ESP-Ausgabepfad nach vollständig
übertragenem Stop beim Firmwarestart. `pending` bezeichnet die noch laufende
Befehlsfolge. `active` bedeutet, dass Play ausgegeben wurde und weder ein
Stop noch ein anschließend beobachtetes BUSY-Ende den Zustand beendet hat.
Auch dieses Bit ist kein Hörbeweis. Nach einem BUSY-Ende kann der zuletzt
ausgegebene Titel bis zum nächsten Stop im Snapshot stehen bleiben.

`busyValid` bestätigt nur eine initialisierte GPIO-Abtastung. Es erkennt
weder den DFPlayer noch eine angeschlossene BUSY-Leitung. `busyLow` ist
der dabei gelesene Rohpegel an GPIO14. Weder der Pegel noch fehlende
UART-Antworten bestätigen oder widerlegen eine bestimmte Audiodatei.

## Granted und laufender Status

Typen bleiben `0x82` und `0x84`. Ohne Audio-Unterstützung bleibt die Länge
**70 Bytes**. Mit Audio-Unterstützung beträgt sie **78 Bytes**:

| Offset | Inhalt |
|---:|---|
| 0–37 | unveränderter LED-/Wake-Status einschließlich Sitzung und gemeinsamer Sequenz |
| 38–45 | Audio-Snapshot |
| 46–77 | HMAC über `LARP-LED/1/status` und die ersten 46 Bytes |

Die Länge zeigt die Unterstützung an; die Audioflags enthalten kein
zusätzliches Supported-Bit. Neue Apps verstehen auch 70-Byte-Status und
zeigen fehlende Audio-Unterstützung. Alte Apps, die ausschließlich
70 Bytes erwarten, sind mit der neuen Firmware nicht kompatibel:
zuerst die passende neue App installieren.

## Audio-Command: 66 Bytes

| Offset | Inhalt |
|---:|---|
| 0–1 | Version 1, Typ `0x04` |
| 2–17 | Session-ID |
| 18–21 | gemeinsame Requestnummer, LE32 |
| 22–25 | erwartete **Audio-Revision**, LE32 |
| 26–29 | `expiresAt` in Geräte-Millisekunden, LE32 |
| 30 | Operation: 1 Start, 2 Stop |
| 31–32 | Titelnummer, LE16; Start **1–255**, Stop **0** |
| 33 | Lautstärke; Start **1–10**, Stop **0** |
| 34–65 | HMAC über `LARP-LED/1/audio-command` und die ersten 34 Bytes |

Auswahl allein sendet nichts. Die Appvorgabe für Lautstärke ist 5. Start
desselben Titels ist ein bewusster Neustart. Der Besitzer überträgt
**Stop (`0x16`) → Lautstärke (`0x06`) → MP3-Ordner-Titel (`0x12`)**,
jeweils mit mindestens 100 ms Abstand nach abgeschlossenem Transfer.
Nach 30 Sekunden ab abgeschlossenem Play-Transfer fordert er Stop an,
auch wenn BUSY vorher ein Ende anzeigte. Das verhindert, dass eine
unbestätigte BUSY-Leitung die Begrenzung außer Kraft setzt.

## Audio-Result: 82 Bytes

| Offset | Inhalt |
|---:|---|
| 0–1 | Version 1, Typ `0x85` |
| 2–17 | Session-ID |
| 18–21 | beantwortete gemeinsame Requestnummer, LE32 |
| 22–25 | nächste gemeinsame Requestnummer, LE32 |
| 26–29 | aktuelle **LED-Revision**, LE32 |
| 30–33 | Geräte-Millisekunden, LE32 |
| 34–37 | gemeinsame Statussequenz, LE32 |
| 38 | Ergebniscode wie im LED-Vertrag, 0–11 |
| 39–41 | LED-Effekt, LED-Flags, Modus |
| 42–49 | Audio-Snapshot, einschließlich eigener Audio-Revision |
| 50–81 | HMAC über `LARP-LED/1/audio-result` und die ersten 50 Bytes |

LED-Results bleiben unverändert **74 Bytes / Typ `0x83`** und enthalten
keinen Audio-Snapshot. Audio-Results enthalten keine Wake-Messung.
Ein frisches Audio-Result darf die Audio-Beobachtung aktualisieren;
ein LED-Result erneuert deren Frist nicht. Die App entwertet Audio nach
1000 ms ohne neuere vollständige Audio-Beobachtung und bei Trennung.
Ältere Sequenzen dürfen eine passende offene Requestkorrelation
abschließen, aber keinen neueren Zustand überschreiben.

`Applied` ist erst erreicht, wenn der Ausgabebesitzer die passende interne
`appliedRevision` ohne Pending meldet. Der SDK-Adapter wartet auf das
physische Leeren des UART-Senders. Das bestätigt **gesendete Bytes**, keine
korrelierte Modulantwort, vorhandene microSD oder hörbare Ausgabe.
Eingehende spontane UART-Daten werden begrenzt verworfen. Ein ATT-ACK
bestätigt weiterhin nur den BLE-Transport.

Start sendet vor Play Stop; Firmwareboot sendet ausschließlich Stop.
Erkannter Verbindungs-/Abonnementsverlust, Sitzungs-/Protokollfehler und
der Sitzungstimeout für ausstehende Ausgabe fordern Stop über denselben
Besitzer an. Ein einzelner UART-Transfer hat eine Softwarefrist von 500 ms;
die Session wartet höchstens 2000 ms auf Anwendung ihrer Zielrevision.
UART-Fehler dürfen keine bestätigte Stille erfinden.

## Hardware- und Prüfgrenze

UART1 verwendet 9600/8N1, ESP-RX25 und ESP-TX26; BUSY wird an GPIO14 ohne
Pull-up/-down abgetastet. GPIO13 wird nicht konfiguriert oder geschaltet.
Der alte `playFromMP3Folder`-Aufruf nutzt ebenfalls Kommando `0x12`.
Die numerische MP3-Ordneradressierung ist damit beibehalten; daraus folgt
keine bestätigte Dateiliste auf der vorhandenen Con-Karte.

Die Kommandocodes und Prüfsumme wurden am 16.10.2026 mit der
[DFRobot-Referenzimplementierung](https://github.com/DFRobot/DFRobotDFPlayerMini/blob/master/DFRobotDFPlayerMini.cpp)
abgeglichen. Die [DFRobot-Moduldokumentation](https://wiki.dfrobot.com/dfr0299/docs/20905)
beschreibt die numerische MP3-Ordneradressierung. Diese Herstellerquellen
ersetzen keine Bestätigung des tatsächlich verbauten Moduls.

Die reproduzierbaren Prüfwege stehen im
[Testplan](test-preparation.md#softwareprüfung-vor-installation). Die
Sitzungs-/Framingtests verwenden echte HMAC-Implementierungen; die
Audio-Suite prüft einen deterministischen asynchronen Bus und den
tatsächlichen SDK-Adapter gegen Stubs. Die App besitzt Protokoll- und
Zustandsprüfungen. Der echte ESP32-Build prüft die SDK-Anbindung. Erst der
gemeinsame Lauf ergänzt getrennte UART-, BUSY- und Hörbelege.
