namespace BleDiagnostics.Core;

public enum BoundedWaitResult { Stopped, TimedOut }

public static class BoundedWait
{
    public static async Task<BoundedWaitResult> ForStopAsync(Task stopped, TimeSpan limit, CancellationToken cancellationToken = default)
    {
        if (limit <= TimeSpan.Zero) throw new ArgumentOutOfRangeException(nameof(limit));
        using var delayCancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        var timeout = Task.Delay(limit, delayCancellation.Token);
        var winner = await Task.WhenAny(stopped, timeout);
        cancellationToken.ThrowIfCancellationRequested();
        if (winner == stopped)
        {
            delayCancellation.Cancel();
            await stopped;
            return BoundedWaitResult.Stopped;
        }
        return BoundedWaitResult.TimedOut;
    }
}
