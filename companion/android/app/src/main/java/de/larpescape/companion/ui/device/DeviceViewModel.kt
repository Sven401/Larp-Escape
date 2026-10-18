package de.larpescape.companion.ui.device

import androidx.lifecycle.ViewModel
import de.larpescape.companion.data.device.DeviceRepository
import de.larpescape.companion.data.device.LedEffect

class DeviceViewModel(private val repository: DeviceRepository) : ViewModel() {
    val state = repository.state
    private var foreground = false
    private var permissionGranted = false
    private var pendingPermissionScan = false
    fun foregroundStarted(granted: Boolean) {
        foreground = true
        permissionsChanged(granted)
        runPendingScan()
    }
    fun permissionsChanged(granted: Boolean) {
        permissionGranted = granted
        if (foreground || !granted) repository.permissionsChanged(granted)
    }
    fun permissionRequestStarted() { pendingPermissionScan = foreground }
    fun permissionResult(granted: Boolean) {
        permissionGranted = granted
        if (foreground || !granted) repository.permissionsChanged(granted, denied = !granted)
        if (!granted) pendingPermissionScan = false
        runPendingScan()
    }
    private fun runPendingScan() {
        if (foreground && permissionGranted && pendingPermissionScan) {
            pendingPermissionScan = false
            repository.scan()
        }
    }
    fun scan() { if (foreground && permissionGranted) repository.scan() }
    fun connect(id: String) { if (foreground && permissionGranted) repository.connect(id) }
    fun selectLedEffect(effect: LedEffect) = repository.selectLedEffect(effect)
    fun startLedEffect() { if (foreground && permissionGranted && state.value.leds.canStart) repository.startLedEffect() }
    fun boxNfc(change: Boolean) { if (foreground && permissionGranted) repository.boxNfc(change) }
    fun stopLeds() { if (foreground && permissionGranted && state.value.leds.canStop) repository.stopLeds() }
    fun selectEspNowPeer(value: String) = repository.selectEspNowPeer(value)
    fun sendEspNowTest() { if (foreground && permissionGranted && state.value.leds.espNow.canSend) repository.sendEspNowTest() }
    fun selectAudioTrack(value: String) = repository.selectAudioTrack(value)
    fun selectAudioVolume(value: String) = repository.selectAudioVolume(value)
    fun startAudio() { if (foreground && permissionGranted && state.value.leds.audio.canStart) repository.startAudio() }
    fun audioAction(operation: Int) {
        val audio = state.value.leds.audio
        if (foreground && permissionGranted && when(operation) {
            3 -> audio.canPause; 4 -> audio.canResume; 5 -> audio.canSetVolume; else -> false
        }) repository.audioAction(operation)
    }
    fun stopAudio() { if (foreground && permissionGranted && state.value.leds.audio.canStop) repository.stopAudio() }
    fun importLedAccess(bytes: ByteArray) = repository.importLedAccess(bytes)
    fun matrixAction(action: String) = repository.matrixAction(action)
    fun disconnect() = repository.disconnect()
    fun foregroundEnded() {
        foreground = false
        pendingPermissionScan = false
        repository.close()
    }
    override fun onCleared() = repository.close()
}
