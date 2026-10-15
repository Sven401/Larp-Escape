package de.larpescape.protocol

import org.junit.Assert.*
import org.junit.Test

class NfcTestSessionTest {
    private fun hex(value: String) = value.replace(" ", "").chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private var time = 0L
    private fun newSession() = NfcTestSession { time }
    private fun send(session: NfcTestSession, command: String, expected: String = "9000") =
        assertArrayEquals(hex(expected), session.process(hex(command)))
    private fun select(session: NfcTestSession) {
        send(session, "00A4040007D276000085010100")
        send(session, "00A4000C02E104")
    }
    private fun armed() = newSession().also { it.start(); select(it) }
    private fun update(session: NfcTestSession, offset: Int, bytes: ByteArray): ByteArray =
        session.process(byteArrayOf(0, 0xd6.toByte(), (offset shr 8).toByte(), offset.toByte(), bytes.size.toByte()) + bytes)
    private fun stage(session: NfcTestSession, bytes: ByteArray = NfcTestFixture.message(NfcTestFixture.changed)) {
        send(session, "00D60000020000")
        assertArrayEquals(hex("9000"), update(session, 2, bytes))
    }
    private fun commit(session: NfcTestSession) = send(session, "00D6000002003B")

    @Test fun fixtureMatchesIndependentPayloadBytesAndRoundTrips() {
        val initial = NfcTestFixture.message(NfcTestFixture.initial)
        val changed = NfcTestFixture.message(NfcTestFixture.changed)
        assertEquals(59, initial.size)
        assertEquals(NfcTestFixture.initial, NfcTestFixture.readMessage(initial))
        assertArrayEquals(hex("019728350101000000000000000000010001010000F4617B57"), NfcTagDataCodec.encode(NfcTestFixture.initial))
        assertArrayEquals(hex("01972835010200000000000000000001000101000006D5B37E"), NfcTagDataCodec.encode(NfcTestFixture.changed))
        assertEquals(NfcTestFixture.changed, NfcTestFixture.readMessage(changed))
    }
    @Test fun startsInactiveAndNeverRestoresArmingOrEvidence() {
        val session = newSession()
        send(session, "00A4040007D2760000850101", "6985")
        assertFalse(session.state().verified)
        session.start(); select(session); stage(session); commit(session)
        session.stop(); send(session, "00B0000201", "6985")
        session.start()
        assertEquals(0, session.state().commits)
        assertEquals(NfcTestFixture.initial, session.state().data)
        assertFalse(newSession().state().active)
    }
    @Test fun readsCcAndPreparedNdefButDoesNotClaimWriteSuccess() {
        val session = newSession().also { it.start() }
        send(session, "00A4040007D2760000850101")
        send(session, "00A4000002E103")
        send(session, "00B000000F", "000F20010000FF0406E104020200009000")
        send(session, "00A4000C02E104")
        send(session, "00B0000002", "003B9000")
        assertArrayEquals(NfcTestFixture.message(NfcTestFixture.initial) + hex("9000"), session.process(hex("00B000023B")))
        assertEquals(2, session.state().reads)
        assertFalse(session.state().verified)
    }
    @Test fun onlyCompleteValidUpdateAndFullReaderReadbackVerify() {
        val session = armed()
        stage(session)
        assertEquals(NfcTestFixture.initial, session.state().data)
        assertEquals(0, session.state().commits)
        send(session, "00B0000002", "00009000")
        commit(session)
        assertTrue(session.state().boxChangeReceived)
        assertFalse(session.state().verified)
        send(session, "00B0000002", "003B9000")
        session.process(hex("00B0000210"))
        assertFalse(session.state().verified)
        assertArrayEquals(NfcTestFixture.message(NfcTestFixture.changed).copyOfRange(16, 59) + hex("9000"), session.process(hex("00B000122B")))
        assertTrue(session.state().verified)
        assertEquals(3, session.state().updates)
    }
    @Test fun splitAndOutOfOrderCompletePayloadIsAccepted() {
        val session = armed()
        val data = NfcTestFixture.message(NfcTestFixture.changed)
        send(session, "00D60000020000")
        assertArrayEquals(hex("9000"), update(session, 22, data.copyOfRange(20, data.size)))
        assertArrayEquals(hex("9000"), update(session, 2, data.copyOfRange(0, 20)))
        commit(session)
        assertTrue(session.state().boxChangeReceived)
    }
    @Test fun missingByteCannotBeCommittedAndFailureDiscardsTransaction() {
        val session = armed()
        stage(session, NfcTestFixture.message(NfcTestFixture.changed).copyOf(58))
        send(session, "00D6000002003B", "6985")
        send(session, "00D6000002003B", "6985")
        assertEquals(NfcTestFixture.initial, session.state().data)
    }
    @Test fun trailingStagedByteAndOutOfCapacityAreRejected() {
        val session = armed()
        stage(session, NfcTestFixture.message(NfcTestFixture.changed) + 0)
        send(session, "00D6000002003B", "6985")
        send(session, "00D602020100", "6B00")
        assertEquals(0, session.state().commits)
    }
    @Test fun invalidCrcForeignRecordAndWrongValidFixtureAreRejected() {
        val correct = NfcTestFixture.message(NfcTestFixture.changed)
        val candidates = listOf(
            correct.copyOf().also { it[it.lastIndex] = (it.last().toInt() xor 1).toByte() },
            correct.copyOf().also { it[3] = 'b'.code.toByte() },
            NfcTestFixture.message(NfcTestFixture.changed.copy(uidHash = 42)),
            NfcTestFixture.message(NfcTestFixture.changed.copy(timestampBinary = 1)),
            NfcTestFixture.message(NfcTestFixture.initial),
        )
        candidates.forEach { bytes ->
            val session = armed(); stage(session, bytes)
            send(session, "00D6000002003B", "6A80")
            assertEquals(0, session.state().commits)
            assertEquals(NfcTestFixture.initial, session.state().data)
        }
    }
    @Test fun updatesRequireAppNdefSelectionAndExplicitEmptyNlen() {
        val session = newSession().also { it.start() }
        send(session, "00D60000020000", "6986")
        send(session, "00A4040007D2760000850101")
        send(session, "00A4000C02E103")
        send(session, "00D60000020000", "6986")
        send(session, "00A4000C02E104")
        send(session, "00D600020100", "6985")
        send(session, "00D6000002003B", "6985")
    }
    @Test fun linkLossPauseAndExpiryDiscardPendingBytes() {
        val session = armed(); stage(session)
        session.linkLost(); select(session)
        send(session, "00D6000002003B", "6985")
        stage(session); session.stop()
        assertFalse(session.state().active)
        send(session, "00D6000002003B", "6985")
        session.start(); select(session); stage(session)
        time = NfcTestSession.SESSION_MILLIS
        send(session, "00D6000002003B", "6985")
        assertFalse(session.state().active)
        assertEquals(0, session.state().commits)
    }
    @Test fun timeoutCanBeObservedWithoutAnotherRfCommand() {
        val session = armed()
        time = NfcTestSession.SESSION_MILLIS + 1
        assertFalse(session.state().active)
    }
    @Test fun malformedApdusAreBoundedAndCannotCommit() {
        val session = armed()
        for (command in listOf("", "00", "00A400", "80B0000001", "00A4040000", "00A4040007D2", "00D6000002000000", "00B0800001", "00B0000200", "00FF0000")) {
            val response = session.process(hex(command))
            assertFalse(response.contentEquals(hex("9000")))
        }
        assertArrayEquals(hex("6700"), session.process(ByteArray(261)))
        assertEquals(0, session.state().commits)
    }
    @Test fun malformedUpdateCancelsExistingStaging() {
        val session = armed(); stage(session)
        send(session, "00D6000003FFFF", "6700")
        send(session, "00D6000002003B", "6985")
        assertEquals(0, session.state().commits)
    }
    @Test fun randomShortApdusNeverThrowOrActivateSession() {
        val session = armed()
        val random = java.util.Random(5)
        repeat(10000) {
            val bytes = ByteArray(random.nextInt(300)); random.nextBytes(bytes)
            assertTrue(session.process(bytes).size >= 2)
        }
        assertEquals(0, session.state().commits)
        session.stop()
        repeat(100) { assertArrayEquals(hex("6985"), session.process(ByteArray(it))) }
    }
}
