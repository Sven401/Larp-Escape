# Diagnose-Controltransport — DG-Pilot v1

Stand 2026-10-10. **Hostadapter mit synthetischen Komponenten**, kein produktiver
GATT-Endpunkt und keine Gerätefreigabe. Baut auf Diagnosevertrag `3ef6781` und
Firmwareowner `510857f` (inhaltlich übernommen) auf. `src/main.cpp` und bestehende
GameCommand-v2-/GATT-Frames bleiben unverändert. Dieses eigene `DG`-Framing ist
ein ausführbarer Control-Entwurf; Android unterstützt diese Bytes noch nicht.

## Feste Bytes und Budget

Ein Request enthält exakt **69 Bytes**, eine Antwort exakt **88 Bytes**. Beide
liegen unter dem 128-Byte-Aufnahmebudget. Kein Heap, keine Fragmente, kein JSON,
keine Prepared Writes, keine implizite Fragmentierung. Vor BLE-Anschluss muss die
effektive ATT-Nutzlast mindestens 88 Bytes tragen; kleine MTUs werden bis zu einem
eigenen geprüften Framingvertrag abgewiesen. Die heutigen GATT-Dispatcher erkennen
`DG` ausdrücklich nicht. Es wird keine vorhandene Nachrichtenart umgewidmet.

Mehrbytezahlen sind unsigned Little Endian. UUIDs sind 16 Bytes in der Reihenfolge
der kanonischen UUID-Hexdarstellung, einschließlich v4-/Variantbits. Geräte-ID
ebenso 16 Bytes in Hexreihenfolge; `bootId` ist der Zahlenwert der 16 Hexziffern.
Bindings und Authority stehen ausschließlich in kopierten vertrauenswürdigen
Adaptermetadaten, nie in diesen Bytes.

| Offset | Bytes | Request | Antwort |
| --- | --- | --- | --- |
| 0 | 2 | ASCII `DG` | identisch |
| 2 | 1 | Version 1 | identisch |
| 3 | 1 | 1 Capability, 2 Grant, 3 Cancel | Requestart OR 0x80 |
| 4 | 4 | Nachricht 1..UINT32_MAX | ursprüngliche Nachricht |
| 8 | 16 | öffentliche Geräte-ID, nicht null | ursprüngliches Gerät |
| 24 | 8 | Boot-ID, nicht null | ursprünglicher Boot |
| 32 | 16 | Lauf-UUID oder null bei Capability | ursprünglicher Lauf |
| 48 | 16 | Grant-UUID nur bei Cancel, sonst null | geräteseitiger Grant bei erfolgreichem Grant; bei Cancel Echo; sonst null |
| 64 | 4/1 | TTL nur bei Grant, 1..60000 ms | Ergebniscode (1 Byte) |
| 65 | 1 | Teil der TTL | Flags = 3: Bit 0 synthetisch, Bit 1 nur Control |
| 66 | 1 | Teil der TTL | logisches Diagnoseschema = 1 |
| 67 | 1 | Teil der TTL | reserviert = 0 |
| 68 | 1/8 | ausdrückliche Zustimmung: nur Grant = 1, sonst 0 | Grant-Ausgabezeit (8 Byte), sonst 0 |
| 76 | 8 | — | Grant-Ablaufzeit, sonst 0 |
| 84 | 4 | — | maximales Grant-TTL = 60000 |

Ergebniscodes: 0 Accepted, 1 PermissionDenied, 2 Invalid, 3 Busy, 4 Consumed,
5 Expired, 6 OwnerError, 7 Capacity. Nicht benötigte Felder müssen null sein.
Unbekannte Art/Version, falsche Länge, UUID, fehlende Zustimmung und fremdes
Gerät/Boot werden vor Owneraufnahme abgewiesen. Es gibt keine Capability, die
einen Wake-/LED-Schritt über diesen Transport behauptet; die Flags melden nur
den geprüften synthetischen Controlumfang. Firmware-/Hardwareprofil-/Appversionen
und vollständige Operationsfähigkeiten benötigen noch einen eigenen Vertrag.

## Originalbindung und Lebenszyklus

`ControlTransport` läuft auf **demselben serialisierten Owner** wie `IdentityAccess`,
`Control`, Lifecycle und Komponenten. Der spätere SDKadapter muss diese Grenze
herstellen. `IdentityAccess.route` allein ist keine Sperre gegen andere Threads.
Die Aufnahme vergleicht die gesamte beim echten Empfang kopierte Authority mit
der aktuellen Route für genau die ursprüngliche Verbindung. Rolle, Principal und
Epoch müssen gültig sein. Die JSON-Gerätekennung kann diese Prüfung nicht ersetzen.

Es gibt einen Antwortplatz. Capability/Grant werden bei belegtem Platz nicht
ausgeführt. Ein gültiger Cancel für den aktuellen Lauf/Grant/Originalbindung
verdrängt die wartende Antwort, sodass eine ungelesene Grantantwort den Abbruch
nicht verhindert. Ein fremder oder alter Grant kann den neuen Lauf nicht abbrechen.
Der Owner vergibt Grants und verhindert bootweit wiederverwendete Läufe/Grant-IDs.

Aufnahme und Ausgabe verwerfen zukünftige oder mindestens 2000 ms alte
Empfangszeitpunkte. Wartezeit wird nie durch erneute Aufnahmezeit ersetzt.
`takeReply` prüft Originalbindung/Authority erneut, verbraucht die Antwort genau
einmal und verwirft bereits abgelaufene/abgebrochene erfolgreiche Grants. Der echte
Adapter muss unmittelbar im selben serialisierten Turn an genau diesen Empfänger
senden; eine spätere SDK-Sendequeue benötigt ihre eigene identische Fence.

Disconnect und `tick` bedienen den echten C++-Owner; Entzug/Timeout beendet den
Grant auch ohne Antwortabruf. Der Produktionsowner benötigt eine garantierte
Bedienfrist. `Cancel/Accepted` bestätigt ausschließlich den verarbeiteten
Freigabeabbruch, **keinen physischen Aus-Zustand**. Ausführungsergebnisse mit
`restoredOff`/Ownerfehler bleiben im separaten Diagnose-Endergebnis. Fehlerhaftes
LED-Ausschalten lässt den Komponentenbesitz im Owner gesperrt. Für den nächsten
Schnitt muss dieses Endergebnis samt Fehlerstatus über Transport/App sichtbar werden.

## Exakte Anschlussgrenzen

1. F0-Produkt `src/main.cpp` startet Spiel/Komponenten, keinen BLE/Authdienst.
   GATT/Auth leben bislang in `src/GattsAbiProbe`. Ein Hostgrant bedeutet daher
   weder echte Authentisierung noch eine sichtbare Geräte-Testfreigabe.
2. F1-SDK-Etappe `d1d3b4e` belegt HCI-Dispatch, aber keinen sicheren Ursprung des
   restaurierten Bondmodus vor dem SMP-Reset. Ohne diesen Ursprung, aktuelle
   Challenge/P256/Registry-ACK und Lebenszyklusentwertung wird kein Bond-/Rollenrecht
   angeschlossen. Ein frischer Pairingpfad benötigt ebenfalls die echte F2-
   Service-/Actor-/Credential-/Registry-Integration im Produkt.
3. `IPort` muss Idleprüfung, exklusiven Komponentenbesitz, unterdrückte Spieleingaben,
   gesperrten Spielstart, sicheren Timer, Aus-Bestätigung und sichtbaren Testmodus
   atomar umsetzen. Aktuell sind diese Ports synthetisch.
4. LED-Revision/Pin und Wake-Zuordnung brauchen das geprüfte Hardwareprofil.
   Dieses Paket trifft keine GPIO-, Pegel- oder Leistungsannahmen.
5. Nächster Implementierungsschnitt: `DG`-Controldecoder/Antwortparser in Android
   gegen dieselben Bytefixtures, dann getrennte Step-/Endergebnisrahmen mit 64
   Flanken und vollständigen Korrelations-/Bytebudgets. Erst anschließend den
   nachweisbar authentisierten Service an den serialisierten realen Owner anschließen.

## Reproduktion

`python scripts/check_diagnostic_transport.py` kompiliert tatsächlichen Transport,
Diagnoseowner und Identitäts-/Authoritykern mit C++11/Werror und führt Hostfälle
aus. `python scripts/check_diagnostic_control.py` prüft den übernommenen Owner.
Die Fixture stellt ausschließlich im Test vertrauenswürdige Peer-Evidence bereit;
sie ist kein SDK-/Funk-/Kryptografiebeleg. Kein Upload, Restore oder Hardwarelauf.
