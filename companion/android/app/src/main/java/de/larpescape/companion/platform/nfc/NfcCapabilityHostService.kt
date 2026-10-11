package de.larpescape.companion.platform.nfc

import android.content.Context
import android.content.pm.PackageManager
import android.nfc.NfcAdapter
import android.nfc.cardemulation.HostApduService
import android.os.Bundle
import android.os.SystemClock
import de.larpescape.companion.BuildConfig
import de.larpescape.companion.data.nfc.NfcCapabilityEvidenceJournalV1
import de.larpescape.companion.data.nfc.NfcCapabilityEvidenceV1
import de.larpescape.protocol.NfcCapabilityGateV1
import de.larpescape.protocol.NfcCapabilitySnapshotV1
import java.security.SecureRandom
import java.time.Instant
import java.util.concurrent.Executors

/** Process-local, volatile HCE gate. Process death removes the arm automatically. */
object NfcCapabilityRun {
    private val gate = NfcCapabilityGateV1(SystemClock::elapsedRealtime)
    private val random = SecureRandom()
    private val writer = Executors.newSingleThreadExecutor()
    private var journal: NfcCapabilityEvidenceJournalV1? = null
    private var latest: NfcCapabilityEvidenceV1? = null
    private var startedUtc: Instant? = null
    private var observedFeature = false
    private var observedEnabled = false

    private fun init(context: Context) {
        if (journal != null) return
        journal = NfcCapabilityEvidenceJournalV1(context.filesDir.resolve("nfc-capability-v1"))
        latest = journal!!.loadRecovered()
    }
    private fun update() {
        val start = startedUtc ?: return
        val next = journal!!.fromSnapshot(gate.snapshot(),
            "${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE})", start,
            observedFeature, observedEnabled)
        if (next == latest) return
        latest = next
        writer.execute { journal!!.save(next) }
    }
    @Synchronized fun start(context: Context): Boolean {
        init(context.applicationContext)
        observedFeature = context.packageManager.hasSystemFeature(PackageManager.FEATURE_NFC_HOST_CARD_EMULATION)
        observedEnabled = NfcAdapter.getDefaultAdapter(context)?.isEnabled == true
        if (!observedFeature || !observedEnabled || !gate.arm(ByteArray(16).also(random::nextBytes))) return false
        startedUtc = Instant.now()
        update()
        return true
    }
    @Synchronized fun stop() { gate.stop(); update() }
    @Synchronized fun snapshot(context: Context): NfcCapabilitySnapshotV1 {
        init(context.applicationContext)
        return gate.snapshot().also { update() }
    }
    @Synchronized fun report(context: Context): NfcCapabilityEvidenceV1? {
        init(context.applicationContext)
        update()
        return latest
    }
    @Synchronized fun exportBytes(context: Context): ByteArray? {
        init(context.applicationContext)
        update()
        return latest?.let { journal!!.exportBytes(it) }
    }
    @Synchronized internal fun process(apdu: ByteArray): ByteArray = gate.process(apdu).also { update() }
    @Synchronized internal fun deactivate() { gate.deactivate(); update() }
}

/** Development-only AID service, declared only by the debug manifest. */
class NfcCapabilityHostService : HostApduService() {
    override fun processCommandApdu(commandApdu: ByteArray, extras: Bundle?): ByteArray =
        NfcCapabilityRun.process(commandApdu)

    override fun onDeactivated(reason: Int) {
        NfcCapabilityRun.deactivate()
    }
}
