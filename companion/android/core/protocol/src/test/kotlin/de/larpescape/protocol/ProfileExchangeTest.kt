package de.larpescape.protocol

import java.io.File
import org.junit.Assert.*
import org.junit.Test

class ProfileExchangeTest {
    private var now = 0L
    private fun exchange() = ProfileExchange(7) { now }
    private fun fixture(name: String): ByteArray = File(System.getProperty("profileFixtures"), "control-$name-v1.txt")
        .readText().trim().chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private fun illegal(block: () -> Unit) { try { block(); fail("Expected state rejection") } catch (_: IllegalStateException) { } }
    private fun sourceSent(exchange: ProfileExchange, transfer: Int = 1) {
        exchange.begin(byteArrayOf(1, 2, 3), transfer, 2, 1)
        repeat(2) { val value = requireNotNull(exchange.nextValue()); assertTrue(exchange.copied(7, value.id)) }
        assertEquals(ProfileExchange.Phase.SOURCE_SENT, exchange.phase)
    }
    private fun draft(exchange: ProfileExchange) {
        sourceSent(exchange); val finish = exchange.finish()
        assertArrayEquals(fixture("finish-command"), finish.bytes())
        assertTrue(exchange.copied(7, finish.id)); assertTrue(exchange.receive(7, fixture("draft")))
    }

    @Test fun fullTransactionRequiresExplicitApplyAndFinalSaved() {
        val exchange = exchange(); sourceSent(exchange)
        illegal { exchange.apply() }
        val finish = exchange.finish(); assertTrue(exchange.copied(7, finish.id))
        assertEquals(ProfileExchange.Phase.FINISHING, exchange.phase)
        assertTrue(exchange.receive(7, fixture("draft")))
        assertEquals(ProfileExchange.Phase.DRAFT_READY, exchange.phase); assertNull(exchange.nextValue())
        val apply = exchange.apply(); assertArrayEquals(fixture("apply-command"), apply.bytes())
        assertTrue(exchange.copied(7, apply.id))
        assertTrue(exchange.receive(7, fixture("queued")))
        assertEquals(ProfileExchange.Phase.QUEUED, exchange.phase)
        assertTrue(exchange.receive(7, fixture("saved")))
        assertEquals(ProfileExchange.Phase.SAVED, exchange.phase)
        assertFalse(exchange.receive(7, fixture("queued")))
    }

    @Test fun staleConnectionsWrongRequestsAndDuplicateWriteCallbacksCannotAdvance() {
        val exchange = exchange(); exchange.begin(byteArrayOf(1), 1, 2, 1)
        val metadata = requireNotNull(exchange.nextValue())
        metadata.bytes().fill(0); assertEquals(1.toByte(), exchange.nextValue()!!.bytes()[0])
        assertFalse(exchange.copied(8, metadata.id)); assertTrue(exchange.copied(7, metadata.id))
        val data = requireNotNull(exchange.nextValue())
        assertFalse(exchange.copied(7, metadata.id)); assertTrue(exchange.copied(7, data.id))
        val finish = exchange.finish(); exchange.copied(7, finish.id)
        assertFalse(exchange.receive(8, fixture("draft")))
        assertFalse(exchange.receive(7, fixture("draft").also { it[2] = 2 }))
        assertFalse(exchange.receive(7, fixture("draft").also { it[4] = 2 }))
        assertEquals(ProfileExchange.Phase.FINISHING, exchange.phase)
        assertTrue(exchange.receive(7, fixture("draft")))
    }

    @Test fun originalOperationDeadlineIsNotRefreshedByQueuedOrDuplicates() {
        val exchange = exchange(); draft(exchange); exchange.apply()
        now = 5000; assertTrue(exchange.receive(7, fixture("queued")))
        now = 9999; assertTrue(exchange.receive(7, fixture("queued")))
        now = 10000; assertFalse(exchange.receive(7, fixture("saved")))
        assertEquals(ProfileExchange.Phase.UNKNOWN, exchange.phase); assertNull(exchange.response)
        illegal { exchange.begin(byteArrayOf(1), 2, 4, 2) }
    }

    @Test fun disconnectAfterApplyCannotSaveOrReplayOnReconnect() {
        val exchange = exchange(); draft(exchange); exchange.apply()
        exchange.disconnected(8); assertEquals(ProfileExchange.Phase.APPLYING, exchange.phase)
        exchange.disconnected(7); assertEquals(ProfileExchange.Phase.UNKNOWN, exchange.phase)
        assertNull(exchange.nextValue()); assertFalse(exchange.receive(7, fixture("saved")))
        illegal { exchange.apply() }
        val reconnect = ProfileExchange(8) { now }
        assertFalse(reconnect.receive(7, fixture("saved")))
        assertEquals(ProfileExchange.Phase.IDLE, reconnect.phase)
    }

    @Test fun semanticImportFailureNeverEnablesApply() {
        val exchange = exchange(); sourceSent(exchange); exchange.finish()
        val rejected = fixture("draft").also { it[9] = 6; it[10] = 6 }
        assertTrue(exchange.receive(7, rejected)); assertEquals(ProfileExchange.Phase.REJECTED, exchange.phase)
        illegal { exchange.apply() }
        sourceSent(exchange, 2)
        assertEquals(2.toByte(), exchange.finish().bytes()[8]) // Request counter belongs to connection.
    }

    @Test fun applyFailureRemainsUncertainAndWrongRevisionCannotConfirm() {
        val exchange = exchange(); draft(exchange); exchange.apply()
        assertTrue(exchange.receive(7, fixture("failed")))
        assertEquals(ProfileExchange.Phase.UNKNOWN, exchange.phase)
        val other = exchange(); draft(other); other.apply()
        assertFalse(other.receive(7, fixture("saved").also { it[16] = 3 }))
        assertEquals(ProfileExchange.Phase.UNKNOWN, other.phase)
        val malformed = exchange(); draft(malformed); malformed.apply()
        try { malformed.receive(7, byteArrayOf(1)); fail("Malformed reply") } catch (_: IllegalArgumentException) { }
        assertEquals(ProfileExchange.Phase.UNKNOWN, malformed.phase)
    }

    @Test fun sourceAbsoluteDeadlineAndClockRollbackCloseTheTransaction() {
        val exchange = exchange(); exchange.begin(ByteArray(128), 1, 2, 1)
        repeat(12) { i ->
            val write = exchange.nextValue()!!; now = (i + 1) * 9999L; exchange.copied(7, write.id)
        }
        now = 120000; exchange.tick(); assertEquals(ProfileExchange.Phase.UNKNOWN, exchange.phase)
        now = 100; val rollback = exchange(); rollback.begin(byteArrayOf(1), 1, 2, 1)
        now = 99; rollback.tick(); assertEquals(ProfileExchange.Phase.UNKNOWN, rollback.phase)
    }

    @Test fun exactDraftAndConfigurationMustMatchEvenIfQueuedIsMissing() {
        for (wrong in listOf(3, 5)) {
            val exchange = exchange(); draft(exchange); val apply = exchange.apply()
            assertFalse(exchange.receive(7, fixture("saved").also { it[12] = wrong.toByte() }))
            assertEquals(ProfileExchange.Phase.UNKNOWN, exchange.phase)
            assertFalse(exchange.copied(7, apply.id))
        }
        val exchange = exchange(); draft(exchange); val apply = exchange.apply()
        assertTrue(exchange.receive(7, fixture("saved")))
        assertEquals(ProfileExchange.Phase.SAVED, exchange.phase)
        assertFalse(exchange.copied(7, apply.id))
        val changed = exchange(); sourceSent(changed); changed.finish()
        assertFalse(changed.receive(7, fixture("draft").also { it[12] = 4 }))
        assertEquals(ProfileExchange.Phase.UNKNOWN, changed.phase)
    }

    @Test fun nonconsumingOrAmbiguousRequestErrorsRequireFreshSession() {
        for (code in listOf(8, 9, 10, 11, 12)) {
            val exchange = exchange(); sourceSent(exchange); exchange.finish()
            assertTrue(exchange.receive(7, fixture("draft").also { it[9] = code.toByte() }))
            assertEquals(ProfileExchange.Phase.UNKNOWN, exchange.phase)
            illegal { exchange.begin(byteArrayOf(1), 2, 3, 1) }
        }
    }

    @Test fun missingWriteConfirmationExpiresAndNoAutomaticRetryBecomesAvailable() {
        val exchange = exchange(); exchange.begin(byteArrayOf(1), 1, 2, 1)
        val original = exchange.nextValue()!!
        now = 10000; assertNull(exchange.nextValue())
        assertFalse(exchange.copied(7, original.id)); assertEquals(ProfileExchange.Phase.UNKNOWN, exchange.phase)
    }
}
