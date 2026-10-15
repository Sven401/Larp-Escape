package de.larpescape.companion.data.device

import de.larpescape.protocol.AudioStatus
import de.larpescape.protocol.LedStatus
import org.junit.Assert.*
import org.junit.Test

class AudioUiStateTest {
    private fun status(sequence: Long, audio: AudioStatus? = AudioStatus(7, 42, 5, 29)) =
        LedStatus(0, 1, 0, sequence, 0, 5, 0, audio = audio)
    private fun state(tracker: AudioStateTracker, now: Long) = tracker.state(now, true, true, false, "bereit")

    @Test fun ledResultsDoNotRefreshAudioAndExpiryClearsAllObservations() {
        val tracker = AudioStateTracker()
        assertTrue(tracker.accept(status(1), 100))
        assertFalse(tracker.accept(status(3, null), 900))
        val fresh = state(tracker, 1099)
        assertTrue(fresh.current); assertTrue(fresh.canStart); assertEquals(true, fresh.busyLow)
        assertEquals(42, fresh.observedTrack); assertEquals(true, fresh.active)
        val stale = state(tracker, 1100)
        assertFalse(stale.canStart); assertFalse(stale.canStop); assertNull(stale.active)
        assertNull(stale.busyLow); assertNull(stale.observedTrack); assertNull(stale.observedVolume)
        assertTrue(stale.message.contains("überholt")); assertNull(tracker.observation(1100))
    }

    @Test fun olderAudioResultCanFinishTransportButCannotRefreshNewerObservation() {
        val tracker = AudioStateTracker()
        tracker.accept(status(4, AudioStatus(8, 1, 5, 9)), 100)
        assertFalse(tracker.accept(status(3), 500))
        assertFalse(state(tracker, 1099).busyLow!!)
        assertNull(state(tracker, 1100).busyLow)
        assertTrue(tracker.accept(status(5), 1200))
        assertTrue(state(tracker, 1200).busyLow!!)
        assertFalse(tracker.accept(status(5), 1800))
        assertNull(state(tracker, 2200).busyLow)
    }

    @Test fun unsupportedUnknownBusyFaultAndDisconnectNeverInventSound() {
        val tracker = AudioStateTracker()
        assertFalse(state(tracker, 0).canStart)
        tracker.accept(status(1, AudioStatus(supported = false)), 0)
        assertTrue(state(tracker, 0).message.contains("unterstützt"))
        assertFalse(state(tracker, 0).supported)
        tracker.accept(status(2, AudioStatus(flags = 1)), 10)
        assertNull(state(tracker, 10).busyLow); assertTrue(state(tracker, 10).canStart)
        tracker.accept(status(3, AudioStatus(flags = 32)), 20)
        assertFalse(state(tracker, 20).canStart); assertFalse(state(tracker, 20).canStop)
        tracker.accept(status(4), 30)
        val disconnected = tracker.state(30, false, true, false, "ready")
        assertFalse(disconnected.authenticated); assertNull(disconnected.busyLow); assertNull(disconnected.active)
        assertNull(state(tracker, 29).busyLow)
        tracker.clear(); assertFalse(state(tracker, 30).current)
        assertEquals("Audio-Befehl über UART übertragen · Höreindruck offen", audioResultMessage(0))
    }

    @Test fun invalidSelectionDisablesStartButLeavesStopAndEditsNeverChangeObservations() {
        val tracker = AudioStateTracker(); tracker.accept(status(1), 0)
        val live = state(tracker, 0)
        assertEquals("100", live.trackText); assertEquals("5", live.volumeText)
        for (invalid in listOf("", "0", "256", "-1", "abc")) {
            val edited = live.copy(trackText = invalid)
            assertFalse(edited.canStart); assertTrue(edited.canStop); assertEquals(42, edited.observedTrack)
        }
        for (invalid in listOf("", "0", "11", "-1")) assertFalse(live.copy(volumeText = invalid).canStart)
        val edited = live.copy(trackText = "255", volumeText = "10")
        assertTrue(edited.canStart); assertTrue(edited.canStop)
        val disconnected = LedUiState(audio = edited).disconnected().audio
        assertEquals("255", disconnected.trackText); assertEquals("10", disconnected.volumeText)
        assertNull(disconnected.active); assertNull(disconnected.busyLow); assertFalse(disconnected.canStart)
        assertFalse(live.copy(modeAllowed = false).canStart)
        assertFalse(live.copy(busy = true).canStart)
        assertFalse(live.copy(authenticated = false).canStart)
    }
}
