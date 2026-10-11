using System.Text.Json;

namespace BleDiagnostics.Core;

public static class BleIdentity
{
    public const string ExpectedAddress = "A8:42:E3:CD:4B:1A";
    public static readonly Guid ProductService = Guid.Parse("407e6a1d-1cc2-43fd-856b-4aff85a20001");
    public static readonly Guid LabService = Guid.Parse("86ddc5b0-3878-4ff7-9f55-17c62aeb1001");

    public static string FormatAddress(ulong address) => string.Join(":", Enumerable.Range(0, 6)
        .Reverse().Select(shift => ((address >> (shift * 8)) & 0xff).ToString("X2")));

    public static bool TryParseAddress(string? text, out ulong address)
    {
        address = 0;
        if (text is null) return false;
        var parts = text.Split(':');
        if (parts.Length != 6) return false;
        foreach (var part in parts)
        {
            if (part.Length != 2 || !byte.TryParse(part, System.Globalization.NumberStyles.HexNumber,
                    System.Globalization.CultureInfo.InvariantCulture, out var value)) return false;
            address = (address << 8) | value;
        }
        return true;
    }
}

public sealed record BleEvent(DateTimeOffset AtUtc, string Kind, string? Address, string Detail);

public sealed record BleObservation(
    string Address, string AddressType, string Name, short Rssi, DateTimeOffset LastSeenUtc,
    IReadOnlyList<string> AdvertisedServices, int CallbackCount);

public sealed record BleReport(
    string Schema, string Source, string RunId, DateTimeOffset StartedUtc, DateTimeOffset? ExportedUtc,
    string ExpectedAddress, string ProductServiceUuid, string LabServiceUuid,
    string? SelectedAddress, string? SelectedAddressType,
    string ScanState, int ScanCallbacks, int ScanDistinctAddresses, int ScanDroppedAddresses,
    string GattState, string? GattStatus, bool? ProductServiceFound, bool? LabServiceFound,
    string PairingState, string? PairingStatus,
    string? PairingInteractionBoundary,
    bool? PairingOsIsPairedAtLastCheck,
    string FirmwareEvidence, string DiagnosticAuthority,
    IReadOnlyList<BleObservation> Observations, IReadOnlyList<BleEvent> Events);

public sealed class BleReportJournal
{
    public const int MaxAddresses = 128;
    public const int MaxEvents = 512;
    private readonly Dictionary<string, BleObservation> observations = new(StringComparer.OrdinalIgnoreCase);
    private readonly List<BleEvent> events = [];
    public string RunId { get; } = Guid.NewGuid().ToString("N");
    public DateTimeOffset StartedUtc { get; } = DateTimeOffset.UtcNow;
    public string? SelectedAddress { get; private set; }
    public string? SelectedAddressType { get; private set; }
    public string ScanState { get; private set; } = "not_started";
    public int ScanCallbacks { get; private set; }
    public int ScanDroppedAddresses { get; private set; }
    public string GattState { get; private set; } = "not_attempted";
    public string? GattStatus { get; private set; }
    public bool? ProductServiceFound { get; private set; }
    public bool? LabServiceFound { get; private set; }
    public string PairingState { get; private set; } = "not_attempted";
    public string? PairingStatus { get; private set; }
    public string? PairingInteractionBoundary { get; private set; }
    public bool? PairingOsIsPairedAtLastCheck { get; private set; }
    public IReadOnlyCollection<BleObservation> Observations => observations.Values;

    public void AddEvent(string kind, string? address, string detail)
    {
        if (events.Count == MaxEvents) events.RemoveAt(0);
        events.Add(new BleEvent(DateTimeOffset.UtcNow, kind, address, detail));
    }

    public void ScanStarted(int seconds = 12)
    {
        ScanState = "running";
        AddEvent("scan_started", null, $"Windows BluetoothLEAdvertisementWatcher; active; bounded {seconds} seconds");
    }

    public void ScanEnded(string reason)
    {
        ScanState = reason;
        AddEvent("scan_ended", null, $"reason={reason};callbacks={ScanCallbacks};distinct={observations.Count};dropped={ScanDroppedAddresses}");
    }

    public void Advertisement(ulong address, string addressType, string name, short rssi, IEnumerable<Guid> services)
    {
        ScanCallbacks++;
        var key = BleIdentity.FormatAddress(address);
        var uuids = services.Select(x => x.ToString("D")).Distinct(StringComparer.OrdinalIgnoreCase).Order().ToArray();
        if (observations.TryGetValue(key, out var prior))
        {
            observations[key] = prior with {
                Name = string.IsNullOrWhiteSpace(name) ? prior.Name : name,
                Rssi = rssi, LastSeenUtc = DateTimeOffset.UtcNow,
                AdvertisedServices = prior.AdvertisedServices.Union(uuids, StringComparer.OrdinalIgnoreCase).Order().ToArray(),
                CallbackCount = prior.CallbackCount + 1
            };
            return;
        }
        if (observations.Count >= MaxAddresses)
        {
            ScanDroppedAddresses++;
            return;
        }
        observations[key] = new BleObservation(key, addressType, name, rssi, DateTimeOffset.UtcNow, uuids, 1);
        AddEvent("advertisement_first_seen", key,
            $"type={addressType};name={name};rssi={rssi};services={string.Join(',', uuids)}");
    }

    public bool Select(string address)
    {
        if (!observations.TryGetValue(address, out var found)) return false;
        if (SelectedAddress is not null && !SelectedAddress.Equals(found.Address, StringComparison.OrdinalIgnoreCase))
        {
            GattState = "not_attempted";
            GattStatus = null;
            ProductServiceFound = null;
            LabServiceFound = null;
            PairingState = "not_attempted";
            PairingStatus = null;
            PairingInteractionBoundary = null;
            PairingOsIsPairedAtLastCheck = null;
        }
        SelectedAddress = found.Address;
        SelectedAddressType = found.AddressType;
        AddEvent("device_selected", SelectedAddress, $"type={SelectedAddressType}");
        return true;
    }

    public void ClearSelection()
    {
        SelectedAddress = null;
        SelectedAddressType = null;
        GattState = "not_attempted";
        GattStatus = null;
        ProductServiceFound = null;
        LabServiceFound = null;
        PairingState = "not_attempted";
        PairingStatus = null;
        PairingInteractionBoundary = null;
        PairingOsIsPairedAtLastCheck = null;
    }

    public void GattStarted()
    {
        GattState = "running";
        GattStatus = null;
        ProductServiceFound = null;
        LabServiceFound = null;
        AddEvent("gatt_started", SelectedAddress, "uncached service discovery requested");
    }

    public void GattEnded(string state, string? status, IEnumerable<Guid>? services)
    {
        GattState = state;
        GattStatus = status;
        if (services is not null)
        {
            var set = services.ToHashSet();
            ProductServiceFound = set.Contains(BleIdentity.ProductService);
            LabServiceFound = set.Contains(BleIdentity.LabService);
        }
        AddEvent("gatt_ended", SelectedAddress,
            $"state={state};status={status};product={ProductServiceFound};lab={LabServiceFound}");
    }

    public void PairingStarted(string interactionBoundary = "Windows system dialog is user-controlled")
    {
        PairingState = "running";
        PairingStatus = null;
        PairingInteractionBoundary = interactionBoundary;
        PairingOsIsPairedAtLastCheck = null;
        AddEvent("pairing_started", SelectedAddress, "explicit Windows PairAsync request");
    }

    public void PairingEnded(string state, string? status)
    {
        PairingState = state;
        PairingStatus = status;
        AddEvent("pairing_ended", SelectedAddress, $"state={state};status={status}");
    }

    public void PairingOsStateObserved(bool? isPaired)
    {
        PairingOsIsPairedAtLastCheck = isPaired;
        AddEvent("windows_is_paired_read", SelectedAddress, isPaired?.ToString() ?? "unavailable");
    }

    public BleReport Snapshot(DateTimeOffset? exportedUtc = null) => new(
        "larp-escape-windows-ble-observation-v1", "windows-winrt-ble-desktop", RunId,
        StartedUtc, exportedUtc, BleIdentity.ExpectedAddress,
        BleIdentity.ProductService.ToString("D"), BleIdentity.LabService.ToString("D"),
        SelectedAddress, SelectedAddressType, ScanState, ScanCallbacks, observations.Count, ScanDroppedAddresses,
        GattState, GattStatus, ProductServiceFound, LabServiceFound, PairingState, PairingStatus,
        PairingInteractionBoundary,
        PairingOsIsPairedAtLastCheck,
        "not_measured_by_desktop_app", "none; observation does not grant a diagnostic role",
        observations.Values.Where(x => x.Address.Equals(SelectedAddress, StringComparison.OrdinalIgnoreCase))
            .OrderBy(x => x.Address).ToArray(),
        events.Where(x => x.Address is null || x.Address.Equals(SelectedAddress, StringComparison.OrdinalIgnoreCase)).ToArray());

    public static string ToJson(BleReport report) => JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true });
}
