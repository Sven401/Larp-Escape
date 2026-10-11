package de.larpescape.companion.data.nfc

import com.google.gson.Gson
import de.larpescape.protocol.NfcCapabilityEventV1
import de.larpescape.protocol.NfcCapabilitySnapshotV1
import org.junit.Assert.*
import org.junit.Test
import java.nio.file.Files
import java.time.Instant

class NfcCapabilityEvidenceJournalV1Test {
    private val start = Instant.parse("2026-10-11T18:00:00Z")
    private val id = "0102030405060708090A0B0C0D0E0F10"
    private val challenge = "202122232425262728292A2B2C2D2E2F"

    @Test fun localEchoExportKeepsDeviceUnverifiedAndBindsUtcEvents() {
        val directory = Files.createTempDirectory("nfc-evidence").toFile()
        try {
            val journal = NfcCapabilityEvidenceJournalV1(directory) { start.plusSeconds(35) }
            val snapshot = NfcCapabilitySnapshotV1(id, 100, challenge, "LOCAL_ECHO_ONLY", listOf(
                NfcCapabilityEventV1("ARMED", 100), NfcCapabilityEventV1("SELECT", 112),
                NfcCapabilityEventV1("CHALLENGE_ECHO", 124)))
            val report = journal.fromSnapshot(snapshot, "0.1-debug (1)", start, true, true)
            journal.save(report)
            val restored = journal.loadRecovered()!!
            assertEquals("2026-10-11T18:00:00.024Z", restored.endedUtc)
            assertTrue(restored.selectReceived)
            assertTrue(restored.challengeReceived)
            assertEquals("UNVERIFIED", restored.deviceCorrelation)
            assertEquals("UNCLEAR", restored.assessment)
            assertEquals(challenge, restored.challenge)
            val exported = Gson().fromJson(String(journal.exportBytes(restored)), NfcCapabilityEvidenceV1::class.java)
            assertEquals("nfc_capability_v1", exported.schema)
            assertEquals("2026-10-11T18:00:35Z", exported.exportedUtc)
            assertEquals("UNVERIFIED", exported.deviceCorrelation)
            assertEquals("2026-10-11T18:00:00.012Z", exported.events[1].recordedUtc)
        } finally { directory.deleteRecursively() }
    }

    @Test fun restartClosesArmedHistoryWithoutRearming() {
        val directory = Files.createTempDirectory("nfc-evidence").toFile()
        try {
            val journal = NfcCapabilityEvidenceJournalV1(directory) { start.plusSeconds(40) }
            val armed = journal.fromSnapshot(NfcCapabilitySnapshotV1(id, 0, null, "ARMED",
                listOf(NfcCapabilityEventV1("ARMED", 0))), "build", start, true, true)
            journal.save(armed)
            val recovered = NfcCapabilityEvidenceJournalV1(directory) { start.plusSeconds(40) }.loadRecovered()!!
            assertEquals("APP_RESTARTED", recovered.localStatus)
            assertEquals("2026-10-11T18:00:40Z", recovered.endedUtc)
            assertEquals("APP_RESTARTED", recovered.events.last().kind)
            assertEquals("UNVERIFIED", recovered.deviceCorrelation)
        } finally { directory.deleteRecursively() }
    }

    @Test fun forgedPositiveDeviceOutcomeCannotBeLoadedOrExported() {
        val directory = Files.createTempDirectory("nfc-evidence").toFile()
        try {
            val journal = NfcCapabilityEvidenceJournalV1(directory)
            val report = journal.fromSnapshot(NfcCapabilitySnapshotV1(id, 0, null, "STOPPED", listOf(
                NfcCapabilityEventV1("ARMED", 0), NfcCapabilityEventV1("STOPPED", 1))),
                "build", start, true, true)
            journal.save(report)
            val file = directory.resolve("nfc-capability-v1.json")
            file.writeText(file.readText().replace("\"UNVERIFIED\"", "\"PASS\""))
            assertThrows(IllegalArgumentException::class.java) { journal.loadRecovered() }
            assertThrows(IllegalArgumentException::class.java) { journal.exportBytes(report.copy(deviceCorrelation = "PASS")) }
        } finally { directory.deleteRecursively() }
    }
}
