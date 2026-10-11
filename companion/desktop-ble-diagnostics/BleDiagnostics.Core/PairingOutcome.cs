namespace BleDiagnostics.Core;

public static class PairingOutcome
{
    public static string ReturnedState(string windowsStatus, bool? osIsPaired)
    {
        if (windowsStatus is "Paired" or "AlreadyPaired")
            return osIsPaired == true ? "paired" : "reported_paired_unconfirmed";
        return "not_paired";
    }
}
