package de.larpescape.protocol

import com.google.gson.JsonParser
import java.math.BigInteger
import org.junit.Assert.*
import org.junit.Test

class PeerAuthV1Test {
    private fun fixture() = requireNotNull(javaClass.getResourceAsStream("/firmware-peer-auth-observation.json"))
        .bufferedReader().use { JsonParser.parseReader(it).asJsonObject }

    private fun hex(text: String) = text.chunked(2).map { it.toInt(16).toByte() }.toByteArray()

    private fun rejects(block: () -> Unit) {
        try { block(); fail("Expected rejection") } catch (_: IllegalArgumentException) { }
    }

    @Test fun matchesActualFirmwareIssueChallengeProofAndResult() {
        val observation = fixture()
        val challengeBytes = hex(observation.get("challengeHex").asString)
        val expectedDevice = challengeBytes.copyOfRange(20, 36)
        val expectedPeer = challengeBytes.copyOfRange(60, 76)
        assertArrayEquals(hex(observation.get("issueHex").asString), PeerAuthV1.issue(expectedPeer))
        val challenge = PeerAuthV1.challenge(challengeBytes, expectedDevice, expectedPeer)
        val expectedFrames = observation.getAsJsonArray("proofFrames").map { hex(it.asString) }
        val proof = expectedFrames.flatMap { it.asList().drop(8) }.toByteArray()
        assertEquals(72, proof.size)
        assertEquals(1L, challenge.sequence)
        val frames = PeerAuthV1.proofFrames(challenge, proof.copyOfRange(8, 72), 9)
        assertEquals(expectedFrames.size, frames.size)
        frames.zip(expectedFrames).forEach { (actual, expected) -> assertArrayEquals(expected, actual) }
        val signature = proof.copyOfRange(8, 72)
        val order = BigInteger("FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551", 16)
        val highS = order.subtract(BigInteger(1, signature.copyOfRange(32, 64)))
            .toByteArray().takeLast(32).toByteArray()
        val highSignature = signature.copyOfRange(0, 32) + ByteArray(32 - highS.size) + highS
        PeerAuthV1.proofFrames(challenge, highSignature, 9).zip(expectedFrames)
            .forEach { (actual, expected) -> assertArrayEquals(expected, actual) }
        val result = PeerAuthV1.result(hex(observation.get("resultHex").asString), challenge, 9)
        assertEquals(PeerAuthState.GRANTED, result.state)
        assertEquals(7L, result.principal)
        assertArrayEquals(expectedPeer, result.peerId())

        // Neither a borrowed callback buffer nor a returned copy may change the bound transcript.
        challengeBytes.fill(0)
        val returned = challenge.transcript(); returned.fill(0)
        assertArrayEquals(expectedDevice, challenge.deviceId())
        assertArrayEquals(expectedPeer, challenge.peerId())
        assertEquals(1L, challenge.sequence)
    }

    @Test fun rejectsWrongScopeFramesAndContradictoryResults() {
        val observation = fixture()
        val raw = hex(observation.get("challengeHex").asString)
        val device = raw.copyOfRange(20, 36)
        val peer = raw.copyOfRange(60, 76)
        val challenge = PeerAuthV1.challenge(raw, device, peer)
        rejects { PeerAuthV1.issue(ByteArray(16)) }
        rejects { PeerAuthV1.challenge(raw, ByteArray(16) { 1 }, peer) }
        rejects { PeerAuthV1.challenge(raw, device, ByteArray(16) { 1 }) }
        raw[55] = 1
        rejects { PeerAuthV1.challenge(raw, device, peer) }
        rejects { PeerAuthV1.proofFrames(challenge, ByteArray(63), 9) }
        rejects { PeerAuthV1.proofFrames(challenge, ByteArray(64), 9) }
        rejects { PeerAuthV1.proofFrames(challenge, ByteArray(64), 0) }
        rejects { PeerAuthV1.proofFrames(challenge, ByteArray(64), 9, 8) }

        val result = hex(observation.get("resultHex").asString)
        rejects { PeerAuthV1.result(result, challenge, 10) }
        rejects { PeerAuthV1.result(result.clone().also { it[8] = 2 }, challenge, 9) }
        rejects { PeerAuthV1.result(result.clone().also { it[16] = 3 }, challenge, 9) }
        rejects { PeerAuthV1.result(result.clone().also { it[12] = 0 }, challenge, 9) }
        rejects { PeerAuthV1.result(result.clone().also { it[3] = 1 }, challenge, 9) }
        rejects { PeerAuthV1.result(result.clone().also { it[6] = 71 }, challenge, 9) }
    }

    @Test fun stateRulesNeverTreatPendingOrAuthenticationAsGranted() {
        val observation = fixture()
        val raw = hex(observation.get("challengeHex").asString)
        val challenge = PeerAuthV1.challenge(raw, raw.copyOfRange(20, 36), raw.copyOfRange(60, 76))
        val granted = hex(observation.get("resultHex").asString)
        val pending = granted.clone().also { it[2] = 3; it[12] = 0 }
        val authenticated = pending.clone().also { it[2] = 4 }
        val receiving = pending.clone().also { it[2] = 1; it[6] = 12 }
        val rejected = pending.clone().also { it[2] = 2; it[3] = 3 }
        assertEquals(PeerAuthState.GRANT_PENDING, PeerAuthV1.result(pending, challenge, 9).state)
        assertEquals(PeerAuthState.AUTHENTICATED, PeerAuthV1.result(authenticated, challenge, 9).state)
        assertEquals(PeerAuthState.RECEIVING, PeerAuthV1.result(receiving, challenge, 9).state)
        assertEquals(PeerAuthState.REJECTED, PeerAuthV1.result(rejected, challenge, 9).state)
        rejects { PeerAuthV1.result(pending.clone().also { it[12] = 7 }, challenge, 9) }
    }
}
