package de.larpescape.companion.data.device

enum class LedEffect(val wire: Int, val label: String, val description: String) {
    TWINKLE(1, "Funkeln", "Läuft bis Stop"),
    BURST(2, "Lichtimpuls", "Leuchtet einmal auf, danach aus"),
    RIPPLE(3, "Ripple", "Lichtwelle einmal, danach aus"),
    RESONANCE(4, "Resonanzkreis", "Vier aktive Wake-Bereiche atmen und leuchten gemeinsam")
}

data class LedUiState(
    val selected: LedEffect = LedEffect.TWINKLE,
    val available: Boolean = false,
    val authenticated: Boolean = false,
    val current: Boolean = false,
    val ready: Boolean = false,
    val gameStopped: Boolean? = null,
    val ledOnly: Boolean = false,
    val busy: Boolean = false,
    val active: LedEffect? = null,
    val message: String = "LED-Bedienung benötigt eine eingerichtete Box.",
    val accessLabel: String? = null,
    val wake: WakeUiState = WakeUiState(),
    val nfc: BoxNfcUiState = BoxNfcUiState(),
    val audio: AudioUiState = AudioUiState(),
    val matrix: MatrixUiState = MatrixUiState(),
    val resonanceSupported: Boolean = false,
) {
    val canStart: Boolean get() = available && authenticated && current && ready && (ledOnly || gameStopped == true) && !busy &&
        (selected != LedEffect.RESONANCE || resonanceSupported)
    val canStop: Boolean get() = available && authenticated && current && ready && (ledOnly || gameStopped == true) && !busy
    fun disconnected() = copy(available = false, authenticated = false, current = false, ready = false, gameStopped = null, ledOnly = false,
        busy = false, active = null, resonanceSupported = false, message = "Getrennt · LED-Zustand nicht mehr bestätigt", wake = WakeUiState(), matrix = MatrixUiState(), audio = audio.disconnected(), nfc = BoxNfcUiState(message = if (nfc.busy) "Verbindung beendet · NFC-Ausgang unklar. Neu verbinden und zuerst lesen." else "Getrennt · Box erneut verbinden und Tag lesen"))
}
