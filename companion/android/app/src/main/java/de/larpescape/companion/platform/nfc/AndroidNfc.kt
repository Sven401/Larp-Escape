package de.larpescape.companion.platform.nfc

import android.content.Context
import android.nfc.NdefMessage
import android.nfc.NdefRecord
import android.nfc.tech.Ndef
import android.util.Base64
import android.util.Log
import de.larpescape.companion.BuildConfig
import de.larpescape.companion.ui.nfc.*
import de.larpescape.protocol.*
import java.io.IOException
import java.util.concurrent.atomic.AtomicBoolean

class AndroidNfcWriteJournal(context: Context) : NfcWriteJournal {
    private val preferences = context.getSharedPreferences("nfc-write-journal", Context.MODE_PRIVATE)
    override fun load(): NfcPendingWrite? {
        val id = preferences.getString("tag", null) ?: return null
        val bytes = preferences.getString("message", null) ?: ""
        // A damaged journal must keep writes blocked until this tag is read again.
        return NfcPendingWrite(id, try { Base64.decode(bytes, Base64.NO_WRAP) } catch (_: IllegalArgumentException) { byteArrayOf() })
    }
    override fun mark(value: NfcPendingWrite) = preferences.edit().putString("tag", value.id)
        .putString("message", Base64.encodeToString(value.message, Base64.NO_WRAP)).commit()
    override fun clear() = preferences.edit().clear().commit()
}

class AndroidNfcEnvelope : NfcEnvelopeCodec {
    private val mime = NfcTagDataCodec.MIME.toByteArray(Charsets.US_ASCII)
    private fun matching(record: NdefRecord) = record.tnf == NdefRecord.TNF_MIME_MEDIA && record.type.contentEquals(mime)
    override fun read(message: ByteArray?): NfcTagData? {
        if (message == null) return null
        val records = NdefMessage(message).records
        val matches = records.filter(::matching)
        if (matches.isEmpty() && records.all { it.tnf == NdefRecord.TNF_EMPTY }) return null
        require(matches.size == 1) { "Kein eindeutiger Spiel-Datensatz · Tag nicht verändert" }
        return NfcTagDataCodec.decode(matches.single().payload)
    }
    override fun replace(message: ByteArray?, payload: ByteArray): ByteArray {
        read(message) // Never overwrite a foreign/corrupt/ambiguous message by initializing it.
        val records = message?.let { NdefMessage(it).records }
        val next = if (records == null || records.all { it.tnf == NdefRecord.TNF_EMPTY }) {
            arrayOf(NdefRecord.createMime(NfcTagDataCodec.MIME, payload.copyOf()))
        } else records.map { record ->
            if (matching(record)) NdefRecord(record.tnf, record.type, record.id, payload.copyOf()) else record
        }.toTypedArray()
        return NdefMessage(next).toByteArray()
    }
}

/** Public live-NDEF API, isolated so its empty-discovery behaviour can be reproduced in tests. */
internal interface NdefIo : AutoCloseable {
    fun connect()
    val writable: Boolean
    val capacity: Int
    fun read(): ByteArray?
    fun write(message: ByteArray)
}

internal fun androidNdefPort(ndef: Ndef): NfcMessagePort = AndroidNdefPort(object : NdefIo {
    override fun connect() = ndef.connect()
    override val writable get() = ndef.isWritable
    override val capacity get() = ndef.maxSize
    override fun read() = ndef.ndefMessage?.toByteArray()
    override fun write(message: ByteArray) = ndef.writeNdefMessage(NdefMessage(message))
    override fun close() = ndef.close()
}) { value -> if (BuildConfig.DEBUG) Log.i("LarpEscape", value) }

internal class AndroidNdefPort(
    private val io: NdefIo,
    private val event: (String) -> Unit = {}
) : NfcMessagePort {
    private var beforeWriteWasEmpty = false
    private var writeCompleted = false
    private val closed = AtomicBoolean(false)
    private fun checkOpen() { if (closed.get()) throw IOException("NFC-Verbindung beendet") }
    override fun connect() {
        checkOpen()
        io.connect()
        // A concurrent foreground cancellation must never reopen this old tag.
        if (closed.get()) {
            io.close()
            throw IOException("NFC-Verbindung beendet")
        }
    }
    override val writable get() = io.writable
    override val capacity get() = io.capacity
    override fun read(): ByteArray? = try {
        checkOpen()
        io.read().also {
            if (!writeCompleted) beforeWriteWasEmpty = it == null
            // Android can return null from its remembered empty discovery without RF I/O.
            event("nfc_ndef_read_result bytes=${it?.size ?: 0} null=${it == null}")
        }
    } catch (e: Exception) { event("nfc_ndef_read_failure=${e.javaClass.simpleName}"); throw e }
    override fun write(message: ByteArray) {
        checkOpen()
        event("nfc_rf_write_start")
        try {
            io.write(message)
            writeCompleted = true
            event("nfc_rf_write_complete")
        }
        catch (e: Exception) { event("nfc_rf_write_failure=${e.javaClass.simpleName}"); throw e }
    }
    override fun readAfterWrite(): NfcReadback {
        val bytes = read()
        // Only the observed first-fill/null case asks for a new detection. Errors,
        // mismatches and unexpected nulls on nonempty tags still remain uncertain.
        return if (writeCompleted && beforeWriteWasEmpty && bytes == null) {
            event("nfc_readback_requires_rediscovery")
            NfcReadback.RequiresRediscovery
        } else {
            NfcReadback.Bytes(bytes)
        }
    }
    override fun close() {
        // Core and ViewModel both clean up. A second close must not reconnect
        // the obsolete Tag after ReaderMode has started its new discovery.
        if (closed.compareAndSet(false, true)) io.close()
    }
}
