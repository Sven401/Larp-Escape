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
    private fun reply(message: Long) = DiagnosticDevelopmentRawStepReplyV1(
        DiagnosticDevelopmentRawStepRequestV1(message, "A".repeat(32), "1234567890ABCDEF",
            "12345678-1234-4123-8123-123456789abc", "12345678-1234-4123-8123-123456789abd", message),
        100, 101, DiagnosticDevelopmentRawStepOutcomeV1.REGISTER_OBSERVED,
        DiagnosticDevelopmentRawStepRejectionV1.NONE, DiagnosticDevelopmentRawI2cStatusV1.VALID,
        0x40, true, 0)

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
