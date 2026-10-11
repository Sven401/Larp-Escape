namespace BleDiagnostics.Core;

public static class AtomicReportFile
{
    public static bool TryWrite(string path, BleReport report, out string? error)
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path))!);
            var temporary = path + ".tmp";
            File.WriteAllText(temporary, BleReportJournal.ToJson(report));
            File.Move(temporary, path, true);
            error = null;
            return true;
        }
        catch (Exception ex)
        {
            error = ex.Message;
            return false;
        }
    }
}
