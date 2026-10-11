using BleDiagnostics.Core;
using System.Text.Json;

static void Check(bool okay, string name)
{
    if (!okay) throw new Exception(name);
    Console.WriteLine($"PASS {name}");
}

Check(BleIdentity.TryParseAddress(BleIdentity.ExpectedAddress, out var address), "parse expected address");
Check(address == 0xA842E3CD4B1AUL && BleIdentity.FormatAddress(address) == BleIdentity.ExpectedAddress,
    "address roundtrip and byte order");
Check(!BleIdentity.TryParseAddress("A8:42:E3:CD:4B:1", out _) &&
      !BleIdentity.TryParseAddress("A8:42:E3:CD:4B:GG", out _) &&
      !BleIdentity.TryParseAddress("00:00:00:00:00:00:00", out _), "invalid address rejected");
var journal = new BleReportJournal();
Check(!journal.Select(BleIdentity.ExpectedAddress), "cannot select unseen device");
journal.ScanStarted();
journal.Advertisement(address, "Public", "", -66, [BleIdentity.ProductService]);
journal.Advertisement(address, "Public", "Reichstagsbox", -62, []);
journal.ScanEnded("bounded_timeout");
Check(journal.Select(BleIdentity.ExpectedAddress), "select observed address");
journal.GattStarted();
journal.GattEnded("completed", "Success", [BleIdentity.LabService]);
var report = journal.Snapshot(DateTimeOffset.UtcNow);
Check(report.ScanCallbacks == 2 && report.ScanDistinctAddresses == 1 &&
      report.Observations.Single().CallbackCount == 2, "scan callback summary distinct from devices");
Check(report.Observations.Single().AdvertisedServices.Contains(BleIdentity.ProductService.ToString("D")),
    "advertisement UUID kept separate from GATT");
Check(report.ProductServiceFound == false && report.LabServiceFound == true,
    "uncached GATT service evidence kept separate from advertisement");
Check(report.PairingState == "not_attempted" && report.FirmwareEvidence == "not_measured_by_desktop_app",
    "no inferred pair or firmware success");
using var parsed = JsonDocument.Parse(BleReportJournal.ToJson(report));
Check(parsed.RootElement.GetProperty("Schema").GetString() == "larp-escape-windows-ble-observation-v1",
    "export schema serializes");
journal.Advertisement(0, "Public", "Other", -80, []);
Check(journal.Select("00:00:00:00:00:00") && journal.Snapshot().ProductServiceFound is null &&
      journal.Snapshot().GattState == "not_attempted", "switching target invalidates previous GATT result");
journal.ClearSelection();
Check(journal.Snapshot().SelectedAddress is null, "new scan can discard stale selection");
for (var i = 0; i < BleReportJournal.MaxAddresses + 10; i++)
    journal.Advertisement((ulong)i, "Public", "", -80, []);
Check(journal.Snapshot().ScanDistinctAddresses == BleReportJournal.MaxAddresses &&
      journal.Snapshot().ScanDroppedAddresses > 0, "bounded address retention and explicit drops");
for (var i = 0; i < BleReportJournal.MaxEvents + 3; i++) journal.AddEvent("test", null, "bounded");
Check(journal.Snapshot().Events.Count == BleReportJournal.MaxEvents,
    "bounded event retention");
Check(CliParser.TryParse(["--scan", "12", "--probe", "a8:42:e3:cd:4b:1a", "--out", "result.json"],
    out var options, out _) && options?.ScanSeconds == 12 &&
    options.ProbeAddress == BleIdentity.ExpectedAddress && options.OutputPath == "result.json",
    "CLI scan and explicit probe parsing");
Check(!CliParser.TryParse(["--scan", "0"], out _, out _) &&
      !CliParser.TryParse(["--scan", "61"], out _, out _) &&
      !CliParser.TryParse(["--scan", "12", "--probe", "invalid"], out _, out _) &&
      !CliParser.TryParse(["--probe", BleIdentity.ExpectedAddress], out _, out _),
    "CLI rejects unbounded, invalid and no-scan runs");
Check(await BoundedWait.ForStopAsync(Task.CompletedTask, TimeSpan.FromSeconds(1)) == BoundedWaitResult.Stopped,
    "scan ends on watcher stop");
Check(await BoundedWait.ForStopAsync(new TaskCompletionSource().Task, TimeSpan.FromMilliseconds(20)) == BoundedWaitResult.TimedOut,
    "scan ends at deadline without watcher event");
Console.WriteLine("All desktop BLE core checks passed.");
