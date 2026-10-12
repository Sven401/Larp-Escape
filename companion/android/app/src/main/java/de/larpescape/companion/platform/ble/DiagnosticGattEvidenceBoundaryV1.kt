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

    fun cccdWriteAck(observedGeneration: Int, characteristic: UUID): Boolean {
        if (observedGeneration != generation || characteristic !in setOf(route.controlReply, route.stepReply,
                DiagnosticDevelopmentControlCodecV1.reply)) return false
        val key = "cccd:$characteristic"
        if (key in acknowledged) return false
        if (!emit(DiagnosticConnectionEventV1.Kind.CCCD_WRITE_ACK, null,
                "characteristic=$characteristic;att_write_ack_only;subscription_unproven")) return false
        acknowledged.add(key)
        return true
    }

    fun transportProbe(observedGeneration: Int, characteristic: UUID): Boolean {
        if (observedGeneration != generation || characteristic !in setOf(route.controlReply, route.stepReply,
                DiagnosticDevelopmentControlCodecV1.reply)) return false
        val key = "probe:$characteristic"
        if (key in acknowledged) return false
        if (!emit(DiagnosticConnectionEventV1.Kind.TRANSPORT_PROBE, null,
                "characteristic=$characteristic;notification_received;transport_only")) return false
        acknowledged.add(key)
        return true
    }

    fun transportProbeComplete(observedGeneration: Int): Boolean {
        if (observedGeneration != generation || "probe:complete" in acknowledged) return false
        if (!emit(DiagnosticConnectionEventV1.Kind.TRANSPORT_PROBE_COMPLETE, null,
                "control_and_step_notification_received;transport_only;role_unverified;physical_unverified")) return false
        acknowledged.add("probe:complete")
        return true
    }

    fun developmentInfo(observedGeneration: Int, characteristic: UUID,
                        info: DiagnosticDevelopmentInfoV1): Boolean {
        if (observedGeneration != generation ||
            characteristic != DiagnosticDevelopmentInfoCodecV1.characteristic ||
            "development-info" in acknowledged) return false
        if (!emit(DiagnosticConnectionEventV1.Kind.DEVELOPMENT_INFO, null,
                "device=${info.deviceId};boot=${info.bootId};image_identity_only;physical_unverified")) return false
        acknowledged.add("development-info")
        return true
    }

    fun writeAck(observedGeneration: Int, characteristic: UUID, message: Long): Boolean {
        if (observedGeneration != generation || characteristic !in setOf(route.controlRequest, route.stepRequest,
                DiagnosticDevelopmentControlCodecV1.write)) return false
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

    fun developmentControlFinal(observedGeneration: Int, characteristic: UUID,
                                info: DiagnosticDevelopmentInfoV1,
                                request: DiagnosticControlRequestV1,
                                reply: DiagnosticControlReplyV1): Boolean {
        if (observedGeneration != generation ||
            characteristic != DiagnosticDevelopmentControlCodecV1.reply ||
            reply.owner != 4 || reply.command != request.command ||
            reply.message != request.message || reply.deviceId != info.deviceId ||
            reply.bootId != info.bootId || reply.runId != request.runId)
            return false
        val key = "development-control:${request.message}"
        if (key in terminal) return false
        if (!emit(DiagnosticConnectionEventV1.Kind.CONTROL_REPLY, request.message,
                "outcome=${reply.outcome};command=${reply.command};development_owner;wire_correlated;physical_unverified")) return false
        terminal.add(key)
        return true
    }

    fun developmentRawFinal(observedGeneration: Int, characteristic: UUID,
                            request: DiagnosticDevelopmentRawStepRequestV1,
                            reply: DiagnosticDevelopmentRawStepReplyV1): Boolean {
        if (observedGeneration != generation ||
            characteristic != DiagnosticDevelopmentControlCodecV1.reply ||
            reply.request != request || reply.physicalChannelQualified)
            return false
        val key = "development-raw:${request.message}"
        if (key in terminal) return false
        if (!emit(DiagnosticConnectionEventV1.Kind.STEP_REPLY, request.message,
                "step=${request.stepId};outcome=${reply.outcome};i2c=${reply.i2cStatus};" +
                    "rawByte=${reply.rawByte ?: "unknown"};rawHigh=${reply.rawHigh ?: "unknown"};" +
                    "busCode=${reply.busCode};rejection=${reply.rejection};" +
                    "register=0x20/0x13/bit6;physical_channel_unqualified")) return false
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

    fun transportError(observedGeneration: Int, operation: String, message: Long?, reason: String,
                       pendingPhase: DiagnosticGattPhaseV1? = null): Boolean {
        if (observedGeneration != generation || operation !in setOf("setup", "control", "step", "link", "development") ||
            reason.length !in 1..100) return false
        val key = "error:$operation:$message"
        if (key in terminal) return false
        if (!emit(DiagnosticConnectionEventV1.Kind.TRANSPORT_ERROR, message,
                "operation=$operation;reason=$reason" +
                    (if (operation == "setup" && pendingPhase != null) ";phase=${pendingPhase.name}" else ""))) return false
        terminal.add(key)
        return true
    }
}
