package de.larpescape.gamemaster

import de.larpescape.gamemaster.station.*
import de.larpescape.protocol.LedAccess
import org.junit.Assert.*
import org.junit.Test
import java.io.File

class StationProofTest {
    private val fixture = File(System.getProperty("ledFixtures"),"wire-v2.json").readText()
    private fun bytes(name: String): ByteArray {
        val value = Regex("\"" + name + "\"\\s*:\\s*\"([a-fA-F0-9]+)\"").find(fixture)!!.groupValues[1]
        return value.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    }
    private fun access(): LedAccess {
        fun hex(name: String) = bytes(name).joinToString("") { "%02x".format(it) }
        return LedAccess.parse(("{\"schemaVersion\":1,\"deviceId\":\"${hex("device")}\",\"secret\":\"${hex("secret")}\"}").toByteArray())
    }
    private fun proof() = StationProof(access(),bytes("client"))
    private fun authenticated(): StationProof = proof().also { it.hello(); it.receive(bytes("challenge")); it.receive(bytes("granted")) }
    @Test fun genuineMutualHandshakeReturnsOnlyHelloAndProofThenSignedState() {
        val proof = proof(); assertArrayEquals(bytes("hello"),proof.hello())
        val challenge = proof.receive(bytes("challenge")); assertArrayEquals(bytes("proof"),challenge.reply); assertNull(challenge.status)
        val granted = proof.receive(bytes("granted")); assertTrue(granted.status!!.off); assertNull(granted.reply)
        val next = proof.receive(bytes("status_twinkle")); assertEquals(1,next.status!!.effect); assertNull(next.reply)
        assertNull(proof.receive(bytes("status_twinkle")).status)
    }
    @Test fun wrongDeviceNeverProducesAProof() {
        val proof = proof(); proof.hello()
        try { proof.receive(bytes("challenge").also { it[2] = (it[2].toInt() xor 1).toByte() }); fail("wrong device accepted") } catch (_: AssignmentMismatch) {}
    }
    @Test fun tamperedMacNeverProducesConfirmedState() {
        val proof = authenticated()
        try { proof.receive(bytes("status_twinkle").also { it[80] = (it[80].toInt() xor 1).toByte() }); fail("forgery accepted") } catch (_: IllegalArgumentException) {}
    }
    @Test fun unsolicitedGrantAndControlResultsAreRejected() {
        for (name in listOf("granted","result_start","result_stop")) {
            val proof = proof(); proof.hello()
            try { proof.receive(bytes(name)); fail("unsolicited message accepted") } catch (_: IllegalStateException) {}
        }
    }
    @Test fun closedConnectionCannotProduceAState() {
        val proof = authenticated(); proof.close()
        try { proof.receive(bytes("status_twinkle")); fail("closed connection accepted") } catch (_: IllegalStateException) {}
    }
}
