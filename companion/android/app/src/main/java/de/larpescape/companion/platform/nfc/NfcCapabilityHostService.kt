package de.larpescape.companion.platform.nfc

import android.nfc.cardemulation.HostApduService
import android.os.Bundle
import android.os.SystemClock
import de.larpescape.protocol.NfcCapabilityGateV1
import de.larpescape.protocol.NfcCapabilitySnapshotV1
import java.security.SecureRandom

/** Process-local, volatile HCE gate. Process death removes the arm automatically. */
object NfcCapabilityRun {
    private val gate = NfcCapabilityGateV1(SystemClock::elapsedRealtime)
    private val random = SecureRandom()
    fun start(): Boolean = gate.arm(ByteArray(16).also(random::nextBytes))
    fun stop() = gate.stop()
    fun snapshot(): NfcCapabilitySnapshotV1 = gate.snapshot()
    internal fun process(apdu: ByteArray): ByteArray = gate.process(apdu)
    internal fun deactivate() = gate.deactivate()
}

/** Development-only AID service, declared only by the debug manifest. */
class NfcCapabilityHostService : HostApduService() {
    override fun processCommandApdu(commandApdu: ByteArray, extras: Bundle?): ByteArray =
        NfcCapabilityRun.process(commandApdu)

    override fun onDeactivated(reason: Int) {
        NfcCapabilityRun.deactivate()
    }
}
