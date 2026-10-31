package de.larpescape.protocol

import java.security.MessageDigest
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

data class ProxPoint(val rssi: Int, val percent: Int)
data class ProxCurve(val revision: Long, val points: List<ProxPoint>, val digest: ByteArray, val durable: Boolean)
data class ProxResult(val request: Long, val code: Int, val curve: ProxCurve)
data class ProxTelemetry(val sequence: Long, val millis: Long, val flags: Int, val fault: Int,
    val configFault: Int, val rawRssi: Int?, val filteredRssi: Int?, val level: Int,
    val sourceAge: Long?, val peer: String, val calculatedRgb: List<Int>, val submittedRgb: List<Int>,
    val confirmedRgb: List<Int>, val revision: Long, val digest: ByteArray,
    val submittedRevision: Long, val confirmedRevision: Long) {
    fun flag(bit: Int) = flags and (1 shl bit) != 0
}

/** PROX-BLE/1 only. Each connection owns a fresh instance and independent counters. */
class NearGlowProtocol(access: NearGlowAccess, private val client: ByteArray) {
    private val accessKey = access.secret()
    private val device = access.deviceId()
    private var sessionKey: ByteArray? = null
    private var sessionId: ByteArray? = null
    private var granted = false
    private var sequence = 0L
    private var request = 0L
    private var pending: Long? = null
    private var expectedPoints: List<ProxPoint>? = null
    private var expectedRevision: Long? = null
    init { require(client.size == 16 && client.any { it != 0.toByte() }) }

    fun proof(bytes: ByteArray): ByteArray {
        check(sessionKey == null)
        header(bytes, 0x81, 90)
        require(bytes.copyOfRange(4, 20).contentEquals(device))
        require(bytes.copyOfRange(20, 36).any { it != 0.toByte() } && bytes.copyOfRange(36, 52).any { it != 0.toByte() })
        require(u32(bytes, 52) > 0 && u16(bytes, 56) == 1)
        verify(accessKey, "challenge", bytes)
        val context = bytes.copyOfRange(4, 52) + client
        sessionKey = hmac(accessKey, "session", context)
        sessionId = hmac(accessKey, "id", context).copyOf(16)
        return head(1, 52) + client + hmac(accessKey, "proof", context)
    }

    fun auth(bytes: ByteArray) {
        check(!granted && sessionKey != null)
        header(bytes, 0x82, 88); binding(bytes); verify(sessionKey!!, "auth-result", bytes)
        require(u32(bytes, 20) > 0)
        granted = true
    }

    fun telemetry(bytes: ByteArray): ProxTelemetry {
        check(granted)
        header(bytes, 0x84, 145); binding(bytes); verify(sessionKey!!, "telemetry", bytes)
        val next = u32(bytes, 20)
        require(next > sequence)
        val flags = u16(bytes, 28)
        require(flags and 0xF800 == 0)
        val age = u32(bytes, 38).takeUnless { it == U32 }
        val raw = bytes[34].toInt().takeUnless { it == 0 }
        val filtered = bytes[35].toInt().takeUnless { it == 0 }
        require(raw == null || raw in -127..-1); require(filtered == null || filtered in -127..-1)
        require(u16(bytes, 36) <= 1000)
        if (flags and 16 != 0) require(age != null && age < 3000 && raw != null && filtered != null)
        else require(u16(bytes, 36) == 0)
        val peerBytes = bytes.copyOfRange(46, 52)
        val peer = peerBytes.joinToString(":") { "%02X".format(it) }
        require((flags and 1 == 0 && peerBytes.all { it == 0.toByte() }) || espNowPeer(peer) != null)
        fun rgb(at: Int) = (at until at + 3).map { byte(bytes, it) }.also { require(it.all { channel -> channel <= 32 }) }
        val confirmed = rgb(58)
        if (flags and 512 != 0) require(confirmed.all { it == 0 })
        require(u32(bytes, 69) > 0)
        val result = ProxTelemetry(next, u32(bytes, 24), flags, u16(bytes, 30), u16(bytes, 32),
            raw, filtered, u16(bytes, 36), age, peer, rgb(52), rgb(55), confirmed,
            u32(bytes, 69), bytes.copyOfRange(73, 105), u32(bytes, 105), u32(bytes, 109))
        sequence = next
        return result
    }

    fun get(observedMillis: Long): ByteArray = command(2, observedMillis, null, null)
    fun set(observedMillis: Long, revision: Long, points: List<ProxPoint>): ByteArray {
        require(revision in 1 until U32); pointBytes(points)
        return command(3, observedMillis, revision, points)
    }
    private fun command(type: Int, observed: Long, revision: Long?, points: List<ProxPoint>?): ByteArray {
        check(granted && sequence > 0 && pending == null && request < U32)
        require(observed in 0..U32)
        val next = request + 1
        val body = head(type, if (type == 2) 62 else 74) + sessionId!! + le32(next) + le32(observed) + le16(2000) +
            (if (revision != null && points != null) le32(revision) + pointBytes(points) else byteArrayOf())
        val result = body + hmac(sessionKey!!, if (type == 2) "get-curve" else "set-curve", body)
        request = next; pending = next; expectedPoints = points; expectedRevision = revision
        return result
    }
    fun result(bytes: ByteArray): ProxResult {
        check(granted)
        header(bytes, 0x83, 102); binding(bytes); verify(sessionKey!!, "curve-result", bytes)
        val requestId = u32(bytes, 20)
        require(pending == requestId)
        val code = byte(bytes, 24)
        require(code in 0..9 && byte(bytes, 69) in 0..1)
        val points = (0..3).map { ProxPoint(bytes[61 + it * 2].toInt(), byte(bytes, 62 + it * 2)) }
        val digest = bytes.copyOfRange(29, 61)
        require(MessageDigest.isEqual(digest, digest(points)))
        val revision = u32(bytes, 25)
        require(revision > 0)
        if (code == 9) require(expectedPoints != null)
        if ((code == 0 || code == 9) && expectedPoints != null) {
            require(points == expectedPoints && revision == expectedRevision!! + 1 && byte(bytes, 69) == 1)
        }
        val value = ProxResult(requestId, code, ProxCurve(revision, points, digest, byte(bytes, 69) == 1))
        pending = null; expectedPoints = null; expectedRevision = null
        return value
    }

    fun close() { accessKey.fill(0); sessionKey?.fill(0); sessionId?.fill(0); client.fill(0); granted = false }
    private fun binding(bytes: ByteArray) { require(bytes.copyOfRange(4, 20).contentEquals(sessionId)) }
    companion object {
        const val U32 = 0xFFFFFFFFL
        private fun byte(bytes: ByteArray, at: Int) = bytes[at].toInt() and 255
        private fun u16(bytes: ByteArray, at: Int) = byte(bytes, at) or (byte(bytes, at + 1) shl 8)
        private fun u32(bytes: ByteArray, at: Int) = (0..3).fold(0L) { value, i -> value or (byte(bytes, at + i).toLong() shl (i * 8)) }
        private fun le16(value: Int) = byteArrayOf(value.toByte(), (value ushr 8).toByte())
        private fun le32(value: Long) = ByteArray(4) { (value ushr (it * 8)).toByte() }
        private fun head(type: Int, length: Int) = byteArrayOf(1, type.toByte()) + le16(length)
        private fun header(bytes: ByteArray, type: Int, length: Int) {
            require(bytes.size == length && byte(bytes, 0) == 1 && byte(bytes, 1) == type && u16(bytes, 2) == length)
        }
        private fun hmac(key: ByteArray, domain: String, bytes: ByteArray): ByteArray = Mac.getInstance("HmacSHA256").run {
            init(SecretKeySpec(key, "HmacSHA256")); update("LARP-PROX-BLE/1/$domain".toByteArray(Charsets.US_ASCII)); doFinal(bytes)
        }
        private fun verify(key: ByteArray, domain: String, bytes: ByteArray) {
            require(MessageDigest.isEqual(bytes.takeLast(32).toByteArray(), hmac(key, domain, bytes.copyOf(bytes.size - 32))))
        }
        fun pointBytes(points: List<ProxPoint>): ByteArray {
            require(points.size == 4 && points.all { it.rssi in -127..-1 && it.percent in 0..100 })
            require(points.zipWithNext().all { (a, b) -> a.rssi < b.rssi && a.percent <= b.percent })
            return points.flatMap { listOf(it.rssi.toByte(), it.percent.toByte()) }.toByteArray()
        }
        fun digest(points: List<ProxPoint>): ByteArray = MessageDigest.getInstance("SHA-256").digest(byteArrayOf(1, 4) + pointBytes(points))
    }
}
