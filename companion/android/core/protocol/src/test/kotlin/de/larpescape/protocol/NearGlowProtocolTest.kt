package de.larpescape.protocol

import org.junit.Assert.*
import org.junit.Test
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec
import com.google.gson.JsonParser
import java.io.File

class NearGlowProtocolTest {
    @Test fun pythonGoldenVectorsMatchAllHandshakeAndManagementBytes() {
        val fixture = JsonParser.parseString(File(System.getProperty("proxFixtures")).readText()).asJsonObject
        fun bytes(name: String): ByteArray {
            val hex = fixture[name].asString
            return ByteArray(hex.length / 2) { hex.substring(it * 2, it * 2 + 2).toInt(16).toByte() }
        }
        val access = NearGlowAccess.parse("""{"schemaVersion":1,"protocol":"LARP-PROX-BLE/1","deviceId":"${fixture["deviceId"].asString}","secret":"${fixture["accessSecret"].asString}","name":"A","stationMac":"02:01:02:03:04:05"}""".toByteArray())
        val p = NearGlowProtocol(access, bytes("clientNonce"))
        assertArrayEquals(bytes("proof"), p.proof(bytes("challenge")))
        p.auth(bytes("authOK"))
        val t = p.telemetry(bytes("telemetry"))
        assertEquals(1000L, t.millis); assertEquals(-65, t.filteredRssi)
        assertEquals("02:01:02:03:04:06", t.peer)
        assertArrayEquals(bytes("getCurve"), p.get(t.millis))
        val curve = p.result(bytes("getResult")).curve
        assertEquals(7L, curve.revision); assertArrayEquals(bytes("curveDigest"), curve.digest)
        val points = listOf(ProxPoint(-95, 0), ProxPoint(-80, 20), ProxPoint(-60, 70), ProxPoint(-30, 100))
        assertArrayEquals(bytes("setCurve"), p.set(t.millis, curve.revision, points))
        val changed = p.result(bytes("setResult")).curve
        assertEquals(8L, changed.revision); assertEquals(points, changed.points)
        assertArrayEquals(bytes("newCurveDigest"), changed.digest)
    }
    private val key = ByteArray(32) { 0x34 }
    private val id = ByteArray(16) { 0x12 }
    private val boot = ByteArray(16) { 0x56 }
    private val link = ByteArray(16) { 0x78 }
    private val client = ByteArray(16) { 0x9a.toByte() }
    private val context = id + boot + link + client
    private fun mac(key: ByteArray, domain: String, bytes: ByteArray) = Mac.getInstance("HmacSHA256").run {
        init(SecretKeySpec(key, "HmacSHA256")); doFinal("LARP-PROX-BLE/1/$domain".toByteArray() + bytes)
    }
    private val sessionKey get() = mac(key, "session", context)
    private val session get() = mac(key, "id", context).copyOf(16)
    private val points = listOf(ProxPoint(-90, 0), ProxPoint(-75, 30), ProxPoint(-60, 60), ProxPoint(-40, 100))
    private fun put(bytes: ByteArray, at: Int, value: Long, count: Int = 4) { repeat(count) { bytes[at + it] = (value ushr (8 * it)).toByte() } }
    private fun message(type: Int, size: Int, domain: String, secret: ByteArray = sessionKey, body: (ByteArray) -> Unit): ByteArray {
        val bytes = ByteArray(size); bytes[0] = 1; bytes[1] = type.toByte(); put(bytes, 2, size.toLong(), 2)
        body(bytes); mac(secret, domain, bytes.copyOf(size - 32)).copyInto(bytes, size - 32); return bytes
    }
    private fun challenge(edit: (ByteArray) -> Unit = {}) = message(0x81, 90, "challenge", key) {
        id.copyInto(it, 4); boot.copyInto(it, 20); link.copyInto(it, 36); put(it, 52, 2); put(it, 56, 1, 2); edit(it)
    }
    private fun auth() = message(0x82, 88, "auth-result") {
        session.copyInto(it, 4); put(it, 20, 2); NearGlowProtocol.digest(points).copyInto(it, 24)
    }
    private fun protocol(): NearGlowProtocol {
        val access = NearGlowAccess.parse("""{"schemaVersion":1,"protocol":"LARP-PROX-BLE/1","deviceId":"${"12".repeat(16)}","secret":"${"34".repeat(32)}","name":"A","stationMac":"02:00:00:00:00:01"}""".toByteArray())
        return NearGlowProtocol(access, client.copyOf())
    }
    private fun ready() = protocol().also { it.proof(challenge()); it.auth(auth()) }
    private fun telemetry(seq: Long = 1, edit: (ByteArray) -> Unit = {}) = message(0x84, 145, "telemetry") {
        session.copyInto(it, 4); put(it, 20, seq); put(it, 24, 1000); put(it, 28, 0x4ff, 2)
        it[34] = (-60).toByte(); it[35] = (-65).toByte(); put(it, 36, 500, 2); put(it, 38, 10)
        byteArrayOf(2, 0, 0, 0, 0, 2).copyInto(it, 46)
        byteArrayOf(12, 3, 0, 11, 3, 0, 10, 3, 0).copyInto(it, 52)
        put(it, 69, 2); NearGlowProtocol.digest(points).copyInto(it, 73); put(it, 105, 2); put(it, 109, 2); edit(it)
    }
    private fun result(request: Long, revision: Long = 2, curve: List<ProxPoint> = points,
        edit: (ByteArray) -> Unit = {}) = message(0x83, 102, "curve-result") {
        session.copyInto(it, 4); put(it, 20, request); put(it, 25, revision)
        NearGlowProtocol.digest(curve).copyInto(it, 29); NearGlowProtocol.pointBytes(curve).copyInto(it, 61)
        it[69] = 1; edit(it)
    }

    @Test fun authenticatedReadAndCasHaveIndependentRequestsAndDurableResults() {
        val p = ready()
        assertThrows(IllegalStateException::class.java) { p.get(1000) }
        val t = p.telemetry(telemetry())
        assertEquals(-65, t.filteredRssi); assertEquals(listOf(10, 3, 0), t.confirmedRgb)
        val get = p.get(t.millis)
        assertEquals(62, get.size); assertEquals(1, get[20].toInt()); assertEquals(2, get[1].toInt())
        assertThrows(IllegalStateException::class.java) { p.get(t.millis) }
        val read = p.result(result(1)); assertTrue(read.curve.durable); assertEquals(points, read.curve.points)
        val next = points.toMutableList().apply { this[2] = ProxPoint(-60, 70) }
        val set = p.set(t.millis, read.curve.revision, next)
        assertEquals(74, set.size); assertEquals(2, set[20].toInt()); assertEquals(3, set[1].toInt())
        assertEquals(3L, p.result(result(2, 3, next)).curve.revision)
    }

    @Test fun wrongIdentityNonceCapabilityTagVersionOrCrossSessionAuthNeverGrants() {
        val malformed = listOf(challenge { it[4] = 0 }, challenge { it.fill(0, 20, 36) },
            challenge { it[56] = 2 }, challenge { it[0] = 2 }, challenge().copyOf(89),
            challenge().also { it[89] = (it[89].toInt() xor 1).toByte() })
        for (bytes in malformed) assertThrows(Exception::class.java) { protocol().proof(bytes) }
        val p = protocol(); p.proof(challenge { it[36] = 0x79 })
        assertThrows(IllegalArgumentException::class.java) { p.auth(auth()) }
    }

    @Test fun replayMalformedFreshnessFlagsAndRgbDoNotRenewTelemetry() {
        val p = ready(); p.telemetry(telemetry())
        assertThrows(IllegalArgumentException::class.java) { p.telemetry(telemetry()) }
        for (bytes in listOf(telemetry(2) { put(it, 28, 0x8ff, 2) },
            telemetry(2) { put(it, 36, 1001, 2) }, telemetry(2) { it[35] = 20 },
            telemetry(2) { put(it, 38, 3000) }, telemetry(2) { put(it, 28, 0x4ef, 2) },
            telemetry(2) { it[58] = 33 }, telemetry(2) { put(it, 28, 0x6ff, 2) },
            telemetry(2).also { it[144] = (it[144].toInt() xor 1).toByte() })) {
            assertThrows(IllegalArgumentException::class.java) { p.telemetry(bytes) }
        }
        assertEquals(2L, p.telemetry(telemetry(2)).sequence)
    }

    @Test fun resultRequiresMatchingRequestDigestDurabilityAndCasRevision() {
        val p = ready(); p.telemetry(telemetry()); p.set(1000, 2, points)
        for (bytes in listOf(result(2, 3), result(1, 2), result(1, 3) { it[29] = 0 },
            result(1, 3) { it[69] = 0 }, result(1, 3) { it[24] = 10 })) {
            assertThrows(IllegalArgumentException::class.java) { p.result(bytes) }
        }
        assertEquals(3L, p.result(result(1, 3)).curve.revision)
        assertThrows(IllegalArgumentException::class.java) { p.result(result(1, 3)) }
        val failedApply = ready(); failedApply.telemetry(telemetry()); failedApply.set(1000, 2, points)
        assertEquals(9, failedApply.result(result(1, 3) { it[24] = 9 }).code)
    }

    private fun frames(bytes: ByteArray, id: Int) = (bytes.indices step 12).map { at ->
        byteArrayOf(1, 0x4c, id.toByte(), 0, bytes.size.toByte(), (bytes.size shr 8).toByte(), at.toByte(), (at shr 8).toByte()) +
            bytes.copyOfRange(at, minOf(at + 12, bytes.size))
    }
    @Test fun telemetryFitsUnchangedBearerButReorderTimeoutAndReplayFail() {
        val bytes = telemetry(); val chunks = frames(bytes, 1); val assembler = NearGlowFrameAssembler()
        var value: ByteArray? = null
        chunks.forEachIndexed { i, frame -> value = assembler.accept(frame, i.toLong()) }
        assertArrayEquals(bytes, value)
        assertEquals(0L, assembler.completedStartedAt)
        assertThrows(IllegalArgumentException::class.java) { assembler.accept(chunks[0], 50) }
        assertThrows(IllegalArgumentException::class.java) { NearGlowFrameAssembler().accept(chunks[1], 0) }
        val expiring = NearGlowFrameAssembler(); expiring.accept(chunks[0], 0)
        assertTrue(expiring.expired(2000)); assertThrows(IllegalArgumentException::class.java) { expiring.accept(chunks[1], 2000) }
        assertThrows(IllegalArgumentException::class.java) { NearGlowFrameAssembler().accept(chunks[0].also { it[5] = 2 }, 0) }
    }
}
