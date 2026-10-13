# GM-Companion NFC v1

Quelle: Referenzprojekt `experiments/gm-companion`, Commit
`73df05a80127dc88eaba68a1701bf7beecb505c9`, `MauiApp1/Data/TagData.cs`.
SHA256 der unverändert ausgeführten C#-Quelldatei:
`55A3705AB3887B7E77F6BC87DB6A93F41FEF71C2A1583FBE05A630D72BEB1B3F`.
Referenz-Service: `MauiApp1/Platforms/Android/NfcService.Android.cs`.
Abgleich am 2026-10-10 mit .NET SDK 10.0.401 und Kotlin-JVM-Tests.
Nutzerentscheidung vom 2026-10-10: Dieses Format ist ein Vorschlag und dient
hier als NFC-Testformat. Tags sollen künftig Spielobjekte für die ESP-Schicht
tragen und Geräteaktionen beeinflussen. Ein gemeinsamer Firmware-/App-Vertrag
dafür ist noch offen. Die Nutzerangabe über früheren Hardwareerfolg ist keine
neue Hardwareprüfung.

NDEF MIME: `application/vnd.yourapp.tagdata`. Der folgende Vertrag beschreibt
nur den Record-Payload, nicht die gesamte NDEF-Nachricht.

| Offset | Inhalt |
|---|---|
| 0 | Version, Byte, genau 1 |
| 1 | TagUIDHash, uint32 little endian |
| 5 | Flags, uint16 little endian |
| 7 | DateTime.ToBinary(), int64 little endian, inklusive Kind-Bits |
| 15 | Anzahl Besuche, uint16 little endian |
| 17 | Je Besuch: byte NodeId, byte Result, uint16 DeltaMinutes little endian |
| 17 + 4 × Anzahl | CRC32 über alle vorangehenden Bytes, uint32 little endian |

CRC32: reflektiertes Polynom 0xEDB88320, Start 0xFFFFFFFF, abschließend
XOR 0xFFFFFFFF. Exakte Länge 21 + 4 × Anzahl, keine Zusatzbytes.
Kotlin begrenzt den Payload auf 65535 Bytes; physische Gesamtkapazität wird
zusätzlich einschließlich aller NDEF-Records geprüft. Zeitstempel sind keine
Java-Epochwerte und werden beim Bearbeiten unverändert erhalten.

Unabhängig durch Ausführung der originalen C#-Klasse erzeugte Fixtures:

- Leer: Hash 0, Flags 0, TimestampBinary 0, keine Besuche.
  `0100000000000000000000000000000000FE3A494E`
- Belegt: Hash 2309737967, Flags 4660, TimestampBinary 5250958343387387904
  (2026-10-10 12:34:56 UTC), Besuche (1,2,300) und (255,254,65535).
  `01EFCDAB8934120098FAE3CA26DF48020001022C01FFFEFFFF88608A1E`

Die Golden-Fixtures werden in `NfcTagDataTest` bytegenau geprüft. Der originale
C#-Service wird nicht kopiert: Android-NDEF-Umschlag und Payload werden getrennt,
Schreib-IO abgewartet, Ressourcen geschlossen und Ergebnisse frisch zurückgelesen.
