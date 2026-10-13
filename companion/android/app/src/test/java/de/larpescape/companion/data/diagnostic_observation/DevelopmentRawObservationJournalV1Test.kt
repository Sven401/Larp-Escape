package de.larpescape.companion.data.diagnostic_observation

import com.google.gson.Gson
import de.larpescape.protocol.DiagnosticDevelopmentRawI2cStatusV1
import de.larpescape.protocol.DiagnosticDevelopmentRawStepOutcomeV1
import de.larpescape.protocol.DiagnosticDevelopmentRawStepRejectionV1
import de.larpescape.protocol.DiagnosticDevelopmentRawStepReplyV1
import de.larpescape.protocol.DiagnosticDevelopmentRawStepRequestV1
import org.junit.Assert.*
import org.junit.Test
import java.nio.file.Files

class DevelopmentRawObservationJournalV1Test {
    private fun reply(message: Long, high: Boolean = true,
                      status: DiagnosticDevelopmentRawI2cStatusV1 = DiagnosticDevelopmentRawI2cStatusV1.VALID) =
        DiagnosticDevelopmentRawStepReplyV1(
        DiagnosticDevelopmentRawStepRequestV1(message, "A".repeat(32), "1234567890ABCDEF",
            "12345678-1234-4123-8123-123456789abc", "12345678-1234-4123-8123-123456789abd", message),
        100, 101, if (status == DiagnosticDevelopmentRawI2cStatusV1.VALID)
            DiagnosticDevelopmentRawStepOutcomeV1.REGISTER_OBSERVED else
            DiagnosticDevelopmentRawStepOutcomeV1.I2C_FAILED,
        DiagnosticDevelopmentRawStepRejectionV1.NONE, status,
        if (status == DiagnosticDevelopmentRawI2cStatusV1.VALID) (if (high) 0x40 else 0) else null,
        if (status == DiagnosticDevelopmentRawI2cStatusV1.VALID) high else null, 0)

    @Test fun threeCorrelatedSamplesYieldOnlyAnUnqualifiedSignalResult() {
        val directory = Files.createTempDirectory("wake-candidate").toFile()
        try {
            val journal = DevelopmentRawObservationJournalV1(directory)
            journal.record(reply(1), "REST")
            journal.record(reply(2, false), "PRESSED")
            assertEquals("NOT_TESTED", journal.load().wake2CandidateSignal)
            journal.record(reply(3), "RELEASED")
            val result = DevelopmentRawObservationJournalV1(directory).load()
            assertEquals("RAW_PRESS_RELEASE_OBSERVED", result.wake2CandidateSignal)
            assertEquals("UNVERIFIED", result.physicalResult)
            assertTrue(result.observations.none { it.physicalChannelQualified })
            val exported = Gson().fromJson(String(journal.exportBytes()!!),
                DevelopmentRawObservationReportV1::class.java)
            assertEquals(result.wake2CandidateSignal, exported.wake2CandidateSignal)
        } finally { directory.deleteRecursively() }
    }

    @Test fun unchangedOrFailedSamplesCannotPassTheSignalComparison() {
        val directory = Files.createTempDirectory("wake-candidate-fail").toFile()
        try {
            val journal = DevelopmentRawObservationJournalV1(directory)
            journal.record(reply(1), "REST")
            journal.record(reply(2), "PRESSED")
            journal.record(reply(3), "RELEASED")
            assertEquals("RAW_PATTERN_NOT_OBSERVED", journal.load().wake2CandidateSignal)
            journal.record(reply(4), "REST")
            journal.record(reply(5, false, DiagnosticDevelopmentRawI2cStatusV1.SHORT_READ), "PRESSED")
            journal.record(reply(6), "RELEASED")
            assertEquals("READ_FAILED", journal.load().wake2CandidateSignal)
        } finally { directory.deleteRecursively() }
    }

    @Test fun samplesFromDifferentRunsCannotFormAnEvaluableSequence() {
        val directory = Files.createTempDirectory("wake-candidate-birth").toFile()
        try {
            val journal = DevelopmentRawObservationJournalV1(directory)
            journal.record(reply(1), "REST")
            journal.record(reply(2, false), "PRESSED")
            val differentRun = reply(3).let {
                it.copy(request = it.request.copy(
                    runId = "22345678-1234-4123-8123-123456789abc"))
            }
            journal.record(differentRun, "RELEASED")
            assertEquals("NOT_TESTED", journal.load().wake2CandidateSignal)
            assertEquals("UNVERIFIED", journal.load().physicalResult)
        } finally { directory.deleteRecursively() }
    }

    @Test fun coldStartKeepsDeviceReplyAndOneUserObservationWithoutGrant() {
        val directory = Files.createTempDirectory("development-raw-observation").toFile()
        try {
            val first = DevelopmentRawObservationJournalV1(directory)
            val recorded = first.record(reply(7))
            assertNull(recorded.userObservation)
            val restarted = DevelopmentRawObservationJournalV1(directory)
            assertEquals(recorded.key, restarted.record(reply(7)).key)
            val observed = restarted.observe(recorded.key, "CHANGE_SEEN")
            assertNotNull(observed.userObservedUtc)
            assertThrows(IllegalArgumentException::class.java) {
                restarted.observe("wrong", "NO_CHANGE_SEEN")
            }
            assertThrows(IllegalArgumentException::class.java) {
                restarted.observe(recorded.key, "NO_CHANGE_SEEN")
            }
            val exported = Gson().fromJson(String(restarted.exportBytes()!!),
                DevelopmentRawObservationReportV1::class.java)
            assertEquals("UNVERIFIED", exported.physicalResult)
            assertEquals(1, exported.observations.size)
            assertFalse(exported.observations.single().physicalChannelQualified)
            assertEquals("CHANGE_SEEN", exported.observations.single().userObservation)
            assertNotNull(exported.exportedUtc)
        } finally { directory.deleteRecursively() }
    }

    @Test fun corruptReportDoesNotGetReplacedByNewReply() {
        val directory = Files.createTempDirectory("development-raw-observation").toFile()
        try {
            val journal = DevelopmentRawObservationJournalV1(directory)
            journal.record(reply(7))
            val file = directory.resolve("development-raw-observation-v1.json")
            file.writeText("{broken")
            assertThrows(Exception::class.java) { journal.record(reply(8)) }
            assertEquals("{broken", file.readText())
        } finally { directory.deleteRecursively() }
    }
}
