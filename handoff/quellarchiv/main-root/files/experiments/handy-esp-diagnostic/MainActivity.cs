using Android;
using Android.App;
using Android.Bluetooth;
using Android.Bluetooth.LE;
using Android.Content;
using Android.Content.PM;
using Android.OS;
using Android.Util;
using Android.Views;
using Android.Widget;
using System.Buffers.Binary;

namespace HandyEspDiagnostic;

[Activity(Label = "Larp ESP Prüfstand", MainLauncher = true, Exported = true)]
public sealed class MainActivity : Activity
{
    const string Tag = "LarpEspDiagnostic";
    const string V2 = "407e6a1d-1cc2-43fd-856b-4aff85a20001";
    const string Legacy = "12345678-1234-1234-1234-1234567890ab";
    const string Diagnostic = "86ddc5b0-3878-4ff7-9f55-17c62aeb0001";
    const string ContractLab = "86ddc5b0-3878-4ff7-9f55-17c62aeb1001";
    V2Session? contractSession;
    uint previousContractLease;
    string? previousContractBoot;
    uint nextDiagnosticRequest = 1;
    readonly Handler timer = new(Looper.MainLooper!);
    readonly Dictionary<string, BluetoothDevice> devices = new();
    readonly HashSet<string> advertisers = new();
    TextView output = null!;
    LinearLayout targets = null!;
    BluetoothAdapter? adapter;
    BluetoothLeScanner? scanner;
    ScanReceiver? scan;
    BluetoothGatt? connection;
    int scanGeneration;
    int connectionGeneration;

    protected override void OnCreate(Bundle? state)
    {
        base.OnCreate(state);
        var layout = new LinearLayout(this) { Orientation = Orientation.Vertical };
        layout.SetPadding(24, 24, 24, 24);
        layout.AddView(new TextView(this) { Text = "Handy ↔ ESP: Kommunikationsprüfung", TextSize = 22 });
        layout.AddView(new TextView(this) { Text = "Vertragslabor antippen: v2-Status und Befehlsregeln prüfen. Geräte und Berechtigung sind dort Testeingänge." });
        var start = new Button(this) { Text = "10 Sekunden scannen" };
        start.Click += (_, _) => StartScan();
        layout.AddView(start);
        var disconnect = new Button(this) { Text = "Verbindung trennen" };
        disconnect.Click += (_, _) => { CloseConnection(); Report("Verbindung geschlossen."); };
        layout.AddView(disconnect);
        targets = new LinearLayout(this) { Orientation = Orientation.Vertical };
        layout.AddView(targets);
        output = new TextView(this) { TextSize = 14 };
        var scroll = new ScrollView(this);
        scroll.AddView(output);
        layout.AddView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MatchParent, 0, 1));
        SetContentView(layout);
        adapter = ((BluetoothManager?)GetSystemService(BluetoothService))?.Adapter;
        Report($"App gestartet: Android {Build.VERSION.Release}, API {(int)Build.VERSION.SdkInt}.");
        StartScan();
    }

    bool HasPermissions() => CheckSelfPermission(Manifest.Permission.BluetoothScan) == Permission.Granted
        && CheckSelfPermission(Manifest.Permission.BluetoothConnect) == Permission.Granted;

    void StartScan()
    {
        if (!HasPermissions())
        {
            Report("Berechtigung für Geräte in der Nähe erforderlich.");
            RequestPermissions(new[] { Manifest.Permission.BluetoothScan, Manifest.Permission.BluetoothConnect }, 1);
            return;
        }
        if (adapter?.IsEnabled != true)
        {
            Report("Bluetooth ist ausgeschaltet. Einschalten, danach erneut scannen.");
            if (adapter != null) StartActivity(new Intent(BluetoothAdapter.ActionRequestEnable));
            return;
        }
        StopScan();
        CloseConnection();
        targets.RemoveAllViews();
        devices.Clear();
        advertisers.Clear();
        var generation = ++scanGeneration;
        scanner = adapter.BluetoothLeScanner;
        if (scanner == null) { Report("BLE-Scanner nicht verfügbar."); return; }
        scan = new ScanReceiver(this, generation);
        scanner.StartScan(null, new ScanSettings.Builder().SetScanMode(Android.Bluetooth.LE.ScanMode.LowLatency)!.Build(), scan);
        Report("BLE-Scan gestartet (10 s); nur bekannte Service-UUIDs oder ESP/Larp-Namen als Ziele.");
        timer.PostDelayed(() =>
        {
            if (generation != scanGeneration) return;
            StopScan();
            Report($"BLE-Scan beendet: {advertisers.Count} Werbesender, {devices.Count} passende Ziele. Kein Treffer beweist keine Funkfreiheit.");
        }, 10000);
    }

    public override void OnRequestPermissionsResult(int requestCode, string[] permissions, Permission[] grantResults)
    {
        base.OnRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode != 1) return;
        Report(HasPermissions() ? "BLE-Laufzeitberechtigungen erteilt." : "BLE-Berechtigung abgelehnt; Scan nicht ausgeführt.");
        if (HasPermissions()) StartScan();
    }

    void Found(ScanResult result, int generation)
    {
        RunOnUiThread(() =>
        {
            if (generation != scanGeneration || scan == null || result.Device == null) return;
            // Bound both collections; discovery is diagnostic, not an exhaustive inventory.
            if (advertisers.Count < 256 && result.Device.Address is string address) advertisers.Add(address);
            var name = result.ScanRecord?.DeviceName ?? result.Device.Name ?? "Unbenannt";
            var uuids = result.ScanRecord?.ServiceUuids?.Select(x => x.ToString()).ToArray() ?? Array.Empty<string>();
            var v2 = uuids.Contains(V2, StringComparer.OrdinalIgnoreCase);
            var legacy = uuids.Contains(Legacy, StringComparer.OrdinalIgnoreCase);
            var diagnostic = uuids.Contains(Diagnostic, StringComparer.OrdinalIgnoreCase);
            if (!v2 && !legacy && !diagnostic && !name.Contains("ESP", StringComparison.OrdinalIgnoreCase)
                && !name.Contains("Larp", StringComparison.OrdinalIgnoreCase)) return;
            // Addresses only deduplicate this scan in memory; no address logging.
            var key = result.Device.Address!;
            if (devices.ContainsKey(key) || devices.Count >= 16) return;
            devices[key] = result.Device;
            var kind = diagnostic ? "Diagnosedienst" : v2 ? "v2-Service" : legacy ? "Legacy-Service" : "Name, Identität ungeprüft";
            Report($"Ziel {devices.Count}: {name}; {kind}; RSSI {result.Rssi}.");
            var button = new Button(this) { Text = $"{name} verbinden ({kind})" };
            var device = result.Device;
            button.Click += (_, _) => Connect(device);
            targets.AddView(button);
        });
    }

    void StopScan()
    {
        ++scanGeneration;
        if (scan != null && HasPermissions()) scanner?.StopScan(scan);
        scan = null;
    }

    void Connect(BluetoothDevice device)
    {
        StopScan();
        CloseConnection();
        var generation = ++connectionGeneration;
        Report("GATT-Verbindung angefordert; Geräteidentität noch ungeprüft.");
        connection = device.ConnectGatt(this, false, new GattReceiver(this, generation), BluetoothTransports.Le);
        timer.PostDelayed(() =>
        {
            if (generation != connectionGeneration) return;
            CloseConnection();
            Report("GATT-Zeitfenster beendet (60 s); zum Reconnect Ziel erneut antippen.");
        }, 60000);
    }

    void CloseConnection()
    {
        contractSession?.Dispose();
        contractSession = null;
        ++connectionGeneration;
        connection?.Disconnect();
        connection?.Close();
        connection = null;
    }

    void Report(string message)
    {
        Log.Info(Tag, message);
        RunOnUiThread(() =>
        {
            var content = output.Text ?? "";
            if (content.Length > 12000) content = content[^8000..];
            output.Text = content + message + "\n";
        });
    }

    protected override void OnStop()
    {
        StopScan();
        CloseConnection();
        base.OnStop();
    }

    sealed class ScanReceiver(MainActivity owner, int generation) : ScanCallback
    {
        public override void OnScanResult(ScanCallbackType type, ScanResult? result)
        {
            if (result != null) owner.Found(result, generation);
        }
        public override void OnScanFailed(ScanFailure errorCode)
        {
            owner.RunOnUiThread(() =>
            {
                if (generation != owner.scanGeneration) return;
                owner.StopScan();
                owner.Report($"BLE-Scanfehler: {errorCode}.");
            });
        }
        public override void OnBatchScanResults(IList<ScanResult>? results)
        {
            if (results != null) foreach (var result in results) owner.Found(result, generation);
        }
    }

    sealed class GattReceiver(MainActivity owner, int generation) : BluetoothGattCallback
    {
        BluetoothGattCharacteristic? diagnosticCommand, diagnosticReply;
        uint boot, request, processed;
        int stage;
        bool Live(BluetoothGatt? gatt) => generation == owner.connectionGeneration && gatt == owner.connection;
        void Fail(string message) { stage = -1; owner.Report("Diagnose abgebrochen: " + message); }
        void Send(BluetoothGatt gatt, byte opcode)
        {
            if (owner.nextDiagnosticRequest == uint.MaxValue) { Fail("Requestfolge erschöpft."); return; }
            request = owner.nextDiagnosticRequest++;
            var bytes = new byte[8]; bytes[0] = 1; bytes[1] = opcode;
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(2), request);
            if ((int)gatt.WriteCharacteristic(diagnosticCommand!, bytes, (int)GattWriteType.Default) != 0)
                Fail("GATT-Schreibauftrag nicht gestartet.");
            else owner.Report($"Diagnose gesendet: request={request}, opcode={opcode}, boot={boot}.");
        }
        public override void OnConnectionStateChange(BluetoothGatt? gatt, GattStatus status, ProfileState newState)
        {
            owner.RunOnUiThread(() =>
            {
                if (generation != owner.connectionGeneration || gatt != owner.connection) return;
                owner.Report($"GATT: {newState}, Ergebnis {status}.");
                if (status == GattStatus.Success && newState == ProfileState.Connected)
                    owner.Report(gatt!.DiscoverServices() ? "Diensterkennung angefordert." : "Diensterkennung nicht gestartet.");
                else if (status != GattStatus.Success || newState == ProfileState.Disconnected)
                    owner.CloseConnection();
            });
        }
        public override void OnServicesDiscovered(BluetoothGatt? gatt, GattStatus status)
        {
            owner.RunOnUiThread(() =>
            {
                if (generation != owner.connectionGeneration || gatt != owner.connection) return;
                owner.Report($"Diensterkennung: {status}.");
                if (status != GattStatus.Success) return;
                foreach (var service in gatt!.Services ?? Array.Empty<BluetoothGattService>())
                {
                    owner.Report($"Service {service.Uuid}");
                    foreach (var characteristic in service.Characteristics ?? Array.Empty<BluetoothGattCharacteristic>())
                        owner.Report($"  Characteristic {characteristic.Uuid}: {characteristic.Properties}");
                }
                owner.Report("Inventar vollständig. Produktionsstatus/Anmeldung bleiben ungeprüft.");
                var lab = gatt.GetService(Java.Util.UUID.FromString(ContractLab));
                if (lab != null)
                {
                    owner.contractSession = new V2Session(gatt, owner.Report,
                        (lease, boot) => { owner.previousContractLease = lease; owner.previousContractBoot = boot; },
                        owner.previousContractLease, owner.previousContractBoot);
                    owner.contractSession.Start(lab);
                    return;
                }
                var diagnostic = gatt.GetService(Java.Util.UUID.FromString(Diagnostic));
                if (diagnostic == null) return;
                var statusCharacteristic = diagnostic.GetCharacteristic(Java.Util.UUID.FromString("86ddc5b0-3878-4ff7-9f55-17c62aeb0002"));
                diagnosticCommand = diagnostic.GetCharacteristic(Java.Util.UUID.FromString("86ddc5b0-3878-4ff7-9f55-17c62aeb0003"));
                diagnosticReply = diagnostic.GetCharacteristic(Java.Util.UUID.FromString("86ddc5b0-3878-4ff7-9f55-17c62aeb0004"));
                if (statusCharacteristic == null || diagnosticCommand == null || diagnosticReply == null
                    || !statusCharacteristic.Properties.HasFlag(GattProperty.Read)
                    || !diagnosticCommand.Properties.HasFlag(GattProperty.Write)
                    || !diagnosticReply.Properties.HasFlag(GattProperty.Read)) { Fail("Diagnoseendpunkte unvollständig."); return; }
                stage = 0;
                if (!gatt.ReadCharacteristic(statusCharacteristic)) Fail("Statuslesen nicht gestartet.");
            });
        }
        public override void OnCharacteristicWrite(BluetoothGatt? gatt, BluetoothGattCharacteristic? characteristic, GattStatus status)
        {
            owner.RunOnUiThread(() =>
            {
                if (Live(gatt) && owner.contractSession != null) { owner.contractSession.Written(characteristic, status); return; }
                if (!Live(gatt) || stage < 1 || stage > 2 || characteristic != diagnosticCommand) return;
                if (status != GattStatus.Success || !gatt!.ReadCharacteristic(diagnosticReply!)) Fail($"Schreiben/Antwortlesen: {status}.");
            });
        }
        public override void OnCharacteristicRead(BluetoothGatt? gatt, BluetoothGattCharacteristic? characteristic, byte[]? value, GattStatus status)
        {
            var bytes = value?.ToArray();
            owner.RunOnUiThread(() =>
            {
                if (Live(gatt) && owner.contractSession != null) { owner.contractSession.Read(characteristic, bytes, status); return; }
                if (!Live(gatt) || stage < 0 || stage > 2) return;
                if (status != GattStatus.Success || bytes?.Length != 16 || bytes[0] != 1) { Fail($"Lesen/Format: {status}."); return; }
                if (stage == 0)
                {
                    if (characteristic?.Uuid?.ToString() != "86ddc5b0-3878-4ff7-9f55-17c62aeb0002" || bytes[1] != 0x49 || bytes[14] != 1 || bytes[15] != 0)
                        { Fail("Statusmarker ungültig."); return; }
                    boot = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(2));
                    if (boot == 0) { Fail("Startkennung fehlt."); return; }
                    owner.Report($"ESP-Diagnosestatus: boot={boot}, uptime_ms={BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(6))}, heap={BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(10))}.");
                    stage = 1; Send(gatt!, 1); return;
                }
                var id = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(2));
                var responseBoot = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(6));
                var count = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(10));
                var reason = BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(14));
                if (characteristic != diagnosticReply || id != request || responseBoot != boot
                    || (stage == 1 && (bytes[1] != 1 || reason != 0 || count == 0))
                    || (stage == 2 && (bytes[1] != 2 || reason != 1 || count != processed + 1)))
                    { Fail("Antwort/Korrelation/Zähler ungültig."); return; }
                owner.Report($"ESP-Antwort geprüft: request={id}, boot={boot}, status={bytes[1]}, reason={reason}, processed={count}.");
                processed = count;
                if (stage == 1) { stage = 2; Send(gatt!, 127); }
                else { stage = 3; owner.Report("BLE-Diagnose PASS: Status, Ping und Ablehnung korreliert. Kein v2-Spieltest. Für Reconnect trennen und Ziel erneut antippen."); }
            });
        }
        public override void OnDescriptorWrite(BluetoothGatt? gatt, BluetoothGattDescriptor? descriptor, GattStatus status)
        {
            owner.RunOnUiThread(() => { if (Live(gatt)) owner.contractSession?.Descriptor(status); });
        }
        public override void OnCharacteristicChanged(BluetoothGatt? gatt, BluetoothGattCharacteristic? characteristic, byte[]? value)
        {
            var bytes = value?.ToArray();
            owner.RunOnUiThread(() => { if (Live(gatt) && bytes != null) owner.contractSession?.Changed(characteristic, bytes); });
        }
    }
}
