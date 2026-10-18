package de.larpescape.protocol

import java.security.MessageDigest
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

/** Independent LED control contract; this does not grant legacy game-master rights. */
data class LedStatus(val revision: Long, val nextRequest: Long, val deviceMillis: Long,
    val statusSequence: Long, val effect: Int, val flags: Int, val mode: Int, val wake: WakeStatus? = null,
    val nfcSupported: Boolean = false, val matrix: MatrixStatus? = null, val audio: AudioStatus? = null,
    val espNowSupported: Boolean? = null, val espNowReady: Boolean? = null, val rssiSupported: Boolean? = null) {
    val ready get() = flags and 1 != 0
    val pending get() = flags and 2 != 0
    val off get() = flags and 4 != 0
    val completed get() = flags and 8 != 0
    val fault get() = flags and 16 != 0
    val canStart get() = ready && !pending && !fault && mode in 0..1
    val canStop get() = ready && !pending && !fault && mode in 0..1
}
/** UART delivery and the raw BUSY pin are observations, never proof of audible sound. */
data class AudioStatus(val revision: Long = 0, val track: Int = 0, val volume: Int = 0,
    val flags: Int = 0, val supported: Boolean = true) {
    init {
        require(revision in 0..LedProtocol.U32 && track in 0..255 && volume in 0..10 && flags in 0..127 &&
            (flags and 16 == 0 || flags and 8 != 0) &&
            !(flags and 1 != 0 && flags and 32 != 0) &&
            (flags and 68 == 0 || (track in 1..255 && volume in 1..10)) && flags and 68 != 68 &&
            (supported || (revision == 0L && track == 0 && volume == 0 && flags == 0))) { "Widersprüchlicher Audiostatus." }
    }
    val ready get() = supported && flags and 1 != 0
    val pending get() = flags and 2 != 0
    val active get() = flags and 4 != 0
    val paused get() = flags and 64 != 0
    val busyValid get() = flags and 8 != 0
    val busyLow get() = flags and 16 != 0
    val fault get() = flags and 32 != 0
    val canStart get() = ready && !pending && !fault
    val canStop get() = ready && !pending && !fault
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
data class EspNowResult(val led: LedResult, val accepted: Boolean, val link: Int, val received: Boolean,
    val peer: ByteArray, val token: ByteArray, val receiverCount: Long, val receiverBoot: Long,
    val rssi: Int? = null, val filteredRssi: Int? = null, val intensity: Int? = null)

fun espNowPeer(text: String): ByteArray? {
    if (!Regex("[0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){5}").matches(text.trim())) return null
    val bytes = text.trim().split(':').map { it.toInt(16).toByte() }.toByteArray()
    return bytes.takeIf { it.any { b -> b != 0.toByte() } && it[0].toInt() and 1 == 0 }
}

class LedProtocol(access: LedAccess, private val clientNonce: ByteArray) {
    private val secret = access.secret()
    private val device = access.deviceId()
    private var session: ByteArray? = null
    private var granted = false
    init { require(clientNonce.size == 16 && clientNonce.any { it != 0.toByte() }) }
    fun hello(): ByteArray = byteArrayOf(2, 1) + clientNonce
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
        return byteArrayOf(2, 2) + mac("proof", context)
    }
    fun granted(bytes: ByteArray): LedStatus {
        check(!granted)
        val status = status(bytes, 0x82)
        granted = true
        return status
    }
    fun status(bytes: ByteArray): LedStatus { check(granted); return status(bytes, 0x84) }
    private fun status(bytes: ByteArray, type: Int): LedStatus {
        require(bytes.size == 91) { "Unbekannte Boxnachricht." }
        header(bytes, type, bytes.size); binding(bytes); verify("status", bytes, bytes.size - 32)
        val wakeByte = byte(bytes, 37)
        val supported = wakeByte and 0x80 != 0
        val valid = wakeByte and 0x40 != 0
        val mask = wakeByte and 15
        require(wakeByte and 0x10 == 0 && (supported || wakeByte and 0xdf == 0) &&
            (valid || mask == 0)) { "Widersprüchlicher Wake-Status." }
        val matrix = run {
            val matrixFlags = byte(bytes, 47)
            require(matrixFlags and 0x1f == 0 && byte(bytes, 58) and 0xfe == 0) {
                "Widersprüchlicher Matrixstatus."
            }
            MatrixStatus(matrixFlags and 0x80 != 0, matrixFlags and 0x40 != 0,
                matrixFlags and 0x20 != 0,
                (0..80).filterTo(mutableSetOf()) { byte(bytes, 48 + it / 8) and (1 shl (it % 8)) != 0 })
        }
        val capabilities = byte(bytes, 38)
        require(capabilities in 0..15 && (capabilities and 12 == 0 || capabilities and 2 != 0)) { "Unbekannte Boxfähigkeiten." }
        val audioSupported = capabilities and 1 != 0
        val audio = audioValues(bytes, 39, audioSupported)
        return values(u32(bytes, 18), u32(bytes, 22), u32(bytes, 26), u32(bytes, 30),
            byte(bytes, 34), byte(bytes, 35), byte(bytes, 36), WakeStatus(supported, valid, mask), wakeByte and 0x20 != 0, matrix,
            audio).copy(espNowSupported = capabilities and 2 != 0, espNowReady = capabilities and 4 != 0, rssiSupported = capabilities and 8 != 0)
    }
    fun command(status: LedStatus, effect: Int): ByteArray {
        check(granted)
        require(effect in 0..3 && if (effect == 0) status.canStop else status.canStart) { "LED-Bedienung derzeit gesperrt." }
        val value = byteArrayOf(2, 3) + session!! + le(status.nextRequest) + le(status.revision) +
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
    fun audioCommand(status: LedStatus, start: Boolean, track: Int = 0, volume: Int = 0): ByteArray {
        check(granted)
        val audio = status.audio ?: error("Bestätigter Audiostatus fehlt.")
        require(status.mode in 0..1 && if (start) audio.canStart else audio.canStop) { "Audio-Bedienung derzeit gesperrt." }
        require(if (start) track in 1..255 && volume in 1..10 else track == 0 && volume == 0) { "Ungültiger Audiotitel oder Lautstärke." }
        val value = byteArrayOf(2, 4) + session!! + le(status.nextRequest) + le(audio.revision) +
            le((status.deviceMillis + 3000) and U32) + byteArrayOf(if (start) 1 else 2,
                track.toByte(), (track shr 8).toByte(), volume.toByte())
        return value + mac("audio-command", value)
    }
    fun audioAction(status: LedStatus, operation: Int, volume: Int = 0): ByteArray {
        check(granted)
        val audio = status.audio ?: error("Audiostatus fehlt.")
        require(status.mode in 0..1 && audio.canStop)
        require(when(operation) {
            3 -> audio.active && volume == 0
            4 -> audio.paused && volume == 0
            5 -> volume in 1..10
            else -> false
        })
        val value = byteArrayOf(2, 4) + session!! + le(status.nextRequest) + le(audio.revision) +
            le((status.deviceMillis + 3000) and U32) + byteArrayOf(operation.toByte(), 0, 0, volume.toByte())
        return value + mac("audio-command", value)
    }
    fun audioResult(bytes: ByteArray): LedResult {
        check(granted); header(bytes, 0x85, 82); binding(bytes); verify("audio-result", bytes, 50)
        val request = u32(bytes, 18); val outcome = byte(bytes, 38)
        require(request != 0L && outcome in 0..11) { "Ungültige Audio-Befehlsantwort." }
        return LedResult(request, outcome, values(u32(bytes, 26), u32(bytes, 22), u32(bytes, 30),
            u32(bytes, 34), byte(bytes, 39), byte(bytes, 40), byte(bytes, 41), audio = audioValues(bytes, 42)))
    }
    fun espNowCommand(status: LedStatus, peer: ByteArray, rssi: Boolean = false, twinkle: Boolean = false): ByteArray {
        check(granted)
        require(status.espNowSupported == true && status.espNowReady == true && status.mode in 0..1)
        require(peer.size == 6 && peer.any { it != 0.toByte() } && peer[0].toInt() and 1 == 0)
        require(status.nextRequest in 1 until U32)
        require(!rssi || status.rssiSupported == true)
        val body = byteArrayOf(2, if (rssi) 7 else 6) + session!! + le(status.nextRequest) + le(0) +
            le((status.deviceMillis + 3000) and U32) + byteArrayOf(if (rssi && twinkle) 2 else 1, 1) + peer
        return body + mac(if (rssi) "rssi-command" else "espnow-command", body)
    }
    fun espNowResult(bytes: ByteArray, rssi: Boolean = false): EspNowResult {
        check(granted); header(bytes, if (rssi) 0x88 else 0x87, if (rssi) 112 else 108); binding(bytes)
        verify(if (rssi) "rssi-result" else "espnow-result", bytes, if (rssi) 80 else 76)
        val request = u32(bytes, 18); val outcome = byte(bytes, 38)
        val accepted = byte(bytes, 42); val link = byte(bytes, 43); val received = byte(bytes, 44)
        val peer = bytes.copyOfRange(46, 52); val token = bytes.copyOfRange(52, 68)
        val count = u32(bytes, 68); val boot = u32(bytes, 72)
        require(request != 0L && outcome in 0..13 && accepted in 0..1 && link in 0..2 && received in 0..1 && byte(bytes,45) == 1)
        require((link == 0 || accepted == 1) && (received == 0 || accepted == 1))
        require((outcome == 0) == (received == 1)) { "Empfang und Ergebnis widersprechen sich." }
        require(received == 1 || (count == 0L && boot == 0L))
        if (accepted == 1 || received == 1) {
            require(peer.any { it != 0.toByte() } && peer[0].toInt() and 1 == 0 && token.any { it != 0.toByte() })
        }
        require(received == 0 || count != 0L)
        require(outcome != 12 || (accepted == 1 && link == 2 && received == 0))
        require(outcome != 13 || (accepted == 1 && received == 0))
        val validRssi = rssi && byte(bytes, 76) == 1
        if (rssi) {
            require(byte(bytes,76) in 0..1 && validRssi == (received == 1))
            if (validRssi) require(bytes[77].toInt() in -127..-1 && bytes[78].toInt() in -127..-1 && byte(bytes,79) in 20..100)
            else require((77..79).all { byte(bytes,it) == 0 })
        }
        return EspNowResult(LedResult(request, outcome, values(u32(bytes,26), u32(bytes,22),
            u32(bytes,30), u32(bytes,34), byte(bytes,39), byte(bytes,40), byte(bytes,41))),
            accepted == 1, link, received == 1, peer, token, count, boot,
            if (validRssi) bytes[77].toInt() else null, if (validRssi) bytes[78].toInt() else null,
            if (validRssi) byte(bytes,79) else null)
    }
    private fun audioValues(bytes: ByteArray, at: Int, supported: Boolean = true) = AudioStatus(u32(bytes, at),
        byte(bytes, at + 4) or (byte(bytes, at + 5) shl 8), byte(bytes, at + 6), byte(bytes, at + 7), supported)
    private fun values(revision: Long, next: Long, millis: Long, sequence: Long, effect: Int, flags: Int, mode: Int,
        wake: WakeStatus? = null, nfcSupported: Boolean = false, matrix: MatrixStatus? = null, audio: AudioStatus? = null): LedStatus {
        require(next != 0L && sequence != 0L && effect in 0..3 && flags in 0..31 && mode in 0..4 &&
            (effect == 0) == (flags and 4 != 0) && !(flags and 1 != 0 && flags and 16 != 0)) { "Widersprüchlicher Boxstatus." }
        return LedStatus(revision, next, millis, sequence, effect, flags, mode, wake, nfcSupported, matrix, audio)
    }
    private fun binding(bytes: ByteArray) {
        require(session != null && MessageDigest.isEqual(session, bytes.copyOfRange(2, 18))) { "Antwort gehört zu einer anderen Verbindung." }
    }
    private fun verify(domain: String, bytes: ByteArray, offset: Int) {
        require(MessageDigest.isEqual(mac(domain, bytes.copyOf(offset)), bytes.copyOfRange(offset, bytes.size))) { "Box-Antwort konnte nicht bestätigt werden." }
    }
    private fun mac(domain: String, bytes: ByteArray): ByteArray = Mac.getInstance("HmacSHA256").run {
        init(SecretKeySpec(secret, "HmacSHA256")); update("LARP-LED/2/$domain".toByteArray(Charsets.US_ASCII)); doFinal(bytes)
    }
    private fun nfcMac(domain: String, bytes: ByteArray): ByteArray = Mac.getInstance("HmacSHA256").run {
        init(SecretKeySpec(secret, "HmacSHA256")); update("LARP-NFC/2/$domain".toByteArray(Charsets.US_ASCII)); doFinal(bytes)
    }
    fun nfcCommand(request: Long, deviceMillis: Long, uid: ByteArray? = null): ByteArray {
        check(granted); require(request in 1 until U32 && (uid == null || uid.size == 7))
        val body = byteArrayOf(2, 5) + session!! + le(request) + le((deviceMillis + 3000) and U32) +
            byteArrayOf(if (uid == null) 1 else 2, (uid?.size ?: 0).toByte()) + (uid ?: byteArrayOf()).copyOf(10)
        return body + nfcMac("command", body)
    }
    fun nfcResult(bytes: ByteArray): BoxNfcResult {
        check(granted); header(bytes, 0x86, 105); binding(bytes)
        require(MessageDigest.isEqual(nfcMac("result", bytes.copyOf(73)), bytes.copyOfRange(73, 105)))
        val size = byte(bytes, 36); val payloadSize = byte(bytes, 47)
        val outcome = byte(bytes, 34); val kind = byte(bytes, 35)
        require(size in 0..10 && payloadSize in listOf(0, 25) && outcome in 0..13 && kind in 0..4)
        require(u32(bytes, 18) != 0L && u32(bytes, 22) != 0L && u32(bytes, 26) != 0L)
        val data = if (payloadSize == 25) NfcTagDataCodec.decode(bytes.copyOfRange(48, 73)) else null
        require(data == null || data == NfcTestFixture.initial || data == NfcTestFixture.changed)
        require(outcome != 8 || (size == 7 && kind in 1..4 && data == NfcTestFixture.changed))
        return BoxNfcResult(u32(bytes, 18), u32(bytes, 22), u32(bytes, 26), outcome, kind,
            bytes.copyOfRange(37, 37 + size), data)
    }
    fun close() { secret.fill(0); clientNonce.fill(0); session?.fill(0); session = null; granted = false }
    companion object {
        const val U32 = 0xffff_ffffL
        private fun byte(bytes: ByteArray, at: Int) = bytes[at].toInt() and 255
        private fun u32(bytes: ByteArray, at: Int): Long = (0..3).fold(0L) { value, i -> value or (byte(bytes, at + i).toLong() shl (i * 8)) }
        private fun le(value: Long) = ByteArray(4) { (value shr (it * 8)).toByte() }
        private fun header(bytes: ByteArray, type: Int, count: Int) {
            require(bytes.isEmpty() || byte(bytes, 0) == 2) { "App und Box-Firmware verwenden unterschiedliche Protokollversionen. Passendes Paar erforderlich." }
            require(bytes.size == count && byte(bytes, 0) == 2 && byte(bytes, 1) == type) { "Unbekannte Boxnachricht." }
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
