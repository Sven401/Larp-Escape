using BleDiagnostics.Core;
using System.ComponentModel;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;

namespace BleDiagnostics.Desktop;

public sealed class MainForm : Form
{
    private const int ScanSeconds = 12;
    private readonly BleReportJournal journal = new();
    private readonly BindingList<DeviceRow> rows = [];
    private readonly DataGridView devices = new();
    private readonly TextBox expected = new();
    private readonly Button scan = new() { Text = "BLE-Suche starten (12 s)" };
    private readonly Button stop = new() { Text = "Suche beenden", Enabled = false };
    private readonly Button gatt = new() { Text = "GATT-Dienste prüfen", Enabled = false };
    private readonly Button pair = new() { Text = "Windows-Kopplung versuchen", Enabled = false };
    private readonly Button export = new() { Text = "Bericht exportieren" };
    private readonly Label status = new() { AutoSize = true, Text = "Bereit. Starte die Suche per Knopfdruck." };
    private BluetoothLEAdvertisementWatcher? watcher;
    private CancellationTokenSource? scanCts;
    private CancellationTokenSource? operationCts;
    private BluetoothLEDevice? connected;
    private bool operationRunning;
    private bool closing;
    private readonly string autosavePath;
    private string? userSelectedAddress;

    public MainForm()
    {
        Text = "Larp-Escape · Desktop-BLE-Diagnose";
        Width = 1100;
        Height = 660;
        MinimumSize = new Size(800, 500);
        autosavePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "Larp-Escape", "DesktopBleDiagnostics", $"{journal.RunId}.json");

        expected.Text = BleIdentity.ExpectedAddress;
        expected.ReadOnly = true;
        expected.Width = 150;
        var targetLine = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 37, AutoSize = false };
        targetLine.Controls.Add(new Label { Text = "Erwartete Box-Adresse:", AutoSize = true, Padding = new Padding(0, 8, 0, 0) });
        targetLine.Controls.Add(expected);
        targetLine.Controls.Add(new Label {
            Text = "Nur ein beobachtetes, ausgewähltes Gerät kann geprüft werden.",
            AutoSize = true, Padding = new Padding(8, 8, 0, 0)
        });
        var controls = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 47 };
        controls.Controls.AddRange([scan, stop, gatt, pair, export]);
        foreach (Control button in controls.Controls) button.Margin = new Padding(4, 7, 4, 4);
        devices.Dock = DockStyle.Fill;
        devices.ReadOnly = true;
        devices.MultiSelect = false;
        devices.SelectionMode = DataGridViewSelectionMode.FullRowSelect;
        devices.AutoGenerateColumns = false;
        devices.AllowUserToAddRows = false;
        devices.AllowUserToDeleteRows = false;
        devices.DataSource = rows;
        devices.Columns.Add(new DataGridViewTextBoxColumn { HeaderText = "Adresse", DataPropertyName = nameof(DeviceRow.Address), Width = 155 });
        devices.Columns.Add(new DataGridViewTextBoxColumn { HeaderText = "Typ", DataPropertyName = nameof(DeviceRow.AddressType), Width = 100 });
        devices.Columns.Add(new DataGridViewTextBoxColumn { HeaderText = "Name", DataPropertyName = nameof(DeviceRow.Name), Width = 180 });
        devices.Columns.Add(new DataGridViewTextBoxColumn { HeaderText = "RSSI", DataPropertyName = nameof(DeviceRow.Rssi), Width = 65 });
        devices.Columns.Add(new DataGridViewTextBoxColumn { HeaderText = "Beworbene Dienste (keine GATT-Bestätigung)", DataPropertyName = nameof(DeviceRow.Services), AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill });
        var foot = new System.Windows.Forms.Panel { Dock = DockStyle.Bottom, Height = 66 };
        status.Dock = DockStyle.Fill;
        status.Padding = new Padding(10, 10, 10, 3);
        foot.Controls.Add(status);
        Controls.Add(devices);
        Controls.Add(foot);
        Controls.Add(controls);
        Controls.Add(targetLine);

        scan.Click += (_, _) => StartScan();
        stop.Click += (_, _) => StopScan("user_stopped");
        gatt.Click += async (_, _) => await CheckGattAsync();
        pair.Click += async (_, _) => await PairAsync();
        export.Click += (_, _) => ExportReport();
        devices.CellClick += (_, e) =>
        {
            if (e.RowIndex < 0) return;
            var row = devices.Rows[e.RowIndex].DataBoundItem as DeviceRow;
            if (row is null) return;
            userSelectedAddress = row.Address;
            journal.Select(row.Address);
            status.Text = $"{row.Address} ausgewählt. Funkwerbung allein bestätigt keinen Produktdienst.";
            SaveJournal();
            UpdateButtons();
        };
        devices.SelectionChanged += (_, _) => UpdateButtons();
        FormClosing += (_, _) => ClosingCleanup();
        SaveJournal();
    }

    private void StartScan()
    {
        if (watcher is not null || operationRunning || closing) return;
        rows.Clear();
        userSelectedAddress = null;
        journal.ClearSelection();
        connected?.Dispose();
        connected = null;
        scanCts = new CancellationTokenSource();
        watcher = new BluetoothLEAdvertisementWatcher { ScanningMode = BluetoothLEScanningMode.Active };
        watcher.Received += OnReceived;
        watcher.Stopped += OnStopped;
        try
        {
            watcher.Start();
            journal.ScanStarted();
            status.Text = "BLE-Suche läuft. Windows-Funkrückrufe werden gezählt.";
            SaveJournal();
            UpdateButtons();
            _ = EndScanAfterDelayAsync(scanCts.Token);
        }
        catch (Exception ex)
        {
            journal.ScanEnded("start_failed");
            journal.AddEvent("scan_error", null, ex.Message);
            status.Text = $"BLE-Suche konnte nicht starten: {ex.Message}";
            DisposeWatcher();
            SaveJournal();
            UpdateButtons();
        }
    }

    private async Task EndScanAfterDelayAsync(CancellationToken token)
    {
        try
        {
            await Task.Delay(TimeSpan.FromSeconds(ScanSeconds), token);
            if (!closing && IsHandleCreated) BeginInvoke(() => StopScan("bounded_timeout"));
        }
        catch (OperationCanceledException) { }
    }

    private void OnReceived(BluetoothLEAdvertisementWatcher sender, BluetoothLEAdvertisementReceivedEventArgs e)
    {
        var address = e.BluetoothAddress;
        var addressType = e.BluetoothAddressType.ToString();
        var name = e.Advertisement.LocalName ?? "";
        var rssi = e.RawSignalStrengthInDBm;
        var services = e.Advertisement.ServiceUuids.ToArray();
        if (!closing && IsHandleCreated)
        {
            try { BeginInvoke(() => RecordReceived(address, addressType, name, rssi, services)); }
            catch (InvalidOperationException) { }
        }
    }

    private void RecordReceived(ulong address, string addressType, string name, short rssi, Guid[] services)
    {
        if (closing || watcher is null) return;
        var distinctBefore = journal.Observations.Count;
        journal.Advertisement(address, addressType, name, rssi, services);
        var formatted = BleIdentity.FormatAddress(address);
        var observation = journal.Observations.FirstOrDefault(x => x.Address == formatted);
        if (observation is not null)
        {
            var row = rows.FirstOrDefault(x => x.Address == formatted);
            if (row is null)
            {
                row = new DeviceRow { Address = formatted };
                rows.Add(row);
            }
            row.AddressType = observation.AddressType;
            row.Name = observation.Name;
            row.Rssi = observation.Rssi;
            row.Services = string.Join(", ", observation.AdvertisedServices);
            rows.ResetItem(rows.IndexOf(row));
            if (formatted == BleIdentity.ExpectedAddress) status.Text = $"Erwartete Adresse {formatted} gesehen; GATT und Kopplung noch ungeprüft.";
        }
        if (journal.Observations.Count > distinctBefore || journal.ScanCallbacks % 10 == 0) SaveJournal();
    }

    private void OnStopped(BluetoothLEAdvertisementWatcher sender, BluetoothLEAdvertisementWatcherStoppedEventArgs e)
    {
        if (!closing && IsHandleCreated)
        {
            try { BeginInvoke(() => StopScan(e.Error.ToString() == "Success" ? "watcher_stopped" : $"watcher_error:{e.Error}")); }
            catch (InvalidOperationException) { }
        }
    }

    private void StopScan(string reason)
    {
        if (watcher is null) return;
        try { watcher.Stop(); }
        catch (Exception ex) { journal.AddEvent("scan_stop_error", null, ex.Message); }
        scanCts?.Cancel();
        DisposeWatcher();
        journal.ScanEnded(reason);
        status.Text = $"Suche beendet: {journal.ScanCallbacks} Rückrufe, {journal.Observations.Count} Adressen. Gerät auswählen, dann GATT prüfen.";
        SaveJournal();
        UpdateButtons();
    }

    private void DisposeWatcher()
    {
        if (watcher is not null)
        {
            watcher.Received -= OnReceived;
            watcher.Stopped -= OnStopped;
            watcher = null;
        }
        scanCts?.Dispose();
        scanCts = null;
    }

    private DeviceRow? SelectedRow => devices.SelectedRows.Count == 1 &&
        devices.SelectedRows[0].DataBoundItem is DeviceRow row && row.Address == userSelectedAddress ? row : null;

    private bool SelectCurrent()
    {
        var row = SelectedRow;
        return row is not null && journal.Select(row.Address);
    }

    private async Task CheckGattAsync()
    {
        if (operationRunning || watcher is not null || !SelectCurrent() || closing) return;
        operationRunning = true;
        UpdateButtons();
        journal.GattStarted();
        SaveJournal();
        status.Text = $"GATT-Dienste auf {journal.SelectedAddress} werden ungecached angefordert …";
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(20));
        operationCts = timeout;
        try
        {
            connected?.Dispose();
            connected = null;
            if (!BleIdentity.TryParseAddress(journal.SelectedAddress, out var address)) throw new InvalidOperationException("Ungültige Adresse");
            var type = journal.SelectedAddressType == BluetoothAddressType.Random.ToString() ? BluetoothAddressType.Random : BluetoothAddressType.Public;
            connected = await BluetoothLEDevice.FromBluetoothAddressAsync(address, type).AsTask(timeout.Token);
            if (closing) return;
            if (connected is null) throw new InvalidOperationException("Windows liefert kein BLE-Gerät für diese Adresse");
            var result = await connected.GetGattServicesAsync(BluetoothCacheMode.Uncached).AsTask(timeout.Token);
            if (closing) return;
            var found = result.Services.Select(x => x.Uuid).ToArray();
            foreach (var service in result.Services) service.Dispose();
            journal.GattEnded(result.Status == GattCommunicationStatus.Success ? "completed" : "failed", result.Status.ToString(),
                result.Status == GattCommunicationStatus.Success ? found : null);
            status.Text = $"GATT: {result.Status}; Produktdienst={journal.ProductServiceFound?.ToString() ?? "unbekannt"}; Labordienst={journal.LabServiceFound?.ToString() ?? "unbekannt"}.";
        }
        catch (OperationCanceledException)
        {
            if (closing) return;
            journal.GattEnded("timeout", "20-second deadline", null);
            status.Text = "GATT-Dienste: Zeitgrenze überschritten. Keine Dienstbestätigung.";
            connected?.Dispose();
            connected = null;
        }
        catch (Exception ex)
        {
            if (closing) return;
            journal.GattEnded("error", ex.Message, null);
            status.Text = $"GATT-Fehler: {ex.Message}";
            connected?.Dispose();
            connected = null;
        }
        finally
        {
            SaveJournal();
            operationCts = null;
            operationRunning = false;
            if (!closing) UpdateButtons();
        }
    }

    private async Task PairAsync()
    {
        if (operationRunning || watcher is not null || !SelectCurrent() || closing) return;
        operationRunning = true;
        UpdateButtons();
        journal.PairingStarted();
        SaveJournal();
        status.Text = $"Windows-Kopplung mit {journal.SelectedAddress} läuft; ein Systemdialog ist möglich …";
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(45));
        operationCts = timeout;
        try
        {
            if (!BleIdentity.TryParseAddress(journal.SelectedAddress, out var address)) throw new InvalidOperationException("Ungültige Adresse");
            var type = journal.SelectedAddressType == BluetoothAddressType.Random.ToString() ? BluetoothAddressType.Random : BluetoothAddressType.Public;
            if (connected is null || BleIdentity.FormatAddress(connected.BluetoothAddress) != journal.SelectedAddress)
            {
                connected?.Dispose();
                connected = await BluetoothLEDevice.FromBluetoothAddressAsync(address, type).AsTask(timeout.Token);
                if (closing) return;
            }
            if (connected is null) throw new InvalidOperationException("Windows liefert kein BLE-Gerät für diese Adresse");
            var result = await connected.DeviceInformation.Pairing.PairAsync().AsTask(timeout.Token);
            if (closing) return;
            bool? osIsPaired = null;
            try { osIsPaired = connected.DeviceInformation.Pairing.IsPaired; } catch { }
            var state = PairingOutcome.ReturnedState(result.Status.ToString(), osIsPaired);
            journal.PairingEnded(state, result.Status.ToString());
            journal.PairingOsStateObserved(osIsPaired);
            status.Text = $"Windows-Kopplungsstatus: {result.Status}; IsPaired={osIsPaired?.ToString() ?? "unbekannt"}. Firmware-Rolle bleibt ungeprüft.";
        }
        catch (OperationCanceledException)
        {
            if (closing) return;
            bool? osIsPaired = null;
            try { osIsPaired = connected?.DeviceInformation.Pairing.IsPaired; } catch { }
            journal.PairingEnded("pending_after_deadline", "PairAsync result unavailable at 45-second deadline; Windows pairing may continue");
            journal.PairingOsStateObserved(osIsPaired);
            status.Text = "Kopplungsfrist erreicht; Windows-Ergebnis ist offen. Der Bericht enthält den separat gelesenen IsPaired-Stand.";
        }
        catch (Exception ex)
        {
            if (closing) return;
            journal.PairingEnded("error", ex.Message);
            bool? osIsPaired = null;
            try { osIsPaired = connected?.DeviceInformation.Pairing.IsPaired; } catch { }
            journal.PairingOsStateObserved(osIsPaired);
            status.Text = $"Kopplungsfehler: {ex.Message}";
        }
        finally
        {
            SaveJournal();
            operationCts = null;
            operationRunning = false;
            if (!closing) UpdateButtons();
        }
    }

    private void UpdateButtons()
    {
        if (closing) return;
        scan.Enabled = watcher is null && !operationRunning;
        stop.Enabled = watcher is not null;
        gatt.Enabled = watcher is null && !operationRunning && SelectedRow is not null;
        pair.Enabled = watcher is null && !operationRunning &&
            SelectedRow?.Address == BleIdentity.ExpectedAddress && journal.GattState == "completed" &&
            journal.ProductServiceFound == true;
    }

    private void SaveJournal()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(autosavePath)!);
            var temp = autosavePath + ".tmp";
            File.WriteAllText(temp, BleReportJournal.ToJson(journal.Snapshot()));
            File.Move(temp, autosavePath, true);
        }
        catch (Exception ex)
        {
            status.Text = $"Berichtssicherung fehlgeschlagen: {ex.Message}";
        }
    }

    private void ExportReport()
    {
        using var dialog = new SaveFileDialog {
            Title = "BLE-Beobachtungsbericht exportieren", Filter = "JSON-Datei (*.json)|*.json",
            FileName = $"desktop-ble-beobachtung-{DateTime.Now:yyyyMMdd-HHmmss}.json"
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        try
        {
            File.WriteAllText(dialog.FileName, BleReportJournal.ToJson(journal.Snapshot(DateTimeOffset.UtcNow)));
            status.Text = $"Bericht gespeichert: {dialog.FileName}";
        }
        catch (Exception ex)
        {
            status.Text = $"Export fehlgeschlagen: {ex.Message}";
        }
    }

    private void ClosingCleanup()
    {
        closing = true;
        if (operationRunning)
        {
            if (journal.GattState == "running") journal.GattEnded("app_closed", "pending operation cancelled", null);
            if (journal.PairingState == "running") journal.PairingEnded("app_closed", "pending operation cancelled");
            operationCts?.Cancel();
        }
        if (watcher is not null)
        {
            try { watcher.Stop(); } catch { }
            DisposeWatcher();
            journal.ScanEnded("app_closed");
        }
        connected?.Dispose();
        SaveJournal();
    }

    private sealed class DeviceRow
    {
        public string Address { get; set; } = "";
        public string AddressType { get; set; } = "";
        public string Name { get; set; } = "";
        public short Rssi { get; set; }
        public string Services { get; set; } = "";
    }
}
