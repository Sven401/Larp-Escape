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
    fun importLedAccess(bytes: ByteArray) = repository.importLedAccess(bytes)
    fun disconnect() = repository.disconnect()
    fun foregroundEnded() {
        foreground = false
        pendingPermissionScan = false
        repository.close()
    }
    override fun onCleared() = repository.close()
}
