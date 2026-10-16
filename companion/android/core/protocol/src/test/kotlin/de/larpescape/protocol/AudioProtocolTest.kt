package de.larpescape.protocol

import com.google.gson.JsonParser
import org.junit.Assert.*
import org.junit.Test
import java.io.File
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

class AudioProtocolTest {
    private val fixture = JsonParser.parseString(File(System.getProperty("ledFixtures"), "wire-v2.json").readText()).asJsonObject
    private fun hex(value: String) = value.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private fun bytes(name: String) = hex(fixture[name].asString)
    private fun signed(body: ByteArray, domain: String): ByteArray {
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(SecretKeySpec(bytes("secret"), "HmacSHA256"))
        mac.update("LARP-LED/2/$domain".toByteArray(Charsets.US_ASCII))
        return body + mac.doFinal(body)
    }
    private fun client(): LedProtocol {
        val access = LedAccess.parse("{\"schemaVersion\":1,\"deviceId\":\"${fixture["device"].asString}\",\"secret\":\"${fixture["secret"].asString}\"}".toByteArray())
        return LedProtocol(access, bytes("client")).also { it.proof(bytes("challenge")) }
    }
    // Independent explicit wire octets: revision 7, track 42, volume 5, ready/BUSY-valid/BUSY-low.
    private fun extended(name: String = "granted", audio: ByteArray = hex("070000002a000519")) =
        signed(bytes(name).copyOf(59).also { it[38] = 1; audio.copyInto(it, 39) }, "status")
    private fun authenticated(): Pair<LedProtocol, LedStatus> {
        val protocol = client()
        return protocol to protocol.granted(extended())
    }
    private fun result(): ByteArray = signed(bytes("result_start").copyOf(42).also { it[1] = 0x85.toByte() } +
        hex("080000002a00051d"), "audio-result")
    private fun rejects(action: () -> Unit) {
        try { action(); fail("Expected rejected input") } catch (_: IllegalArgumentException) { } catch (_: IllegalStateException) { }
    }

    @Test fun pauseResumeAndVolumeHaveDistinctCommandsAndValidatedState() {
        val (protocol, initial) = authenticated()
        val playing = initial.copy(audio = AudioStatus(7, 42, 5, 13))
        val pause = protocol.audioAction(playing, 3)
        assertArrayEquals(signed(pause.copyOf(34).also { it[30] = 3; it[31] = 0; it[32] = 0; it[33] = 0 }, "audio-command"), pause)
        assertEquals(3, pause[30].toInt())
        rejects { protocol.audioAction(initial, 3) }
        rejects { protocol.audioAction(initial, 4) }
        val paused = protocol.status(extended("status_twinkle", hex("070000002a000549")))
        assertTrue(paused.audio!!.paused); assertFalse(paused.audio!!.active)
        assertEquals(4, protocol.audioAction(paused, 4)[30].toInt())
        val volume = protocol.audioAction(paused, 5, 8)
        assertEquals(5, volume[30].toInt()); assertEquals(8, volume[33].toInt())
        assertEquals(0, volume[31].toInt()); assertEquals(0, volume[32].toInt())
        for (v in listOf(0, 11, -1)) rejects { protocol.audioAction(paused, 5, v) }
        rejects { protocol.status(extended("status_twinkle", hex("070000002a00054d"))) }
    }
    @Test fun legacyStatusCannotEnableAudioAndLedResultsNeverCarryAnObservation() {
        val protocol = client()
        assertFalse(protocol.granted(bytes("granted")).audio!!.supported)
        val legacy = protocol.status(bytes("status_twinkle"))
        assertFalse(legacy.audio!!.supported)
        rejects { protocol.audioCommand(legacy, true, 1, 5) }
        rejects { protocol.audioCommand(legacy, false) }
        assertNull(protocol.result(bytes("result_start")).status.audio)
    }

    @Test fun extendedStatusPreservesLegacyFieldsAndAuthenticatesEveryAudioByte() {
        val (protocol, status) = authenticated()
        assertEquals(0L, status.revision); assertEquals(1L, status.nextRequest)
        assertEquals(AudioStatus(7, 42, 5, 25), status.audio)
        assertEquals(WakeStatus(false, false, 0), status.wake)
        assertTrue(status.audio!!.busyValid); assertTrue(status.audio!!.busyLow)
        val extended = extended("status_twinkle")
        assertEquals(AudioStatus(7, 42, 5, 25), protocol.status(extended).audio)
        for (offset in 38..46) rejects {
            protocol.status(extended.copyOf().also { it[offset] = (it[offset].toInt() xor 1).toByte() })
        }
        rejects { protocol.status(extended.copyOf(77)) }
        rejects { protocol.status(signed(extended.copyOf(46), "audio-result")) }
    }

    @Test fun malformedSignedObservationsCannotEnableControls() {
        val (protocol, _) = authenticated()
        for (flags in 128..255) rejects {
            protocol.status(extended("status_twinkle", hex("070000002a000519").also { it[7] = flags.toByte() }))
        }
        for (audio in listOf("0700000000010519", "070000002a000b19", "070000002a000511",
            "070000002a000521", "0700000000000505", "070000002a000005")) {
            rejects { protocol.status(extended("status_twinkle", hex(audio))) }
        }
    }

    @Test fun commandsBindAudioRevisionSharedRequestAndExactDomain() {
        val (protocol, status) = authenticated()
        val start = protocol.audioCommand(status, true, 42, 5)
        val expected = hex("0204") + bytes("session") + hex("0100000007000000a20f0000012a0005")
        assertEquals(66, start.size)
        assertArrayEquals(signed(expected, "audio-command"), start)
        val stop = hex("0204") + bytes("session") + hex("0100000007000000a20f000002000000")
        assertArrayEquals(signed(stop, "audio-command"), protocol.audioCommand(status, false))
        assertFalse(signed(expected, "command").contentEquals(start))
        val wrapped = protocol.audioCommand(status.copy(deviceMillis = 0xffff_ff00L), true, 1, 5)
        assertArrayEquals(hex("b80a0000"), wrapped.copyOfRange(26, 30))
    }

    @Test fun audioGatingAndBoundsAreIndependentOfLedDriver() {
        val (protocol, status) = authenticated()
        for (track in listOf(0, 256, -1)) rejects { protocol.audioCommand(status, true, track, 5) }
        for (volume in listOf(0, 11, -1)) rejects { protocol.audioCommand(status, true, 1, volume) }
        rejects { protocol.audioCommand(status, false, 1, 0) }
        rejects { protocol.audioCommand(status, false, 0, 1) }
        for (mode in 2..4) {
            rejects { protocol.audioCommand(status.copy(mode = mode), true, 1, 5) }
            rejects { protocol.audioCommand(status.copy(mode = mode), false) }
        }
        for (flags in listOf(0, 3, 32)) {
            rejects { protocol.audioCommand(status.copy(audio = AudioStatus(flags = flags)), true, 1, 5) }
            rejects { protocol.audioCommand(status.copy(audio = AudioStatus(flags = flags)), false) }
        }
        assertEquals(66, protocol.audioCommand(status.copy(flags = 20), true, 255, 10).size)
        assertEquals(66, protocol.audioCommand(status.copy(mode = 1), true, 1, 1).size)
    }

    @Test fun audioResultIsAuthenticatedAndRequiresWriteAckInEitherOrder() {
        val (protocol, _) = authenticated()
        val result = protocol.audioResult(result())
        assertEquals(1L, result.request); assertEquals(0, result.outcome)
        assertEquals(AudioStatus(8, 42, 5, 29), result.status.audio)
        assertNull(result.status.wake)
        for (offset in 42..49) rejects {
            protocol.audioResult(result().also { it[offset] = (it[offset].toInt() xor 1).toByte() })
        }
        rejects { protocol.audioResult(signed(result().copyOf(50), "result")) }
        rejects { protocol.result(result()) }
        val beforeAck = LedCommandTracker(); beforeAck.begin(1)
        assertNull(beforeAck.received(result)); assertEquals(1L, beforeAck.request)
        assertEquals(result, beforeAck.wrote()); assertNull(beforeAck.request)
        val afterAck = LedCommandTracker(); afterAck.begin(1)
        assertNull(afterAck.wrote()); assertEquals(result, afterAck.received(result))
        assertNull(afterAck.request)
    }
    @Test fun matchesIndependentPythonAndFirmwareAudioWireVectors() {
        val protocol = client()
        val initial = protocol.granted(bytes("granted_audio"))
        assertEquals(AudioStatus(0, 0, 5, 9), initial.audio)
        assertArrayEquals(bytes("command_audio_100"), protocol.audioCommand(initial, true, 100, 5))
        val result = protocol.audioResult(bytes("result_audio_100"))
        assertEquals(1L, result.request); assertEquals(0, result.outcome)
        assertEquals(0L, result.status.revision); assertEquals(2L, result.status.nextRequest)
        assertEquals(AudioStatus(1, 100, 5, 13), result.status.audio)
    }
}
