using BleDiagnostics.Core;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Foundation;

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
            AtomicReportFile.TryWrite(output, journal.Snapshot(), out saveError);
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
            Interlocked.Exchange(ref timeoutIssued, 1);
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
                if (chosen is null) journal.AddEvent("probe_skipped", null, "requested address not observed in this scan");
                else journal.Select(chosen.Address);
                Save();
            }
            if (chosen is not null) await ProbeGattAsync(journal, chosen, sync, Save);
        }

        if (options.PairOnce)
        {
            if (CliPairGate.MayRequestOnce(options, journal.Snapshot()))
                await PairOnceAsync(journal, sync, Save);
            else
            {
                lock (sync)
                {
                    journal.PairingEnded("skipped", "same-run advertisement and successful uncached product GATT service required");
                    Save();
                }
            }
        }

        lock (sync) Save();
        Console.WriteLine($"BLE report: {output}");
        Console.WriteLine($"scan={journal.ScanState}; callbacks={journal.ScanCallbacks}; devices={journal.Observations.Count}; gatt={journal.GattState}; product={journal.ProductServiceFound}; pairing={journal.PairingState}");
        if (saveError is not null)
        {
            Console.Error.WriteLine($"JSON report could not be saved: {saveError}");
        }
        return CliExitCode.ForResult(stopReason, saveError, options.PairOnce, journal.PairingState);
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

    private static async Task PairOnceAsync(BleReportJournal journal, object sync, Action save)
    {
        lock (sync)
        {
            journal.PairingStarted("CLI never enters PIN/passkey or confirms a Windows system dialog; user action, if required, remains open");
            save();
        }
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(45));
        BluetoothLEDevice? device = null;
        IAsyncOperation<DevicePairingResult>? pairingOperation = null;
        try
        {
            if (!BleIdentity.TryParseAddress(BleIdentity.ExpectedAddress, out var address))
                throw new InvalidOperationException("invalid fixed product address");
            device = await BluetoothLEDevice.FromBluetoothAddressAsync(address, BluetoothAddressType.Public).AsTask(timeout.Token);
            if (device is null) throw new InvalidOperationException("Windows did not return the observed product BLE device");
            // Exactly one public Windows pairing request. No custom ceremony or credential handler is registered.
            pairingOperation = device.DeviceInformation.Pairing.PairAsync();
            var result = await pairingOperation.AsTask(timeout.Token);
            var osIsPaired = ReadOsIsPaired(device, journal, sync);
            var state = PairingOutcome.ReturnedState(result.Status.ToString(), osIsPaired);
            lock (sync)
            {
                journal.PairingEnded(state, result.Status.ToString());
                journal.PairingOsStateObserved(osIsPaired);
                save();
            }
        }
        catch (OperationCanceledException)
        {
            try { pairingOperation?.Cancel(); } catch { }
            var osIsPaired = ReadOsIsPaired(device, journal, sync);
            lock (sync)
            {
                journal.PairingEnded("pending_after_deadline",
                    "PairAsync result unavailable at 45-second deadline; cancellation requested but Windows pairing may continue");
                journal.PairingOsStateObserved(osIsPaired);
                save();
            }
        }
        catch (Exception ex)
        {
            var osIsPaired = ReadOsIsPaired(device, journal, sync);
            lock (sync)
            {
                journal.PairingEnded("error", ex.Message);
                journal.PairingOsStateObserved(osIsPaired);
                save();
            }
        }
        finally { device?.Dispose(); }
    }

    private static bool? ReadOsIsPaired(BluetoothLEDevice? device, BleReportJournal journal, object sync)
    {
        if (device is null) return null;
        try { return device.DeviceInformation.Pairing.IsPaired; }
        catch (Exception ex)
        {
            lock (sync) journal.AddEvent("windows_is_paired_read_error", journal.SelectedAddress, ex.GetType().Name);
            return null;
        }
    }
}
