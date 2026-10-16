package de.larpescape.companion.data.device

import de.larpescape.protocol.LedStatus
import de.larpescape.protocol.MatrixStatus

data class MatrixUiState(
    val supported: Boolean = false,
    val current: Boolean = false,
    val ambiguous: Boolean = false,
    val pressed: Set<Int>? = null,
    val message: String = "Getrennt · Matrixzustand nicht bestätigt",
) {
    /** Raw electrical contact only; an ambiguous observation cannot identify a physical key. */
    fun active(index: Int): Boolean? {
        require(index in 0..80)
        return pressed?.takeIf { supported && current }?.contains(index)
    }

    val observedCoordinates: List<String> get() = if (supported && current) {
        pressed.orEmpty().sorted().map(::matrixCoordinate)
    } else emptyList()
}

/** Electrical scan coordinates; a physical key assignment is established in the joint test. */
fun matrixCoordinate(index: Int): String {
    require(index in 0..80)
    return "${'A' + index % 9}${index / 9 + 1}"
}

/** Only complete, authenticated status samples refresh the matrix observation. */
internal class MatrixStateTracker {
    private var latestSequence = 0L
    private var sample: MatrixStatus? = null
    private var receivedAt = 0L

    fun accept(status: LedStatus, now: Long): Boolean {
        if (status.statusSequence <= latestSequence) return false
        latestSequence = status.statusSequence
        // Old 70-byte status messages contain Wake but no matrix. Results contain neither.
        val matrix = status.matrix ?: if (status.wake != null) {
            MatrixStatus(false, false, false, emptySet())
        } else return false
        sample = matrix.copy(pressed = matrix.pressed.toSet())
        receivedAt = now
        return true
    }

    fun state(now: Long, authenticated: Boolean): MatrixUiState {
        if (!authenticated) return MatrixUiState()
        val matrix = sample ?: return MatrixUiState(message = "Warte auf bestätigten Matrixstatus …")
        val fresh = now - receivedAt in 0..999
        val readable = fresh && matrix.supported && matrix.valid
        val message = when {
            !fresh -> "Matrixstatus überholt · warte auf neue Boxmeldung"
            !matrix.supported -> "Matrix-Test vorbereitet · die verbundene Firmware liefert noch keine Matrixdaten"
            !matrix.valid -> "Matrix nicht lesbar · Box meldet einen Lesefehler"
            matrix.ambiguous -> "Mehrfachkontakt – Zuordnung unklar"
            else -> "Live · elektrische Kontaktkoordinaten"
        }
        return MatrixUiState(matrix.supported, readable, readable && matrix.ambiguous,
            if (readable) matrix.pressed else null, message)
    }

    fun clear() {
        latestSequence = 0L
        sample = null
        receivedAt = 0L
    }
}
