namespace BleDiagnostics.Core;

public sealed record CliOptions(int ScanSeconds, string? ProbeAddress, string? OutputPath, bool PairOnce);

public static class CliParser
{
    public static bool TryParse(string[] args, out CliOptions? options, out string? error)
    {
        options = null;
        error = null;
        if (args.Length == 0) { error = "No CLI arguments"; return false; }
        int? seconds = null;
        string? address = null;
        string? output = null;
        var pairOnce = false;
        for (var i = 0; i < args.Length; i++)
        {
            switch (args[i])
            {
                case "--scan":
                    if (++i >= args.Length || !int.TryParse(args[i], out var parsed) || parsed is < 1 or > 60 || seconds is not null)
                    { error = "--scan requires one duration from 1 to 60 seconds"; return false; }
                    seconds = parsed;
                    break;
                case "--probe":
                    if (++i >= args.Length || !BleIdentity.TryParseAddress(args[i], out var parsedAddress) || address is not null)
                    { error = "--probe requires one six-byte BLE address"; return false; }
                    address = BleIdentity.FormatAddress(parsedAddress);
                    break;
                case "--out":
                    if (++i >= args.Length || string.IsNullOrWhiteSpace(args[i]) || output is not null)
                    { error = "--out requires one JSON file path"; return false; }
                    output = args[i];
                    break;
                case "--pair-once":
                    if (pairOnce) { error = "--pair-once can appear only once"; return false; }
                    pairOnce = true;
                    break;
                default:
                    error = $"Unknown CLI argument: {args[i]}";
                    return false;
            }
        }
        if (seconds is null) { error = "--scan is required (1 to 60 seconds)"; return false; }
        if (pairOnce && address != BleIdentity.ExpectedAddress)
        {
            error = $"--pair-once requires --probe {BleIdentity.ExpectedAddress}";
            return false;
        }
        options = new CliOptions(seconds.Value, address, output, pairOnce);
        return true;
    }
}
