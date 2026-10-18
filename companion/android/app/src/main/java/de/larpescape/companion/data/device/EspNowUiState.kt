package de.larpescape.companion.data.device

import de.larpescape.protocol.EspNowResult
import de.larpescape.protocol.espNowPeer

data class EspNowUiState(
    val peerText: String = "",
    val supported: Boolean = false,
    val current: Boolean = false,
    val ready: Boolean = false,
    val busy: Boolean = false,
    val waiting: Boolean = false,
    val savedSamples: Long = 0,
    val hasJournal: Boolean = false,
    val rssiSupported: Boolean = false,
    val measuring: Boolean = false,
    val rssiResult: EspNowResult? = null,
    val rssiCurrent: Boolean = false,
    val rssiAgeMillis: Long? = null,
    val result: EspNowResult? = null,
    val message: String = "Box verbinden, um ESP-NOW zu testen",
) {
    val peer get() = espNowPeer(peerText)
    val canSend get() = supported && current && ready && !busy && !measuring && peer != null
    fun disconnected() = EspNowUiState(peerText = peerText, savedSamples = savedSamples, hasJournal = hasJournal,
        message = if (waiting) "Verbindung beendet · Empfang unbekannt. Erneut verbinden und bewusst testen."
        else "Getrennt · Empfang nicht bestätigt")
}

internal fun espNowResultMessage(result: EspNowResult): String = when (result.led.outcome) {
    0 -> "Empfänger bestätigt diesen Test"
    4 -> "ESP-NOW nicht bereit · auf frischen Boxstatus warten"
    8 -> "Auftrag abgelaufen · erneut bewusst testen"
    9 -> "Box beschäftigt · erneut bewusst testen"
    10 -> "Box meldet einen Funkfehler · Empfang nicht bestätigt"
    12 -> "Funkzustellung fehlgeschlagen · Empfang nicht bestätigt"
    13 -> "Keine passende Empfangsbestätigung innerhalb von zwei Sekunden"
    else -> "Box hat den Test abgelehnt (${result.led.outcome})"
}
