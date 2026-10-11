package de.larpescape.protocol

import com.google.gson.JsonObject
import java.time.Instant

/**
 * Journaled evidence draft. request() commits the intent before returning it to
 * a caller that may send it; this class never sends, replays or authenticates a
 * device command. A replace failure must prevent the caller from sending.
 */
class DurableHardwareDiagnosticRunV1 private constructor(
    private val journal: DiagnosticJournalV1,
    private var ledger: HardwareDiagnosticEvidenceLedgerV1?,
    private var saved: HardwareDiagnosticReportV1,
    private var finalized: Boolean,
) {
    val runId: String get() = saved.runId
    val isFinalized: Boolean get() = finalized

    @Synchronized
    fun snapshot(): HardwareDiagnosticReportV1 = HardwareDiagnosticReportCodecV1.fromDocument(saved.snapshot())

    @Synchronized
    fun request(action: HardwareDiagnosticActionV1, correlationId: String, utc: String): DiagnosticRequestEvidenceV1 {
        change(utc) { it.request(action, correlationId, utc) }
        return DiagnosticRequestEvidenceV1(correlationId, utc)
    }

    @Synchronized
    fun recordAcceptanceClaim(
        action: HardwareDiagnosticActionV1, correlationId: String, state: DeviceAcceptanceV1,
        deviceMs: Long?, detail: String, utc: String,
    ) = change(utc) { it.recordAcceptanceClaim(action, correlationId, state, deviceMs, detail) }

    @Synchronized
    fun recordExecutionClaim(
        action: HardwareDiagnosticActionV1, correlationId: String, state: DeviceExecutionV1,
        deviceMs: Long?, detail: String, utc: String,
    ) = change(utc) { it.recordExecutionClaim(action, correlationId, state, deviceMs, detail) }

    @Synchronized
    fun observe(
        action: HardwareDiagnosticActionV1, correlationId: String, state: UserObservationV1,
        utc: String, detail: String,
    ) = change(utc) { it.observe(action, correlationId, state, utc, detail) }

    @Synchronized
    fun finish(utc: String): HardwareDiagnosticReportV1 {
        if (finalized) return snapshot()
        val current = checkNotNull(ledger)
        require(Instant.parse(utc) >= Instant.parse(saved.snapshot().text("endedUtc")))
        val next = current.snapshotAt(utc)
        journal.replace(journalBytes(next, true))
        saved = next
        ledger = null
        finalized = true
        return snapshot()
    }

    private fun change(utc: String, action: (HardwareDiagnosticEvidenceLedgerV1) -> Unit) {
        check(!finalized) { "Finalized diagnostic run is immutable" }
        require(Instant.parse(utc) >= Instant.parse(saved.snapshot().text("endedUtc")))
        val nextLedger = HardwareDiagnosticEvidenceLedgerV1.restore(saved)
        action(nextLedger)
        val next = nextLedger.snapshotAt(utc)
        journal.replace(journalBytes(next, false))
        ledger = nextLedger
        saved = next
    }

    companion object {
        private const val SCHEMA = "hardware_diagnostic_run_journal_v1"

        private fun journalBytes(report: HardwareDiagnosticReportV1, finalized: Boolean): ByteArray {
            val document = report.snapshot()
            val envelope = JsonObject().apply {
                addProperty("schema", SCHEMA)
                addProperty("finalized", finalized)
                add("report", document)
                addProperty("sha256", DiagnosticJsonV1.sha256(DiagnosticJsonV1.canonical(document)))
            }
            return DiagnosticJsonV1.canonical(envelope) + byteArrayOf(10)
        }

        private fun parse(bytes: ByteArray): Pair<HardwareDiagnosticReportV1, Boolean> {
            val envelope = DiagnosticJsonV1.parse(bytes).asJsonObject
            require(envelope.keySet() == setOf("schema", "finalized", "report", "sha256"))
            require(envelope.text("schema") == SCHEMA)
            require(envelope["finalized"].isJsonPrimitive && envelope["finalized"].asJsonPrimitive.isBoolean)
            val document = envelope.getAsJsonObject("report")
            require(envelope.text("sha256") == DiagnosticJsonV1.sha256(DiagnosticJsonV1.canonical(document)))
            return HardwareDiagnosticReportCodecV1.fromDocument(document) to envelope["finalized"].asBoolean
        }

        fun start(runId: String, startedUtc: String, journal: DiagnosticJournalV1): DurableHardwareDiagnosticRunV1 {
            require(journal.load() == null) { "Existing journal must be recovered, never overwritten" }
            val ledger = HardwareDiagnosticEvidenceLedgerV1(runId, startedUtc)
            val report = ledger.snapshotAt(startedUtc)
            journal.replace(journalBytes(report, false))
            return DurableHardwareDiagnosticRunV1(journal, ledger, report, false)
        }

        /** Pending intents become unverified, unclear outcomes; none is resent. */
        fun recover(journal: DiagnosticJournalV1, utc: String): DurableHardwareDiagnosticRunV1 {
            val (saved, finalized) = parse(checkNotNull(journal.load()) { "No run to recover" })
            if (finalized) return DurableHardwareDiagnosticRunV1(journal, null, saved, true)
            require(Instant.parse(utc) >= Instant.parse(saved.snapshot().text("endedUtc")))
            val ledger = HardwareDiagnosticEvidenceLedgerV1.restore(saved)
            var retired = false
            saved.actions.forEach { action ->
                action.attempts.forEach { attempt ->
                    val id = attempt.request.correlationId
                    when {
                        attempt.acceptanceClaim == null -> {
                            ledger.recordAcceptanceClaim(action.action, id, DeviceAcceptanceV1.UNCLEAR, null, "app_restarted; device acceptance unknown")
                            retired = true
                        }
                        attempt.acceptanceClaim.state == DeviceAcceptanceV1.ACCEPTED && attempt.executionClaim == null -> {
                            ledger.recordExecutionClaim(action.action, id, DeviceExecutionV1.UNCLEAR, null, "app_restarted; device execution unknown")
                            retired = true
                        }
                    }
                }
            }
            val current = if (retired) ledger.snapshotAt(utc) else saved
            if (retired) journal.replace(journalBytes(current, false))
            return DurableHardwareDiagnosticRunV1(journal, ledger, current, false)
        }
    }
}
