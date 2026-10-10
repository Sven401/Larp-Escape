package de.larpescape.companion.ui.nfc

import de.larpescape.protocol.*
import org.junit.Assert.*
import org.junit.Test
import java.util.concurrent.Executor

class NfcViewModelTest {
    private val direct = Executor { it.run() }
    private val codec = object : NfcEnvelopeCodec {
        override fun read(message: ByteArray?) = message?.let(NfcTagDataCodec::decode)
        override fun replace(message: ByteArray?, payload: ByteArray) = payload.copyOf()
    }
    private class Journal : NfcWriteJournal {
        var pending: NfcPendingWrite? = null
        var works = true
        override fun load() = pending
        override fun mark(value: NfcPendingWrite): Boolean { if (works) pending = value; return works }
        override fun clear(): Boolean { if (works) pending = null; return works }
    }
    private class Port(var bytes: ByteArray? = NfcTagDataCodec.encode(NfcTagData())) : NfcMessagePort {
        var writes = 0; var connects = 0; var failAfterWrite = false
        override fun connect() { connects++ }
        override val writable = true
        override val capacity = 512
        override fun read(): ByteArray? { if (failAfterWrite && writes > 0) error("lost"); return bytes }
        override fun write(message: ByteArray) { writes++; bytes = message }
        override fun close() {}
    }
    private fun model(j: Journal = Journal(), executor: Executor = direct) = NfcViewModel(codec, j, executor, direct)

    @Test fun writesOnlyPreparedSameTagAndConsumesRequest() {
        val m = model(); val token = m.resume(true, true); val p = Port()
        m.tagDiscovered(token, "A", p); assertEquals(0, p.writes)
        m.prepareWrite(NfcTagData(flags = 7)); assertTrue(m.state.value.armed)
        m.tagDiscovered(token, "A", p)
        assertEquals(1, p.writes); assertEquals(7, m.state.value.data!!.flags)
        m.tagDiscovered(token, "A", p); assertEquals(1, p.writes)
    }
    @Test fun wrongOrModifiedTagNeverWritten() {
        for (differentId in listOf(true, false)) {
            val m = model(); val t = m.resume(true, true); m.tagDiscovered(t, "A", Port())
            m.prepareWrite(NfcTagData(flags = 7))
            val other = Port(NfcTagDataCodec.encode(NfcTagData(flags = 2)))
            m.tagDiscovered(t, if (differentId) "B" else "A", other)
            assertEquals(0, other.writes); assertFalse(m.state.value.armed)
        }
    }
    @Test fun pausedQueuedCallbackDoesNotAccessTagOrReplaceNewState() {
        val jobs = mutableListOf<Runnable>(); val m = model(executor = Executor { jobs.add(it) })
        val old = m.resume(true, true); val p = Port(); m.tagDiscovered(old, "A", p)
        m.pause(); m.resume(true, false); jobs.removeAt(0).run()
        assertEquals(0, p.connects); assertFalse(m.state.value.selected); assertFalse(m.state.value.listening)
        m.tagDiscovered(old, "A", p); assertEquals(0, jobs.size)
    }
    @Test fun storageFailurePreventsWriting() {
        val j = Journal(); val m = model(j); val t = m.resume(true, true); val p = Port()
        m.tagDiscovered(t, "A", p); m.prepareWrite(NfcTagData(flags = 3)); j.works = false
        m.tagDiscovered(t, "A", p); assertEquals(0, p.writes)
    }
    @Test fun uncertainWriteSurvivesRecreationAndRequiresActualSameTagRead() {
        val j = Journal(); val m = model(j); val t = m.resume(true, true); val p = Port()
        m.tagDiscovered(t, "A", p); m.prepareWrite(NfcTagData(flags = 3)); p.failAfterWrite = true
        m.tagDiscovered(t, "A", p); assertNotNull(j.pending)
        val next = model(j); val nt = next.resume(true, true)
        next.tagDiscovered(nt, "B", Port()); assertFalse(next.state.value.canWrite); assertNotNull(j.pending)
        next.prepareWrite(NfcTagData()); assertFalse(next.state.value.armed)
        next.tagDiscovered(nt, "A", Port(p.bytes)); assertNull(j.pending)
        assertEquals(3, next.state.value.data!!.flags); assertTrue(next.state.value.message.contains("bestätigt"))
    }
    @Test fun corruptReadAndUnsupportedTagCannotBeInitialized() {
        val m = model(); val t = m.resume(true, true)
        m.tagDiscovered(t, "A", Port(byteArrayOf(1, 2))); assertFalse(m.state.value.selected)
        m.prepareWrite(NfcTagData()); assertFalse(m.state.value.armed)
        m.tagDiscovered(t, "A", Port()); m.prepareWrite(NfcTagData()); m.unsupported(t)
        assertFalse(m.state.value.armed); assertFalse(m.state.value.selected)
    }
    @Test fun editorRejectsOverflowAndPreservesTimestamp() {
        assertEquals(Long.MIN_VALUE, editedNfcData("4294967295", "65535", "255, 254, 65535", Long.MIN_VALUE).timestampBinary)
        for (line in listOf("256, 1, 0", "1, 1, 65536", "1, 2")) {
            try { editedNfcData("0", "0", line, 0); fail("accepted invalid visit") } catch (_: IllegalArgumentException) {}
        }
    }
    @Test fun oldEditorConfirmationCannotArmAfterAnotherReadOfSameUid() {
        val m = model(); val t = m.resume(true, true)
        m.tagDiscovered(t, "A", Port()); val old = m.state.value.selectionId
        m.tagDiscovered(t, "A", Port(NfcTagDataCodec.encode(NfcTagData(flags = 4))))
        m.prepareWrite(NfcTagData(flags = 7), old)
        assertFalse(m.state.value.armed); assertEquals(4, m.state.value.data!!.flags)
    }
    @Test fun cancelPreparedWriteConsumesItAndLateCancelDoesNotClaimUndo() {
        val jobs = mutableListOf<Runnable>(); val m = model(executor = Executor { jobs.add(it) })
        val t = m.resume(true, true); val p = Port()
        m.tagDiscovered(t, "A", p); jobs.removeAt(0).run()
        m.prepareWrite(NfcTagData(flags = 2)); m.cancelWrite()
        m.tagDiscovered(t, "A", p); jobs.removeAt(0).run(); assertEquals(0, p.writes)
        m.prepareWrite(NfcTagData(flags = 2)); m.tagDiscovered(t, "A", p)
        m.cancelWrite(); assertTrue(m.state.value.busy); assertFalse(m.state.value.message.contains("abgebrochen"))
        jobs.removeAt(0).run(); assertEquals(1, p.writes)
    }
}
