package de.larpescape.companion.platform.nfc

import de.larpescape.protocol.*
import org.junit.Assert.*
import org.junit.Test

class AndroidNdefPortTest {
    private class Io(private val initial: ByteArray?) : NdefIo {
        var written: ByteArray? = null
        var postWriteRead: ByteArray? = null
        var returnActualAfterWrite = false
        var failWrite = false
        var failReadback = false
        var writes = 0
        var reads = 0
        var closes = 0
        override val writable = true
        override val capacity = 137
        override fun connect() {}
        override fun read(): ByteArray? {
            reads++
            if (writes == 0) return initial?.copyOf()
            if (failReadback) error("tag lost")
            return if (returnActualAfterWrite) written?.copyOf() else postWriteRead?.copyOf()
        }
        override fun write(message: ByteArray) {
            writes++
            if (failWrite) error("write failed")
            written = message.copyOf()
        }
        override fun close() { closes++ }
    }

    @Test fun emptyDiscoveryIsNotReusedAsEvidenceAfterFirstWrite() {
        val io = Io(null)
        val events = mutableListOf<String>()
        val result = VerifiedNfcWrite.execute(AndroidNdefPort(io, events::add), null, byteArrayOf(1, 2))
        assertEquals(NfcWriteResult.VERIFICATION_REQUIRED, result)
        assertArrayEquals(byteArrayOf(1, 2), io.written)
        assertEquals(1, io.writes); assertEquals(2, io.reads); assertEquals(1, io.closes)
        assertTrue(events.contains("nfc_readback_requires_rediscovery"))
        assertFalse(events.any { it.startsWith("nfc_rf_read_complete") })
    }

    @Test fun stackReturningActualFirstWriteBytesStillVerifiesDirectly() {
        val io = Io(null).apply { returnActualAfterWrite = true }
        assertEquals(NfcWriteResult.VERIFIED,
            VerifiedNfcWrite.execute(AndroidNdefPort(io), null, byteArrayOf(1, 2)))
        assertEquals(1, io.writes)
    }

    @Test fun nullOnPreviouslyNonemptyTagRemainsUncertain() {
        val io = Io(byteArrayOf(1))
        assertEquals(NfcWriteResult.UNCERTAIN,
            VerifiedNfcWrite.execute(AndroidNdefPort(io), byteArrayOf(1), byteArrayOf(2)))
        assertEquals(1, io.writes)
    }

    @Test fun nonemptyMismatchOnFirstFillIsNotTreatedAsStaleEmptyDiscovery() {
        val io = Io(null).apply { postWriteRead = byteArrayOf(9) }
        assertEquals(NfcWriteResult.UNCERTAIN,
            VerifiedNfcWrite.execute(AndroidNdefPort(io), null, byteArrayOf(2)))
        assertEquals(1, io.writes)
    }

    @Test fun failedWriteOrReadDoesNotTriggerRediscoveryOrAnotherWrite() {
        for (writeFails in listOf(false, true)) {
            val io = Io(null).apply { failWrite = writeFails; failReadback = !writeFails }
            val events = mutableListOf<String>()
            assertEquals(NfcWriteResult.UNCERTAIN,
                VerifiedNfcWrite.execute(AndroidNdefPort(io, events::add), null, byteArrayOf(2)))
            assertFalse(events.contains("nfc_readback_requires_rediscovery"))
            assertEquals(1, io.writes)
            assertEquals(if (writeFails) 1 else 2, io.reads)
        }
    }

    @Test fun finalCleanupCannotReuseOrReconnectAnOldDiscovery() {
        val io = Io(null)
        val port = AndroidNdefPort(io)
        assertEquals(NfcWriteResult.VERIFICATION_REQUIRED,
            VerifiedNfcWrite.execute(port, null, byteArrayOf(2)))
        port.close()
        assertEquals(1, io.closes)
        assertThrows(java.io.IOException::class.java) { port.connect() }
        assertThrows(java.io.IOException::class.java) { port.read() }
        assertThrows(java.io.IOException::class.java) { port.write(byteArrayOf(3)) }
        assertEquals(1, io.writes)
    }
}
