package de.larpescape.protocol

import org.junit.Assert.*
import org.junit.Test

class DiagnosticDevelopmentLedStepV1Test {
    private val request = DiagnosticDevelopmentLedStepRequestV1(
        7, "5A" + "00".repeat(15), "0000000000000063",
        "00000000-0000-4000-8000-000000000001",
        "00000000-0000-4000-8000-000000000002", 2)
    private val requestHex = "44470107070000005A0000000000000000000000000000006300000000000000" +
        "0000000000004000800000000000000100000000000040008000000000000002" +
        "02000000E8030000020010000010FA00"
    private val replyHex = requestHex + "640000000000000008020000000000000000020101000000"
    private fun bytes(hex: String) = hex.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private fun rejects(action: () -> Unit) {
        try { action(); fail("Expected invalid development LED-step data") }
        catch (_: IllegalArgumentException) { }
    }

    @Test fun requestMatchesFirmwareVectorAndBounds() {
        assertArrayEquals(bytes(requestHex), DiagnosticDevelopmentLedStepV1.encode(request))
        rejects { DiagnosticDevelopmentLedStepV1.encode(request.copy(index = 23)) }
        rejects { DiagnosticDevelopmentLedStepV1.encode(request.copy(red = 33)) }
        rejects { DiagnosticDevelopmentLedStepV1.encode(request.copy(durationMs = 251)) }
        rejects { DiagnosticDevelopmentLedStepV1.encode(request.copy(red = 0)) }
    }

    @Test fun replyRetainsSoftwareAndPhysicalDistinction() {
        val reply = DiagnosticDevelopmentLedStepV1.correlate(bytes(replyHex), request)
        assertEquals(DiagnosticDevelopmentLedStepOutcomeV1.BLACK_SUBMITTED, reply.outcome)
        assertEquals(2, reply.renderCalls)
        assertTrue(reply.blackSubmitted && reply.timerFired)
        assertFalse(reply.physicalOutputConfirmed)
        assertEquals(520L, reply.endedDeviceMs)
        rejects { DiagnosticDevelopmentLedStepV1.correlate(bytes(replyHex), request.copy(stepId = 3)) }
        rejects { DiagnosticDevelopmentLedStepV1.correlate(bytes(replyHex).copyOf(103), request) }
        rejects { DiagnosticDevelopmentLedStepV1.correlate(bytes(replyHex).apply { this[101] = 1 }, request) }
        rejects { DiagnosticDevelopmentLedStepV1.correlate(bytes(replyHex).apply { this[100] = 0 }, request) }
    }

    @Test fun hardwareUnqualifiedRejectionIsExplicit() {
        val rejected = bytes(replyHex).apply {
            (80..103).forEach { this[it] = 0 }
            this[96] = 2; this[97] = 1
        }
        val answer = DiagnosticDevelopmentLedStepV1.correlate(rejected, request)
        assertEquals(DiagnosticDevelopmentLedStepRejectionV1.HARDWARE_UNQUALIFIED, answer.rejection)
        assertFalse(answer.physicalOutputConfirmed)
    }
}
