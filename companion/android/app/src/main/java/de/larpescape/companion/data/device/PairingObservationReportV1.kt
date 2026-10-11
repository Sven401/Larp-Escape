package de.larpescape.companion.data.device

import com.google.gson.Gson
import java.time.Instant
import java.util.UUID

/** Android-side observations only. No field in this report asserts firmware security. */
data class PairingObservationEventV1(
    val sequence: Int,
    val recordedUtc: String,
    val source: String,
    val connectionGeneration: Int?,
    val targetAddress: String?,
    val kind: String,
    val value: String,
)

data class PairingObservationReportV1(
    val schema: String = "ble_pairing_observation_v1",
    val reportId: String,
    val startedUtc: String,
    val exportedUtc: String? = null,
    val provenance: String = "Android Bluetooth API and BluetoothGatt callbacks; unverified by ESP firmware",
    val events: List<PairingObservationEventV1> = emptyList(),
    val missingFirmwareEvidence: List<String> = listOf(
        "original firmware connection ID and boot identity",
        "firmware-observed bond and reconnect key provenance",
        "firmware-observed link encryption and MITM security",
        "firmware role or diagnostic authorization",
    ),
) {
    fun observe(utc: String, source: String, generation: Int?, target: String?, kind: String, value: String) =
        copy(events = events + PairingObservationEventV1(events.size + 1, utc, source, generation, target, kind, value))

    fun export(utc: String): ByteArray = Gson().toJson(copy(exportedUtc = utc)).toByteArray(Charsets.UTF_8)

    companion object {
        fun start(utc: String = Instant.now().toString(), id: String = UUID.randomUUID().toString()) =
            PairingObservationReportV1(reportId = id, startedUtc = utc)
    }
}
