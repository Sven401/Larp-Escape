package de.larpescape.companion.platform.nfc

import android.app.Activity
import android.content.Context
import android.nfc.NdefMessage
import android.nfc.NdefRecord
import android.nfc.NfcAdapter
import android.nfc.Tag
import android.nfc.tech.Ndef
import android.util.Base64
import android.util.Log
import de.larpescape.companion.BuildConfig
import de.larpescape.companion.ui.nfc.*
import de.larpescape.protocol.*

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

private class AndroidNdefPort(private val ndef: Ndef) : NfcMessagePort {
    private fun event(value: String) { if (BuildConfig.DEBUG) Log.i("LarpEscape", value) }
    override fun connect() = ndef.connect()
    override val writable get() = ndef.isWritable
    override val capacity get() = ndef.maxSize
    override fun read(): ByteArray? = try {
        ndef.ndefMessage?.toByteArray().also { event("nfc_rf_read_complete bytes=${it?.size ?: 0}") }
    } catch (e: Exception) { event("nfc_rf_read_failure=${e.javaClass.simpleName}"); throw e }
    override fun write(message: ByteArray) {
        event("nfc_rf_write_start")
        try { ndef.writeNdefMessage(NdefMessage(message)); event("nfc_rf_write_complete") }
        catch (e: Exception) { event("nfc_rf_write_failure=${e.javaClass.simpleName}"); throw e }
    }
    override fun close() = ndef.close()
}

/** Activity owns ReaderMode registration; ViewModel owns serialized I/O and the write request. */
class AndroidNfcReader(private val activity: Activity, private val model: NfcViewModel) {
    private val adapter = NfcAdapter.getDefaultAdapter(activity)
    private var registered = false
    fun start() {
        stop()
        val token = model.resume(adapter != null, adapter?.isEnabled == true)
        if (adapter?.isEnabled != true) return
        try {
            adapter.enableReaderMode(activity, { tag: Tag ->
                val ndef = Ndef.get(tag)
                if (ndef == null) model.unsupported(token)
                else model.tagDiscovered(token, tag.id.joinToString("") { "%02X".format(it.toInt() and 255) }, AndroidNdefPort(ndef))
            }, NfcAdapter.FLAG_READER_NFC_A or NfcAdapter.FLAG_READER_NFC_B or
                NfcAdapter.FLAG_READER_NFC_F or NfcAdapter.FLAG_READER_NFC_V, null)
            registered = true
        } catch (_: Exception) { model.pause() }
    }
    fun stop() {
        model.pause()
        if (registered) {
            try { adapter?.disableReaderMode(activity) } catch (_: Exception) { }
            registered = false
        }
    }
}
