package de.larpescape.protocol

import com.google.gson.JsonParser
import org.junit.Assert.*
import org.junit.Test
import java.io.File
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

class WledLedProtocolTest {
    private val fixture = JsonParser.parseString(File(System.getProperty("wledFixtures"), "wire-v1.json").readText()).asJsonObject
    private fun hex(value: String): ByteArray = value.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private fun bytes(name: String) = hex(fixture[name].asString)
    private fun access(extra: String = "") = WledAccess.parse(("{\"schemaVersion\":1,\"deviceId\":\"${fixture["device"].asString}\",\"secret\":\"${fixture["secret"].asString}\"$extra}").toByteArray())
    private fun client() = WledLedProtocol(access(), bytes("client"))
    private fun authenticated(): Pair<WledLedProtocol, WledLedStatus> {
        val protocol = client(); protocol.proof(bytes("challenge")); return protocol to protocol.granted(bytes("granted"))
    }
    private fun rejects(action: () -> Unit) {
        try { action(); fail("Expected rejected input") } catch (_: IllegalArgumentException) { } catch (_: IllegalStateException) { }
    }
    private fun resign(data: ByteArray, offset: Int, domain: String): ByteArray {
        val mac = Mac.getInstance("HmacSHA256"); mac.init(SecretKeySpec(bytes("secret"), "HmacSHA256"))
        mac.update("LARP-WLED-LED/1/$domain".toByteArray(Charsets.US_ASCII))
        return data.copyOf(offset) + mac.doFinal(data.copyOf(offset))
    }
    @Test fun matchesIndependentFirmwareHandshakeCommandsAndReplies() {
        val protocol = client()
        assertArrayEquals(bytes("hello"), protocol.hello())
        assertArrayEquals(bytes("proof"), protocol.proof(bytes("challenge")))
        val initial = protocol.granted(bytes("granted"))
        assertEquals(0L, initial.revision); assertTrue(initial.canStart); assertTrue(initial.off); assertEquals(0, initial.mode)
        assertArrayEquals(bytes("command_start_twinkle"), protocol.command(initial, 1))
        val applied = protocol.result(bytes("result_start"))
        assertEquals(1L, applied.request); assertEquals(0, applied.outcome); assertEquals(1, applied.status.effect)
        val status = protocol.status(bytes("status_twinkle"))
        assertEquals(3L, status.statusSequence)
        assertArrayEquals(bytes("command_stop"), protocol.command(status, 0))
        assertTrue(protocol.result(bytes("result_stop")).status.off)
    }
    @Test fun rejectsWrongDeviceNonceMacAndUnsolicitedAuthentication() {
        for (offset in listOf(0, 1, 2, 18, 34, 50, 66, 97)) rejects {
            client().proof(bytes("challenge").also { it[offset] = (it[offset].toInt() xor 1).toByte() })
        }
        rejects { client().granted(bytes("granted")) }
        val (protocol, _) = authenticated()
        rejects { protocol.proof(bytes("challenge")) }
        rejects { protocol.granted(bytes("granted")) }
        rejects { protocol.status(bytes("status_twinkle").also { it[40] = (it[40].toInt() xor 1).toByte() }) }
        rejects { protocol.result(bytes("result_start").also { it[2] = 0 }) }
        protocol.close()
        rejects { protocol.status(bytes("status_twinkle")) }
    }
    @Test fun validatesSignedStatusBeforeItCanEnableControls() {
        val (protocol, _) = authenticated()
        for ((offset, value) in listOf(22 to 0, 30 to 0, 34 to 4, 35 to 0x81, 35 to 0x11, 35 to 5, 36 to 5, 37 to 1)) {
            val modified = bytes("status_twinkle").also { it[offset] = value.toByte() }
            rejects { protocol.status(resign(modified, 38, "status")) }
        }
        val (other, _) = authenticated()
        rejects { other.result(resign(bytes("result_start").also { it[38] = 12 }, 42, "result")) }
    }
    @Test fun onlyStoppedOrExplicitLedOnlyModeAllowsStarting() {
        val (protocol, initial) = authenticated()
        for (mode in 2..4) rejects { protocol.command(initial.copy(mode = mode), 1) }
        rejects { protocol.command(initial.copy(flags = 7), 1) }
        rejects { protocol.command(initial.copy(flags = 4), 1) }
        rejects { protocol.command(initial, 4) }
        assertEquals(64, protocol.command(initial.copy(mode = 1), 3).size)
        rejects { protocol.command(initial.copy(mode = 2), 0) }
    }
    @Test fun writeAcknowledgementAndResultAreBothRequiredInEitherOrder() {
        val result = authenticated().first.result(bytes("result_start"))
        val first = WledCommandTracker(); first.begin(1)
        assertNull(first.wrote()); assertEquals(1L, first.request)
        assertEquals(result, first.received(result)); assertNull(first.request)
        val second = WledCommandTracker(); second.begin(1)
        assertNull(second.received(result)); assertEquals(1L, second.request)
        assertEquals(result, second.wrote()); assertNull(second.request)
        // A valid older result can finish this request even if a later status has arrived.
        val third = WledCommandTracker(); third.begin(1)
        rejects { third.received(result.copy(request = 2)) }
        assertEquals(1L, third.request)
        assertNull(third.received(result)); rejects { third.received(result) }
    }
    @Test fun framingIsBoundedAndNeverReassemblesACompletedTransferTwice() {
        for (count in 1..128) {
            val payload = ByteArray(count) { it.toByte() }
            val frames = WledLedFrames.encode(payload, 1)
            assertTrue(frames.all { it.size in 9..20 })
            val assembler = WledLedAssembler()
            frames.dropLast(1).forEach { assertNull(assembler.accept(it, 10)) }
            assertArrayEquals(payload, assembler.accept(frames.last(), 10))
            rejects { assembler.accept(frames.last(), 11) }
            if (frames.size > 1) rejects { assembler.accept(frames.first(), 12) }
        }
        rejects { WledLedFrames.encode(ByteArray(129), 1) }
        rejects { WledLedFrames.encode(byteArrayOf(1), 0) }
    }
    @Test fun rejectsMissingInterleavedOrExpiredFragmentsWithoutExtendingDeadline() {
        val frames = WledLedFrames.encode(bytes("challenge"), 1)
        val missing = WledLedAssembler(); missing.accept(frames.first(), 0)
        rejects { missing.accept(frames[2], 1) }
        val interleaved = WledLedAssembler(); interleaved.accept(frames.first(), 0)
        rejects { interleaved.accept(WledLedFrames.encode(bytes("proof"), 2).first(), 1) }
        val expired = WledLedAssembler(); expired.accept(frames.first(), 0)
        assertNull(expired.accept(frames.first(), 1999))
        assertTrue(expired.expired(2000))
        rejects { expired.accept(frames[1], 2000) }
    }
    @Test fun trustedSetupImportIsStrictAndBounded() {
        assertEquals("Testbox", access(",\"name\":\"Testbox\"").name)
        rejects { access(",\"schemaVersion\":1") }
        rejects { access(",\"extra\":true") }
        rejects { access(",\"name\":\"\"") }
        rejects { WledAccess.parse(ByteArray(2049)) }
        rejects { WledAccess.parse("{\"schemaVersion\":1}".toByteArray()) }
        rejects { WledAccess.parse(byteArrayOf(0xff.toByte())) }
        val device = access().deviceId(); device.fill(0)
        assertArrayEquals(bytes("device"), access().deviceId())
    }
}
