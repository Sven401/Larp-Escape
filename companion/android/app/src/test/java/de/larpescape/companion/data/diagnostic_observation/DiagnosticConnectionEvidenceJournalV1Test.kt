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
            assertEquals("NOT_GRANTED", history.originalAuthority)
            assertEquals("UNVERIFIED", history.physicalResult)
            assertEquals(1, history.events.size)
            assertNull(recovered.nextSequence())
            assertFalse(recovered.append(DiagnosticConnectionEventV1(2, "2026-10-11T10:00:01Z", 7,
                "A8:42:E3:CD:4B:18", DiagnosticConnectionEventV1.Kind.CONTROL_REPLY, 8, "reply")))
            recovered.begin(8, "A8:42:E3:CD:4B:18")
            assertTrue(recovered.loadRecovered()!!.events.isEmpty())
            val exportedHistory = Gson().fromJson(String(recovered.exportHistoryBytes()!!, Charsets.UTF_8),
                DiagnosticConnectionHistoryV1::class.java)
            assertEquals(2, exportedHistory.connections.size)
            assertEquals(7, exportedHistory.connections.first().connectionGeneration)
            assertEquals(1, exportedHistory.connections.first().events.size)
            assertEquals(8, exportedHistory.connections.last().connectionGeneration)
            assertEquals("UNVERIFIED", exportedHistory.physicalResult)
        } finally { directory.deleteRecursively() }
    }

    @Test fun coldStartHistoryPreservesPriorLinksAndRejectsCorruptedArchive() {
        val directory = Files.createTempDirectory("diagnostic-connection-history-v1").toFile()
        try {
            val first = DiagnosticConnectionEvidenceJournalV1(directory)
            first.begin(7, "A8:42:E3:CD:4B:18")
            assertTrue(first.append(DiagnosticConnectionEventV1(1, "2026-10-11T10:00:00Z", 7,
                "A8:42:E3:CD:4B:18", DiagnosticConnectionEventV1.Kind.TRANSPORT_PROBE, null, "transport_only")))
            val restarted = DiagnosticConnectionEvidenceJournalV1(directory)
            restarted.begin(8, "A8:42:E3:CD:4B:18")
            assertTrue(restarted.append(DiagnosticConnectionEventV1(1, "2026-10-11T10:01:00Z", 8,
                "A8:42:E3:CD:4B:18", DiagnosticConnectionEventV1.Kind.TRANSPORT_ERROR, null, "DISCONNECTED")))
            val afterSecondRestart = DiagnosticConnectionEvidenceJournalV1(directory)
            val exported = afterSecondRestart.exportHistoryBytes()!!
            val history = Gson().fromJson(String(exported, Charsets.UTF_8), DiagnosticConnectionHistoryV1::class.java)
            assertEquals("diagnostic_connection_history_v1", history.schema)
            assertEquals(listOf(7, 8), history.connections.map { it.connectionGeneration })
            assertTrue(history.connections.all { it.originalAuthority == "NOT_GRANTED" && it.physicalResult == "UNVERIFIED" })
            assertNull(afterSecondRestart.nextSequence())
            val archive = directory.listFiles()!!.single { it.name.startsWith("diagnostic-connection-evidence-v1-") }
            archive.writeText("{broken")
            assertThrows(Exception::class.java) { afterSecondRestart.exportHistoryBytes() }
        } finally { directory.deleteRecursively() }
    }

    @Test fun retryAfterFreshWriteFailureDoesNotArchivePriorLinkTwice() {
        val directory = Files.createTempDirectory("diagnostic-connection-retry-v1").toFile()
        try {
            val journal = DiagnosticConnectionEvidenceJournalV1(directory)
            journal.begin(7, "A8:42:E3:CD:4B:18")
            assertTrue(journal.append(DiagnosticConnectionEventV1(1, "2026-10-11T10:00:00Z", 7,
                "A8:42:E3:CD:4B:18", DiagnosticConnectionEventV1.Kind.TRANSPORT_ERROR, null, "DISCONNECTED")))
            val blockedTemp = directory.resolve("diagnostic-connection-evidence-v1.json.tmp")
            assertTrue(blockedTemp.mkdir())
            blockedTemp.resolve("keep").writeText("prevent replacement")
            assertThrows(Exception::class.java) { journal.begin(8, "A8:42:E3:CD:4B:18") }
            assertEquals(7, journal.loadRecovered()!!.connectionGeneration)
            assertEquals(1, directory.listFiles()!!.count { it.name.matches(
                Regex("diagnostic-connection-evidence-v1-[0-9]{13}-[0-9a-f-]{36}\\.json")) })
            blockedTemp.deleteRecursively()
            journal.begin(8, "A8:42:E3:CD:4B:18")
            val history = Gson().fromJson(String(journal.exportHistoryBytes()!!, Charsets.UTF_8),
                DiagnosticConnectionHistoryV1::class.java)
            assertEquals(listOf(7, 8), history.connections.map { it.connectionGeneration })
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
            assertEquals("NOT_GRANTED", decoded.originalAuthority)
            assertEquals("UNVERIFIED", decoded.physicalResult)
            assertNotNull(decoded.exportedUtc)
            assertNull(restarted.loadRecovered()!!.exportedUtc)
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
            file.writeText(file.readText().replace("\"NOT_GRANTED\"", "\"GRANTED\""))
            assertThrows(IllegalArgumentException::class.java) { journal.exportBytes() }
            file.writeText("{broken")
            assertThrows(Exception::class.java) { journal.exportBytes() }
            file.writeBytes(ByteArray(DiagnosticConnectionEvidenceJournalV1.MAX_BYTES + 1))
            assertThrows(IllegalArgumentException::class.java) { journal.exportBytes() }
        } finally { directory.deleteRecursively() }
    }

    @Test fun legacyOpenJournalIsReadableButExportsUnverifiedAuthority() {
        val directory = Files.createTempDirectory("diagnostic-connection-v1").toFile()
        try {
            val journal = DiagnosticConnectionEvidenceJournalV1(directory)
            journal.begin(7, "A8:42:E3:CD:4B:1A")
            val file = directory.resolve("diagnostic-connection-evidence-v1.json")
            file.writeText(file.readText().replace("\"NOT_GRANTED\"", "\"OPEN\""))
            assertEquals("UNVERIFIED", journal.loadRecovered()!!.originalAuthority)
            val exported = Gson().fromJson(String(journal.exportBytes()!!, Charsets.UTF_8),
                DiagnosticConnectionEvidenceV1::class.java)
            assertEquals("UNVERIFIED", exported.originalAuthority)
            assertTrue(file.readText().contains("\"originalAuthority\":\"OPEN\""))
        } finally { directory.deleteRecursively() }
    }
}
