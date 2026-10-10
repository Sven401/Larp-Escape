package de.larpescape.protocol

import java.math.BigInteger

/** Client-side bytes for the firmware's optional peer-auth v1 service. No value here grants access. */
class PeerChallenge internal constructor(private val value: ByteArray) {
    val sequence: Long get() = PeerAuthV1.u32(value, 56)
    fun transcript(): ByteArray = value.copyOf()
    fun peerId(): ByteArray = value.copyOfRange(60, 76)
    fun deviceId(): ByteArray = value.copyOfRange(20, 36)
}

enum class PeerAuthState { RECEIVING, REJECTED, GRANT_PENDING, AUTHENTICATED, GRANTED }

class PeerAuthResult internal constructor(
    val state: PeerAuthState,
    val reason: Int,
    val message: Int,
    val received: Int,
    val sequence: Long,
    val principal: Long,
    private val peer: ByteArray,
) {
    fun peerId(): ByteArray = peer.copyOf()
}

object PeerAuthV1 {
    private val prefix = "LARP-PEER-P256-1".toByteArray(Charsets.US_ASCII)
    private val p256Order = BigInteger("FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551", 16)

    internal fun u16(bytes: ByteArray, at: Int): Int =
        (bytes[at].toInt() and 255) or ((bytes[at + 1].toInt() and 255) shl 8)

    internal fun u32(bytes: ByteArray, at: Int): Long =
        (u16(bytes, at).toLong() or (u16(bytes, at + 2).toLong() shl 16)) and 0xffff_ffffL

    private fun nonzero(bytes: ByteArray) = bytes.any { it.toInt() != 0 }
    private fun id(bytes: ByteArray) = bytes.size == 16 && nonzero(bytes)

    fun issue(peerId: ByteArray): ByteArray {
        require(id(peerId)) { "Peerkennung muss 16 nichtleere Bytes enthalten." }
        return byteArrayOf(1, 11) + peerId.copyOf()
    }

    /** Expected IDs must come from trusted setup; the public device ID is not server proof. */
    fun challenge(bytes: ByteArray, expectedDeviceId: ByteArray, expectedPeerId: ByteArray): PeerChallenge {
        require(id(expectedDeviceId) && id(expectedPeerId)) { "Erwartete Kennungen fehlen." }
        require(bytes.size == 108 && bytes.copyOfRange(0, 16).contentEquals(prefix)) { "Challengeformat ungültig." }
        require(bytes[16] == 1.toByte() && bytes[17] == 9.toByte() && bytes[18] == 1.toByte() &&
            bytes[19] == 0.toByte() && bytes[55] == 0.toByte()) { "Challengekopf ungültig." }
        require(bytes.copyOfRange(20, 36).contentEquals(expectedDeviceId) &&
            bytes.copyOfRange(60, 76).contentEquals(expectedPeerId)) { "Challengekennung stimmt nicht überein." }
        val interfaceId = bytes[54].toInt() and 255
        require((u32(bytes, 36) != 0L || u32(bytes, 40) != 0L) && u32(bytes, 44) != 0L &&
            u32(bytes, 48) != 0L && interfaceId in 1..254 && u16(bytes, 52) and 255 == interfaceId &&
            u32(bytes, 56) != 0L && nonzero(bytes.copyOfRange(76, 108))) { "Challengebindung ungültig." }
        return PeerChallenge(bytes.copyOf())
    }

    private fun canonicalSignature(raw: ByteArray): ByteArray {
        require(raw.size == 64) { "P256-Signatur muss 64 Bytes enthalten." }
        val r = BigInteger(1, raw.copyOfRange(0, 32))
        val s = BigInteger(1, raw.copyOfRange(32, 64))
        require(r.signum() > 0 && r < p256Order && s.signum() > 0 && s < p256Order) { "P256-Signaturwert ungültig." }
        val lowS = if (s > p256Order.shiftRight(1)) p256Order.subtract(s) else s
        val sBytes = lowS.toByteArray().takeLast(32).toByteArray()
        return raw.copyOfRange(0, 32) + ByteArray(32 - sBytes.size) + sBytes
    }

    /** Raw P256 r32||s32 must sign the exact transcript(); no private key is stored here. */
    fun proofFrames(challenge: PeerChallenge, signature: ByteArray, message: Int, maxValueBytes: Int = 20): List<ByteArray> {
        require(message in 1..65535 && maxValueBytes in 9..128) { "Proofparameter ungültig." }
        val canonical = canonicalSignature(signature)
        val proof = byteArrayOf(1, 10, 1, 0) + byteArrayOf(
            challenge.sequence.toByte(), (challenge.sequence shr 8).toByte(),
            (challenge.sequence shr 16).toByte(), (challenge.sequence shr 24).toByte()) + canonical
        val chunk = maxValueBytes - 8
        return (0 until 72 step chunk).map { offset ->
            val count = minOf(chunk, 72 - offset)
            byteArrayOf(1, 10, message.toByte(), (message shr 8).toByte(), 72, 0,
                offset.toByte(), (offset shr 8).toByte()) + proof.copyOfRange(offset, offset + count)
        }
    }

    /** Correlates a raw read to this connection's submitted proof; status alone is never a role grant. */
    fun result(bytes: ByteArray, challenge: PeerChallenge, proofMessage: Int): PeerAuthResult {
        require(bytes.size == 32 && bytes[0] == 1.toByte() && bytes[1] == 12.toByte()) { "Ergebnisformat ungültig." }
        val state = PeerAuthState.entries.getOrNull((bytes[2].toInt() and 255) - 1)
            ?: throw IllegalArgumentException("Ergebniszustand ungültig.")
        val reason = bytes[3].toInt() and 255
        val message = u16(bytes, 4)
        val received = u16(bytes, 6)
        val sequence = u32(bytes, 8)
        val principal = u32(bytes, 12)
        val peer = bytes.copyOfRange(16, 32)
        require(proofMessage in 1..65535 && message == proofMessage && sequence == challenge.sequence &&
            peer.contentEquals(challenge.peerId()) && received <= 72) { "Ergebniskorrelation ungültig." }
        val valid = when (state) {
            PeerAuthState.RECEIVING -> received in 1..71 && reason == 0 && principal == 0L
            PeerAuthState.REJECTED -> reason in 1..3 && principal == 0L
            PeerAuthState.GRANT_PENDING, PeerAuthState.AUTHENTICATED -> received == 72 && reason == 0 && principal == 0L
            PeerAuthState.GRANTED -> received == 72 && reason == 0 && principal != 0L
        }
        require(valid) { "Ergebniszustand widersprüchlich." }
        return PeerAuthResult(state, reason, message, received, sequence, principal, peer)
    }
}
