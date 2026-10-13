package de.larpescape.companion.data.diagnostic_observation

import com.google.gson.Gson
import de.larpescape.protocol.DiagnosticDevelopmentLedStepOutcomeV1
import de.larpescape.protocol.DiagnosticDevelopmentLedStepRejectionV1
import de.larpescape.protocol.DiagnosticDevelopmentLedStepReplyV1
import de.larpescape.protocol.DiagnosticDevelopmentLedStepRequestV1
import org.junit.Assert.*
import org.junit.Test
import java.nio.file.Files

class DevelopmentLedObservationJournalV1Test {
    @Test fun boundedLightReplyKeepsVisibleObservationSeparateFromElectricalQualification() {
        val directory = Files.createTempDirectory("development-led-light-v1").toFile()
        try {
            val request = DiagnosticDevelopmentLedStepRequestV1(
                8, "5A" + "00".repeat(15), "0000000000000064",
                "00000000-0000-4000-8000-000000000001",
                "00000000-0000-4000-8000-000000000002", 3)
            val reply = DiagnosticDevelopmentLedStepReplyV1(request, 10, 420,
                DiagnosticDevelopmentLedStepOutcomeV1.BLACK_SUBMITTED,
                DiagnosticDevelopmentLedStepRejectionV1.NONE, 2, true, true)
            val journal = DevelopmentLedObservationJournalV1(directory)
            val saved = journal.record(reply)
            journal.observe(saved.key, "LIGHT_SEEN")
            val report = DevelopmentLedObservationJournalV1(directory).load()
            assertEquals("LIGHT_SEEN", report.observations.single().userObservation)
            assertEquals("BLACK_SUBMITTED", report.observations.single().outcome)
            assertEquals("UNVERIFIED", report.physicalResult)
            assertFalse(report.observations.single().physicalOutputConfirmed)
        } finally { directory.deleteRecursively() }
    }

    @Test fun blockedReplyAndUserObservationSurviveRestartAndExportWithoutQualification() {
        val directory = Files.createTempDirectory("development-led-observation-v1").toFile()
        try {
            val request = DiagnosticDevelopmentLedStepRequestV1(
                7, "5A" + "00".repeat(15), "0000000000000063",
                "00000000-0000-4000-8000-000000000001",
                "00000000-0000-4000-8000-000000000002", 2)
            val reply = DiagnosticDevelopmentLedStepReplyV1(request, 0, 0,
                DiagnosticDevelopmentLedStepOutcomeV1.REJECTED,
                DiagnosticDevelopmentLedStepRejectionV1.HARDWARE_UNQUALIFIED, 0, false, false)
            val first = DevelopmentLedObservationJournalV1(directory)
            val saved = first.record(reply)
            assertEquals(saved, first.record(reply))
            first.observe(saved.key, "NO_LIGHT_SEEN")
            val reopened = DevelopmentLedObservationJournalV1(directory)
            val report = reopened.load()
            assertEquals(1, report.observations.size)
            assertEquals("NO_LIGHT_SEEN", report.observations.single().userObservation)
            assertEquals("HARDWARE_UNQUALIFIED", report.observations.single().rejection)
            assertEquals(0, report.observations.single().renderCalls)
            assertFalse(report.observations.single().physicalOutputConfirmed)
            assertEquals("UNVERIFIED", report.physicalResult)
            val exported = Gson().fromJson(String(reopened.exportBytes()!!),
                DevelopmentLedObservationReportV1::class.java)
            assertEquals("UNVERIFIED", exported.physicalResult)
            assertEquals("NO_LIGHT_SEEN", exported.observations.single().userObservation)
            assertNotNull(exported.exportedUtc)
        } finally { directory.deleteRecursively() }
    }
}
