package de.larpescape.protocol

import com.google.gson.JsonParser
import org.junit.Assert.*
import org.junit.Test
import java.io.File
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

class LedProtocolTest {
    private val fixture = JsonParser.parseString(File(System.getProperty("ledFixtures"), "wire-v2.json").readText()).asJsonObject
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
        mac.update("LARP-LED/2/$domain".toByteArray(Charsets.US_ASCII))
        return data.copyOf(offset) + mac.doFinal(data.copyOf(offset))
    }
    private fun nfcResign(data: ByteArray): ByteArray {
        val body = data.copyOf(73)
        val mac = Mac.getInstance("HmacSHA256").apply { init(SecretKeySpec(bytes("secret"), "HmacSHA256")) }
        mac.update("LARP-NFC/2/result".toByteArray(Charsets.US_ASCII))
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
        val message = resign(bytes("status_twinkle").also { it[37] = 0xe5.toByte() }, 59, "status")
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
        for ((offset, value) in listOf(22 to 0, 30 to 0, 34 to 5, 35 to 0x81, 35 to 0x11, 35 to 5, 36 to 5, 37 to 1)) {
            val modified = bytes("status_twinkle").also { it[offset] = value.toByte() }
            rejects { protocol.status(resign(modified, 59, "status")) }
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
            val message = resign(bytes("status_twinkle").also { it[37] = (0xc0 or mask).toByte() }, 59, "status")
            assertEquals(WakeStatus(true, true, mask), protocol.status(message).wake)
            val other = client(); other.proof(bytes("challenge"))
            val granted = resign(bytes("granted").also { it[37] = (0xc0 or mask).toByte() }, 59, "status")
            assertEquals(WakeStatus(true, true, mask), other.granted(granted).wake)
        }
        val legacy = resign(bytes("status_twinkle").also { it[37] = 0 }, 59, "status")
        assertEquals(WakeStatus(false, false, 0), protocol.status(legacy).wake)
        val unavailable = resign(bytes("status_twinkle").also { it[37] = 0x80.toByte() }, 59, "status")
        assertEquals(WakeStatus(true, false, 0), protocol.status(unavailable).wake)
    }
    @Test fun rejectsEveryReservedOrContradictorySignedWakeByte() {
        val (protocol, _) = authenticated()
        for (value in 0..255) {
            val wake = value and 0xcf // NFC and Resonance are independent capability bits.
            if (wake == 0 && value and 0x10 == 0 || wake == 0x80 || wake in 0xc0..0xcf) continue
            val message = resign(bytes("status_twinkle").also { it[37] = value.toByte() }, 59, "status")
            rejects { protocol.status(message) }
        }
    }
    @Test fun wakeIsCoveredByTheStatusMacAndNeverInferredFromLedResults() {
        val (protocol, _) = authenticated()
        val signed = resign(bytes("status_twinkle").also { it[37] = 0xc5.toByte() }, 59, "status")
        for (bit in 0..7) rejects {
            protocol.status(signed.copyOf().also { it[37] = (it[37].toInt() xor (1 shl bit)).toByte() })
        }
        assertNull(protocol.result(bytes("result_start")).status.wake)
        assertNull(protocol.result(bytes("result_stop")).status.wake)
    }
    @Test fun resonanceRequiresCapabilityAndMatchesIndependentCommandResultAndStatus() {
        val (old, status) = authenticated()
        assertFalse(status.resonanceSupported)
        rejects { old.command(status, 4) }
        val protocol = client(); protocol.proof(bytes("challenge"))
        val supported = protocol.granted(bytes("granted_resonance"))
        assertTrue(supported.resonanceSupported)
        assertArrayEquals(bytes("command_start_resonance"), protocol.command(supported, 4))
        assertEquals(4, protocol.result(bytes("result_resonance")).status.effect)
        val running = protocol.status(bytes("status_resonance"))
        assertEquals(4, running.effect); assertTrue(running.resonanceSupported)
        assertEquals(WakeStatus(true, true, 15), running.wake)
        rejects { protocol.command(supported, 5) }
        rejects { protocol.status(resign(bytes("status_resonance").also { it[37] = 0xcf.toByte() }, 59, "status")) }
        rejects { protocol.status(resign(bytes("status_resonance").also { it[37] = 0x10 }, 59, "status")) }
        rejects { protocol.status(bytes("status_resonance").also { it[37] = 0xcf.toByte() }) }
        val unknown = protocol.status(resign(bytes("status_resonance").also { it[37] = 0x90.toByte() }, 59, "status"))
        assertTrue(unknown.resonanceSupported); assertEquals(WakeStatus(true, false, 0), unknown.wake)
    }
    @Test fun matchesIndependentMatrixGrantAndStatusIncludingLastContact() {
        val unknown = client(); unknown.proof(bytes("challenge"))
        val unknownStatus = unknown.granted(bytes("granted_matrix_unknown"))
        assertEquals(MatrixStatus(true, false, false, emptySet()), unknownStatus.matrix)
        assertEquals(WakeStatus(true, true, 5), unknownStatus.wake)
        val corners = client(); corners.proof(bytes("challenge"))
        assertEquals(MatrixStatus(true, true, false, setOf(0, 80)),
            corners.granted(bytes("granted_matrix_corners")).matrix)
        assertEquals(MatrixStatus(true, true, true, setOf(0, 1, 9, 10)),
            corners.status(bytes("status_matrix_rectangle")).matrix)
    }
    @Test fun matrixAcceptsEveryContactAndExplicitUnknownOrUnsupported() {
        val (protocol, legacy) = authenticated()
        assertEquals(MatrixStatus(false, false, false, emptySet()), legacy.matrix)
        assertEquals(MatrixStatus(false, false, false, emptySet()), protocol.status(bytes("status_twinkle")).matrix)
        for (contact in 0..80) {
            val message = bytes("status_matrix_rectangle").also {
                it[47] = 0xc0.toByte(); it.fill(0, 48, 59)
                it[48 + contact / 8] = (1 shl (contact % 8)).toByte()
            }
            assertEquals(MatrixStatus(true, true, false, setOf(contact)),
                protocol.status(resign(message, 59, "status")).matrix)
        }
        for (flags in listOf(0, 0x80, 0xc0, 0xe0)) {
            val message = bytes("status_matrix_rectangle").also { it[47] = flags.toByte(); it.fill(0, 48, 59) }
            assertEquals(MatrixStatus(flags and 0x80 != 0, flags and 0x40 != 0, flags and 0x20 != 0, emptySet()),
                protocol.status(resign(message, 59, "status")).matrix)
        }
    }
    @Test fun rejectsReservedFlagsAndContradictoryAuthenticatedMatrixData() {
        val (protocol, _) = authenticated()
        for (flags in 0..255) {
            if (flags in listOf(0, 0x80, 0xc0, 0xe0)) continue
            val invalid = bytes("status_matrix_rectangle").also { it[47] = flags.toByte(); it.fill(0, 48, 59) }
            rejects { protocol.status(resign(invalid, 59, "status")) }
        }
        for (lastByte in 2..255) {
            val invalid = bytes("status_matrix_rectangle").also { it[58] = lastByte.toByte() }
            rejects { protocol.status(resign(invalid, 59, "status")) }
        }
        for (flags in listOf(0, 0x80)) for (contact in 0..80) {
            val invalid = bytes("status_matrix_rectangle").also {
                it[47] = flags.toByte(); it.fill(0, 48, 59)
                it[48 + contact / 8] = (1 shl (contact % 8)).toByte()
            }
            rejects { protocol.status(resign(invalid, 59, "status")) }
        }
    }
    @Test fun matrixAuthenticatesCompleteBodyAndRejectsWrongSessionLengthAndDomain() {
        val (protocol, _) = authenticated()
        val message = bytes("status_matrix_rectangle")
        for (offset in message.indices) rejects {
            protocol.status(message.copyOf().also { it[offset] = (it[offset].toInt() xor 1).toByte() })
        }
        rejects { protocol.status(resign(message.copyOf().also { it[2] = (it[2].toInt() xor 1).toByte() }, 59, "status")) }
        rejects { protocol.status(resign(message, 59, "result")) }
        for (length in 0..128) {
            if (length == 91) continue
            rejects { protocol.status(message.copyOf(length)) }
        }
        rejects { protocol.status(resign(message, 49, "status")) }
        rejects { protocol.status(resign(message.copyOf(83), 51, "status")) }
        val noGrant = client(); noGrant.proof(bytes("challenge"))
        rejects { noGrant.status(message) }
    }
    @Test fun extendedGrantValidatesBeforeGrantingAndResultNeverInventsMatrix() {
        for ((offset, value) in listOf(47 to 0x81, 47 to 0x40, 58 to 2, 2 to 0)) {
            val protocol = client(); protocol.proof(bytes("challenge"))
            val invalid = bytes("granted_matrix_corners").also { it[offset] = value.toByte() }
            rejects { protocol.granted(resign(invalid, 59, "status")) }
            assertEquals(setOf(0, 80), protocol.granted(bytes("granted_matrix_corners")).matrix!!.pressed)
        }
        val protocol = client(); protocol.proof(bytes("challenge")); protocol.granted(bytes("granted_matrix_corners"))
        assertNull(protocol.result(bytes("result_start")).status.matrix)
        assertNull(protocol.result(bytes("result_stop")).status.matrix)
        rejects { protocol.result(resign(bytes("result_start").copyOf(86), 54, "result")) }
    }
    @Test fun matrixConstructorRejectsImpossibleObservations() {
        for (contact in listOf(-1, 81, Int.MAX_VALUE)) rejects { MatrixStatus(true, true, false, setOf(contact)) }
        rejects { MatrixStatus(false, true, false, emptySet()) }
        rejects { MatrixStatus(false, false, true, emptySet()) }
        rejects { MatrixStatus(true, false, true, emptySet()) }
        rejects { MatrixStatus(true, false, false, setOf(0)) }
        rejects { MatrixStatus(false, false, false, setOf(80)) }
    }
    @Test fun extendedMatrixStatusReassemblesBeforeItBecomesAnObservation() {
        val (protocol, _) = authenticated()
        val frames = LedFrames.encode(bytes("status_matrix_rectangle"), 1)
        assertEquals(8, frames.size)
        val assembler = LedFrameAssembler()
        frames.dropLast(1).forEach { assertNull(assembler.accept(it, 10)) }
        assertEquals(setOf(0, 1, 9, 10), protocol.status(assembler.accept(frames.last(), 10)!!).matrix!!.pressed)
        rejects { assembler.accept(frames.first(), 11) }
        val missing = LedFrameAssembler(); missing.accept(frames.first(), 0)
        rejects { missing.accept(frames[2], 1) }
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
