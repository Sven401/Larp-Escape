package de.larpescape.companion.data.device

import org.junit.Assert.*
import org.junit.Test
import de.larpescape.protocol.NearGlowFrameAssembler

class NearGlowStateTest {
    @Test fun assemblyAndQueuedDeliveryCannotRefreshOldSource() {
        val payload = ByteArray(145) { it.toByte() }
        val assembler = NearGlowFrameAssembler()
        var complete: ByteArray? = null
        (payload.indices step 12).forEachIndexed { index, offset ->
            val frame = byteArrayOf(1, 0x4c, 1, 0, 145.toByte(), 0, offset.toByte(), 0) +
                payload.copyOfRange(offset, minOf(offset + 12, payload.size))
            complete = assembler.accept(frame, 1000L + index * 125)
        }
        assertArrayEquals(payload, complete)
        val start = requireNotNull(assembler.completedStartedAt)
        assertEquals(1000L, start)
        val freshAtFirstFragment = reading.copy(receivedAt = start, sourceAgeMs = 0)
        assertTrue(freshAtFirstFragment.sourceFresh(2500))
        assertFalse(freshAtFirstFragment.copy(sourceAgeMs = 2000).sourceFresh(2500))
        // Delayed main-thread delivery must not replace the earlier callback timestamp.
        assertFalse(freshAtFirstFragment.handsetFresh(3500))
        assertFalse(freshAtFirstFragment.sourceFresh(3500))
    }
    @Test fun lateAckConfirmsOnlyItsOwnCurveNotAnEditedOrInvalidNewDraft() {
        val old = NearGlowCurve.DEFAULT
        val edited = NearGlowCurve(old.points.map { it.copy(percent = 40) })
        assertEquals(NearGlowChange.CONFIRMED, nearGlowChangeOutcome(old, old, true, true, NearGlowChange.SENT, 8, 0).first)
        for (draft in listOf(edited, null)) {
            val result = nearGlowChangeOutcome(draft, old, true, true, NearGlowChange.SENT, 8, 0)
            assertEquals(NearGlowChange.DRAFT, result.first)
            assertTrue(result.second.contains("neuer Entwurf"))
        }
        assertEquals(NearGlowChange.UNCLEAR, nearGlowChangeOutcome(edited, old, false, false, NearGlowChange.UNCLEAR, 8, 0).first)
        assertEquals(NearGlowChange.DRAFT, nearGlowChangeOutcome(edited, old, false, false, NearGlowChange.CONFIRMED, 8, 0).first)
        assertEquals(NearGlowChange.UNCLEAR, nearGlowChangeOutcome(old, old, false, true, NearGlowChange.SENT, 8, 5, durable = false).first)
        assertEquals(NearGlowChange.UNCLEAR, nearGlowChangeOutcome(old, old, false, true, NearGlowChange.SENT, 8, 5, durable = true).first)
        val stored = nearGlowChangeOutcome(old, old, false, true, NearGlowChange.SENT, 8, 9)
        assertEquals(NearGlowChange.REJECTED, stored.first); assertTrue(stored.second.startsWith("Gespeichert"))
    }
    @Test fun curveRejectsReversedDuplicateDescendingAndOutOfRangeButAllowsPlateaus() {
        val good = NearGlowCurve.DEFAULT.points
        for (points in listOf(good.reversed(), good.take(3),
            good.toMutableList().apply { this[1] = this[0] },
            good.toMutableList().apply { this[1] = NearGlowPoint(-75, 80) },
            good.toMutableList().apply { this[0] = NearGlowPoint(-128, 0) },
            good.toMutableList().apply { this[3] = NearGlowPoint(-40, 101) })) {
            assertThrows(IllegalArgumentException::class.java) { NearGlowCurve(points) }
        }
        assertNotNull(NearGlowCurve(good.map { it.copy(percent = 30) }))
        for (text in listOf("", "NaN", "1.5", "999999999999")) {
            val invalid = NearGlowDraft(NearGlowDraft().points.toMutableList().apply { this[0] = NearGlowPointInput(text, "0") })
            assertNull(invalid.curve); assertNotNull(invalid.error)
        }
    }

    private val reading = NearGlowReading(1000, true, 100, -60, 600, listOf(10, 3, 0), true, 0)
    @Test fun handsetLossIsDifferentFromPartnerLossAndAgingNeverRefreshesSource() {
        assertTrue(reading.handsetFresh(2000)); assertTrue(reading.sourceFresh(2000))
        assertTrue(reading.copy(peerFresh = false).handsetFresh(2000))
        assertFalse(reading.copy(peerFresh = false).sourceFresh(2000))
        assertFalse(reading.handsetFresh(3500)); assertFalse(reading.sourceFresh(3500))
        assertFalse(reading.copy(sourceAgeMs = 2500).sourceFresh(1500))
        assertFalse(reading.sourceFresh(999))
        // Receipt starts at the first fragment, including known assembly time.
        assertFalse(reading.copy(sourceAgeMs = 2000).sourceFresh(2500))
        assertTrue(reading.copy(sourceAgeMs = 0).sourceFresh(2500))
    }

    @Test fun disconnectDoesNotKeepReadinessOrTurnPendingIntoSuccess() {
        val station = NearGlowStationUi("A", connected = true, authenticated = true,
            reading = reading, revision = 7, confirmed = NearGlowCurve.DEFAULT, configMatchesTelemetry = true, change = NearGlowChange.SENT)
        val dropped = station.disconnected()
        assertEquals(NearGlowChange.UNCLEAR, dropped.change)
        assertNull(dropped.reading); assertNull(dropped.revision); assertNull(dropped.confirmed)
        assertFalse(dropped.authenticated)
        val state = NearGlowUiState(stations = listOf(station, dropped), now = 1200)
        assertFalse(state.canApply)
        assertTrue(state.copy(stations = listOf(station.copy(change = NearGlowChange.CONFIRMED),
            station.copy(change = NearGlowChange.DRAFT))).canApply)
        assertFalse(state.copy(now = 9000).canApply)
        val available = station.copy(change = NearGlowChange.CONFIRMED)
        for (blocked in listOf(available.copy(busy = true), available.copy(configMatchesTelemetry = false),
            available.copy(revision = 0xFFFFFFFFL), available.copy(reading = reading.copy(configFault = 5)))) {
            assertFalse(NearGlowUiState(stations = listOf(available, blocked), now = 1200).canApply)
        }
    }
}
