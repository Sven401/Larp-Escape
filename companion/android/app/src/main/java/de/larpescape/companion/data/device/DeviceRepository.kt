package de.larpescape.companion.data.device

import kotlinx.coroutines.flow.StateFlow
import de.larpescape.protocol.ConfiguredStatus
import de.larpescape.protocol.StatusFreshness

data class DeviceTarget(val id: String, val name: String)
data class DeviceUiState(
    val message: String = "Bereit zum Suchen", val targets: List<DeviceTarget> = emptyList(),
    val scanning: Boolean = false, val connected: Boolean = false, val status: String? = null,
    val lab: Boolean = false, val configuredStatus: ConfiguredStatus? = null,
    val freshness: StatusFreshness = StatusFreshness.DISCONNECTED,
    val permissionMissing: Boolean = false, val leds: LedUiState = LedUiState(),
    val matrixCollection: MatrixCollection = MatrixCollection(),
)
interface DeviceRepository {
    val state: StateFlow<DeviceUiState>
    fun scan()
    fun connect(id: String)
    fun selectLedEffect(effect: LedEffect)
    fun startLedEffect()
    fun boxNfc(change: Boolean) {}
    fun stopLeds()
    fun selectEspNowPeer(value: String) {}
    fun sendEspNowTest() {}
    fun selectAudioTrack(value: String)
    fun selectAudioVolume(value: String)
    fun startAudio()
    fun audioAction(operation: Int) {}
    fun stopAudio()
    fun importLedAccess(bytes: ByteArray)
    fun matrixAction(action: String) {}
    fun disconnect()
    fun permissionsChanged(granted: Boolean, denied: Boolean = false)
    fun close()
}
