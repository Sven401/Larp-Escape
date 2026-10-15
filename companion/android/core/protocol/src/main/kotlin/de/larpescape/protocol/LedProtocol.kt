package de.larpescape.protocol

import java.security.MessageDigest
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

/** Independent LED control contract; this does not grant legacy game-master rights. */
data class LedStatus(val revision: Long, val nextRequest: Long, val deviceMillis: Long,
    val statusSequence: Long, val effect: Int, val flags: Int, val mode: Int,
    val wake: WakeStatus? = null, val matrix: MatrixStatus? = null) {
    val ready get() = flags and 1 != 0
    val pending get() = flags and 2 != 0
    val off get() = flags and 4 != 0
    val completed get() = flags and 8 != 0
    val fault get() = flags and 16 != 0
    val canStart get() = ready && !pending && !fault && mode in 0..1
    val canStop get() = ready && !pending && !fault && mode in 0..1
}
/** Four raw HIGH levels interpreted as active by the provisional box configuration. */
data class WakeStatus(val supported: Boolean, val valid: Boolean, val activeMask: Int) {
    init {
        require(activeMask in 0..15 && (supported || (!valid && activeMask == 0)) &&
            (valid || activeMask == 0))
    }
}

/** Row-major raw contacts 0..80; ambiguous means contacts cannot identify unique keys. */
data class MatrixStatus(val supported: Boolean, val valid: Boolean,
    val ambiguous: Boolean, val pressed: Set<Int>) {
    init {
        require(pressed.all { it in 0..80 } && (supported || !valid) &&
            (valid || (pressed.isEmpty() && !ambiguous)))
    }
}

data class LedResult(val request: Long, val outcome: Int, val status: LedStatus)

class LedProtocol(access: LedAccess, private val clientNonce: ByteArray) {
    private val secret = access.secret()
    private val device = access.deviceId()
    private var session: ByteArray? = null
    private var granted = false
    init { require(clientNonce.size == 16 && clientNonce.any { it != 0.toByte() }) }
    fun hello(): ByteArray = byteArrayOf(1, 1) + clientNonce
    fun proof(challenge: ByteArray): ByteArray {
        check(session == null) { "Unerwartete erneute Box-Anmeldung." }
        header(challenge, 0x81, 98)
        require(challenge.copyOfRange(2, 18).contentEquals(device) &&
            challenge.copyOfRange(50, 66).contentEquals(clientNonce)) { "Die Box passt nicht zum eingerichteten Zugang." }
        require(challenge.copyOfRange(18, 34).any { it != 0.toByte() } &&
            challenge.copyOfRange(34, 50).any { it != 0.toByte() }) { "Ungültige Box-Anmeldung." }
        verify("challenge", challenge, 66)
        val context = challenge.copyOfRange(2, 66)
        session = mac("session", context).copyOf(16)
        return byteArrayOf(1, 2) + mac("proof", context)
    }
    fun granted(bytes: ByteArray): LedStatus {
        check(!granted)
        val status = status(bytes, 0x82)
        granted = true
        return status
    }
    fun status(bytes: ByteArray): LedStatus { check(granted); return status(bytes, 0x84) }
    private fun status(bytes: ByteArray, type: Int): LedStatus {
        require(bytes.size == 70 || bytes.size == 82) { "Unbekannte Boxnachricht." }
        header(bytes, type, bytes.size); binding(bytes)
        verify("status", bytes, if (bytes.size == 82) 50 else 38)
        val wakeByte = byte(bytes, 37)
        val supported = wakeByte and 0x80 != 0
        val valid = wakeByte and 0x40 != 0
        val mask = wakeByte and 15
        require(wakeByte and 0x30 == 0 && (supported || wakeByte == 0) &&
            (valid || mask == 0)) { "Widersprüchlicher Wake-Status." }
        val matrix = if (bytes.size == 82) {
            val matrixFlags = byte(bytes, 38)
            require(matrixFlags and 0x1f == 0 && byte(bytes, 49) and 0xfe == 0) {
                "Widersprüchlicher Matrixstatus."
            }
            MatrixStatus(matrixFlags and 0x80 != 0, matrixFlags and 0x40 != 0,
                matrixFlags and 0x20 != 0,
                (0..80).filterTo(mutableSetOf()) { byte(bytes, 39 + it / 8) and (1 shl (it % 8)) != 0 })
        } else null
        return values(u32(bytes, 18), u32(bytes, 22), u32(bytes, 26), u32(bytes, 30),
            byte(bytes, 34), byte(bytes, 35), byte(bytes, 36), WakeStatus(supported, valid, mask), matrix)
    }
    fun command(status: LedStatus, effect: Int): ByteArray {
        check(granted)
        require(effect in 0..3 && if (effect == 0) status.canStop else status.canStart) { "LED-Bedienung derzeit gesperrt." }
        val value = byteArrayOf(1, 3) + session!! + le(status.nextRequest) + le(status.revision) +
            le((status.deviceMillis + 3000) and U32) + byteArrayOf(if (effect == 0) 2 else 1, effect.toByte())
        return value + mac("command", value)
    }
    fun result(bytes: ByteArray): LedResult {
        check(granted); header(bytes, 0x83, 74); binding(bytes); verify("result", bytes, 42)
        val request = u32(bytes, 18); val outcome = byte(bytes, 38)
        require(request != 0L && outcome in 0..11) { "Ungültige Befehlsantwort." }
        return LedResult(request, outcome, values(u32(bytes, 26), u32(bytes, 22), u32(bytes, 30),
            u32(bytes, 34), byte(bytes, 39), byte(bytes, 40), byte(bytes, 41)))
    }
    private fun values(revision: Long, next: Long, millis: Long, sequence: Long, effect: Int, flags: Int, mode: Int, wake: WakeStatus? = null, matrix: MatrixStatus? = null): LedStatus {
        require(next != 0L && sequence != 0L && effect in 0..3 && flags in 0..31 && mode in 0..4 &&
            (effect == 0) == (flags and 4 != 0) && !(flags and 1 != 0 && flags and 16 != 0)) { "Widersprüchlicher Boxstatus." }
        return LedStatus(revision, next, millis, sequence, effect, flags, mode, wake, matrix)
    }
    private fun binding(bytes: ByteArray) {
        require(session != null && MessageDigest.isEqual(session, bytes.copyOfRange(2, 18))) { "Antwort gehört zu einer anderen Verbindung." }
    }
    private fun verify(domain: String, bytes: ByteArray, offset: Int) {
        require(MessageDigest.isEqual(mac(domain, bytes.copyOf(offset)), bytes.copyOfRange(offset, bytes.size))) { "Box-Antwort konnte nicht bestätigt werden." }
    }
    private fun mac(domain: String, bytes: ByteArray): ByteArray = Mac.getInstance("HmacSHA256").run {
        init(SecretKeySpec(secret, "HmacSHA256")); update("LARP-LED/1/$domain".toByteArray(Charsets.US_ASCII)); doFinal(bytes)
    }
    fun close() { secret.fill(0); clientNonce.fill(0); session?.fill(0); session = null; granted = false }
    companion object {
        const val U32 = 0xffff_ffffL
        private fun byte(bytes: ByteArray, at: Int) = bytes[at].toInt() and 255
        private fun u32(bytes: ByteArray, at: Int): Long = (0..3).fold(0L) { value, i -> value or (byte(bytes, at + i).toLong() shl (i * 8)) }
        private fun le(value: Long) = ByteArray(4) { (value shr (it * 8)).toByte() }
        private fun header(bytes: ByteArray, type: Int, count: Int) {
            require(bytes.size == count && byte(bytes, 0) == 1 && byte(bytes, 1) == type) { "Unbekannte Boxnachricht." }
        }
    }
}

/** All transport values fit the default ATT MTU; no long-write or MTU negotiation required. */
object LedFrames {
    fun encode(message: ByteArray, transfer: Int): List<ByteArray> {
        require(message.size in 1..128 && transfer in 1..65535)
        return (message.indices step 12).map { offset ->
            byteArrayOf(1, 0x4c, transfer.toByte(), (transfer shr 8).toByte(), message.size.toByte(),
                (message.size shr 8).toByte(), offset.toByte(), (offset shr 8).toByte()) +
                message.copyOfRange(offset, minOf(offset + 12, message.size))
        }
    }
}
class LedFrameAssembler {
    private var transfer = 0
    private var completedTransfer = 0
    private var started = 0L
    private var expected = 0
    private var size = 0
    private var bytes = ByteArray(128)
    private var previous: ByteArray? = null
    fun accept(frame: ByteArray, now: Long): ByteArray? {
        require(frame.size in 9..20 && frame[0] == 1.toByte() && frame[1] == 0x4c.toByte()) { "Ungültiges BLE-Fragment." }
        val id = u16(frame, 2); val total = u16(frame, 4); val offset = u16(frame, 6)
        require(id != 0 && total in 1..128 && offset < total && offset + frame.size - 8 <= total)
        if (transfer != 0) require(now >= started && now - started < 2000) { "Boxnachricht unvollständig." }
        if (frame.contentEquals(previous)) return null
        if (transfer == 0) {
            require(id > completedTransfer && offset == 0) { "Veraltetes BLE-Fragment." }
            transfer = id; size = total; started = now; expected = 0
        }
        require(id == transfer && total == size && offset == expected) { "BLE-Fragmente sind nicht vollständig geordnet." }
        frame.copyInto(bytes, expected, 8); expected += frame.size - 8; previous = frame.copyOf()
        if (expected != size) return null
        val result = bytes.copyOf(size); completedTransfer = transfer; transfer = 0; previous = null
        return result
    }
    fun expired(now: Long): Boolean = transfer != 0 && (now < started || now - started >= 2000)
    private fun u16(bytes: ByteArray, at: Int) = (bytes[at].toInt() and 255) or ((bytes[at + 1].toInt() and 255) shl 8)
}

/** A GATT write ACK alone never completes a command. Result order is independent of status order. */
class LedCommandTracker {
    var request: Long? = null; private set
    private var writeAcknowledged = false
    private var reply: LedResult? = null
    fun begin(value: Long) { check(request == null && value in 1..LedProtocol.U32); request = value; writeAcknowledged = false; reply = null }
    fun wrote(): LedResult? { check(request != null); writeAcknowledged = true; return take() }
    fun received(result: LedResult): LedResult? {
        require(request == result.request) { "Befehlsantwort passt nicht zum offenen Auftrag." }
        check(reply == null) { "Doppelte Befehlsantwort." }; reply = result; return take()
    }
    private fun take(): LedResult? {
        if (!writeAcknowledged) return null
        return reply?.also { request = null; reply = null; writeAcknowledged = false }
    }
}
