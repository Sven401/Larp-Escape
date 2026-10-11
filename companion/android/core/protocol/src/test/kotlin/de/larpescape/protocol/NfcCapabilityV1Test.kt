package de.larpescape.protocol

import org.junit.Assert.*
import org.junit.Test

class NfcCapabilityV1Test {
    private val id = ByteArray(16) { it.toByte() }
    private val challenge = ByteArray(16) { (it + 32).toByte() }
    private fun assertBytes(expected: ByteArray, actual: ByteArray) = assertArrayEquals(expected, actual)

    @Test fun exactGoldenFramesAndSingleUse() {
        var now = 100L
        val gate = NfcCapabilityGateV1 { now }
        assertBytes(NfcCapabilityV1.NOT_ARMED, gate.process(NfcCapabilityV1.SELECT))
        assertTrue(gate.arm(id))
        assertBytes(NfcCapabilityV1.hex("4E4301") + id + NfcCapabilityV1.hex("9000"),
            gate.process(NfcCapabilityV1.SELECT))
        assertBytes(NfcCapabilityV1.NOT_ARMED, gate.process(NfcCapabilityV1.SELECT))
        assertBytes(NfcCapabilityV1.hex("4E4301") + challenge + NfcCapabilityV1.hex("9000"),
            gate.process(NfcCapabilityV1.challengeCommand(challenge)))
        assertBytes(NfcCapabilityV1.NOT_ARMED, gate.process(NfcCapabilityV1.challengeCommand(challenge)))
        assertEquals("LOCAL_ECHO_ONLY", gate.snapshot().status)
        assertEquals("UNVERIFIED", gate.snapshot().deviceCorrelation)
        assertEquals(NfcCapabilityV1.bytesToHex(challenge), gate.snapshot().challenge)
        assertEquals(3, gate.snapshot().events.size)
        gate.deactivate()
        assertEquals("LOCAL_ECHO_ONLY", gate.snapshot().status)
        assertEquals("DEACTIVATED", gate.snapshot().events.last().kind)
    }

    @Test fun malformedWrongAidTimeoutAndDeactivationFailClosed() {
        var now = 5L
        val gate = NfcCapabilityGateV1 { now }
        assertTrue(gate.arm(id))
        assertBytes(NfcCapabilityV1.UNAVAILABLE, gate.process(NfcCapabilityV1.hex("00A4040008F04C4152504E464400")))
        assertBytes(NfcCapabilityV1.MALFORMED, gate.process(NfcCapabilityV1.SELECT + byteArrayOf(0)))
        assertBytes(NfcCapabilityV1.MALFORMED, gate.process(NfcCapabilityV1.hex("8010000010") + challenge.dropLast(1)))
        assertBytes(NfcCapabilityV1.NOT_ARMED, gate.process(NfcCapabilityV1.challengeCommand(challenge)))
        assertBytes(NfcCapabilityV1.response(id), gate.process(NfcCapabilityV1.SELECT))
        gate.deactivate()
        assertBytes(NfcCapabilityV1.NOT_ARMED, gate.process(NfcCapabilityV1.challengeCommand(challenge)))
        assertEquals("DEACTIVATED", gate.snapshot().status)
        assertTrue(gate.arm(id))
        now += 30_000L
        assertBytes(NfcCapabilityV1.NOT_ARMED, gate.process(NfcCapabilityV1.SELECT))
        assertEquals("TIMEOUT", gate.snapshot().status)
    }

    @Test fun stopAndBoundedEvents() {
        var now = 0L
        val gate = NfcCapabilityGateV1 { now }
        repeat(20) { gate.arm(id); gate.stop(); now++ }
        assertBytes(NfcCapabilityV1.NOT_ARMED, gate.process(NfcCapabilityV1.SELECT))
        assertTrue(gate.snapshot().events.size <= 12)
    }
}
