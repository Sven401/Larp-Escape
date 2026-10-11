using BleDiagnostics.Core;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Devices.Bluetooth.GenericAttributeProfile;

namespace BleDiagnostics.Desktop;

internal static class CliRunner
{
    public static async Task<int> RunAsync(CliOptions options)
    {
        var journal = new BleReportJournal();
        var output = Path.GetFullPath(options.OutputPath ?? Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Larp-Escape",
            "DesktopBleDiagnostics", $"{journal.RunId}.json"));
        var sync = new object();
        string? saveError = null;
        void Save()
        {
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(output)!);
                var temp = output + ".tmp";
                File.WriteAllText(temp, BleReportJournal.ToJson(journal.Snapshot()));
                File.Move(temp, output, true);
                saveError = null;
            }
            catch (Exception ex) { saveError = ex.Message; }
        }

        var watcher = new BluetoothLEAdvertisementWatcher { ScanningMode = BluetoothLEScanningMode.Active };
        var stopped = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        string stopReason = "watcher_stopped";
        var timeoutIssued = 0;
        var accepting = 1;
        watcher.Received += (_, e) =>
        {
            if (Volatile.Read(ref accepting) == 0) return;
            lock (sync)
            {
                if (Volatile.Read(ref accepting) == 0) return;
                var before = journal.Observations.Count;
                journal.Advertisement(e.BluetoothAddress, e.BluetoothAddressType.ToString(),
                    e.Advertisement.LocalName ?? "", e.RawSignalStrengthInDBm, e.Advertisement.ServiceUuids);
                if (journal.Observations.Count > before || journal.ScanCallbacks % 10 == 0) Save();
            }
        };
        watcher.Stopped += (_, e) =>
        {
            if (Volatile.Read(ref timeoutIssued) == 0)
                stopReason = e.Error.ToString() == "Success" ? "watcher_stopped" : $"watcher_error:{e.Error}";
            stopped.TrySetResult();
        };
        try
        {
            watcher.Start();
            lock (sync)
            {
                journal.ScanStarted(options.ScanSeconds);
                Save();
            }
            var outcome = await BoundedWait.ForStopAsync(stopped.Task, TimeSpan.FromSeconds(options.ScanSeconds));
            if (outcome == BoundedWaitResult.TimedOut)
            {
                Interlocked.Exchange(ref timeoutIssued, 1);
                watcher.Stop();
                stopReason = "bounded_timeout";
            }
        }
        catch (Exception ex)
        {
            stopReason = "scan_error";
            lock (sync) journal.AddEvent("scan_error", null, ex.Message);
        }
        finally
        {
            try { watcher.Stop(); } catch { }
            lock (sync)
            {
                Interlocked.Exchange(ref accepting, 0);
                journal.ScanEnded(stopReason);
                Save();
            }
        }

        if (options.ProbeAddress is not null)
        {
            BleObservation? chosen;
            lock (sync)
            {
                chosen = journal.Observations.FirstOrDefault(x => x.Address == options.ProbeAddress);
                if (chosen is null) journal.AddEvent("probe_skipped", options.ProbeAddress, "address not observed in this scan");
                else journal.Select(chosen.Address);
                Save();
            }
            if (chosen is not null) await ProbeGattAsync(journal, chosen, sync, Save);
        }

        lock (sync) Save();
        Console.WriteLine($"BLE report: {output}");
        Console.WriteLine($"scan={journal.ScanState}; callbacks={journal.ScanCallbacks}; devices={journal.Observations.Count}; gatt={journal.GattState}; product={journal.ProductServiceFound}; pairing={journal.PairingState}");
        if (saveError is not null)
        {
            Console.Error.WriteLine($"JSON report could not be saved: {saveError}");
            return 4;
        }
        return stopReason == "scan_error" ? 3 : 0;
    }

    private static async Task ProbeGattAsync(BleReportJournal journal, BleObservation chosen, object sync, Action save)
    {
        lock (sync) { journal.GattStarted(); save(); }
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(20));
        try
        {
            if (!BleIdentity.TryParseAddress(chosen.Address, out var address)) throw new InvalidOperationException("invalid observed address");
            var type = chosen.AddressType == BluetoothAddressType.Random.ToString()
                ? BluetoothAddressType.Random : BluetoothAddressType.Public;
            using var device = await BluetoothLEDevice.FromBluetoothAddressAsync(address, type).AsTask(timeout.Token);
            if (device is null) throw new InvalidOperationException("Windows did not return a BLE device");
            var result = await device.GetGattServicesAsync(BluetoothCacheMode.Uncached).AsTask(timeout.Token);
            var services = result.Services.Select(x => x.Uuid).ToArray();
            foreach (var service in result.Services) service.Dispose();
            lock (sync)
            {
                journal.GattEnded(result.Status == GattCommunicationStatus.Success ? "completed" : "failed",
                    result.Status.ToString(), result.Status == GattCommunicationStatus.Success ? services : null);
                save();
            }
        }
        catch (OperationCanceledException)
        {
            lock (sync) { journal.GattEnded("timeout", "20-second deadline", null); save(); }
        }
        catch (Exception ex)
        {
            lock (sync) { journal.GattEnded("error", ex.Message, null); save(); }
        }
    }
}
