using System.Windows.Forms;
using BleDiagnostics.Core;

namespace BleDiagnostics.Desktop;

internal static class Program
{
    [STAThread]
    private static async Task<int> Main(string[] args)
    {
        if (args.Length != 0)
        {
            if (!CliParser.TryParse(args, out var options, out var error))
            {
                Console.Error.WriteLine(error);
                Console.Error.WriteLine("Usage: BleDiagnostics.Desktop.exe --scan <1..60> [--probe AA:BB:CC:DD:EE:FF] [--pair-once] [--out report.json]");
                return 2;
            }
            return await CliRunner.RunAsync(options!);
        }
        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm());
        return 0;
    }
}
