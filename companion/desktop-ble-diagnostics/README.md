# Desktop-BLE-Diagnose (Windows)

Dieser unabhängige zweite Prüfweg zeigt Windows-BLE-Funkrückrufe und kann auf ausdrücklichen Knopfdruck GATT-Dienste oder die Windows-Kopplung eines **zuvor empfangenen und ausgewählten** Geräts prüfen. Der erwartete, aus der ESP32-MAC-Belegung abgeleitete Bluetooth-Adresseintrag ist `A8:42:E3:CD:4B:1A`; die tatsächlich empfangene Adresse erscheint in der Liste. Die App liest keine Charakteristik und schreibt nichts an die Box.

## Bauen und starten

Voraussetzung: Windows 10 (19041+) oder Windows 11, .NET SDK 10, Bluetooth-LE-Adapter. Im Ordner `companion/desktop-ble-diagnostics`:

```powershell
dotnet restore BleDiagnostics.Desktop/BleDiagnostics.Desktop.csproj
dotnet build BleDiagnostics.Desktop/BleDiagnostics.Desktop.csproj --no-restore
dotnet run --project BleDiagnostics.Desktop/BleDiagnostics.Desktop.csproj --no-restore
```

Die offline geprüfte Entwicklungsumgebung besitzt `Microsoft.Windows.SDK.NET.Ref` 10.0.19041.57 im lokalen NuGet-Cache. Für einen Cache-gestützten Restore ohne Online-Quelle kann `dotnet restore BleDiagnostics.Desktop/BleDiagnostics.Desktop.csproj --source "$env:USERPROFILE\.nuget\packages" -p:NuGetAudit=false` verwendet werden. Reine Modelltests laufen mit `dotnet run --project BleDiagnostics.Tests/BleDiagnostics.Tests.csproj`.

Für einen eigenständigen, **rein lesenden** Lauf aus der Konsole verwendet derselbe Desktop-Prozess einen CLI-Modus:

```powershell
dotnet run --project BleDiagnostics.Desktop/BleDiagnostics.Desktop.csproj --no-build --no-restore -- --scan 12 --probe A8:42:E3:CD:4B:1A --out "$env:LOCALAPPDATA\Larp-Escape\DesktopBleDiagnostics\probe.json"
```

`--scan` ist Pflicht und akzeptiert 1–60 Sekunden. `--probe` ist optional und fordert ungecachete GATT-Dienste **nur dann** an, wenn genau diese Adresse im vorangehenden Scan empfangen wurde. Bei fehlender Adresse steht `probe_skipped` im Bericht. Ohne `--out` wird eine Datei mit Laufkennung unter `%LOCALAPPDATA%\Larp-Escape\DesktopBleDiagnostics` angelegt. Der CLI-Modus koppelt niemals. Auch ein fehlgeschlagener oder leerer Lauf speichert seinen beobachteten Status; ein Scanfehler oder Dateifehler führt zu einem Exitcode ungleich null.

## Nutzerablauf

1. **BLE-Suche starten**. Die aktive Suche endet nach 12 Sekunden oder über **Suche beenden**. Sie meldet Rückrufzahl und unterschiedliche Adressen. Eine leere Liste ist nur eine Beobachtung dieses Windows-Adapters zu diesem Zeitpunkt.
2. Die Box anhand der empfangenen Adresse **per Klick in der Liste** auswählen. Die UUID-Spalte enthält **beworbene** Dienste; sie bestätigt keine GATT-Verbindung. Die Produktdienst-UUID ist `407e6a1d-1cc2-43fd-856b-4aff85a20001`, die Labor-UUID `86ddc5b0-3878-4ff7-9f55-17c62aeb1001`.
3. **GATT-Dienste prüfen** fordert die Dienste explizit mit `BluetoothCacheMode.Uncached` an und zeigt Status und Dienstfund getrennt. Der Versuch ist auf 20 Sekunden begrenzt.
4. **Windows-Kopplung versuchen** wird erst für die erwartete Box-Adresse mit erfolgreich bestätigtem Produkt-GATT-Dienst aktiv. Der eigene Klick ruft `DeviceInformation.Pairing.PairAsync` auf. Windows kann dabei einen Systemdialog zeigen. Der Versuch ist auf 45 Sekunden begrenzt und kann durch Windows später anders abgeschlossen werden; der Bericht enthält nur das von Windows zurückgegebene Ergebnis.
5. **Bericht exportieren** speichert JSON. Zusätzlich wird während des Laufs eine wiederherstellbare Momentaufnahme unter `%LOCALAPPDATA%\Larp-Escape\DesktopBleDiagnostics\<RunId>.json` aktualisiert. Nur die ausdrücklich ausgewählte Adresse und ihre Einzelereignisse erscheinen darin; andere empfangene Geräte gehen allein in die aggregierte Rückruf- und Adresszahl ein. Ohne Auswahl enthält der Bericht keine beobachtete Geräteadresse. Intern begrenzt die App die Geräteliste auf 128 und das Ereignisjournal auf 512 Einträge; verworfene zusätzliche Adressen werden gezählt. Ereignisse enthalten Zeitstempel und Quelle. Keine gespeicherte Beobachtung verleiht Diagnosefreigabe oder Firmware-Rolle.

Die Desktop-App belegt weder einen Android-Kopplungsversuch noch LED-Abschaltung, Firmware-Sicherheitsbindung oder ein physisches Gerätetestergebnis. Sie kann insbesondere eine fehlende BLE-Sichtbarkeit auf dem Moto G14 nicht allein erklären. Der Nutzer führt die Geräteprüfung aus und exportiert den Bericht zur anschließenden Auswertung.

## Windows-API-Grundlagen

- [BluetoothLEAdvertisementWatcher: Received, Start und Stop](https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementwatcher)
- [BluetoothLEDevice.FromBluetoothAddressAsync und Verbindungsaufbau durch ungecachte GATT-Diensterkennung](https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothledevice.frombluetoothaddressasync)
- [DeviceInformationPairing.PairAsync](https://learn.microsoft.com/en-us/uwp/api/windows.devices.enumeration.deviceinformationpairing.pairasync)
