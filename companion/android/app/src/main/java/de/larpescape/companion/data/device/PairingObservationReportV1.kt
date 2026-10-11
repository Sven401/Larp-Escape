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

data class PairingAndroidAssessmentV1(
    val code: String,
    val label: String,
    val connectionGeneration: Int?,
    val targetAddress: String?,
    val evidenceSequences: List<Int>,
    val firmwareSecurityEvidence: String = "OPEN",
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

    fun androidAssessment(): PairingAndroidAssessmentV1 {
        val attempt = events.lastOrNull { it.kind == "connection_attempt" && it.connectionGeneration != null && it.targetAddress != null }
            ?: return PairingAndroidAssessmentV1("NO_ATTEMPT", "Noch kein Verbindungsversuch beobachtet", null, null, emptyList())
        val scope = events.filter {
            it.sequence >= attempt.sequence && it.connectionGeneration == attempt.connectionGeneration &&
                it.targetAddress == attempt.targetAddress
        }
        fun result(code: String, label: String, vararg evidence: PairingObservationEventV1?) =
            PairingAndroidAssessmentV1(code, label, attempt.connectionGeneration, attempt.targetAddress,
                evidence.filterNotNull().map { it.sequence })
        val connection = scope.lastOrNull { it.kind == "connection_state" }
        if (scope.any { it.kind == "connection_closed" } || connection?.value != "status=0;state=2")
            return result("NOT_CONNECTED", "Keine aktuelle Android-GATT-Verbindung belegt", attempt, connection,
                scope.lastOrNull { it.kind == "connection_closed" })
        val service = scope.lastOrNull { it.kind == "product_service_present" }
        if (service?.value == "false")
            return result("SERVICE_MISSING", "Produktdienst auf dieser Verbindung nicht vorhanden", attempt, connection, service)
        if (service?.value != "true")
            return result("SERVICE_OPEN", "Produktdienst noch nicht bestätigt", attempt, connection, service)
        val bond = scope.lastOrNull { it.kind == "bond_state" || it.kind == "bond_request" }
        if (bond?.kind != "bond_state" || bond.value != "OS_BONDED")
            return result("BOND_OPEN", "Android-Bond noch nicht bestätigt", attempt, connection, service, bond)
        return result("ANDROID_PAIRING_OBSERVED", "Android-Kopplung beobachtet · Firmware-Sicherheitsnachweis offen",
            attempt, connection, service, bond)
    }

    fun export(utc: String): ByteArray {
        val gson = Gson()
        val json = gson.toJsonTree(copy(exportedUtc = utc)).asJsonObject
        json.add("androidAssessment", gson.toJsonTree(androidAssessment()))
        return gson.toJson(json).toByteArray(Charsets.UTF_8)
    }

    companion object {
        fun start(utc: String = Instant.now().toString(), id: String = UUID.randomUUID().toString()) =
            PairingObservationReportV1(reportId = id, startedUtc = utc)
    }
}
