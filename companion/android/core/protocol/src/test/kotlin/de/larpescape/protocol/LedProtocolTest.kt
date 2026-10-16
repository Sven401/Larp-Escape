package de.larpescape.protocol

import com.google.gson.JsonParser
import org.junit.Assert.*
import org.junit.Test
import java.io.File
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

class LedProtocolTest {
    private val fixture = JsonParser.parseString(File(System.getProperty("ledFixtures"), "wire-v1.json").readText()).asJsonObject
    private fun hex(value: String): ByteArray = value.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private fun bytes(name: String) = hex(fixture[name].asString)
    private fun access(extra: String = "") = LedAccess.parse(("{\"schemaVersion\":1,\"deviceId\":\"${fixture["device"].asString}\",\"secret\":\"${fixture["secret"].asString}\"$extra}").toByteArray())
    private fun client() = LedProtocol(access(), bytes("client"))
    private fun authenticated(): Pair<LedProtocol, LedStatus> {
        val protocol = client(); protocol.proof(bytes("challenge")); return protocol to protocol.granted(bytes("granted"))
    }
    private fun rejects(action: () -> Unit) {
        try { action(); fail("Expected rejected input") } catch (_: IllegalArgumentException) { } catch (_: IllegalStateException) { }
    }
    private fun resign(data: ByteArray, offset: Int, domain: String): ByteArray {
        val mac = Mac.getInstance("HmacSHA256"); mac.init(SecretKeySpec(bytes("secret"), "HmacSHA256"))
        mac.update("LARP-LED/1/$domain".toByteArray(Charsets.US_ASCII))
        return data.copyOf(offset) + mac.doFinal(data.copyOf(offset))
    }
    private fun nfcResign(data: ByteArray): ByteArray {
        val body = data.copyOf(73)
        val mac = Mac.getInstance("HmacSHA256").apply { init(SecretKeySpec(bytes("secret"), "HmacSHA256")) }
        mac.update("LARP-NFC/1/result".toByteArray(Charsets.US_ASCII))
        return body + mac.doFinal(body)
    }
    @Test fun nfcMatchesIndependentPythonAndFirmwareVectors() {
        val (protocol, _) = authenticated()
        assertArrayEquals(bytes("nfc_read"), protocol.nfcCommand(1, 1002))
        val read = protocol.nfcResult(bytes("nfc_read_result"))
        assertEquals(NfcTestFixture.initial, read.data); assertEquals("04010203040506", read.uidText)
        assertEquals(0, read.outcome); assertEquals(2L, read.nextRequest)
        assertArrayEquals(bytes("nfc_change"), protocol.nfcCommand(2, 1250, read.uid))
        val written = protocol.nfcResult(bytes("nfc_write_result"))
        assertEquals(8, written.outcome); assertEquals(NfcTestFixture.changed, written.data)
        assertEquals(2L, written.request); assertEquals(2L, written.sequence)
    }
    @Test fun nfcRejectsForeignSessionsBadMacMalformedPayloadAndFalseSuccess() {
        val (protocol, _) = authenticated()
        for (offset in listOf(0, 1, 2, 18, 34, 36, 48, 73, 104)) rejects {
            protocol.nfcResult(bytes("nfc_write_result").also { it[offset] = (it[offset].toInt() xor 1).toByte() })
        }
        for ((offset, value) in listOf(34 to 14, 35 to 5, 36 to 11, 47 to 24, 48 to 2, 18 to 0, 26 to 0)) rejects {
            protocol.nfcResult(nfcResign(bytes("nfc_write_result").also { it[offset] = value.toByte() }))
        }
        rejects { protocol.nfcResult(nfcResign(bytes("nfc_read_result").also { it[34] = 8 })) }
        rejects { protocol.nfcCommand(0, 1000) }
        rejects { protocol.nfcCommand(1, 1000, ByteArray(4)) }
        protocol.close(); rejects { protocol.nfcResult(bytes("nfc_write_result")) }
    }
    @Test fun nfcCapabilityIsExplicitAndMacCovered() {
        val (protocol, _) = authenticated()
        assertFalse(protocol.status(bytes("status_twinkle")).nfcSupported)
        val message = resign(bytes("status_twinkle").also { it[37] = 0xe5.toByte() }, 38, "status")
        assertTrue(protocol.status(message).nfcSupported)
        assertEquals(WakeStatus(true, true, 5), protocol.status(message).wake)
        rejects { protocol.status(message.copyOf().also { it[37] = 0xc5.toByte() }) }
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
    @Test fun matchesIndependentWakeHandshakeAndStatusFixtures() {
        val unknown = client(); unknown.proof(bytes("challenge"))
        assertEquals(WakeStatus(true, false, 0), unknown.granted(bytes("granted_wake_unknown")).wake)
        val all = client(); all.proof(bytes("challenge"))
        assertEquals(WakeStatus(true, true, 15), all.granted(bytes("granted_wake_all")).wake)
        assertEquals(WakeStatus(true, true, 5), all.status(bytes("status_wake_1_3")).wake)
    }
    @Test fun wakeStatusAcceptsEveryMaskAndKeepsLegacyFirmwareExplicit() {
        val (protocol, _) = authenticated()
        for (mask in 0..15) {
            val message = resign(bytes("status_twinkle").also { it[37] = (0xc0 or mask).toByte() }, 38, "status")
            assertEquals(WakeStatus(true, true, mask), protocol.status(message).wake)
            val other = client(); other.proof(bytes("challenge"))
            val granted = resign(bytes("granted").also { it[37] = (0xc0 or mask).toByte() }, 38, "status")
            assertEquals(WakeStatus(true, true, mask), other.granted(granted).wake)
        }
        val legacy = resign(bytes("status_twinkle").also { it[37] = 0 }, 38, "status")
        assertEquals(WakeStatus(false, false, 0), protocol.status(legacy).wake)
        val unavailable = resign(bytes("status_twinkle").also { it[37] = 0x80.toByte() }, 38, "status")
        assertEquals(WakeStatus(true, false, 0), protocol.status(unavailable).wake)
    }
    @Test fun rejectsEveryReservedOrContradictorySignedWakeByte() {
        val (protocol, _) = authenticated()
        for (value in 0..255) {
            if (value and 0xdf == 0 || value and 0xdf == 0x80 || value and 0xdf in 0xc0..0xcf) continue
            val message = resign(bytes("status_twinkle").also { it[37] = value.toByte() }, 38, "status")
            rejects { protocol.status(message) }
        }
    }
    @Test fun wakeIsCoveredByTheStatusMacAndNeverInferredFromLedResults() {
        val (protocol, _) = authenticated()
        val signed = resign(bytes("status_twinkle").also { it[37] = 0xc5.toByte() }, 38, "status")
        for (bit in 0..7) rejects {
            protocol.status(signed.copyOf().also { it[37] = (it[37].toInt() xor (1 shl bit)).toByte() })
        }
        assertNull(protocol.result(bytes("result_start")).status.wake)
        assertNull(protocol.result(bytes("result_stop")).status.wake)
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
        val first = LedCommandTracker(); first.begin(1)
        assertNull(first.wrote()); assertEquals(1L, first.request)
        assertEquals(result, first.received(result)); assertNull(first.request)
        val second = LedCommandTracker(); second.begin(1)
        assertNull(second.received(result)); assertEquals(1L, second.request)
        assertEquals(result, second.wrote()); assertNull(second.request)
        // A valid older result can finish this request even if a later status has arrived.
        val third = LedCommandTracker(); third.begin(1)
        rejects { third.received(result.copy(request = 2)) }
        assertEquals(1L, third.request)
        assertNull(third.received(result)); rejects { third.received(result) }
    }
    @Test fun framingIsBoundedAndNeverReassemblesACompletedTransferTwice() {
        for (count in 1..128) {
            val payload = ByteArray(count) { it.toByte() }
            val frames = LedFrames.encode(payload, 1)
            assertTrue(frames.all { it.size in 9..20 })
            val assembler = LedFrameAssembler()
            frames.dropLast(1).forEach { assertNull(assembler.accept(it, 10)) }
            assertArrayEquals(payload, assembler.accept(frames.last(), 10))
            rejects { assembler.accept(frames.last(), 11) }
            if (frames.size > 1) rejects { assembler.accept(frames.first(), 12) }
        }
        rejects { LedFrames.encode(ByteArray(129), 1) }
        rejects { LedFrames.encode(byteArrayOf(1), 0) }
    }
    @Test fun rejectsMissingInterleavedOrExpiredFragmentsWithoutExtendingDeadline() {
        val frames = LedFrames.encode(bytes("challenge"), 1)
        val missing = LedFrameAssembler(); missing.accept(frames.first(), 0)
        rejects { missing.accept(frames[2], 1) }
        val interleaved = LedFrameAssembler(); interleaved.accept(frames.first(), 0)
        rejects { interleaved.accept(LedFrames.encode(bytes("proof"), 2).first(), 1) }
        val expired = LedFrameAssembler(); expired.accept(frames.first(), 0)
        assertNull(expired.accept(frames.first(), 1999))
        assertTrue(expired.expired(2000))
        rejects { expired.accept(frames[1], 2000) }
    }
    @Test fun importedMetadataNeverChangesProductLabelOrCredential() {
        val imported = access(",\"name\":\"Alter Produktname\"")
        assertEquals("Alter Produktname", imported.name)
        assertEquals("LED-Box ${fixture["device"].asString.take(8).uppercase()}", imported.displayName)
        assertArrayEquals(bytes("device"), imported.deviceId())
        assertArrayEquals(bytes("secret"), imported.secret())
        assertEquals(imported.displayName, access().displayName)
    }
    @Test fun trustedSetupImportIsStrictAndBounded() {
        assertEquals("Testbox", access(",\"name\":\"Testbox\"").name)
        rejects { access(",\"schemaVersion\":1") }
        rejects { access(",\"extra\":true") }
        rejects { access(",\"name\":\"\"") }
        rejects { LedAccess.parse(ByteArray(2049)) }
        rejects { LedAccess.parse("{\"schemaVersion\":1}".toByteArray()) }
        rejects { LedAccess.parse(byteArrayOf(0xff.toByte())) }
        val device = access().deviceId(); device.fill(0)
        assertArrayEquals(bytes("device"), access().deviceId())
    }
}
