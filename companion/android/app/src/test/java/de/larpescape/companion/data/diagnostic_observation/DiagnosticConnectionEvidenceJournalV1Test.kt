package de.larpescape.companion.data.diagnostic_observation

import org.junit.Assert.*
import org.junit.Test
import java.nio.file.Files
import com.google.gson.Gson

class DiagnosticConnectionEvidenceJournalV1Test {
    @Test fun recoveryRetainsOnlyHistoricalEvidenceAndCannotAppendOrReplay() {
        val directory = Files.createTempDirectory("diagnostic-connection-v1").toFile()
        try {
            val first = DiagnosticConnectionEvidenceJournalV1(directory)
            first.begin(7, "A8:42:E3:CD:4B:18")
            assertTrue(first.append(DiagnosticConnectionEventV1(1, "2026-10-11T10:00:00Z", 7,
                "A8:42:E3:CD:4B:18", DiagnosticConnectionEventV1.Kind.WRITE_ACK, 8, "ATT_WRITE_ACK_ONLY")))
            val recovered = DiagnosticConnectionEvidenceJournalV1(directory)
            val history = recovered.loadRecovered()!!
            assertEquals(DiagnosticConnectionEvidenceV1.SCHEMA, history.schema)
            assertEquals("OPEN", history.originalAuthority)
            assertEquals("UNVERIFIED", history.physicalResult)
            assertEquals(1, history.events.size)
            assertNull(recovered.nextSequence())
            assertFalse(recovered.append(DiagnosticConnectionEventV1(2, "2026-10-11T10:00:01Z", 7,
                "A8:42:E3:CD:4B:18", DiagnosticConnectionEventV1.Kind.CONTROL_REPLY, 8, "reply")))
            recovered.begin(8, "A8:42:E3:CD:4B:18")
            assertTrue(recovered.loadRecovered()!!.events.isEmpty())
        } finally { directory.deleteRecursively() }
    }

    @Test fun wrongGenerationAndSequenceCannotEnterCurrentConnection() {
        val directory = Files.createTempDirectory("diagnostic-connection-v1").toFile()
        try {
            val journal = DiagnosticConnectionEvidenceJournalV1(directory)
            journal.begin(7, "A8:42:E3:CD:4B:18")
            val event = DiagnosticConnectionEventV1(1, "2026-10-11T10:00:00Z", 7,
                "A8:42:E3:CD:4B:18", DiagnosticConnectionEventV1.Kind.TRANSPORT_ERROR, 8, "TIMEOUT")
            assertFalse(journal.append(event.copy(connectionGeneration = 6)))
            assertFalse(journal.append(event.copy(sequence = 2)))
            assertFalse(journal.append(event.copy(targetAddress = "AA:BB:CC:DD:EE:FF")))
            assertTrue(journal.append(event))
            assertEquals(1, journal.loadRecovered()!!.events.size)
        } finally { directory.deleteRecursively() }
    }

    @Test fun exportAfterRestartContainsOnlyValidatedHistoricalTransportEvidence() {
        val directory = Files.createTempDirectory("diagnostic-connection-v1").toFile()
        try {
            val first = DiagnosticConnectionEvidenceJournalV1(directory)
            assertNull(first.exportBytes())
            first.begin(7, "A8:42:E3:CD:4B:1A")
            assertTrue(first.append(DiagnosticConnectionEventV1(1, "2026-10-11T10:00:00Z", 7,
                "A8:42:E3:CD:4B:1A", DiagnosticConnectionEventV1.Kind.CONTROL_REPLY, 8, "REJECTED")))
            val restarted = DiagnosticConnectionEvidenceJournalV1(directory)
            val exported = restarted.exportBytes()!!
            assertTrue(exported.size <= DiagnosticConnectionEvidenceJournalV1.MAX_BYTES)
            val decoded = Gson().fromJson(String(exported, Charsets.UTF_8), DiagnosticConnectionEvidenceV1::class.java)
            assertEquals(DiagnosticConnectionEvidenceV1.SCHEMA, decoded.schema)
            assertEquals("OPEN", decoded.originalAuthority)
            assertEquals("UNVERIFIED", decoded.physicalResult)
            assertEquals("A8:42:E3:CD:4B:1A", decoded.targetAddress)
            assertEquals(listOf("REJECTED"), decoded.events.map { it.detail })
            assertNull(restarted.nextSequence())
        } finally { directory.deleteRecursively() }
    }

    @Test fun unreadableOrForgedAuthorityCannotBeExported() {
        val directory = Files.createTempDirectory("diagnostic-connection-v1").toFile()
        try {
            val journal = DiagnosticConnectionEvidenceJournalV1(directory)
            journal.begin(7, "A8:42:E3:CD:4B:1A")
            val file = directory.resolve("diagnostic-connection-evidence-v1.json")
            file.writeText(file.readText().replace("\"OPEN\"", "\"GRANTED\""))
            assertThrows(IllegalArgumentException::class.java) { journal.exportBytes() }
            file.writeText("{broken")
            assertThrows(Exception::class.java) { journal.exportBytes() }
            file.writeBytes(ByteArray(DiagnosticConnectionEvidenceJournalV1.MAX_BYTES + 1))
            assertThrows(IllegalArgumentException::class.java) { journal.exportBytes() }
        } finally { directory.deleteRecursively() }
    }
}
