package de.larpescape.companion.data.device

import com.google.gson.Gson
import java.time.Instant
import java.util.UUID

enum class BondRequestDecisionV1 { ALREADY_BONDED, IN_PROGRESS, REQUEST }

fun bondRequestDecisionV1(observedOsState: String) = when (observedOsState) {
    "OS_BONDED" -> BondRequestDecisionV1.ALREADY_BONDED
    "OS_BONDING" -> BondRequestDecisionV1.IN_PROGRESS
    else -> BondRequestDecisionV1.REQUEST
}

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
    val previousBondFailureSequences: List<Int> = emptyList(),
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
    fun observe(utc: String, source: String, generation: Int?, target: String?, kind: String, value: String): PairingObservationReportV1 {
        if (events.size >= MAX_EVENTS) return this
        val lastAttempt = events.lastOrNull { it.kind == "connection_attempt" }
        val capped = events.size == MAX_EVENTS - 1
        val next = PairingObservationEventV1((events.lastOrNull()?.sequence ?: 0) + 1, utc,
            if (capped) "ANDROID_APP_JOURNAL" else source,
            if (capped) lastAttempt?.connectionGeneration else generation,
            if (capped) lastAttempt?.targetAddress else target,
            if (capped) "connection_closed" else kind,
            if (capped) "journal_limit_reached;start_new_report" else value)
        return copy(events = events + next)
    }

    fun closeRecoveredSession(utc: String): PairingObservationReportV1 {
        val attempt = events.lastOrNull { it.kind == "connection_attempt" }
        if (events.lastOrNull()?.kind == "connection_closed" && events.last().value == "journal_limit_reached;start_new_report")
            return this
        val closure = PairingObservationEventV1((events.lastOrNull()?.sequence ?: 0) + 1, utc,
            "ANDROID_APP_RECOVERY", attempt?.connectionGeneration, attempt?.targetAddress,
            "connection_closed", "process_restarted;live_link_unverified")
        return if (events.size < MAX_EVENTS) copy(events = events + closure)
            else copy(events = events.dropLast(1) + closure.copy(sequence = events.last().sequence))
    }

    fun androidAssessment(): PairingAndroidAssessmentV1 {
        val attempt = events.lastOrNull { it.kind == "connection_attempt" && it.connectionGeneration != null && it.targetAddress != null }
            ?: return PairingAndroidAssessmentV1("NO_ATTEMPT", "Noch kein Verbindungsversuch beobachtet", null, null, emptyList())
        val scope = events.filter {
            it.sequence >= attempt.sequence && it.connectionGeneration == attempt.connectionGeneration &&
                it.targetAddress == attempt.targetAddress
        }
        val priorFailures = events.filter { it.kind == "connection_attempt" && it.sequence < attempt.sequence &&
            it.targetAddress == attempt.targetAddress && it.connectionGeneration != null }
            .mapNotNull { priorAttempt ->
                bondFailureEvidence(events.filter { it.sequence >= priorAttempt.sequence && it.sequence < attempt.sequence &&
                    it.connectionGeneration == priorAttempt.connectionGeneration &&
                    it.targetAddress == priorAttempt.targetAddress })
            }.lastOrNull().orEmpty().map { it.sequence }
        fun result(code: String, label: String, vararg evidence: PairingObservationEventV1?) =
            PairingAndroidAssessmentV1(code, label, attempt.connectionGeneration, attempt.targetAddress,
                evidence.filterNotNull().map { it.sequence }, previousBondFailureSequences = priorFailures)
        val connection = scope.lastOrNull { it.kind == "connection_state" }
        val service = scope.lastOrNull { it.kind == "product_service_present" }
        // A missing service is a completed discovery result. The app then
        // closes that GATT link, which must not erase this useful diagnosis.
        if (service?.value == "false")
            return result("SERVICE_MISSING", "Produktdienst bei der Dienstsuche nicht vorhanden",
                attempt, connection, service, scope.lastOrNull { it.kind == "connection_closed" })
        val bondFailure = bondFailureEvidence(scope)
        if (bondFailure != null)
            return result("BOND_FAILED", "Android-Kopplung gescheitert · kein OS-Bond bestätigt",
                attempt, service, *bondFailure.toTypedArray(), connection,
                scope.lastOrNull { it.kind == "connection_closed" })
        if (scope.any { it.kind == "connection_closed" } || connection?.value != "status=0;state=2")
            return result("NOT_CONNECTED", "Keine aktuelle Android-GATT-Verbindung belegt", attempt, connection,
                scope.lastOrNull { it.kind == "connection_closed" })
        if (service?.value != "true")
            return result("SERVICE_OPEN", "Produktdienst noch nicht bestätigt", attempt, connection, service)
        val bond = scope.lastOrNull { it.kind == "bond_state" || it.kind == "bond_request" }
        if (bond?.kind != "bond_state" || bond.value != "OS_BONDED")
            return result("BOND_OPEN", "Android-Bond noch nicht bestätigt", attempt, connection, service, bond)
        return result("ANDROID_PAIRING_OBSERVED", "Android-Kopplung beobachtet · Firmware-Sicherheitsnachweis offen",
            attempt, connection, service, bond)
    }

    private fun bondFailureEvidence(scope: List<PairingObservationEventV1>): List<PairingObservationEventV1>? {
        val request = scope.lastOrNull { it.kind == "bond_request" && it.value == "OS_REQUEST_ACCEPTED" }
            ?: return null
        val afterRequest = scope.filter { it.sequence > request.sequence }
        val bonding = afterRequest.firstOrNull { it.kind == "bond_state" && it.value == "OS_BONDING" }
            ?: return null
        val lastState = afterRequest.lastOrNull { it.kind == "bond_state" }
        if (lastState?.value != "OS_NOT_BONDED" || lastState.sequence <= bonding.sequence) return null
        return listOf(request, bonding, lastState)
    }

    fun export(utc: String): ByteArray {
        val gson = Gson()
        val json = gson.toJsonTree(copy(exportedUtc = utc)).asJsonObject
        json.add("androidAssessment", gson.toJsonTree(androidAssessment()))
        return gson.toJson(json).toByteArray(Charsets.UTF_8)
    }

    companion object {
        const val MAX_EVENTS = 256
        fun start(utc: String = Instant.now().toString(), id: String = UUID.randomUUID().toString()) =
            PairingObservationReportV1(reportId = id, startedUtc = utc)
    }
}
