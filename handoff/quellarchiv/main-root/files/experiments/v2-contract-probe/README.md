# Erste v2-Kommunikationsprüfung

Isoliertes Vertragslabor für die echte `GameControl`-/`ProfileApplication`-Logik
und unveränderte v2-JSON-Codecs, FreeRTOS-Kanäle und Frame-Sender des Projekts.
Die Firmwarequellen sind eingefrorene Dateikopien aus `firmware-peer-ingress`;
Herkunft und SHA-256 stehen in `firmware-source-manifest.json`. Dieser Worktree
hat zusätzliche lokale Änderungen; die HEAD-Angabe ersetzt die Dateihashes nicht.

**Testeingänge:** synthetische Gerätebesitzer aus der vorhandenen Hostfixture,
RAM-Profiljournal und direkt eingespeiste GM-Berechtigung (Principal 7).
Kein Pairing, kein Peer-Schlüsselnachweis, kein dauerhafter Profilspeicher und
keine physische Spielgeräteprüfung. Nicht als Produktfirmware einsetzen.

## Durchführung

1. `platformio run -d experiments/v2-contract-probe -e host-contract`
   und danach `.pio/build/host-contract/program.exe` ausführen. Die Checks
   bleiben auch im Release-Build aktiv und schlagen mit Exitcode 1 fehl.
2. `platformio run -d experiments/v2-contract-probe -e esp32-contract`.
   Vor einem Upload Partitionstabelle, aktive OTA-Partition und passenden
   Rückweg prüfen. Nicht blind den normalen Upload benutzen: Auf diesem
   Prüfgerät ist die gesicherte aktive Apppartition bei `0x10000`, Länge
   `0x140000`. Nur das gebaute Appabbild wurde dort geschrieben.
3. Android-App `experiments/handy-esp-diagnostic` bauen/installieren,
   Bluetooth und „Geräte in der Nähe“ erlauben. „Larp v2 Vertragslabor“ wählen.
   Die App prüft automatisch den Labmarker und abonniert die Fragmentschnittstelle.
4. Nach `V2 VERTRAG PASS` erneut dasselbe Ziel wählen. `V2 RECONNECT PASS`
   prüft die neue Lease und die Ablehnung der vorherigen. Eine weitere
   vollständige Prüffolge benötigt einen App-Kaltstart; der ESP muss dafür
   nicht neu starten. Das Spiel muss vorher gestoppt sein.

PlatformIO kann beim Wechsel der Umgebung das vorherige Buildverzeichnis
entfernen. Benötigte Abbilder vor dem Wechsel lokal sichern. Lokale Abbilder
und Rohbelege liegen ignoriert unter `artifacts/` und `evidence/`; kanonische
Prüfbelege und Arbeitsstand gehören in die getrennte Projekt-KI-Werkbank.

## Übertragung und Abnahme

Eigener Labservice `86ddc5b0-3878-4ff7-9f55-17c62aeb1001`: Endung `1002`
READ Labmarker, `1003` WRITE Befehlsframes, `1004` NOTIFY Antwort-/Statusframes.
Das ist kein Nachweis des kanonischen Produkt-GATT-/Anmeldepfads.

Tatsächlicher Projektrahmen: Version 1, Typ 1 Command / 2 Reply / 4 Status,
8-Byte-Header mit message/total/offset als LE16. Im Lab 20 Byte pro ATT-Wert,
also höchstens 12 Nutzbytes. Vollständige JSON-Nachrichten haben Schema 2;
Status bis 6143 Byte, Reply bis 511 Byte, Command-Decoder bis 1024 Byte.
Absolute Fristen: Command/Reply 2 s, Status 30 s. Status bleibt beim Senden
ein unveränderlicher Snapshot; Antworten erhalten Vorrang. Eine erfolgreich
kopierte Notification ist kein Zustellnachweis. Die App wertet erst eine
vollständige, korrekt zusammengesetzte Nachricht aus.

Prüffolge: vollständiger bereiter Status, Start, identische Wiederholung,
Requestkonflikt, veraltete Konfigurationsrevision, falsche Bootkennung,
Anfragelücke, veraltete Zustandsrevision, Stop mit anschließendem Status;
danach Reconnect mit alter Lease. Die echte Anfragenlogik prüft Verbrauch,
nextRequest, duplicate, Boot/Lease und Revisionen. Hostchecks prüfen zusätzlich
die tatsächlichen ausgehenden Frames und Fragmentkonflikt, Wiederholung,
absolute Frist, Mindestlänge, falschen Typ und verbotene Client-Berechtigungsfelder.

Der BLE-Adapter startet den Controller ausdrücklich im BLE-Modus und gibt
Classic-Bluetooth-Speicher frei. Der anfängliche BTDM-Start stürzte im SDK ab;
das gehört zum dokumentierten Laborbefund, nicht zu einer Änderung am
Firmwarekern. Versorgungsschutz bleibt aktiv. Keine Langzeit-, Sicherheits-
oder Spielhardwarefreigabe aus dieser Grundprüfung ableiten.

Rückwege: ursprüngliche USBprobe als komplette app0-Kopie unter
`experiments/esp-ble-diagnostic/.local-backup/20261010-original-app0.bin`;
vorige Ping-Diagnose anhand ihres eigenen Quell-/Abbildmanifests wieder bauen.
Kein Restore wurde durch diese Prüfung ausgeführt oder geprüft.
