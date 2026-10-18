package de.larpescape.protocol

import com.google.gson.JsonParser
import org.junit.Assert.*
import org.junit.Test
import java.io.File
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

class CommonWireTest {
    private val fixture = JsonParser.parseString(File(System.getProperty("ledFixtures"), "wire-v2.json").readText()).asJsonObject
    private fun bytes(name: String) = fixture[name].asString.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private fun client(): LedProtocol {
        val access = LedAccess.parse("{\"schemaVersion\":1,\"deviceId\":\"${fixture["device"].asString}\",\"secret\":\"${fixture["secret"].asString}\"}".toByteArray())
        return LedProtocol(access, bytes("client"))
    }
    private fun signed(body: ByteArray, domain: String = "LARP-LED/2/status"): ByteArray {
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(SecretKeySpec(bytes("secret"), "HmacSHA256"))
        mac.update(domain.toByteArray(Charsets.US_ASCII))
        return body + mac.doFinal(body)
    }
    private fun rejects(action: () -> Unit) {
        try { action(); fail("Expected rejection") } catch (_: IllegalArgumentException) {} catch (_: IllegalStateException) {}
    }
    private fun authenticated() = client().also { it.proof(bytes("challenge")); it.granted(bytes("granted")) }

    @Test fun allCapabilitiesHaveIndependentAuthenticatedFieldsInOneStatus() {
        val protocol = authenticated()
        val status = protocol.status(bytes("status_all"))
        assertEquals(WakeStatus(true, true, 5), status.wake)
        assertTrue(status.nfcSupported)
        assertEquals(AudioStatus(0, 0, 5, 9), status.audio)
        assertEquals(MatrixStatus(true, true, false, setOf(0, 80)), status.matrix)
        for (offset in bytes("status_all").indices) rejects {
            protocol.status(bytes("status_all").also { it[offset] = (it[offset].toInt() xor 1).toByte() })
        }
    }

    @Test fun unsupportedCapabilitiesRequireZeroPayloadAndUnknownFlagsFailClosed() {
        val protocol = authenticated()
        val status = protocol.status(bytes("status_twinkle"))
        assertFalse(status.nfcSupported); assertFalse(status.wake!!.supported)
        assertFalse(status.audio!!.supported); assertFalse(status.matrix!!.supported)
        for (flag in 8..255) rejects {
            protocol.status(signed(bytes("status_all").copyOf(59).also { it[38] = flag.toByte() }))
        }
        rejects { protocol.status(signed(bytes("status_all").copyOf(59).also { it[38] = 0 })) }
        rejects { protocol.status(signed(bytes("status_all").copyOf(59).also { it[47] = 0 })) }
        rejects { protocol.audioCommand(status, true, 1, 5) }
    }

    @Test fun versionOneAndMixedDomainsCannotGrantOrDecodeAnyControlResult() {
        rejects { client().proof(bytes("challenge").also { it[0] = 1 }) }
        val protocol = authenticated()
        val body = bytes("status_all").copyOf(59)
        rejects { protocol.status(signed(body, "LARP-LED/1/status")) }
        rejects { protocol.status(signed(body.also { it[0] = 1 })) }
        for (size in listOf(70, 78, 82, 90, 92, 128)) rejects { protocol.status(bytes("status_all").copyOf(size)) }
        rejects { protocol.audioResult(bytes("nfc_read_result")) }
        rejects { protocol.nfcResult(bytes("result_audio_100")) }
        rejects { protocol.result(bytes("result_audio_100")) }
        rejects { protocol.audioResult(bytes("result_audio_100").also { it[1] = 0x86.toByte() }) }
        rejects { protocol.nfcResult(bytes("nfc_read_result").also { it[1] = 0x85.toByte() }) }
    }

    @Test fun priorConnectionCannotAuthenticateStatusAfterSessionChanges() {
        val next = client()
        val challenge = bytes("challenge").copyOf(66).also { it[34] = (it[34].toInt() xor 1).toByte() }
        next.proof(signed(challenge, "LARP-LED/2/challenge"))
        rejects { next.granted(bytes("granted")) }
        next.close()
        rejects { next.status(bytes("status_all")) }
    }
}
