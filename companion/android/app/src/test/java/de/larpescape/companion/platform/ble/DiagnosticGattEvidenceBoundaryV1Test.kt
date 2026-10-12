package de.larpescape.companion.platform.ble

import de.larpescape.companion.data.diagnostic_observation.DiagnosticConnectionEventV1
import de.larpescape.protocol.*
import org.junit.Assert.*
import org.junit.Test
import java.util.UUID

class DiagnosticGattEvidenceBoundaryV1Test {
    private val route = DiagnosticGattFirmwareV1.route
    private val device = "1234567890ABCDEF1234567890ABCDEF"
    private val boot = "0000000000000001"
    private val run = "12345678-1234-4123-8123-123456789abc"
    private val grant = "12345678-1234-4123-8123-123456789abd"
    private val origin = DiagnosticControlOriginV1(7, 1, 1, 1, 1, true)

    @Test fun developmentIdentityIsRecordedOnceForTheCurrentGenerationOnly() {
        val received = mutableListOf<DiagnosticConnectionEventV1>()
        val gate = DiagnosticGattEvidenceBoundaryV1(7, "A8:42:E3:CD:4B:18") { received.add(it); true }
        val info = DiagnosticDevelopmentInfoV1(device, boot)
        assertFalse(gate.developmentInfo(6, DiagnosticDevelopmentInfoCodecV1.characteristic, info))
        assertFalse(gate.developmentInfo(7, route.controlReply, info))
        assertTrue(gate.developmentInfo(7, DiagnosticDevelopmentInfoCodecV1.characteristic, info))
        assertFalse(gate.developmentInfo(7, DiagnosticDevelopmentInfoCodecV1.characteristic, info))
        assertEquals(DiagnosticConnectionEventV1.Kind.DEVELOPMENT_INFO, received.single().kind)
        assertTrue(received.single().detail.contains("physical_unverified"))
    }

    private fun controlReply(request: DiagnosticControlRequestV1): DiagnosticControlReplyV1 {
        val bytes = ByteArray(DiagnosticControlV1.REPLY_BYTES)
        val encoded = DiagnosticControlV1.encode(request)
        encoded.copyInto(bytes, 0, 0, 64)
        bytes[3] = 0x81.toByte()
        bytes[64] = 1 // PERMISSION_DENIED is still a final wire reply, never a grant.
        bytes[65] = 3; bytes[66] = 1; bytes[67] = 0
        bytes[84] = 0x60; bytes[85] = 0xEA.toByte()
        return DiagnosticControlV1.reply(bytes)
    }

    private fun stepReply(request: DiagnosticStepRequestV1): DiagnosticStepReplyV1 {
        val bytes = ByteArray(DiagnosticStepV1.REPLY_HEADER_BYTES)
        DiagnosticStepV1.encode(request).copyInto(bytes)
        bytes[80] = 10; bytes[88] = 110.toByte(); bytes[89] = 2 // 622 ms
        bytes[96] = DiagnosticStepOutcomeV1.COMPLETED.ordinal.toByte()
        bytes[97] = DiagnosticStepReasonV1.NONE.ordinal.toByte()
        bytes[99] = 1; bytes[100] = 2; bytes[103] = 1
        return DiagnosticStepV1.reply(bytes)
    }

    @Test fun staleGenerationWrongCharacteristicOriginAndCorrelationAreIgnored() {
        val received = mutableListOf<DiagnosticConnectionEventV1>()
        val gate = DiagnosticGattEvidenceBoundaryV1(7, "A8:42:E3:CD:4B:18") { received.add(it); true }
        val request = DiagnosticControlRequestV1(DiagnosticControlCommandV1.CAPABILITY, 8, device, boot)
        val reply = controlReply(request)
        assertFalse(gate.controlFinal(6, route.controlReply, origin, origin, request, reply))
        assertFalse(gate.controlFinal(7, route.stepReply, origin, origin, request, reply))
        assertFalse(gate.controlFinal(7, route.controlReply, origin, origin.copy(authorityEpoch = 2), request, reply))
        assertFalse(gate.controlFinal(7, route.controlReply, origin, origin, request.copy(message = 9), reply))
        assertTrue(received.isEmpty())
        assertTrue(gate.controlFinal(7, route.controlReply, origin, origin, request, reply))
        assertFalse(gate.controlFinal(7, route.controlReply, origin, origin, request, reply))
        assertEquals(listOf(DiagnosticConnectionEventV1.Kind.CONTROL_REPLY), received.map { it.kind })
    }

    @Test fun stepReplyAndWriteAckRemainWireEvidenceOnly() {
        val received = mutableListOf<DiagnosticConnectionEventV1>()
        val gate = DiagnosticGattEvidenceBoundaryV1(7, "A8:42:E3:CD:4B:18") { received.add(it); true }
        val request = DiagnosticStepRequestV1(12, device, boot, run, grant, 1, 1000, DiagnosticStepOperationV1.LED_SINGLE)
        val reply = stepReply(request)
        assertTrue(gate.writeAck(7, route.stepRequest, 12))
        assertFalse(gate.writeAck(7, route.stepRequest, 12))
        assertEquals(DiagnosticConnectionEventV1.Kind.WRITE_ACK, received.single().kind)
        assertEquals("ATT_WRITE_ACK_ONLY", received.single().detail)
        assertFalse(gate.stepFinal(7, UUID.randomUUID(), origin, origin, request, reply))
        assertFalse(gate.stepFinal(6, route.stepReply, origin, origin, request, reply))
        assertFalse(gate.stepFinal(7, route.stepReply, origin, origin, request.copy(stepId = 2), reply))
        assertTrue(gate.stepFinal(7, route.stepReply, origin, origin, request, reply))
        assertFalse(gate.stepFinal(7, route.stepReply, origin, origin, request, reply))
        assertTrue(received.last().detail.contains("physical_unverified"))
    }

    @Test fun timeoutDisconnectAndWriteFailureAreTerminalTransportEvidenceOnly() {
        val received = mutableListOf<DiagnosticConnectionEventV1>()
        val gate = DiagnosticGattEvidenceBoundaryV1(7, "A8:42:E3:CD:4B:18") { received.add(it); true }
        assertFalse(gate.transportError(6, "control", 1, "TIMEOUT"))
        assertFalse(gate.transportError(7, "invalid", 1, "TIMEOUT"))
        assertTrue(gate.transportError(7, "control", 1, "TIMEOUT"))
        assertFalse(gate.transportError(7, "control", 1, "DISCONNECTED"))
        assertTrue(gate.transportError(7, "step", 2, "ATT_WRITE_FAILED"))
        assertTrue(gate.transportError(7, "link", null, "DISCONNECTED"))
        assertEquals(3, received.size)
        assertTrue(received.all { it.kind == DiagnosticConnectionEventV1.Kind.TRANSPORT_ERROR })
    }

    @Test fun setupTimeoutRecordsPendingPhaseWithoutGrantingAuthority() {
        val received = mutableListOf<DiagnosticConnectionEventV1>()
        val gate = DiagnosticGattEvidenceBoundaryV1(7, "A8:42:E3:CD:4B:1A") { received.add(it); true }
        assertTrue(gate.transportError(7, "setup", null, "SETUP_TIMEOUT", DiagnosticGattPhaseV1.MTU_PENDING))
        assertEquals("operation=setup;reason=SETUP_TIMEOUT;phase=MTU_PENDING", received.single().detail)
        assertEquals(DiagnosticConnectionEventV1.Kind.TRANSPORT_ERROR, received.single().kind)
    }

    @Test fun cccdAckIsRecordedOnlyForExpectedReplyDescriptorsAndIsNotSubscriptionProof() {
        val received = mutableListOf<DiagnosticConnectionEventV1>()
        val gate = DiagnosticGattEvidenceBoundaryV1(7, "A8:42:E3:CD:4B:1A") { received.add(it); true }
        assertFalse(gate.cccdWriteAck(6, route.controlReply))
        assertFalse(gate.cccdWriteAck(7, route.controlRequest))
        assertTrue(gate.cccdWriteAck(7, route.controlReply))
        assertFalse(gate.cccdWriteAck(7, route.controlReply))
        assertTrue(gate.cccdWriteAck(7, route.stepReply))
        assertEquals(2, received.size)
        assertTrue(received.all { it.kind == DiagnosticConnectionEventV1.Kind.CCCD_WRITE_ACK &&
            it.detail.endsWith("subscription_unproven") && it.message == null })
    }

    @Test fun transportProbeEvidenceIsDistinctFromCccdAckAndNeverPhysicalProof() {
        val received = mutableListOf<DiagnosticConnectionEventV1>()
        val gate = DiagnosticGattEvidenceBoundaryV1(7, "A8:42:E3:CD:4B:1A") { received.add(it); true }
        assertFalse(gate.transportProbe(6, route.controlReply))
        assertFalse(gate.transportProbe(7, route.controlRequest))
        assertTrue(gate.cccdWriteAck(7, route.controlReply))
        assertTrue(gate.transportProbe(7, route.controlReply))
        assertFalse(gate.transportProbe(7, route.controlReply))
        assertTrue(gate.cccdWriteAck(7, route.stepReply))
        assertTrue(gate.transportProbe(7, route.stepReply))
        assertTrue(gate.transportProbeComplete(7))
        assertFalse(gate.transportProbeComplete(7))
        assertEquals(listOf(DiagnosticConnectionEventV1.Kind.CCCD_WRITE_ACK,
            DiagnosticConnectionEventV1.Kind.TRANSPORT_PROBE,
            DiagnosticConnectionEventV1.Kind.CCCD_WRITE_ACK,
            DiagnosticConnectionEventV1.Kind.TRANSPORT_PROBE,
            DiagnosticConnectionEventV1.Kind.TRANSPORT_PROBE_COMPLETE), received.map { it.kind })
        assertTrue(received.last().detail.contains("physical_unverified"))
    }

    @Test fun rejectedCccdEvidenceCanBeRetriedWithoutASequenceGap() {
        val received = mutableListOf<DiagnosticConnectionEventV1>()
        var persist = false
        val gate = DiagnosticGattEvidenceBoundaryV1(7, "A8:42:E3:CD:4B:1A") {
            if (!persist) false else { received.add(it); true }
        }
        assertFalse(gate.cccdWriteAck(7, route.controlReply))
        assertTrue(received.isEmpty())
        persist = true
        assertTrue(gate.cccdWriteAck(7, route.controlReply))
        assertEquals(1, received.single().sequence)
        assertFalse(gate.cccdWriteAck(7, route.controlReply))
    }
}
