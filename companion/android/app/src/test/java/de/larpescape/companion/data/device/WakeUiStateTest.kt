package de.larpescape.companion.data.device

import de.larpescape.protocol.LedStatus
import de.larpescape.protocol.WakeStatus
import org.junit.Assert.*
import org.junit.Test

class WakeUiStateTest {
    private val box = "00112233445566778899aabbccddeeff"
    private fun status(sequence: Long, wake: WakeStatus? = WakeStatus(true, true, 5)) =
        LedStatus(0, 1, 0, sequence, 0, 5, 0, wake)

    @Test fun allMasksFollowStableInputIdsAndNeverInventAnUnknownValue() {
        for (mask in 0..15) {
            val tracker = WakeStateTracker(box)
            assertTrue(tracker.accept(status(1, WakeStatus(true, true, mask)), 100))
            val state = tracker.state(100, true)
            assertTrue(state.supported); assertTrue(state.current); assertEquals(box, state.boxId)
            for (input in 1..4) assertEquals(mask and (1 shl (input - 1)) != 0, state.active(input))
        }
        assertNull(WakeUiState().active(1))
        assertNull(WakeUiState(supported = true, activeMask = 15).active(1))
        assertNull(WakeUiState(supported = true, current = true, activeMask = null).active(1))
        assertNull(WakeUiState(supported = false, current = true, activeMask = 15).active(1))
    }

    @Test fun resultCannotExtendWakeFreshnessOrEraseTheLatestObservation() {
        val tracker = WakeStateTracker(box)
        tracker.accept(status(1), 100)
        assertFalse(tracker.accept(status(2, null), 900))
        assertEquals(5, tracker.state(1099, true).activeMask)
        val expired = tracker.state(1100, true)
        assertFalse(expired.current); assertNull(expired.activeMask)
        assertTrue(expired.message.contains("überholt"))
    }

    @Test fun lateStatusAfterNewerResultAndRepeatedStatusNeverRefreshWake() {
        val tracker = WakeStateTracker(box)
        tracker.accept(status(1), 10)
        assertFalse(tracker.accept(status(4, null), 500))
        assertFalse(tracker.accept(status(3, WakeStatus(true, true, 15)), 700))
        assertFalse(tracker.accept(status(1), 900))
        assertEquals(5, tracker.state(1009, true).activeMask)
        assertNull(tracker.state(1010, true).activeMask)
        assertTrue(tracker.accept(status(5, WakeStatus(true, true, 2)), 1100))
        assertEquals(2, tracker.state(1100, true).activeMask)
    }

    @Test fun unsupportedReadErrorStaleAndDisconnectedStayDistinct() {
        val tracker = WakeStateTracker(box)
        val waiting = tracker.state(0, true)
        assertEquals(box, waiting.boxId); assertNull(waiting.active(1))
        tracker.accept(status(1, WakeStatus(false, false, 0)), 0)
        val unsupported = tracker.state(0, true)
        assertFalse(unsupported.supported); assertFalse(unsupported.current)
        assertTrue(unsupported.message.contains("unterstützt")); assertNull(unsupported.active(1))
        tracker.accept(status(2, WakeStatus(true, false, 0)), 100)
        val unavailable = tracker.state(100, true)
        assertTrue(unavailable.supported); assertFalse(unavailable.current)
        assertTrue(unavailable.message.contains("Lesefehler")); assertNull(unavailable.active(1))
        val stale = tracker.state(1100, true)
        assertTrue(stale.message.contains("überholt")); assertNull(stale.active(1))
        val disconnected = tracker.state(1100, false)
        assertNull(disconnected.boxId); assertNull(disconnected.active(1))
        assertTrue(disconnected.message.contains("Getrennt"))
    }

    @Test fun invalidationAndClockRegressionNeverKeepAnActiveTile() {
        val tracker = WakeStateTracker(box)
        tracker.accept(status(1), 100)
        assertNull(tracker.state(99, true).activeMask)
        assertNull(tracker.state(100, false).activeMask)
        tracker.clear()
        assertNull(tracker.state(100, true).activeMask)
        assertTrue(tracker.accept(status(1), 200))
        val live = tracker.state(200, true)
        val disconnected = LedUiState(authenticated = true, wake = live).disconnected()
        assertFalse(disconnected.authenticated); assertNull(disconnected.wake.boxId)
        assertNull(disconnected.wake.activeMask)
    }
}