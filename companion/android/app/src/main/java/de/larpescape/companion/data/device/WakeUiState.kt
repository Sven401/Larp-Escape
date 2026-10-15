package de.larpescape.companion.data.device

import de.larpescape.protocol.LedStatus
import de.larpescape.protocol.WakeStatus

data class WakeUiState(
    val supported: Boolean = false,
    val current: Boolean = false,
    val activeMask: Int? = null,
    val message: String = "Getrennt · Wake-Zustand nicht bestätigt",
    val boxId: String? = null,
) {
    fun active(input: Int): Boolean? {
        require(input in 1..4)
        return activeMask?.takeIf { supported && current }?.let { it and (1 shl (input - 1)) != 0 }
    }
}

/** Results share the status sequence but never carry or refresh a Wake observation. */
internal class WakeStateTracker(private val boxId: String) {
    private var latestSequence = 0L
    private var sample: WakeStatus? = null
    private var receivedAt = 0L

    fun accept(status: LedStatus, now: Long): Boolean {
        if (status.statusSequence <= latestSequence) return false
        latestSequence = status.statusSequence
        val wake = status.wake ?: return false
        sample = wake
        receivedAt = now
        return true
    }

    fun state(now: Long, authenticated: Boolean): WakeUiState {
        if (!authenticated) return WakeUiState()
        val wake = sample ?: return WakeUiState(
            message = "Warte auf bestätigten Wake-Status …", boxId = boxId)
        val fresh = now - receivedAt in 0..999
        val readable = fresh && wake.supported && wake.valid
        val message = when {
            !fresh -> "Wake-Status überholt · warte auf neue Boxmeldung"
            !wake.supported -> "Diese Firmware unterstützt die Wake-Anzeige noch nicht"
            !wake.valid -> "Wake-Eingänge nicht lesbar · Box meldet einen Lesefehler"
            else -> "Live · HIGH wird vorläufig als aktiv angezeigt"
        }
        return WakeUiState(wake.supported, readable, if (readable) wake.activeMask else null,
            message, boxId)
    }

    fun clear() {
        latestSequence = 0L
        sample = null
        receivedAt = 0L
    }
}