namespace BleDiagnostics.Core;

public static class CliExitCode
{
    public const int Success = 0;
    public const int ScanFailed = 3;
    public const int ReportSaveFailed = 4;

    public static int ForResult(string scanState, string? saveError)
    {
        if (saveError is not null) return ReportSaveFailed;
        if (scanState == "scan_error" || scanState.StartsWith("watcher_error:", StringComparison.Ordinal))
            return ScanFailed;
        return Success;
    }
}
