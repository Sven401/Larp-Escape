package de.larpescape.protocol

import org.junit.Assert.*
import org.junit.Test

class DiagnosticDevelopmentRawStepV1Test {
    private val request = DiagnosticDevelopmentRawStepRequestV1(
        7, "5A" + "00".repeat(15), "0000000000000063",
        "00000000-0000-4000-8000-000000000001",
        "00000000-0000-4000-8000-000000000002", 1)
    private val requestHex = "44470106070000005A0000000000000000000000000000006300000000000000" +
        "0000000000004000800000000000000100000000000040008000000000000002" +
        "01000000E80300000120130600000000"
    private val replyHex = requestHex + "640000000000000069000000000000000000400100000000"
    private fun bytes(hex: String) = hex.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private fun rejects(action: () -> Unit) {
        try { action(); fail("Expected invalid development raw-step data") }
        catch (_: IllegalArgumentException) { }
    }

    @Test fun requestMatchesIndependentFixedWidthVector() {
        assertArrayEquals(bytes(requestHex), DiagnosticDevelopmentRawStepV1.encode(request))
        assertEquals(80, DiagnosticDevelopmentRawStepV1.encode(request).size)
        rejects { DiagnosticDevelopmentRawStepV1.encode(request.copy(timeoutMs = 999)) }
        rejects { DiagnosticDevelopmentRawStepV1.encode(request.copy(stepId = 0)) }
    }

    @Test fun responseSeparatesRawRegisterValueFromPhysicalQualification() {
        val reply = DiagnosticDevelopmentRawStepV1.correlate(bytes(replyHex), request)
        assertEquals(DiagnosticDevelopmentRawStepOutcomeV1.REGISTER_OBSERVED, reply.outcome)
        assertEquals(DiagnosticDevelopmentRawStepRejectionV1.NONE, reply.rejection)
        assertEquals(DiagnosticDevelopmentRawI2cStatusV1.VALID, reply.i2cStatus)
        assertEquals(0x40, reply.rawByte)
        assertEquals(true, reply.rawHigh)
        assertFalse(reply.physicalChannelQualified)
        assertEquals(100L, reply.startedDeviceMs)
        assertEquals(105L, reply.endedDeviceMs)
    }

    @Test fun correlationAndBusFailuresRemainStrict() {
        val valid = bytes(replyHex)
        rejects { DiagnosticDevelopmentRawStepV1.correlate(valid, request.copy(stepId = 2)) }
        rejects { DiagnosticDevelopmentRawStepV1.correlate(valid.copyOf(103), request) }
        rejects { DiagnosticDevelopmentRawStepV1.correlate(valid.copyOf().apply { this[101] = 1 }, request) }
        rejects { DiagnosticDevelopmentRawStepV1.correlate(valid.copyOf().apply { this[99] = 0 }, request) }
        val failed = valid.copyOf().apply {
            this[96] = 1; this[97] = 1; this[98] = 0; this[99] = 0; this[100] = 2
        }
        val result = DiagnosticDevelopmentRawStepV1.correlate(failed, request)
        assertEquals(DiagnosticDevelopmentRawStepOutcomeV1.I2C_FAILED, result.outcome)
        assertNull(result.rawByte)
        assertNull(result.rawHigh)
        assertEquals(2, result.busCode)
        val rejected = valid.copyOf().apply {
            (80..95).forEach { this[it] = 0 }
            this[96] = 2; this[97] = 3; this[98] = 0; this[99] = 0; this[102] = 1
        }
        assertEquals(DiagnosticDevelopmentRawStepRejectionV1.PERMISSION,
            DiagnosticDevelopmentRawStepV1.correlate(rejected, request).rejection)
        rejects { DiagnosticDevelopmentRawStepV1.correlate(rejected.copyOf().apply { this[102] = 0 }, request) }
    }
}
