package de.larpescape.companion.data.device

import kotlinx.coroutines.flow.StateFlow
import de.larpescape.protocol.ConfiguredStatus
import de.larpescape.protocol.StatusFreshness

enum class BleScanModeV1(val reportValue: String) {
    PRODUCT("product"), UNVERIFIED_DIAGNOSTIC("unverified_diagnostic")
}

enum class DeviceTargetKindV1 { PRODUCT_ADVERTISED, LAB_ADVERTISED, UNVERIFIED }

data class DeviceTarget(val id: String, val name: String, val kind: DeviceTargetKindV1)
data class DeviceUiState(
    val message: String = "Bereit zum Suchen", val targets: List<DeviceTarget> = emptyList(),
    val scanning: Boolean = false, val scanMode: BleScanModeV1 = BleScanModeV1.PRODUCT,
    val connected: Boolean = false, val status: String? = null,
    val lab: Boolean = false, val configuredStatus: ConfiguredStatus? = null,
    val freshness: StatusFreshness = StatusFreshness.DISCONNECTED,
    val permissionMissing: Boolean = false,
    val diagnosticAvailability: DiagnosticAvailabilityV1 = DiagnosticAvailabilityV1.DISCONNECTED,
    val pairingReport: PairingObservationReportV1? = null,
)
interface DeviceRepository {
    val state: StateFlow<DeviceUiState>
    fun scan(mode: BleScanModeV1 = BleScanModeV1.PRODUCT)
    fun connect(id: String)
    fun disconnect()
    fun startPairingReport() {}
    fun requestBond() {}
    fun pairingReportBytes(): ByteArray? = null
    fun pendingPairingReportBytes(): ByteArray? = null
    fun pairingExportCompleted() {}
    fun connectionEvidenceExportBytes(): ByteArray? = null
    fun permissionsChanged(granted: Boolean, denied: Boolean = false)
    fun close()
}
