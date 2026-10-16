package de.larpescape.companion.platform.nfc

import android.app.Activity
import android.content.ComponentName
import android.content.Context
import android.content.pm.PackageManager
import android.nfc.NfcAdapter
import android.nfc.cardemulation.CardEmulation
import android.nfc.cardemulation.HostApduService
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import de.larpescape.companion.BuildConfig
import de.larpescape.protocol.NfcTestSession
import de.larpescape.protocol.NfcTestState
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.lang.ref.WeakReference

/** Process-local only. No persisted arm, boot receiver, or background activation. */
object PhoneNfcTest {
    private val session = NfcTestSession(SystemClock::elapsedRealtime)
    private val mutable = MutableStateFlow(NfcTestState())
    val state = mutable.asStateFlow()
    private val handler = Handler(Looper.getMainLooper())
    private var owner: WeakReference<Activity>? = null
    private val timeout = object : Runnable {
        override fun run() {
            publish()
            if (mutable.value.active) handler.postDelayed(this, 500)
            else owner?.get()?.let { stop(it, mutable.value.note) }
        }
    }
    fun supported(activity: Activity): Boolean = activity.packageManager.hasSystemFeature(PackageManager.FEATURE_NFC_HOST_CARD_EMULATION)
    fun start(activity: Activity) {
        stop(activity)
        val adapter = NfcAdapter.getDefaultAdapter(activity)
        if (!supported(activity) || adapter?.isEnabled != true) {
            session.stop("NFC einschalten · dieses Handy muss Kartenemulation unterstützen"); publish(); return
        }
        try {
            val emulation = CardEmulation.getInstance(adapter)
            val component = ComponentName(activity, PhoneNfcTestService::class.java)
            val registered = emulation.registerAidsForService(component, CardEmulation.CATEGORY_OTHER, listOf("D2760000850101"))
            val accepted = registered && emulation.setPreferredService(activity, component)
            if (!accepted) { stop(activity, "Testtag konnte nicht aktiviert werden"); return }
            owner = WeakReference(activity)
            session.start(); publish(); handler.postDelayed(timeout, 500)
        } catch (_: Exception) {
            stop(activity, "Testtag konnte nicht aktiviert werden")
        }
    }
    fun stop(activity: Activity, reason: String = "Testtag deaktiviert · erneut bewusst aktivieren") {
        handler.removeCallbacks(timeout)
        owner = null
        session.stop(reason); publish()
        try { NfcAdapter.getDefaultAdapter(activity)?.let { CardEmulation.getInstance(it).unsetPreferredService(activity) } }
        catch (_: Exception) { }
        clearInactiveRegistration(activity)
    }
    /** Also called on cold service creation to remove routing left by a killed process. */
    fun clearInactiveRegistration(context: Context) {
        if (session.state().active) return
        try { NfcAdapter.getDefaultAdapter(context)?.let {
            CardEmulation.getInstance(it).removeAidsForService(
                ComponentName(context, PhoneNfcTestService::class.java), CardEmulation.CATEGORY_OTHER)
        } } catch (_: Exception) { }
    }
    fun process(bytes: ByteArray): ByteArray {
        val response = session.process(bytes)
        publish()
        if (BuildConfig.DEBUG) {
            val s = mutable.value
            Log.i("LarpEscape", "nfc_test_apdu ins=${bytes.getOrNull(1)?.toInt()?.and(255)} sw=${response.takeLast(2).joinToString("") { "%02X".format(it.toInt() and 255) }} reads=${s.reads} updates=${s.updates} commits=${s.commits} verified=${s.verified}")
        }
        return response
    }
    fun linkLost() { session.linkLost(); publish() }
    private fun publish() { mutable.value = session.state() }
}

class PhoneNfcTestService : HostApduService() {
    override fun onCreate() { super.onCreate(); PhoneNfcTest.clearInactiveRegistration(this) }
    override fun processCommandApdu(commandApdu: ByteArray?, extras: Bundle?): ByteArray =
        PhoneNfcTest.process(commandApdu ?: byteArrayOf())
    override fun onDeactivated(reason: Int) = PhoneNfcTest.linkLost()
}
