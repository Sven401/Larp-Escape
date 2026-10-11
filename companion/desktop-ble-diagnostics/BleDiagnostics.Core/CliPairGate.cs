namespace BleDiagnostics.Core;

public static class CliPairGate
{
    public static bool MayRequestOnce(CliOptions options, BleReport report) =>
        options.PairOnce && options.ProbeAddress == BleIdentity.ExpectedAddress &&
        report.SelectedAddress == BleIdentity.ExpectedAddress &&
        report.Observations.Any(x => x.Address == BleIdentity.ExpectedAddress) &&
        (report.ScanState is "bounded_timeout" or "watcher_stopped") &&
        report.GattState == "completed" && report.GattStatus == "Success" &&
        report.ProductServiceFound == true;
}
