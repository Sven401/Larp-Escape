package de.larpescape.protocol

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test

class DurableHardwareDiagnosticRunV1Test {
    private val runId = "11111111-1111-4111-8111-111111111111"
    private val firstId = "22222222-2222-4222-8222-222222222222"
    private val secondId = "33333333-3333-4333-8333-333333333333"
    private val start = "2026-10-11T20:00:00Z"
    private val action = HardwareDiagnosticActionV1.NFC_BOX_WRITE_PHONE_READ

    @Test
    fun requestIsCommittedBeforeItCanBeReturnedForSending() {
        val journal = MemoryDiagnosticJournalV1()
        val run = DurableHardwareDiagnosticRunV1.start(runId, start, journal)
        val request = run.request(action, firstId, "2026-10-11T20:00:01Z")
        assertEquals(firstId, request.correlationId)
        val recovered = DurableHardwareDiagnosticRunV1.recover(journal, "2026-10-11T20:00:02Z")
        val attempt = recovered.snapshot().attempt(action)
        assertEquals(firstId, attempt.request.correlationId)
        assertEquals(DeviceAcceptanceV1.UNCLEAR, attempt.acceptanceClaim?.state)
        assertEquals("unverified_local", recovered.snapshot().snapshot().getAsJsonArray("actions")
            .first { it.asJsonObject["id"].asString == action.wireId }.asJsonObject
            .getAsJsonArray("attempts")[0].asJsonObject.getAsJsonObject("acceptanceClaim")["provenance"].asString)
        rejects { recovered.request(action, secondId, "2026-10-11T20:00:03Z") }
    }

    @Test
    fun journalFailureDoesNotPublishAnIntent() {
        val memory = MemoryDiagnosticJournalV1()
        var failWrites = false
        val journal = object : DiagnosticJournalV1 {
            override fun load() = memory.load()
            override fun replace(bytes: ByteArray) {
                if (failWrites) throw IllegalStateException("storage failed")
                memory.replace(bytes)
            }
        }
        val run = DurableHardwareDiagnosticRunV1.start(runId, start, journal)
        failWrites = true
        try { run.request(action, firstId, "2026-10-11T20:00:01Z"); fail("Expected storage failure") }
        catch (_: IllegalStateException) { }
        assertTrue(run.snapshot().attempts(action).isEmpty())
        assertTrue(DurableHardwareDiagnosticRunV1.recover(journal, "2026-10-11T20:00:02Z")
            .snapshot().attempts(action).isEmpty())
    }

    @Test
    fun acceptedButUnfinishedWriteBecomesUnclearOnRestart() {
        val journal = MemoryDiagnosticJournalV1()
        val run = DurableHardwareDiagnosticRunV1.start(runId, start, journal)
        run.request(action, firstId, "2026-10-11T20:00:01Z")
        run.recordAcceptanceClaim(action, firstId, DeviceAcceptanceV1.ACCEPTED, 100, "local claim", "2026-10-11T20:00:02Z")
        val reopened = DurableHardwareDiagnosticRunV1.recover(journal, "2026-10-11T20:00:03Z")
        assertEquals(DeviceExecutionV1.UNCLEAR, reopened.snapshot().attempt(action).executionClaim?.state)
        rejects { reopened.request(action, secondId, "2026-10-11T20:00:04Z") }
        val reopenedAgain = DurableHardwareDiagnosticRunV1.recover(journal, "2026-10-11T20:00:05Z")
        assertEquals(DeviceExecutionV1.UNCLEAR, reopenedAgain.snapshot().attempt(action).executionClaim?.state)
    }

    @Test
    fun finishedRunReopensReadOnlyAndDoesNotOverwriteHistory() {
        val journal = MemoryDiagnosticJournalV1()
        val run = DurableHardwareDiagnosticRunV1.start(runId, start, journal)
        run.request(HardwareDiagnosticActionV1.WAKE_EDGE, firstId, "2026-10-11T20:00:01Z")
        run.recordAcceptanceClaim(HardwareDiagnosticActionV1.WAKE_EDGE, firstId, DeviceAcceptanceV1.REJECTED, null, "not armed", "2026-10-11T20:00:02Z")
        val report = run.finish("2026-10-11T20:00:03Z")
        val before = journal.load()!!
        val reopened = DurableHardwareDiagnosticRunV1.recover(journal, "2026-10-11T20:00:04Z")
        assertTrue(reopened.isFinalized)
        assertEquals(report.snapshot(), reopened.snapshot().snapshot())
        rejects { reopened.request(HardwareDiagnosticActionV1.WAKE_EDGE, secondId, "2026-10-11T20:00:05Z") }
        assertTrue(before.contentEquals(journal.load()))
    }

    @Test
    fun corruptedJournalCannotBeRecoveredOrReplacedByStart() {
        val journal = MemoryDiagnosticJournalV1()
        DurableHardwareDiagnosticRunV1.start(runId, start, journal)
        rejects { DurableHardwareDiagnosticRunV1.start(runId, start, journal) }
        val changed = DiagnosticJsonV1.parse(journal.load()!!).asJsonObject
        changed.getAsJsonObject("report").addProperty("runId", secondId)
        journal.replace(DiagnosticJsonV1.canonical(changed))
        rejects { DurableHardwareDiagnosticRunV1.recover(journal, "2026-10-11T20:00:01Z") }
    }

    private fun HardwareDiagnosticReportV1.attempts(action: HardwareDiagnosticActionV1) =
        actions.single { it.action == action }.attempts
    private fun HardwareDiagnosticReportV1.attempt(action: HardwareDiagnosticActionV1) = attempts(action).single()
    private fun rejects(block: () -> Unit) {
        try { block(); fail("Expected rejection") }
        catch (_: IllegalArgumentException) { }
        catch (_: IllegalStateException) { }
    }
}
