namespace BleDiagnostics.Core;

public static class CliExitCode
{
    public const int Success = 0;
    public const int ScanFailed = 3;
    public const int ReportSaveFailed = 4;
    public const int PairNotConfirmed = 5;

    public static int ForResult(string scanState, string? saveError, bool pairRequested = false, string? pairingState = null)
    {
        if (saveError is not null) return ReportSaveFailed;
        if (scanState == "scan_error" || scanState.StartsWith("watcher_error:", StringComparison.Ordinal))
            return ScanFailed;
        if (pairRequested && pairingState != "paired") return PairNotConfirmed;
        return Success;
    }
}
