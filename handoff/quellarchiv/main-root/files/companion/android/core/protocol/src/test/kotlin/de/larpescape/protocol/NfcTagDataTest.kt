package de.larpescape.protocol

import org.junit.Assert.*
import org.junit.Test

class NfcTagDataTest {
    private fun hex(text: String) = text.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    // Independently produced by unmodified GM-Companion TagData.cs on .NET10, 2026-10-10.
    private val empty = hex("0100000000000000000000000000000000FE3A494E")
    private val populated = hex("01EFCDAB8934120098FAE3CA26DF48020001022C01FFFEFFFF88608A1E")
    @Test fun matchesUnmodifiedCSharpReferenceBytes() {
        assertArrayEquals(empty, NfcTagDataCodec.encode(NfcTagData()))
        val expected = NfcTagData(2309737967, 4660, 5250958343387387904,
            listOf(NfcNodeVisit(1, 2, 300), NfcNodeVisit(255, 254, 65535)))
        assertEquals(expected, NfcTagDataCodec.decode(populated))
        assertArrayEquals(populated, NfcTagDataCodec.encode(expected))
    }
    @Test fun rejectsEveryTruncationAndTrailingBytes() {
        for (size in populated.indices) assertThrows(IllegalArgumentException::class.java) { NfcTagDataCodec.decode(populated.copyOf(size)) }
        assertThrows(IllegalArgumentException::class.java) { NfcTagDataCodec.decode(populated + 0) }
    }
    @Test fun rejectsCorruptionVersionAndFalseVisitCount() {
        for (at in populated.indices) {
            val bytes = populated.copyOf(); bytes[at] = (bytes[at].toInt() xor 1).toByte()
            assertThrows(IllegalArgumentException::class.java) { NfcTagDataCodec.decode(bytes) }
        }
    }
    @Test fun boundsNumbersAndVisitCapacityAndPreservesDateTimeBits() {
        for (data in listOf(NfcTagData(uidHash = -1), NfcTagData(uidHash = 4294967296), NfcTagData(flags = 65536),
            NfcTagData(visits = listOf(NfcNodeVisit(256, 0, 0))), NfcTagData(visits = listOf(NfcNodeVisit(1, -1, 0))),
            NfcTagData(visits = listOf(NfcNodeVisit(1, 0, 65536))),
            NfcTagData(visits = List(NfcTagDataCodec.MAX_VISITS + 1) { NfcNodeVisit(1, 0, 0) }))) {
            assertThrows(IllegalArgumentException::class.java) { NfcTagDataCodec.encode(data) }
        }
        val maximal = NfcTagData(4294967295, 65535, Long.MIN_VALUE, List(NfcTagDataCodec.MAX_VISITS) { NfcNodeVisit(255, 255, 65535) })
        assertEquals(maximal, NfcTagDataCodec.decode(NfcTagDataCodec.encode(maximal)))
    }
    private class Port : NfcMessagePort {
        var message: ByteArray? = byteArrayOf(1)
        override var capacity = 100
        override var writable = true
        var writes = 0; var reads = 0; var closed = false
        var failRead = false; var failWrite = false; var corruptWrite = false
        var afterRead: (() -> Unit)? = null
        override fun connect() { }
        override fun read(): ByteArray? {
            reads++
            if (failRead) error("tag lost")
            afterRead?.invoke()
            return message?.copyOf()
        }
        override fun write(message: ByteArray) {
            writes++
            if (failWrite) error("tag lost during write")
            this.message = if (corruptWrite) byteArrayOf(0) else message.copyOf()
        }
        override fun close() { closed = true }
    }
    @Test fun verifiesRealReadAfterWriteAndClosesPort() {
        val p = Port()
        assertEquals(NfcWriteResult.VERIFIED, VerifiedNfcWrite.execute(p, byteArrayOf(1), byteArrayOf(2)))
        assertEquals(2, p.reads); assertEquals(1, p.writes); assertTrue(p.closed)
    }
    @Test fun changedReadOnlyAndTooSmallTagsNeverWrite() {
        for ((port, result) in listOf(Port().apply { message = byteArrayOf(3) } to NfcWriteResult.CHANGED,
            Port().apply { writable = false } to NfcWriteResult.READ_ONLY,
            Port().apply { capacity = 0 } to NfcWriteResult.TOO_SMALL,
            Port().apply { message = null } to NfcWriteResult.CHANGED)) {
            assertEquals(result, VerifiedNfcWrite.execute(port, byteArrayOf(1), byteArrayOf(2)))
            assertEquals(0, port.writes); assertTrue(port.closed)
        }
        val emptyMessage = Port().apply { message = byteArrayOf() }
        assertEquals(NfcWriteResult.CHANGED, VerifiedNfcWrite.execute(emptyMessage, null, byteArrayOf(2)))
    }
    @Test fun lossBeforeAndDuringWriteHaveDifferentOutcomesAndNeverRetry() {
        for ((port, result) in listOf(Port().apply { failRead = true } to NfcWriteResult.READ_FAILED,
            Port().apply { failWrite = true } to NfcWriteResult.UNCERTAIN,
            Port().apply { corruptWrite = true } to NfcWriteResult.UNCERTAIN)) {
            assertEquals(result, VerifiedNfcWrite.execute(port, byteArrayOf(1), byteArrayOf(2)))
            assertTrue(port.writes <= 1); assertTrue(port.closed)
        }
    }
    @Test fun pauseBeforeWriteCancelsAndPauseDuringWriteCannotReportSuccess() {
        var current = true
        val p = Port().apply { afterRead = { current = false } }
        assertEquals(NfcWriteResult.CANCELLED, VerifiedNfcWrite.execute(p, byteArrayOf(1), byteArrayOf(2)) { current })
        assertEquals(0, p.writes)
        current = true
        val q = Port().apply { afterRead = { if (reads == 2) current = false } }
        assertEquals(NfcWriteResult.UNCERTAIN, VerifiedNfcWrite.execute(q, byteArrayOf(1), byteArrayOf(2)) { current })
        assertEquals(1, q.writes)
    }
}
