package de.larpescape.companion.platform.nfc

import android.app.Activity
import android.nfc.NfcAdapter
import android.nfc.Tag
import android.nfc.tech.Ndef
import android.os.Handler
import android.os.Looper
import android.util.Log
import de.larpescape.companion.BuildConfig
import de.larpescape.companion.ui.nfc.NfcViewModel

/** Activity owns ReaderMode registration; ViewModel owns serialized I/O and the write request. */
class AndroidNfcReader(private val activity: Activity, private val model: NfcViewModel) {
    private val adapter = NfcAdapter.getDefaultAdapter(activity)
    private val main = Handler(Looper.getMainLooper())
    private var registered = false
    private var token: Long? = null
    private var generation = 0L
    private var verificationGeneration: Long? = null
    private var lastTag: Tag? = null
    private var retiredTag: Tag? = null
    private var timeout: Runnable? = null
    private var verificationListener: ((Long) -> Unit)? = null

    fun start() {
        stop()
        val session = model.resume(adapter != null, adapter?.isEnabled == true)
        if (adapter?.isEnabled != true) return
        token = session
        val listener: (Long) -> Unit = { requestedSession ->
            main.post {
                // Bind to this foreground session, not to a reader generation: a later write
                // in the same session must be able to request its own single fresh detection.
                if (requestedSession == session && token == session && registered) {
                    refreshForVerification(session)
                }
            }
        }
        verificationListener = listener
        model.onVerificationRequired = listener
        try {
            registered = true
            register(session, ++generation)
        } catch (_: Exception) {
            stop()
        }
    }

    private fun register(session: Long, callbackGeneration: Long) {
        adapter?.enableReaderMode(activity, { tag: Tag ->
            // Serialize callback admission with stop/refresh on the activity's main thread.
            main.post {
                if (!registered || token != session || generation != callbackGeneration) return@post
                val pending = model.isVerificationPending(session)
                if (pending && (verificationGeneration != callbackGeneration || tag === retiredTag)) return@post
                if (!pending) verificationGeneration = null
                if (pending) event("nfc_reader_rediscovered")
                lastTag = tag
                val ndef = Ndef.get(tag)
                if (ndef == null) model.unsupported(session)
                else model.tagDiscovered(session,
                    tag.id.joinToString("") { "%02X".format(it.toInt() and 255) }, androidNdefPort(ndef))
            }
        }, NfcAdapter.FLAG_READER_NFC_A or NfcAdapter.FLAG_READER_NFC_B or
            NfcAdapter.FLAG_READER_NFC_F or NfcAdapter.FLAG_READER_NFC_V, null)
    }

    private fun refreshForVerification(session: Long) {
        if (!registered || token != session || !model.isVerificationPending(session)) return
        timeout?.let(main::removeCallbacks)
        timeout = null
        retiredTag = lastTag
        val freshGeneration = ++generation
        verificationGeneration = freshGeneration
        event("nfc_reader_refresh_requested")
        try {
            // The old write port is already closed. Restart discovery using public APIs;
            // keeping the model epoch preserves its read-only pending verification.
            adapter?.disableReaderMode(activity)
            register(session, freshGeneration)
            val deadline = Runnable {
                timeout = null
                if (registered && token == session && generation == freshGeneration &&
                    model.isVerificationPending(session)) {
                    event("nfc_reader_refresh_timeout")
                    model.verificationNeedsRetap(session)
                }
            }
            timeout = deadline
            main.postDelayed(deadline, 2_000L)
        } catch (e: Exception) {
            event("nfc_reader_refresh_failure=${e.javaClass.simpleName}")
            model.verificationNeedsRetap(session, readerUnavailable = true)
        }
    }

    fun stop() {
        val listener = verificationListener
        if (listener != null && model.onVerificationRequired === listener) model.onVerificationRequired = {}
        verificationListener = null
        token = null
        ++generation
        verificationGeneration = null
        lastTag = null
        retiredTag = null
        main.removeCallbacksAndMessages(null)
        timeout = null
        model.pause()
        if (registered) {
            try { adapter?.disableReaderMode(activity) } catch (_: Exception) { }
            registered = false
        }
    }

    private fun event(value: String) { if (BuildConfig.DEBUG) Log.i("LarpEscape", value) }
}
