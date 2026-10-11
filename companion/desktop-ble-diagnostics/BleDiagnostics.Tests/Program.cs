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
Check(journal.Snapshot().ScanDistinctAddresses == 2 && journal.Snapshot().Observations.Count == 1 &&
      journal.Snapshot().Observations.Single().Address == BleIdentity.ExpectedAddress &&
      journal.Snapshot().Events.All(x => x.Address is null || x.Address == BleIdentity.ExpectedAddress),
    "export keeps totals but redacts unselected device and its events");
Check(journal.Select("00:00:00:00:00:00") && journal.Snapshot().ProductServiceFound is null &&
      journal.Snapshot().GattState == "not_attempted", "switching target invalidates previous GATT result");
Check(journal.Snapshot().Observations.Single().Address == "00:00:00:00:00:00" &&
      journal.Snapshot().Events.All(x => x.Address is null || x.Address == "00:00:00:00:00:00"),
    "explicitly selecting another device changes exported address and events");
journal.ClearSelection();
Check(journal.Snapshot().SelectedAddress is null, "new scan can discard stale selection");
Check(journal.Snapshot().Observations.Count == 0 && journal.Snapshot().Events.All(x => x.Address is null),
    "unselected scan exports no observed addresses");
for (var i = 0; i < BleReportJournal.MaxAddresses + 10; i++)
    journal.Advertisement((ulong)i, "Public", "", -80, []);
Check(journal.Snapshot().ScanDistinctAddresses == BleReportJournal.MaxAddresses &&
      journal.Snapshot().ScanDroppedAddresses > 0, "bounded address retention and explicit drops");
for (var i = 0; i < BleReportJournal.MaxEvents + 3; i++) journal.AddEvent("test", null, "bounded");
Check(journal.Snapshot().Events.Count == BleReportJournal.MaxEvents,
    "bounded event retention");
Check(CliParser.TryParse(["--scan", "12", "--probe", "a8:42:e3:cd:4b:1a", "--out", "result.json"],
    out var options, out _) && options?.ScanSeconds == 12 &&
    options.ProbeAddress == BleIdentity.ExpectedAddress && options.OutputPath == "result.json" && !options.PairOnce,
    "CLI scan and explicit probe parsing");
Check(!CliParser.TryParse(["--scan", "0"], out _, out _) &&
      !CliParser.TryParse(["--scan", "61"], out _, out _) &&
      !CliParser.TryParse(["--scan", "12", "--probe", "invalid"], out _, out _) &&
      !CliParser.TryParse(["--probe", BleIdentity.ExpectedAddress], out _, out _),
    "CLI rejects unbounded, invalid and no-scan runs");
Check(CliParser.TryParse(["--scan", "12", "--probe", BleIdentity.ExpectedAddress, "--pair-once"],
    out var pairOptions, out _) && pairOptions!.PairOnce,
    "one-time pairing requires explicit flag and fixed target");
Check(!CliParser.TryParse(["--scan", "12", "--pair-once"], out _, out _) &&
      !CliParser.TryParse(["--scan", "12", "--probe", "00:00:00:00:00:01", "--pair-once"], out _, out _) &&
      !CliParser.TryParse(["--scan", "12", "--probe", BleIdentity.ExpectedAddress, "--pair-once", "--pair-once"], out _, out _),
    "one-time pairing rejects missing, foreign, or repeated target flag");
var pairJournal = new BleReportJournal();
pairJournal.ScanStarted();
pairJournal.Advertisement(address, "Public", "Box", -25, [BleIdentity.ProductService]);
pairJournal.ScanEnded("bounded_timeout");
Check(!CliPairGate.MayRequestOnce(pairOptions!, pairJournal.Snapshot()), "advertisement alone cannot trigger pairing");
Check(pairJournal.Select(BleIdentity.ExpectedAddress), "observed product address selected for gate");
pairJournal.GattStarted();
pairJournal.GattEnded("completed", "Success", [BleIdentity.LabService]);
Check(!CliPairGate.MayRequestOnce(pairOptions!, pairJournal.Snapshot()), "lab-only GATT cannot trigger pairing");
pairJournal.GattStarted();
pairJournal.GattEnded("completed", "Success", [BleIdentity.ProductService]);
Check(CliPairGate.MayRequestOnce(pairOptions!, pairJournal.Snapshot()), "same-run product GATT enables one explicit pairing request");
pairJournal.PairingStarted("CLI does not handle user dialogs");
pairJournal.PairingEnded("pending_after_deadline", "Windows may continue");
pairJournal.PairingOsStateObserved(false);
Check(pairJournal.Snapshot().PairingState == "pending_after_deadline" &&
      pairJournal.Snapshot().PairingOsIsPairedAtLastCheck == false &&
      pairJournal.Snapshot().PairingInteractionBoundary == "CLI does not handle user dialogs",
    "deadline, OS state, and interaction boundary remain separate");
Check(PairingOutcome.ReturnedState("Paired", true) == "paired" &&
      PairingOutcome.ReturnedState("Paired", false) == "reported_paired_unconfirmed" &&
      PairingOutcome.ReturnedState("AlreadyPaired", null) == "reported_paired_unconfirmed" &&
      PairingOutcome.ReturnedState("NotPaired", true) == "not_paired",
    "reported pair result needs separate IsPaired confirmation");
Check(await BoundedWait.ForStopAsync(Task.CompletedTask, TimeSpan.FromSeconds(1)) == BoundedWaitResult.Stopped,
    "scan ends on watcher stop");
Check(await BoundedWait.ForStopAsync(new TaskCompletionSource().Task, TimeSpan.FromMilliseconds(20)) == BoundedWaitResult.TimedOut,
    "scan ends at deadline without watcher event");
Check(CliExitCode.ForResult("watcher_error:DisabledByUser", null) == CliExitCode.ScanFailed &&
      CliExitCode.ForResult("scan_error", null) == CliExitCode.ScanFailed &&
      CliExitCode.ForResult("bounded_timeout", null) == CliExitCode.Success,
    "CLI reports watcher and start errors as failed runs");
Check(CliExitCode.ForResult("watcher_error:DisabledByUser", "access denied") == CliExitCode.ReportSaveFailed &&
      CliExitCode.ForResult("bounded_timeout", "access denied") == CliExitCode.ReportSaveFailed,
    "CLI reports denied JSON save with dedicated exit status");
Check(CliExitCode.ForResult("bounded_timeout", null, true, "pending_after_deadline") == CliExitCode.PairNotConfirmed &&
      CliExitCode.ForResult("bounded_timeout", null, true, "paired") == CliExitCode.Success,
    "CLI pairing exit code reflects confirmed OS state only");
var occupiedReportPath = Path.Combine(Path.GetTempPath(), "ble-report-occupied-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(occupiedReportPath);
try
{
    Check(!AtomicReportFile.TryWrite(occupiedReportPath, report, out var saveFailure) &&
          !string.IsNullOrWhiteSpace(saveFailure),
        "occupied report destination is handled without an unhandled exception");
}
finally
{
    var temporary = occupiedReportPath + ".tmp";
    if (File.Exists(temporary)) File.Delete(temporary);
    Directory.Delete(occupiedReportPath);
}
Console.WriteLine("All desktop BLE core checks passed.");
