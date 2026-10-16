package de.larpescape.companion.data.device

import de.larpescape.protocol.AudioStatus
import de.larpescape.protocol.LedStatus

data class AudioUiState(
    val trackText: String = "100",
    val volumeText: String = "5",
    val supported: Boolean = false,
    val authenticated: Boolean = false,
    val current: Boolean = false,
    val ready: Boolean = false,
    val modeAllowed: Boolean = false,
    val busy: Boolean = false,
    val fault: Boolean = false,
    val active: Boolean? = null,
    val paused: Boolean = false,
    val observedTrack: Int? = null,
    val observedVolume: Int? = null,
    val busyLow: Boolean? = null,
    val message: String = "Getrennt · Audiozustand nicht bestätigt",
) {
    val selectedTrack get() = trackText.toIntOrNull()?.takeIf { it in 1..255 }
    val selectedVolume get() = volumeText.toIntOrNull()?.takeIf { it in 1..10 }
    val canStop get() = supported && authenticated && current && ready && modeAllowed && !busy && !fault
    val canPause get() = canStop && active == true
    val canResume get() = canStop && paused
    val canSetVolume get() = canStop && selectedVolume != null
    val canStart get() = canStop && selectedTrack != null && selectedVolume != null
    fun disconnected() = AudioUiState(trackText = trackText, volumeText = volumeText)
}

/** A LED result advances ordering, but cannot refresh this independent observation clock. */
internal class AudioStateTracker {
    private var latestSequence = 0L
    private var sample: AudioStatus? = null
    private var receivedAt = 0L

    fun accept(status: LedStatus, now: Long): Boolean {
        if (status.statusSequence <= latestSequence) return false
        latestSequence = status.statusSequence
        val audio = status.audio ?: return false
        sample = audio
        receivedAt = now
        return true
    }

    fun observation(now: Long): AudioStatus? = sample?.takeIf { now - receivedAt in 0..999 }

    fun state(now: Long, authenticated: Boolean, modeAllowed: Boolean, sharedBusy: Boolean,
        notice: String): AudioUiState {
        if (!authenticated) return AudioUiState()
        val audio = sample ?: return AudioUiState(authenticated = true, message = "Warte auf bestätigten Audiostatus …")
        val fresh = observation(now) != null
        val readable = fresh && audio.supported
        val message = when {
            !fresh -> "Audiostatus überholt · warte auf neue Boxmeldung"
            !audio.supported -> "Diese Firmware unterstützt den Audiotest noch nicht"
            audio.fault -> "Audio-Ausgabe meldet einen Fehler · Höreindruck offen"
            !audio.ready -> "Audio-Ausgabe noch nicht bereit"
            sharedBusy || audio.pending -> "Befehl läuft · warte auf Boxbestätigung …"
            !modeAllowed -> "Audio gesperrt · Spiel nicht vollständig gestoppt"
            else -> notice
        }
        return AudioUiState(supported = audio.supported, authenticated = true, current = readable,
            ready = readable && audio.ready, modeAllowed = modeAllowed, busy = sharedBusy || audio.pending,
            fault = audio.fault, active = if (readable) audio.active else null, paused = readable && audio.paused,
            observedTrack = audio.track.takeIf { readable && it != 0 },
            observedVolume = audio.volume.takeIf { readable && it != 0 },
            busyLow = if (readable && audio.busyValid) audio.busyLow else null, message = message)
    }

    fun clear() { latestSequence = 0L; sample = null; receivedAt = 0L }
}
