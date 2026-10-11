package de.larpescape.companion.platform.ble

import de.larpescape.companion.data.diagnostic_observation.DiagnosticConnectionEventV1
import de.larpescape.protocol.*
import java.time.Instant
import java.util.UUID

/** A final wire answer enters the sink only after the live GATT shell has completed its checks. */
internal class DiagnosticGattEvidenceBoundaryV1(
    private val generation: Int,
    private val address: String,
    private val sink: (DiagnosticConnectionEventV1) -> Boolean,
) {
    private val route = DiagnosticGattFirmwareV1.route
    private val terminal = HashSet<String>()
    private val acknowledged = HashSet<String>()
    private var sequence = 0

    private fun emit(kind: DiagnosticConnectionEventV1.Kind, message: Long?, detail: String): Boolean {
        val record = DiagnosticConnectionEventV1(sequence + 1, Instant.now().toString(), generation,
            address, kind, message, detail)
        if (!sink(record)) return false
        sequence++
        return true
    }

    fun writeAck(observedGeneration: Int, characteristic: UUID, message: Long): Boolean {
        if (observedGeneration != generation || characteristic !in setOf(route.controlRequest, route.stepRequest)) return false
        val key = "$characteristic:$message"
        if (key in acknowledged) return false
        if (!emit(DiagnosticConnectionEventV1.Kind.WRITE_ACK, message, "ATT_WRITE_ACK_ONLY")) return false
        acknowledged.add(key)
        return true
    }

    fun controlFinal(observedGeneration: Int, characteristic: UUID,
                     captured: DiagnosticControlOriginV1, current: DiagnosticControlOriginV1?,
                     request: DiagnosticControlRequestV1, reply: DiagnosticControlReplyV1): Boolean {
        if (observedGeneration != generation || characteristic != route.controlReply || current != captured ||
            captured.localGattGeneration != generation.toLong() ||
            reply.command != request.command || reply.message != request.message ||
            reply.deviceId != request.deviceId || reply.bootId != request.bootId ||
            reply.runId != request.runId) return false
        val key = "control:${request.message}"
        if (key in terminal) return false
        if (!emit(DiagnosticConnectionEventV1.Kind.CONTROL_REPLY, request.message,
                "outcome=${reply.outcome};command=${reply.command};wire_correlated;physical_unverified")) return false
        terminal.add(key)
        return true
    }

    fun stepFinal(observedGeneration: Int, characteristic: UUID,
                  captured: DiagnosticControlOriginV1, current: DiagnosticControlOriginV1?,
                  request: DiagnosticStepRequestV1, reply: DiagnosticStepReplyV1): Boolean {
        if (observedGeneration != generation || characteristic != route.stepReply || current != captured ||
            captured.localGattGeneration != generation.toLong() || reply.request != request) return false
        val key = "step:${request.message}"
        if (key in terminal) return false
        if (!emit(DiagnosticConnectionEventV1.Kind.STEP_REPLY, request.message,
                "outcome=${reply.outcome};reason=${reply.reason};reportedOff=${reply.restoredOff};physical_unverified")) return false
        terminal.add(key)
        return true
    }

    fun transportError(observedGeneration: Int, operation: String, message: Long?, reason: String): Boolean {
        if (observedGeneration != generation || operation !in setOf("setup", "control", "step", "link") ||
            reason.length !in 1..100) return false
        val key = "error:$operation:$message"
        if (key in terminal) return false
        if (!emit(DiagnosticConnectionEventV1.Kind.TRANSPORT_ERROR, message,
                "operation=$operation;reason=$reason")) return false
        terminal.add(key)
        return true
    }
}
