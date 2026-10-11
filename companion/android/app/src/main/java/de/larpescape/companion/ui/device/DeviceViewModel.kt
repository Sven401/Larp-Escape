package de.larpescape.companion.ui.device

import androidx.lifecycle.ViewModel
import de.larpescape.companion.data.device.DeviceRepository

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
    fun disconnect() = repository.disconnect()
    fun startPairingReport() = repository.startPairingReport()
    fun requestBond() = repository.requestBond()
    fun pairingReportBytes(): ByteArray? = repository.pairingReportBytes()
    fun foregroundEnded() {
        foreground = false
        pendingPermissionScan = false
        repository.close()
    }
    override fun onCleared() = repository.close()
}
