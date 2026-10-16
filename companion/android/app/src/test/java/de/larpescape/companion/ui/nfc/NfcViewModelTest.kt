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
        var writes = 0; var connects = 0; var closes = 0; var failAfterWrite = false
        var requiresRediscovery = false
        var afterRead: (() -> Unit)? = null
        var afterReadback: (() -> Unit)? = null
        override fun connect() { connects++ }
        override val writable = true
        override val capacity = 512
        override fun read(): ByteArray? {
            if (failAfterWrite && writes > 0) error("lost")
            afterRead?.invoke()
            return bytes
        }
        override fun write(message: ByteArray) { writes++; bytes = message }
        override fun readAfterWrite(): NfcReadback {
            val result = if (requiresRediscovery) NfcReadback.RequiresRediscovery else NfcReadback.Bytes(read())
            afterReadback?.invoke()
            return result
        }
        override fun close() { closes++ }
    }
    private fun model(j: Journal = Journal(), executor: Executor = direct,
                      onVerificationRequired: (Long) -> Unit = {}) =
        NfcViewModel(codec, j, executor, direct, onVerificationRequired = onVerificationRequired)

    @Test fun onlyIndependentReadAdvancesReadOnlyProof() {
        val m = model(); val token = m.resume(true, true); val p = Port()
        m.tagDiscovered(token, "A", p)
        assertEquals(m.state.value.selectionId, m.state.value.readOnlySelectionId)
        m.prepareWrite(NfcTagData(flags = 2)); m.tagDiscovered(token, "A", p)
        assertEquals(0L, m.state.value.readOnlySelectionId)
        m.tagDiscovered(token, "A", p)
        assertEquals(m.state.value.selectionId, m.state.value.readOnlySelectionId)
        assertEquals(1, p.writes)
    }

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
    @Test fun freshDiscoveryConfirmsEmptyTagWriteWithoutAnotherWrite() {
        val j = Journal(); val m = model(j); val t = m.resume(true, true)
        m.tagDiscovered(t, "A", Port(null)); m.prepareWrite(NfcTagData(flags = 3))
        val writingPort = Port(null).apply { requiresRediscovery = true }
        var callbacks = 0
        m.onVerificationRequired = { token ->
            callbacks++
            assertEquals(t, token); assertTrue(m.isVerificationPending(token))
            assertNotNull(j.pending); assertFalse(m.state.value.canWrite); assertFalse(m.state.value.busy)
            assertEquals(1, writingPort.closes)
            assertEquals("Schreiben abgeschlossen · Inhalt wird geprüft …", m.state.value.message)
        }
        m.tagDiscovered(t, "A", writingPort)
        assertEquals(1, callbacks); assertEquals(1, writingPort.writes); assertEquals(1, writingPort.closes)
        m.prepareWrite(NfcTagData(flags = 9)); assertFalse(m.state.value.armed)
        val fresh = Port(writingPort.bytes)
        m.tagDiscovered(t, "A", fresh)
        assertEquals("Geschrieben und durch erneutes Lesen bestätigt", m.state.value.message)
        assertEquals(3, m.state.value.data!!.flags); assertNull(j.pending)
        assertFalse(m.isVerificationPending(t)); assertTrue(m.state.value.canWrite)
        assertEquals(0, fresh.writes); assertEquals(1, writingPort.writes); assertEquals(1, callbacks)
        val message = m.state.value.message
        m.verificationNeedsRetap(t); assertEquals(message, m.state.value.message)
    }
    @Test fun differentTagAndDifferentContentCannotConfirmPendingVerification() {
        val j = Journal(); val m = model(j); val t = m.resume(true, true)
        m.tagDiscovered(t, "A", Port(null)); m.prepareWrite(NfcTagData(flags = 3))
        val written = Port(null).apply { requiresRediscovery = true }
        m.tagDiscovered(t, "A", written)
        val other = Port(written.bytes)
        m.tagDiscovered(t, "B", other)
        assertNotNull(j.pending); assertTrue(m.isVerificationPending(t)); assertFalse(m.state.value.canWrite)
        assertEquals("Tag gelesen · früherer Schreibausgang eines anderen Tags bleibt unklar", m.state.value.message)
        val different = Port(NfcTagDataCodec.encode(NfcTagData(flags = 4)))
        m.tagDiscovered(t, "A", different)
        assertEquals("Frühere Schreibung nicht bestätigt · tatsächlicher Inhalt gelesen", m.state.value.message)
        assertEquals(4, m.state.value.data!!.flags); assertNull(j.pending); assertFalse(m.isVerificationPending(t))
        assertEquals(1, written.writes); assertEquals(0, other.writes); assertEquals(0, different.writes)
    }
    @Test fun missingDiscoveryOffersRetapAndPauseKeepsJournalForRecreation() {
        val j = Journal(); val m = model(j); val t = m.resume(true, true)
        m.tagDiscovered(t, "A", Port(null)); m.prepareWrite(NfcTagData(flags = 3))
        val written = Port(null).apply { requiresRediscovery = true }
        m.tagDiscovered(t, "A", written)
        m.verificationNeedsRetap(t)
        assertEquals("Inhalt noch nicht bestätigt · Tag entfernen und denselben Tag erneut anhalten", m.state.value.message)
        assertNotNull(j.pending); assertFalse(m.state.value.canWrite); assertEquals(1, written.writes)
        m.pause(); assertFalse(m.isVerificationPending(t)); assertNotNull(j.pending)
        val pausedMessage = m.state.value.message
        m.verificationNeedsRetap(t); assertEquals(pausedMessage, m.state.value.message)
        val recreated = model(j); val next = recreated.resume(true, true)
        assertFalse(recreated.isVerificationPending(next)); assertFalse(recreated.state.value.canWrite)
        val fresh = Port(written.bytes); recreated.tagDiscovered(next, "A", fresh)
        assertEquals("Frühere Schreibung durch erneutes Lesen bestätigt", recreated.state.value.message)
        assertNull(j.pending); assertEquals(0, fresh.writes)
    }
    @Test fun failedRefreshCallbackKeepsVerificationPendingAndRequestsRetap() {
        val j = Journal(); val m = model(j, onVerificationRequired = { error("reader unavailable") })
        val t = m.resume(true, true); m.tagDiscovered(t, "A", Port(null)); m.prepareWrite(NfcTagData(flags = 3))
        val written = Port(null).apply { requiresRediscovery = true }; m.tagDiscovered(t, "A", written)
        assertTrue(m.isVerificationPending(t)); assertNotNull(j.pending); assertFalse(m.state.value.canWrite)
        assertEquals("Inhalt noch nicht bestätigt · Tag entfernen und denselben Tag erneut anhalten", m.state.value.message)
        assertEquals(1, written.writes)
    }
    @Test fun pauseDuringReadbackDoesNotStartRediscoveryOrClearJournal() {
        val j = Journal(); var callbacks = 0; val m = model(j, onVerificationRequired = { callbacks++ })
        val t = m.resume(true, true); m.tagDiscovered(t, "A", Port(null)); m.prepareWrite(NfcTagData(flags = 3))
        val written = Port(null).apply {
            requiresRediscovery = true
            afterReadback = { m.pause(); m.resume(true, true) }
        }
        m.tagDiscovered(t, "A", written)
        assertEquals(0, callbacks); assertNotNull(j.pending); assertFalse(m.isVerificationPending(t))
        assertFalse(m.state.value.selected); assertFalse(m.state.value.canWrite); assertEquals(1, written.writes)
    }
    @Test fun pausedFreshReadCannotClearPendingJournalOrPublishSuccess() {
        val j = Journal(); val m = model(j); val t = m.resume(true, true)
        m.tagDiscovered(t, "A", Port(null)); m.prepareWrite(NfcTagData(flags = 3))
        val written = Port(null).apply { requiresRediscovery = true }; m.tagDiscovered(t, "A", written)
        val fresh = Port(written.bytes).apply { afterRead = { m.pause(); m.resume(true, true) } }
        m.tagDiscovered(t, "A", fresh)
        assertNotNull(j.pending); assertFalse(m.state.value.selected); assertFalse(m.state.value.canWrite)
        assertEquals("Früherer Schreibausgang unklar · denselben Tag erneut lesen", m.state.value.message)
        assertFalse(m.isVerificationPending(t)); assertEquals(0, fresh.writes)
    }
    @Test fun closingOldWritePortDoesNotLoseOwnershipOfQueuedFreshPort() {
        val jobs = mutableListOf<Runnable>(); val j = Journal()
        val m = model(j, Executor { jobs.add(it) }); val t = m.resume(true, true)
        m.tagDiscovered(t, "A", Port(null)); jobs.removeAt(0).run(); m.prepareWrite(NfcTagData(flags = 3))
        val written = Port(null).apply { requiresRediscovery = true }
        val fresh = Port(NfcTagDataCodec.encode(NfcTagData(flags = 3)))
        m.onVerificationRequired = { token -> m.tagDiscovered(token, "A", fresh) }
        m.tagDiscovered(t, "A", written); jobs.removeAt(0).run()
        assertEquals(1, jobs.size); assertTrue(m.state.value.busy)
        m.pause(); assertTrue(fresh.closes > 0)
        jobs.removeAt(0).run()
        assertEquals(0, fresh.connects); assertEquals(0, fresh.writes); assertNotNull(j.pending)
        assertEquals(1, written.writes)
    }
}
