# ESP-NOW in der Baugruppenbedienung

Stand: **18. Oktober 2026**. Implementiert und softwareseitig geprüft;
**Installation und gemeinsame Handy-/BLE-/Funkprüfung noch offen**.
Der frühere [Zwei-Geräte-Nachweis](device-test-2026-10-18.md) bleibt gültig
für seine isolierte Probe und ist keine Abnahme dieses neuen App-Paars.

## Bedienung und Installation

Die bestehende ESP-Seite enthält die Karte **ESP-NOW testen**. Dort die
STA-MAC des zweiten ESP eingeben und bewusst **Testpaket senden** drücken.
Die Empfängerfirmware gibt ihre MAC beim Start per USB aus. Auswahl, Anmeldung,
Wiederverbindung und Appstart senden kein Funk-Testpaket. Es gibt keine
automatischen Wiederholungen. Ein laufender Test sperrt weitere Aufträge in
derselben BLE-Sitzung; LED, Audio und NFC teilen weiterhin den Auftragszähler.

Nach Abschluss zeigt die Karte getrennt:

- **Senden:** Hat die Box den Auftrag lokal an den Funk übergeben?
- **Funkzustellung:** Liegt die Sendecallback-Beobachtung der MAC-Schicht vor?
- **Empfang:** Hat der gewählte Empfänger eine zu diesem Test passende Rückantwort geliefert?

Eine erfolgreiche Funkzustellung allein bestätigt den Empfang ausdrücklich
nicht. Ergebnisse sind als **letzter abgeschlossener Test** mit Auftragsnummer
gekennzeichnet. Abgelaufener Boxstatus sperrt neue Tests. Ein Verbindungsende
verwirft das alte Ergebnis; bei laufendem Test lautet der Ausgang unbekannt.
Ein schon über Funk übergebenes Paket kann nicht zurückgerufen werden.

Benötigt werden die zusammengehörigen Artefakte:

| Gerät | Variante |
|---|---|
| Box | `firmware/led`, Umgebung `larp-espnow-test` mit bestehendem BLE-Zugang, LED/Wake/Audio |
| Zweiter ESP | `firmware/espnow-probe`, Umgebung `espnow-app-receiver` |
| Handy | neue Companion-App aus diesem Branch |

Die bisherige 32-Byte-USB-Probe des Test-ESP versteht diese neue Funkhülle
nicht. Ein Empfängerwechsel ist erforderlich. Die neue Empfängerfirmware
bestätigt nur Testdaten und schaltet keine externen Ausgänge.

Vor einer separat beauftragten Installation beide Chip-MACs und die
chipgebundenen Rückwege prüfen, aktuelle vollständige Flash-Sicherungen
erstellen und verifizieren. Vorherige Handy-APK/Signatur sichern und ein
Update mit Datenerhalt vorbereiten. Bestehenden Boxzugang und `larpkeys`
erhalten; ausschließlich die Anwendung ersetzen. Die vorangegangene
Freigabe galt dem abgeschlossenen temporären Versuch mit Wiederherstellung.
Dieser Implementierungsauftrag hat noch keinen neuen Geräteupload ausgeführt.

## Ergänzung des Baugruppenvertrags V2

Die BLE-Fragmenthülle, Anmeldung, V2-Domänen und gemeinsame Auftragsfolge
bleiben erhalten. Im unverändert 91 Byte langen Gesamtstatus ist Offset 38
nun ein Fähigkeitsbyte: Bit 0 Audio unterstützt, Bit 1 ESP-NOW unterstützt,
Bit 2 ESP-NOW bereit; Bits 3–7 null. Bereit ohne unterstützt ist ungültig.
Nur vollständige neue Statussequenzen erneuern die 1000-ms-Frische. Ergebnisse
anderer Baugruppen erneuern die ESP-NOW-Fähigkeitsbeobachtung nicht.

Neue App mit bisheriger V2-Firmware: ESP-NOW sichtbar, aber gesperrt.
Die bisherigen Firmwarevarianten starten weiterhin kein WiFi. Eine alte V2-App
lehnt die erweiterten Fähigkeitsbits der ESP-NOW-Variante ab; hierfür ist das
neue Paar nötig. Keine Größenheuristik und keine neue Behandlung von V1.

| Typ | Größe | HMAC-Domäne / Bodygröße |
|---|---:|---|
| `0x06` ESP-NOW-Auftrag | 70 Byte | `LARP-LED/2/espnow-command`, Body 38 |
| `0x87` ESP-NOW-Ergebnis | 108 Byte | `LARP-LED/2/espnow-result`, Body 76 |

Auftrag: Version/Typ 0–1, Session-ID 2–17, LE32 Auftrag 18–21,
reservierte LE32 Null 22–25, LE32 Gerätefrist 26–29 (maximal 3 s),
Operation 1 an 30, fester Kanal 1 an 31, sechs Peer-MAC-Bytes 32–37,
HMAC 38–69. Null-, Broadcast- und Multicast-Ziele werden verworfen.

Ergebnis 0–41 folgt den bisherigen LED-Ergebnisfeldern: Auftrag an 18,
nächster Auftrag an 22, LED-Revision an 26, Gerätezeit an 30,
gemeinsame Statussequenz an 34, Ausgang an 38, LED-Effekt/Flags/Modus 39–41.
Keine Behauptung einer LED-Änderung durch einen Funk-Test.

| Offset | ESP-NOW-Ergebnisfeld |
|---|---|
| 42 | lokale Sendeannahme, genau 0 oder 1 |
| 43 | MAC-Zustellung: 0 unbekannt, 1 erfolgreich, 2 fehlgeschlagen |
| 44 | passende Rückantwort erhalten, genau 0 oder 1 |
| 45 | Kanal 1 |
| 46–51 | Peer-MAC |
| 52–67 | zufällige Kennung dieses Funkversuchs |
| 68–71 / 72–75 | LE32 Empfangszähler / Bootkennung des Empfängers |
| 76–107 | HMAC |

Ausgang 0 bedeutet für diesen Typ **Empfang bestätigt** und verlangt lokale
Sendeannahme, frische Kennung, gültigen Peer und einen Empfangszähler > 0.
12 bedeutet fehlgeschlagene MAC-Zustellung, 13 keine passende Rückantwort
nach 2000 ms; 10 bleibt lokaler Treiber-/Queuefehler. Sonstige Ablehnungen
behalten die bisherigen Werte. Ablehnungen oder Busy-Antworten dürfen keine
Empfangsbeobachtung eines früheren Tests übernehmen. Nur identische Wiederholung
des zuletzt abgeschlossenen BLE-Auftrags liefert einen Cache, ohne erneut zu
senden. Die App selbst wiederholt den Auftrag nicht.

## Funkhülle und Besitzergrenzen

STA, Kanal 1, unverschlüsseltes Unicast. Ein Auftrag hat exakt **56 Byte**:
`LENT` an 0–3, Version 1 an 4, Typ 1 an 5, LE16 Nutzlänge 32 an 6–7,
zufällige 16-Byte-Kennung an 8–23, `ACTIVATE` mit Nullauffüllung auf 32 Byte
an 24–55. Das Format kapselt die bekannte Nutzlast; es ist kein unverändert
ausgesendetes Legacy-Spielpaket.

Der Empfänger prüft die gesamte Hülle und die 32 Nutzbytes. Seine **64-Byte-
Rückantwort** kopiert die ersten 56 Byte, setzt Typ 2 und hängt LE32 Bootkennung
an 56 und LE32 Empfangszähler an 60 an. Die Box prüft Länge, gesamte Hülle,
Quell-MAC und genaue Versuchkennung. Alte oder fremde Rückantworten werden
ignoriert. Korrelation ersetzt keine kryptografische Empfängeranmeldung;
aktive Funkangreifer, verschlüsselte Peers und Spielrollen sind hier nicht abgenommen.

WiFi-Callbacks kopieren nur in eine begrenzte Queue. Auswertung, Fristen und
BLE-Abschluss erfolgen im vorhandenen Anwendungslauf. Ein noch ausstehender
Sendecallback sperrt seine Sendespur auch nach Abbruch/Timeout, bis er abgeholt
wurde; ein später Callback darf keinen neuen Versuch bestätigen. Queuedrops
sperren neue Tests bis zum Neustart. BLE-Abbruch verwirft den offenen Versuch.

## Gemeinsamer Handy-Test nach Installation

1. Empfänger mit neuer Variante starten, MAC ablesen; Box in der App anmelden.
2. Richtige Empfänger-MAC eintragen. Ohne Tastendruck darf kein Test entstehen.
3. **Testpaket senden:** Empfang bestätigt, Auftragsnummer und Empfängerzähler
   sichtbar. Derselbe bewusste Test erneut: neuer Auftrag, höherer Zähler.
4. Zweiten ESP stromlos machen und testen: kein Empfangserfolg; Funkfehler
   oder Zeitüberschreitung sauber von lokaler Sendeannahme getrennt.
5. Empfänger neu starten und erneut bewusst testen: neue Bootkennung,
   neuer Empfangszähler möglich, aktuelle Rückantwort erforderlich.
6. Während eines Tests BLE trennen oder App verlassen: alter Ausgang wird
   nicht als neuer Empfang übernommen; Wiederverbindung sendet nichts.
7. LED und Audio danach bewusst prüfen; deren Wirkung und Funk-/BLE-Koexistenz
   am tatsächlichen Paar getrennt dokumentieren.

Installation und Bereitschaftsprüfung dieses Paars sind im
[Gerätebericht](app-installation-2026-10-18.md) dokumentiert; der gemeinsame
bewusste Sendetest am Handy bleibt offen. Der
[Softwareprüfbericht](app-preparation-2026-10-18.md) führt den konkret geprüften
Quellstand, Artefakte und verbleibende Abnahmen.
