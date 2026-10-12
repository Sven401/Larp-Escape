package de.larpescape.companion.ui.device

import androidx.lifecycle.ViewModel
import de.larpescape.companion.data.device.DeviceRepository
import de.larpescape.companion.data.device.BleScanModeV1

class DeviceViewModel(private val repository: DeviceRepository) : ViewModel() {
    val state = repository.state
    private var foreground = false
    private var permissionGranted = false
    private var pendingPermissionScan: BleScanModeV1? = null
    fun foregroundStarted(granted: Boolean) {
        foreground = true
        permissionsChanged(granted)
        runPendingScan()
    }
    fun permissionsChanged(granted: Boolean) {
        permissionGranted = granted
        if (foreground || !granted) repository.permissionsChanged(granted)
    }
    fun permissionRequestStarted(mode: BleScanModeV1 = BleScanModeV1.PRODUCT) {
        pendingPermissionScan = mode.takeIf { foreground }
    }
    fun permissionResult(granted: Boolean) {
        permissionGranted = granted
        if (foreground || !granted) repository.permissionsChanged(granted, denied = !granted)
        if (!granted) pendingPermissionScan = null
        runPendingScan()
    }
    private fun runPendingScan() {
        if (foreground && permissionGranted && pendingPermissionScan != null) {
            val mode = pendingPermissionScan ?: return
            pendingPermissionScan = null
            repository.scan(mode)
        }
    }
    fun scan(mode: BleScanModeV1 = BleScanModeV1.PRODUCT) {
        if (foreground && permissionGranted) repository.scan(mode)
    }
    fun connect(id: String) { if (foreground && permissionGranted) repository.connect(id) }
    fun disconnect() = repository.disconnect()
    fun startPairingReport() = repository.startPairingReport()
    fun requestBond() = repository.requestBond()
    fun requestDevelopmentGrant() {
        if (foreground && permissionGranted) repository.requestDevelopmentGrant()
    }
    fun requestDevelopmentRawObservation() {
        if (foreground && permissionGranted) repository.requestDevelopmentRawObservation()
    }
    fun pairingReportBytes(): ByteArray? = repository.pairingReportBytes()
    fun pendingPairingReportBytes(): ByteArray? = repository.pendingPairingReportBytes()
    fun pairingExportCompleted() = repository.pairingExportCompleted()
    fun connectionEvidenceExportBytes(): ByteArray? = repository.connectionEvidenceExportBytes()
    fun foregroundEnded() {
        foreground = false
        pendingPermissionScan = null
        repository.close()
    }
    override fun onCleared() = repository.close()
}
