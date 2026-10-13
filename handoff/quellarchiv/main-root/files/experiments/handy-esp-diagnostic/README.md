# Handy-/ESP-Diagnoseclient

Getrennter Android-Prüfpfad für den Hardwaretest vom 9. Oktober 2026. Die originale GM-Companion-App bleibt unverändert; dieser Client repariert ihren Editor nicht. Keine Bibliothek des fremden `companion/`-Entwurfs wird eingebunden.

Voraussetzung der erweiterten Fassung: Android 13/API 33 oder neuer, .NET SDK 10 mit Androidworkload, Android SDK 36. Debug-APK mit eingebetteten Assemblies, eigener Paketname `de.larpescape.hardwarediagnostic`.

```powershell
dotnet build experiments/handy-esp-diagnostic/HandyEspDiagnostic.csproj -p:AndroidSdkDirectory="C:/Program Files (x86)/Android/android-sdk" -p:NuGetInteractive=false
adb install -r experiments/handy-esp-diagnostic/bin/Debug/net10.0-android/de.larpescape.hardwarediagnostic-Signed.apk
adb shell am start -n de.larpescape.hardwarediagnostic/crc6454ffad3214313f73.MainActivity
```

Messwerte direkt im Appbildschirm ablesen. Der Android-Logcat-Tag `LarpEspDiagnostic` protokolliert die tatsächlichen Status-/Antwortnachrichten und Prüfergebnisse. Die Anzeige zählt bis zu 256 verschiedene Werbesender; höchstens 16 passende Ziele erhalten eine Verbindungsschaltfläche.

Bluetooth einschalten und „Geräte in der Nähe“ erlauben. Der Scan endet nach zehn Sekunden. Als Ziele erscheinen bekannte Service-UUIDs oder ESP/Larp-Namen; das ist keine Identitätsprüfung. Ein Ziel antippen verbindet und listet tatsächliche GATT-Services und Characteristic-Eigenschaften. Nach sechzig Sekunden wird geschlossen. Für Reconnect dasselbe Ziel nochmals antippen. Beim Verlassen der App werden Scan und Verbindung geschlossen; überholte Rückrufe werden verworfen.

Am getrennten Diagnosedienst `86ddc5b0-3878-4ff7-9f55-17c62aeb0001` liest die erweiterte Fassung automatisch den 16-Byte-Status, sendet einen 8-Byte-Ping (Opcode1), prüft dessen 16-Byte-Antwort und sendet danach Opcode127 für die erwartete Ablehnung. Startkennung, Request-ID und Zähler werden gegen den eigenen Diagnosevertrag in `../esp-ble-diagnostic/README.md` geprüft. Die vorherige Fassung vom 9. Oktober hatte nur Scan/Diensteinventar.

Am eigenen **v2-Vertragslabor** `86ddc5b0-3878-4ff7-9f55-17c62aeb1001` prüft `V2Session` den Labmarker, abonniert Notifications, setzt echte Typ2-/Typ4-Projektframes zusammen und sendet fragmentierte v2-Befehle an die echte Firmwarelogik. Ablauf und Grenzen: [Vertragslabor](../v2-contract-probe/README.md). Nach vollständigem PASS erneut verbinden für die alte-Lease-Prüfung. Der Laborpfad besitzt synthetische Geräte, RAM-Journal und eingespeiste GM-Rechte; er führt keine Produktanmeldung durch. Kein Schreiben an den kanonischen oder Legacy-Service, kein Pairing oder Profiltransfer.

Kanonischer Service: `407e6a1d-1cc2-43fd-856b-4aff85a20001`, aus dem little-endian Bytearray in `lib/GattsWriteAdmission/GattsServiceDefinition.h`, Firmware-Worktree `firmware-peer-ingress`, HEAD `7f2a3e44223cc045aea46f78b3c124ede615573c` plus dortiger uncommitteter Diff. Legacy-Service `12345678-1234-1234-1234-1234567890ab` nur separat erkennen; kein v2-JSON dorthin senden.

Androidberechtigungen folgen der [offiziellen Android-Dokumentation](https://developer.android.com/develop/connectivity/bluetooth/bt-permissions). `neverForLocation` kann bestimmte Beacon-Ergebnisse filtern. Kein Treffer beweist daher weder einen abgeschalteten ESP noch das Fehlen aller Funkgeräte. Adressen werden nur im Arbeitsspeicher zum Deduplizieren verwendet; Gerätebezeichnungen können in flüchtigen Android-Diagnoselogs erscheinen.

Installation, sichtbarer Appstart, Scan, Verbindung und GATT-Inventar getrennt messen. Ein Inventar belegt noch keine Identität oder Befehlsverarbeitung. Im eigenen Vertragslabor werden zusätzlich vollständige v2-Nachrichten und konkrete Befehlsfolgen tatsächlich geprüft. Der kanonische Produkt-GATT-/Anmeldepfad braucht seine eigene Abnahme.

Erzeugnisse unter `bin/` und `obj/` sind ignoriert. Keine Signierschlüssel oder Gerätekennungen im Repository ablegen. Gemessener Hardwarestand steht in der getrennten Projekt-Werkbank unter `Referenzbibliothek/GM-Companion/github-73df05a/handy-esp-pruefstand.md`.
