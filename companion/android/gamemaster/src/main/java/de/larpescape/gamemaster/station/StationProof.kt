package de.larpescape.gamemaster.station

import de.larpescape.protocol.LedAccess
import de.larpescape.protocol.LedProtocol
import de.larpescape.protocol.LedStatus
import java.security.SecureRandom

class AssignmentMismatch : IllegalArgumentException()
data class ProofReading(val reply: ByteArray? = null, val status: LedStatus? = null)

/** Only Hello/Proof can leave this adapter; it exposes no LED, NFC or game command. */
class StationProof(access: LedAccess, nonce: ByteArray = ByteArray(16).also { SecureRandom().nextBytes(it) }) {
    private val device = access.deviceId()
    private val protocol = LedProtocol(access, nonce)
    private var phase = 0
    private var sequence = 0L
    fun hello(): ByteArray { check(phase == 0); phase = 1; return protocol.hello() }
    fun receive(message: ByteArray): ProofReading {
        require(message.size >= 2)
        return when (message[1].toInt() and 255) {
            0x81 -> {
                check(phase == 1)
                if (message.size == 98 && !message.copyOfRange(2, 18).contentEquals(device)) throw AssignmentMismatch()
                val proof = protocol.proof(message); phase = 2; ProofReading(reply = proof)
            }
            0x82 -> { check(phase == 2); val status = protocol.granted(message); phase = 3; reading(status) }
            0x84 -> { check(phase == 3); reading(protocol.status(message)) }
            else -> error("Unerwartete Nachricht in der lesenden Stationsprüfung.")
        }
    }
    private fun reading(status: LedStatus): ProofReading {
        if (status.statusSequence <= sequence) return ProofReading()
        sequence = status.statusSequence
        return ProofReading(status = status)
    }
    fun close() { phase = -1; protocol.close(); device.fill(0) }
}
