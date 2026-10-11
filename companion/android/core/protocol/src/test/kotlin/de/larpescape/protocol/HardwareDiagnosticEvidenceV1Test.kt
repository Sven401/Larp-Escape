package de.larpescape.protocol

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import java.util.UUID

class HardwareDiagnosticEvidenceV1Test {
    private val runId = "11111111-1111-4111-8111-111111111111"
    private val requestId = "22222222-2222-4222-8222-222222222222"
    private val start = "2026-10-11T20:00:00Z"
    private val end = "2026-10-11T20:05:00Z"

    @Test
    fun exportContainsEveryRequiredActionInStableOrderEvenWhenNotTested() {
        val report = HardwareDiagnosticEvidenceLedgerV1(runId, start).finish(end)
        assertEquals(HardwareDiagnosticActionV1.entries, report.actions.map { it.action })
        assertTrue(report.actions.all { it.attempts.isEmpty() })
    }

    @Test
    fun requestLocalClaimsAndUserObservationRemainSeparate() {
        val ledger = HardwareDiagnosticEvidenceLedgerV1(runId, start)
        ledger.request(HardwareDiagnosticActionV1.LED_START, requestId, "2026-10-11T20:00:01Z")
        ledger.recordAcceptanceClaim(HardwareDiagnosticActionV1.LED_START, requestId, DeviceAcceptanceV1.ACCEPTED, 100, "local claim")
        ledger.recordExecutionClaim(HardwareDiagnosticActionV1.LED_START, requestId, DeviceExecutionV1.EXECUTED, 110, "render submitted")
        ledger.observe(HardwareDiagnosticActionV1.LED_START, requestId, UserObservationV1.MATCHES, "2026-10-11T20:00:02Z", "effect visible")
        val report = ledger.finish(end)
        val evidence = report.actions.single { it.action == HardwareDiagnosticActionV1.LED_START }.attempts.single()
        assertEquals(requestId, evidence.request.correlationId)
        assertEquals(DeviceAcceptanceV1.ACCEPTED, evidence.acceptanceClaim!!.state)
        assertEquals(DeviceExecutionV1.EXECUTED, evidence.executionClaim!!.state)
        assertEquals(UserObservationV1.MATCHES, evidence.observation!!.state)
        val encoded = report.snapshot().getAsJsonArray("actions")
            .first { it.asJsonObject["id"].asString == "led_start" }.asJsonObject
            .getAsJsonArray("attempts")[0].asJsonObject
        assertEquals("unverified_local", encoded.getAsJsonObject("acceptanceClaim")["provenance"].asString)
        assertEquals("unverified_local", encoded.getAsJsonObject("executionClaim")["provenance"].asString)
    }

    @Test
    fun repeatableMeasurementsKeepSeparateCorrelationsForAllMatrixPointsAndColdStarts() {
        val ledger = HardwareDiagnosticEvidenceLedgerV1(runId, start)
        repeat(81) { index ->
            val id = UUID.nameUUIDFromBytes("matrix-$index".toByteArray()).toString()
            ledger.request(HardwareDiagnosticActionV1.MATRIX_VALIDATE_81, id, "2026-10-11T20:00:01Z")
            ledger.recordAcceptanceClaim(HardwareDiagnosticActionV1.MATRIX_VALIDATE_81, id, DeviceAcceptanceV1.ACCEPTED, index.toLong(), "raw point $index")
            ledger.recordExecutionClaim(HardwareDiagnosticActionV1.MATRIX_VALIDATE_81, id, DeviceExecutionV1.MEASURED, index.toLong(), "point $index measured")
        }
        repeat(3) { index ->
            val id = UUID.nameUUIDFromBytes("cold-$index".toByteArray()).toString()
            ledger.request(HardwareDiagnosticActionV1.COMPLETION_THREE_COLD_STARTS, id, "2026-10-11T20:00:02Z")
            ledger.recordAcceptanceClaim(HardwareDiagnosticActionV1.COMPLETION_THREE_COLD_STARTS, id, DeviceAcceptanceV1.ACCEPTED, index.toLong(), "start $index")
            ledger.recordExecutionClaim(HardwareDiagnosticActionV1.COMPLETION_THREE_COLD_STARTS, id, DeviceExecutionV1.MEASURED, index.toLong(), "boot $index")
        }
        val report = HardwareDiagnosticReportCodecV1.import(HardwareDiagnosticReportCodecV1.export(ledger.finish(end)))
        assertEquals(81, report.actions.single { it.action == HardwareDiagnosticActionV1.MATRIX_VALIDATE_81 }.attempts.size)
        assertEquals(3, report.actions.single { it.action == HardwareDiagnosticActionV1.COMPLETION_THREE_COLD_STARTS }.attempts.size)
    }

    @Test
    fun unclearWriteBlocksAnotherAttemptButRejectedAttemptMayBeRetriedExplicitly() {
        val ledger = HardwareDiagnosticEvidenceLedgerV1(runId, start)
        ledger.request(HardwareDiagnosticActionV1.NFC_BOX_WRITE_PHONE_READ, requestId, "2026-10-11T20:00:01Z")
        ledger.recordAcceptanceClaim(HardwareDiagnosticActionV1.NFC_BOX_WRITE_PHONE_READ, requestId, DeviceAcceptanceV1.UNCLEAR, null, "link lost after write")
        rejects { ledger.request(HardwareDiagnosticActionV1.NFC_BOX_WRITE_PHONE_READ, "33333333-3333-4333-8333-333333333333", "2026-10-11T20:00:02Z") }
        rejects { ledger.recordExecutionClaim(HardwareDiagnosticActionV1.NFC_BOX_WRITE_PHONE_READ, requestId, DeviceExecutionV1.EXECUTED, 5, "guessed") }

        val other = HardwareDiagnosticEvidenceLedgerV1("44444444-4444-4444-8444-444444444444", start)
        other.request(HardwareDiagnosticActionV1.LED_START, requestId, "2026-10-11T20:00:01Z")
        other.recordAcceptanceClaim(HardwareDiagnosticActionV1.LED_START, requestId, DeviceAcceptanceV1.REJECTED, 1, "not started")
        other.request(HardwareDiagnosticActionV1.LED_START, "33333333-3333-4333-8333-333333333333", "2026-10-11T20:00:02Z")
        assertEquals(2, other.finish(end).actions.single { it.action == HardwareDiagnosticActionV1.LED_START }.attempts.size)
    }

    @Test
    fun exportIsCanonicalReplayableAndRejectsFalseDeviceProvenance() {
        val ledger = HardwareDiagnosticEvidenceLedgerV1(runId, start)
        ledger.request(HardwareDiagnosticActionV1.WAKE_EDGE, requestId, "2026-10-11T20:00:01Z")
        ledger.recordAcceptanceClaim(HardwareDiagnosticActionV1.WAKE_EDGE, requestId, DeviceAcceptanceV1.ACCEPTED, 10, "capture armed")
        ledger.recordExecutionClaim(HardwareDiagnosticActionV1.WAKE_EDGE, requestId, DeviceExecutionV1.MEASURED, 20, "rawHigh=false")
        ledger.observe(HardwareDiagnosticActionV1.WAKE_EDGE, requestId, UserObservationV1.NOT_OBSERVED, "2026-10-11T20:00:03Z", "physical run not started")
        val report = ledger.finish(end)
        val bytes = HardwareDiagnosticReportCodecV1.export(report)
        assertArrayEquals(bytes, HardwareDiagnosticReportCodecV1.export(HardwareDiagnosticReportCodecV1.import(bytes)))
        val changedEnvelope = DiagnosticJsonV1.parse(bytes).asJsonObject
        changedEnvelope.getAsJsonObject("report").addProperty("endedUtc", "2026-10-11T20:06:00Z")
        rejects { HardwareDiagnosticReportCodecV1.import(DiagnosticJsonV1.canonical(changedEnvelope)) }
        val forged = report.snapshot()
        val attempt = forged.getAsJsonArray("actions").first { it.asJsonObject["id"].asString == "wake_edge" }
            .asJsonObject.getAsJsonArray("attempts")[0].asJsonObject
        attempt.getAsJsonObject("acceptanceClaim").addProperty("provenance", "authenticated_device")
        val forgedEnvelope = com.google.gson.JsonObject().apply {
            add("report", forged)
            addProperty("sha256", DiagnosticJsonV1.sha256(DiagnosticJsonV1.canonical(forged)))
        }
        rejects { HardwareDiagnosticReportCodecV1.import(DiagnosticJsonV1.canonical(forgedEnvelope)) }
    }

    @Test
    fun duplicateCorrelationAndBackdatedDeviceExecutionAreRejected() {
        val ledger = HardwareDiagnosticEvidenceLedgerV1(runId, start)
        ledger.request(HardwareDiagnosticActionV1.WAKE_EDGE, requestId, "2026-10-11T20:00:01Z")
        rejects { ledger.request(HardwareDiagnosticActionV1.AUDIO_PLAY, requestId, "2026-10-11T20:00:02Z") }
        ledger.recordAcceptanceClaim(HardwareDiagnosticActionV1.WAKE_EDGE, requestId, DeviceAcceptanceV1.ACCEPTED, 20, "accepted")
        rejects { ledger.recordExecutionClaim(HardwareDiagnosticActionV1.WAKE_EDGE, requestId, DeviceExecutionV1.MEASURED, 10, "before acceptance") }
    }

    @Test
    fun finalizedLedgerIsImmutableAndRepeatedFinishReturnsSameReport() {
        val ledger = HardwareDiagnosticEvidenceLedgerV1(runId, start)
        val first = ledger.finish(end)
        val second = ledger.finish("2026-10-11T20:06:00Z")
        assertArrayEquals(HardwareDiagnosticReportCodecV1.export(first), HardwareDiagnosticReportCodecV1.export(second))
        rejects { ledger.request(HardwareDiagnosticActionV1.AUDIO_PLAY, requestId, "2026-10-11T20:00:01Z") }
        assertNull(first.actions.single { it.action == HardwareDiagnosticActionV1.AUDIO_PLAY }.attempts.firstOrNull())
    }

    private fun rejects(block: () -> Unit) {
        try { block(); fail("Expected rejection") } catch (_: IllegalArgumentException) { }
    }
}
