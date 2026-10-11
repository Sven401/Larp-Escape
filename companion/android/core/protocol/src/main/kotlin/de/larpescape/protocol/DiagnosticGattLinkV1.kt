package de.larpescape.protocol

import java.util.UUID

enum class DiagnosticGattPhaseV1 { DISCOVERING, MTU_PENDING, CONTROL_CCCD_PENDING, STEP_CCCD_PENDING, TRANSPORT_READY, BLOCKED }
enum class DiagnosticGattFailureV1 {
    NONE, WRONG_SERVICE, MISSING_CHARACTERISTIC, AMBIGUOUS_CHARACTERISTIC, WRONG_PROPERTIES, MISSING_CCCD,
    MTU_REQUEST_FAILED, MTU_TOO_SMALL, MTU_CALLBACK_FAILED, CONTROL_CCCD_FAILED, STEP_CCCD_FAILED,
    SETUP_TIMEOUT, DISCONNECTED,
}

/** One connection generation. It never grants authority to send a diagnostic command. */
class DiagnosticGattLinkV1(val generation: Int) {
    init { require(generation > 0) }
    var phase = DiagnosticGattPhaseV1.DISCOVERING
        private set
    var failure = DiagnosticGattFailureV1.NONE
        private set
    var effectiveAttValueBytes = 0
        private set

    fun discovered(service: UUID?, writes: Set<UUID>, notifies: Set<UUID>, cccds: Set<UUID>): Boolean {
        if (phase != DiagnosticGattPhaseV1.DISCOVERING) return false
        val route = DiagnosticGattFirmwareV1.route
        val reason = when {
            service != route.service -> DiagnosticGattFailureV1.WRONG_SERVICE
            !((writes + notifies).containsAll(setOf(route.controlRequest, route.controlReply, route.stepRequest, route.stepReply))) -> DiagnosticGattFailureV1.MISSING_CHARACTERISTIC
            !writes.containsAll(setOf(route.controlRequest, route.stepRequest)) || !notifies.containsAll(setOf(route.controlReply, route.stepReply)) -> DiagnosticGattFailureV1.WRONG_PROPERTIES
            !cccds.containsAll(setOf(route.controlReply, route.stepReply)) -> DiagnosticGattFailureV1.MISSING_CCCD
            else -> DiagnosticGattFailureV1.NONE
        }
        if (reason != DiagnosticGattFailureV1.NONE) return block(reason)
        phase = DiagnosticGattPhaseV1.MTU_PENDING
        return true
    }

    fun mtu(mtu: Int, success: Boolean): Boolean {
        if (phase != DiagnosticGattPhaseV1.MTU_PENDING) return false
        if (!success) return block(DiagnosticGattFailureV1.MTU_CALLBACK_FAILED)
        if (mtu < DiagnosticGattFirmwareV1.REQUIRED_MTU) return block(DiagnosticGattFailureV1.MTU_TOO_SMALL)
        effectiveAttValueBytes = mtu - 3
        phase = DiagnosticGattPhaseV1.CONTROL_CCCD_PENDING
        return true
    }

    fun subscribed(characteristic: UUID, success: Boolean): Boolean {
        val route = DiagnosticGattFirmwareV1.route
        val expected = when (phase) {
            DiagnosticGattPhaseV1.CONTROL_CCCD_PENDING -> route.controlReply
            DiagnosticGattPhaseV1.STEP_CCCD_PENDING -> route.stepReply
            else -> return false
        }
        if (characteristic != expected) return false
        if (!success) return block(if (expected == route.controlReply) DiagnosticGattFailureV1.CONTROL_CCCD_FAILED else DiagnosticGattFailureV1.STEP_CCCD_FAILED)
        phase = if (expected == route.controlReply) DiagnosticGattPhaseV1.STEP_CCCD_PENDING else DiagnosticGattPhaseV1.TRANSPORT_READY
        return true
    }

    fun failed(reason: DiagnosticGattFailureV1) { require(reason != DiagnosticGattFailureV1.NONE); block(reason) }
    private fun block(reason: DiagnosticGattFailureV1): Boolean {
        phase = DiagnosticGattPhaseV1.BLOCKED; failure = reason; effectiveAttValueBytes = 0; return false
    }
    fun canReceive(generation: Int): Boolean = generation == this.generation && phase == DiagnosticGattPhaseV1.TRANSPORT_READY
    fun preflight(): Set<DiagnosticGattBlockV1> = if (phase == DiagnosticGattPhaseV1.TRANSPORT_READY)
        DiagnosticGattPreflightV1.blocks(DiagnosticGattFirmwareV1.route, DiagnosticGattObservationV1(
            DiagnosticGattFirmwareV1.route.service,
            setOf(DiagnosticGattFirmwareV1.route.controlRequest, DiagnosticGattFirmwareV1.route.stepRequest),
            setOf(DiagnosticGattFirmwareV1.route.controlReply, DiagnosticGattFirmwareV1.route.stepReply),
            effectiveAttValueBytes,
        )) else setOf(DiagnosticGattBlockV1.MISSING_CONNECTION_OBSERVATION)
}
