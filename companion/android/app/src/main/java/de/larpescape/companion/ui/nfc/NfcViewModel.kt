package de.larpescape.companion.ui.nfc

import androidx.lifecycle.ViewModel
import de.larpescape.protocol.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.util.concurrent.Executor
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

interface NfcEnvelopeCodec {
    /** Null data means an empty NDEF tag; foreign/corrupt/ambiguous records throw. */
    fun read(message: ByteArray?): NfcTagData?
    fun replace(message: ByteArray?, payload: ByteArray): ByteArray
}
data class NfcPendingWrite(val id: String, val message: ByteArray)
interface NfcWriteJournal {
    fun load(): NfcPendingWrite?
    fun mark(value: NfcPendingWrite): Boolean
    fun clear(): Boolean
}
data class NfcUiState(
    val available: Boolean = false, val enabled: Boolean = false, val listening: Boolean = false,
    val busy: Boolean = false, val armed: Boolean = false, val canWrite: Boolean = false,
    val selected: Boolean = false, val tagId: String? = null, val data: NfcTagData? = null,
    val capacity: Int = 0, val selectionId: Long = 0, val readOnlySelectionId: Long = 0, val message: String = "NFC-Seite öffnen und Tag anhalten"
)

/** One foreground owner. Ports and whole NDEF snapshots never enter the UI. */
class NfcViewModel(
    private val envelope: NfcEnvelopeCodec,
    private val journal: NfcWriteJournal,
    private val worker: Executor = Executors.newSingleThreadExecutor(),
    private val cancellation: Executor = Executors.newSingleThreadExecutor(),
    private val event: (String) -> Unit = {},
    @Volatile var onVerificationRequired: (Long) -> Unit = {}
) : ViewModel() {
    private data class Snapshot(val id: String, val bytes: ByteArray?, val writable: Boolean, val capacity: Int)
    private data class Request(val snapshot: Snapshot, val message: ByteArray, val data: NfcTagData)
    private val lock = Any()
    private val mutable = MutableStateFlow(NfcUiState())
    val state = mutable.asStateFlow()
    private var epoch = 0L
    private var selection = 0L
    private var snapshot: Snapshot? = null
    private var request: Request? = null
    private var activePort: NfcMessagePort? = null
    private var writing = false
    private var uncertain = journal.load()
    private var verificationPending = false

    fun resume(available: Boolean, enabled: Boolean): Long {
        pause()
        return synchronized(lock) {
            mutable.value = NfcUiState(available = available, enabled = enabled,
                listening = available && enabled, message = when {
                    !available -> "Dieses Handy unterstützt kein NFC"
                    !enabled -> "NFC einschalten und die NFC-Seite erneut öffnen"
                    uncertain != null -> "Früherer Schreibausgang unklar · denselben Tag erneut lesen"
                    else -> "Tag ruhig an die NFC-Antenne halten"
                })
            epoch
        }
    }
    fun pause() {
        event("nfc_foreground_paused")
        val port = synchronized(lock) {
            ++epoch
            val message = if (writing) "Schreibausgang unklar · denselben Tag erneut lesen" else "NFC-Lesen pausiert"
            snapshot = null; request = null; writing = false; verificationPending = false
            mutable.value = mutable.value.copy(listening = false, busy = false, armed = false,
                selected = false, canWrite = false, tagId = null, data = null, capacity = 0, message = message)
            activePort.also { activePort = null }
        }
        if (port != null) cancellation.execute { try { port.close() } catch (_: Exception) { } }
    }
    fun isVerificationPending(token: Long): Boolean = synchronized(lock) {
        token == epoch && mutable.value.listening && verificationPending && uncertain != null
    }
    fun verificationNeedsRetap(token: Long, readerUnavailable: Boolean = false) = synchronized(lock) {
        if (isVerificationPending(token)) mutable.value = mutable.value.copy(
            message = if (readerUnavailable) "Prüfung unterbrochen · NFC-Seite erneut öffnen und denselben Tag anhalten"
                else "Inhalt noch nicht bestätigt · Tag entfernen und denselben Tag erneut anhalten")
    }
    fun cancelWrite() = synchronized(lock) {
        // A delayed button event cannot claim that an already-started RF write was undone.
        if (request == null || writing) return@synchronized
        request = null
        mutable.value = mutable.value.copy(armed = false, message = "Schreibauftrag abgebrochen")
    }
    fun prepareWrite(data: NfcTagData, expectedSelection: Long = state.value.selectionId) = synchronized(lock) {
        if (!mutable.value.listening || mutable.value.busy) return@synchronized
        if (expectedSelection != mutable.value.selectionId) {
            request = null
            mutable.value = mutable.value.copy(armed = false, message = "Tag inzwischen erneut gelesen · aktuelle Daten prüfen")
            return@synchronized
        }
        if (uncertain != null) {
            mutable.value = mutable.value.copy(message = "Zuerst den Tag des unklaren Schreibvorgangs erneut lesen")
            return@synchronized
        }
        val old = snapshot ?: return@synchronized
        if (!old.writable || old.id.isEmpty()) return@synchronized
        try {
            val payload = NfcTagDataCodec.encode(data)
            val message = envelope.replace(old.bytes?.copyOf(), payload)
            require(message.size <= old.capacity) { "Tag ist für diese Daten zu klein" }
            request = Request(old, message.copyOf(), NfcTagDataCodec.decode(payload))
            event("nfc_write_armed")
            mutable.value = mutable.value.copy(armed = true, message = "Schreiben bereit · Tag entfernen und denselben Tag erneut anhalten")
        } catch (e: Exception) {
            request = null
            mutable.value = mutable.value.copy(armed = false, message = e.message ?: "Tagdaten ungültig")
        }
    }
    fun unsupported(token: Long) = synchronized(lock) {
        if (token != epoch || !mutable.value.listening || mutable.value.busy) return@synchronized
        request = null; snapshot = null
        mutable.value = mutable.value.copy(armed = false, selected = false, canWrite = false,
            tagId = null, data = null, capacity = 0,
            message = "Dieser Tag unterstützt kein NDEF · Tag nicht verändert")
    }
    fun tagDiscovered(token: Long, id: String, port: NfcMessagePort) {
        val pending = synchronized(lock) {
            if (token != epoch || !mutable.value.listening || mutable.value.busy) return
            val pending = request
            request = null
            writing = pending != null
            activePort = port
            mutable.value = mutable.value.copy(busy = true, armed = false, canWrite = false,
                message = if (pending == null) "Tag wird gelesen …" else "Tag wird geschrieben und geprüft …")
            pending
        }
        worker.execute {
            fun current() = synchronized(lock) { epoch == token && mutable.value.listening }
            var writeOwnsPort = false
            try {
                if (!current()) return@execute
                if (pending != null) {
                    val intended = NfcPendingWrite(pending.snapshot.id, pending.message.copyOf())
                    val result = if (pending.snapshot.id != id) NfcWriteResult.CHANGED else {
                        check(journal.mark(intended)) { "Schreibauftrag konnte nicht gesichert werden · nichts geschrieben" }
                        synchronized(lock) { uncertain = intended }
                        writeOwnsPort = true
                        VerifiedNfcWrite.execute(port, pending.snapshot.bytes, pending.message, ::current)
                    }
                    event("nfc_write_result=$result")
                    if (result != NfcWriteResult.UNCERTAIN && result != NfcWriteResult.VERIFICATION_REQUIRED && journal.clear()) {
                        synchronized(lock) { uncertain = null }
                    }
                    synchronized(lock) {
                        if (!current()) { return@synchronized }
                        writing = false
                        if (activePort === port) activePort = null // Core has closed the write port before rediscovery.
                        verificationPending = result == NfcWriteResult.VERIFICATION_REQUIRED
                        if (result == NfcWriteResult.VERIFIED) {
                            snapshot = pending.snapshot.copy(bytes = pending.message.copyOf())
                            mutable.value = mutable.value.copy(data = pending.data, selectionId = ++selection, readOnlySelectionId = 0, canWrite = uncertain == null, busy = false,
                                message = "Geschrieben und durch erneutes Lesen bestätigt")
                        } else {
                            snapshot = null
                            mutable.value = mutable.value.copy(selected = false, canWrite = false, data = null,
                                tagId = null, busy = false, message = when (result) {
                                    NfcWriteResult.CHANGED -> "Anderer oder veränderter Tag · nichts geschrieben, erneut lesen"
                                    NfcWriteResult.READ_ONLY -> "Tag ist schreibgeschützt · nichts geschrieben"
                                    NfcWriteResult.TOO_SMALL -> "Tag ist zu klein · nichts geschrieben"
                                    NfcWriteResult.READ_FAILED -> "Tag nicht lesbar · nichts geschrieben, erneut anhalten"
                                    NfcWriteResult.CANCELLED -> "Schreibauftrag abgebrochen"
                                    NfcWriteResult.VERIFICATION_REQUIRED -> "Schreiben abgeschlossen · Inhalt wird geprüft …"
                                    else -> "Schreibausgang unklar · denselben Tag erneut lesen"
                                })
                        }
                    }
                    if (result == NfcWriteResult.VERIFICATION_REQUIRED && isVerificationPending(token)) {
                        try { onVerificationRequired(token) }
                        catch (_: Exception) { verificationNeedsRetap(token) }
                    }
                } else {
                    port.connect()
                    val bytes = port.read()?.copyOf()
                    val data = envelope.read(bytes?.copyOf())
                    val writable = port.writable
                    val capacity = port.capacity
                    val (previous, awaitingVerification) = synchronized(lock) { uncertain to verificationPending }
                    val recovered = previous != null && previous.id == id
                    val previousVerified = recovered && bytes?.contentEquals(previous!!.message) == true
                    event("nfc_read_valid empty=${data == null} recovered=$recovered previousVerified=$previousVerified")
                    synchronized(lock) {
                        if (!current()) { return@synchronized }
                        if (recovered) {
                            // Only a current discovery may resolve the pending write; a paused read keeps the journal.
                            if (journal.clear()) uncertain = null
                            verificationPending = false
                        }
                        snapshot = Snapshot(id, bytes, writable, capacity)
                        mutable.value = mutable.value.copy(selected = true, tagId = id, data = data, selectionId = ++selection,
                            readOnlySelectionId = if (!recovered && !awaitingVerification) selection else 0,
                            capacity = capacity, canWrite = writable && id.isNotEmpty() && uncertain == null, busy = false,
                            message = when {
                                previousVerified && awaitingVerification -> "Geschrieben und durch erneutes Lesen bestätigt"
                                previousVerified -> "Frühere Schreibung durch erneutes Lesen bestätigt"
                                recovered -> "Frühere Schreibung nicht bestätigt · tatsächlicher Inhalt gelesen"
                                previous != null -> "Tag gelesen · früherer Schreibausgang eines anderen Tags bleibt unklar"
                                !writable -> "Tag gelesen · schreibgeschützt"
                                data == null -> "Leerer NDEF-Tag gelesen"
                                else -> "Tagdaten gelesen · Prüfsumme bestätigt"
                            })
                    }
                }
            } catch (e: Exception) {
                event("nfc_io_failure=${e.javaClass.simpleName}")
                synchronized(lock) {
                    if (current()) {
                        snapshot = null
                        mutable.value = mutable.value.copy(selected = false, canWrite = false, busy = false,
                            tagId = null, data = null, message = e.message ?: "Tag nicht lesbar · erneut anhalten")
                    }
                }
            } finally {
                if (!writeOwnsPort) try { port.close() } catch (_: Exception) { }
                synchronized(lock) { if (token == epoch && activePort === port) { activePort = null; writing = false } }
            }
        }
    }
    override fun onCleared() {
        pause()
        (worker as? ExecutorService)?.shutdownNow()
        (cancellation as? ExecutorService)?.shutdown()
    }
}
