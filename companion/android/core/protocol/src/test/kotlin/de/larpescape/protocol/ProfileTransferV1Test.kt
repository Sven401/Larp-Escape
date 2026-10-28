package de.larpescape.protocol

import java.io.File
import org.junit.Assert.*
import org.junit.Test

class ProfileTransferV1Test {
    private val fixtures get() = File(requireNotNull(System.getProperty("profileFixtures")))
    private fun hex(s: String) = s.trim().chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private fun fixture(name: String) = hex(File(fixtures, "control-$name-v1.txt").readText())
    private fun rejects(block: () -> Unit) { try { block(); fail("Expected rejection") } catch (_: IllegalArgumentException) { } }

    @Test fun allValuesMatchActualCppGoldenForBothValueBudgets() {
        for (budget in listOf(20, 128)) {
            val golden = File(fixtures, "frames-$budget-v1.txt").readLines().filter { it.isNotBlank() }.map(::hex)
            val source = golden.drop(1).flatMap { it.drop(8) }.toByteArray()
            assertEquals(20220, source.size)
            val values = ProfileSourceValues(source, 1, 2, 1, budget)
            assertEquals(golden.size, values.size)
            golden.forEachIndexed { i, expected -> assertArrayEquals("frame $i at $budget", expected, values.value(i)) }
        }
    }

    @Test fun commandsMatchCppGolden() {
        for ((name, request, operation) in listOf(
            Triple("finish", 1L, ProfileOperation.FINISH), Triple("apply", 2L, ProfileOperation.APPLY), Triple("abort", 3L, ProfileOperation.ABORT),
        )) assertArrayEquals(fixture("$name-command"), ProfileTransferV1.control(1, request, operation))
    }

    @Test fun actualFirmwareResponsesRemainDistinct() {
        val draft = ProfileTransferV1.response(fixture("draft"))
        val queued = ProfileTransferV1.response(fixture("queued"))
        val saved = ProfileTransferV1.response(fixture("saved"))
        assertEquals(ProfileResponseCode.DRAFT_READY, draft.code)
        assertEquals(ProfileOperation.FINISH, draft.operation)
        assertEquals(ProfileResponseCode.APPLY_QUEUED, queued.code)
        assertEquals(ProfileResponseCode.SAVED, saved.code)
        assertEquals(queued.request, saved.request)
        assertEquals(4L, saved.draftRevision); assertEquals(2L, saved.configurationRevision)
        val failed = ProfileTransferV1.response(fixture("failed"))
        assertEquals(ProfileResponseCode.APPLY_FAILED, failed.code); assertEquals(11, failed.detail)
        assertEquals(ProfileResponseCode.ABORTED, ProfileTransferV1.response(fixture("aborted")).code)
        val raw = fixture("saved"); val snapshot = ProfileTransferV1.response(raw); raw.fill(0)
        assertEquals(saved, snapshot)
    }

    @Test fun rejectsMalformedAndContradictoryResponses() {
        val saved = fixture("saved")
        for (n in 0 until 20) rejects { ProfileTransferV1.response(saved.copyOf(n)) }
        rejects { ProfileTransferV1.response(saved + 0.toByte()) }
        for ((at, value) in listOf(0 to 2, 1 to 7, 2 to 0, 4 to 0, 8 to 0, 8 to 1, 9 to 0, 9 to 14, 10 to 1, 10 to 12, 12 to 0, 16 to 0))
            rejects { ProfileTransferV1.response(saved.copyOf().also { it[at] = value.toByte() }) }
        for (name in listOf("draft", "queued", "aborted", "failed")) {
            val invalidOperation = if (name == "draft") 3 else 1
            rejects { ProfileTransferV1.response(fixture(name).also { it[8] = invalidOperation.toByte() }) }
        }
    }

    @Test fun unsignedLimitsAndIndependentCrcVector() {
        val meta = ProfileTransferV1.metadata("123456789".toByteArray(), 65535, 0xffff_fffeL, 0xffff_ffffL)
        assertArrayEquals(hex("0105FFFF090000002639F4CBFEFFFFFFFFFFFFFF"), meta)
        assertArrayEquals(hex("0107FFFF03000000FFFFFFFF"), ProfileTransferV1.control(65535, 0xffff_ffffL, ProfileOperation.APPLY))
        val raw = fixture("saved").also { for (i in 4..7) it[i] = 0xff.toByte(); for (i in 12..19) it[i] = 0xff.toByte() }
        val value = ProfileTransferV1.response(raw)
        assertEquals(0xffff_ffffL, value.request); assertEquals(0xffff_ffffL, value.draftRevision)
        assertEquals(0xffff_ffffL, value.configurationRevision)
    }

    @Test fun maximumAndCopiesNeverBorrowSourceOrReturnedValues() {
        for ((budget, expected) in listOf(20 to 10924, 128 to 1094)) {
            val values = ProfileSourceValues(ByteArray(131072), 65535, 0, 1, budget)
            assertEquals(expected, values.size); assertTrue(values.value(values.size - 1).size <= budget)
        }
        val raw = byteArrayOf(1, 2, 3); val values = ProfileSourceValues(raw, 1, 0, 1)
        raw.fill(0); values.value(1).fill(0); values.value(0).fill(0)
        assertArrayEquals(byteArrayOf(1, 2, 3), values.value(1).copyOfRange(8, 11))
        assertEquals(1.toByte(), values.value(0)[0])
        rejects { values.value(-1) }; rejects { values.value(values.size) }
    }

    @Test fun rejectsEveryEncoderBound() {
        val raw = byteArrayOf(1)
        for (size in listOf(0, 131073)) rejects { ProfileTransferV1.metadata(ByteArray(size), 1, 0, 1) }
        for (id in listOf(0, -1, 65536)) rejects { ProfileTransferV1.metadata(raw, id, 0, 1) }
        for (revision in listOf(-1L, 0xffff_ffffL, 0x1_0000_0000L)) rejects { ProfileTransferV1.metadata(raw, 1, revision, 1) }
        for (revision in listOf(-1L, 0L, 0x1_0000_0000L)) rejects { ProfileTransferV1.metadata(raw, 1, 0, revision) }
        for (request in listOf(-1L, 0L, 0x1_0000_0000L)) rejects { ProfileTransferV1.control(1, request, ProfileOperation.FINISH) }
        for (budget in listOf(8, 19, 129)) rejects { ProfileSourceValues(raw, 1, 0, 1, budget) }
        rejects { ProfileTransferV1.data(1, -1, raw) }; rejects { ProfileTransferV1.data(1, 131072, raw) }
        rejects { ProfileTransferV1.data(1, 0, ByteArray(121)) }; rejects { ProfileTransferV1.data(1, 0, ByteArray(0)) }
    }
}
