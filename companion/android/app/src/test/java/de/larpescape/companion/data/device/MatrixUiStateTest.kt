package de.larpescape.companion.data.device

import de.larpescape.protocol.LedFrameAssembler
import de.larpescape.protocol.LedFrames
import de.larpescape.protocol.LedStatus
import de.larpescape.protocol.MatrixStatus
import de.larpescape.protocol.WakeStatus
import org.junit.Assert.*
import org.junit.Test

class MatrixUiStateTest {
    private fun status(sequence: Long, matrix: MatrixStatus? = MatrixStatus(true, true, false, setOf(0, 80)),
                       wake: WakeStatus? = WakeStatus(true, true, 0)) =
        LedStatus(0, 1, 0, sequence, 0, 5, 0, wake, matrix)

    private fun result(sequence: Long) = status(sequence, matrix = null, wake = null)

    @Test fun all81ContactsUseStableCoordinatesIncludingTheLastBit() {
        val tracker = MatrixStateTracker()
        for (index in 0..80) {
            assertTrue(tracker.accept(status(index + 1L,
                MatrixStatus(true, true, false, setOf(index))), index.toLong()))
            val state = tracker.state(index.toLong(), true)
            assertTrue(state.supported); assertTrue(state.current)
            for (contact in 0..80) assertEquals(contact == index, state.active(contact))
            assertEquals(listOf("${'A' + index % 9}${index / 9 + 1}"), state.observedCoordinates)
        }
        assertEquals("A1", matrixCoordinate(0))
        assertEquals("I1", matrixCoordinate(8))
        assertEquals("A2", matrixCoordinate(9))
        assertEquals("I9", matrixCoordinate(80))
        assertEquals(listOf("I9"), tracker.state(80, true).observedCoordinates)
    }

    @Test fun unknownNeverBecomesAnInactiveOrActiveContact() {
        val states = listOf(MatrixUiState(), MatrixUiState(supported = true, pressed = setOf(80)),
            MatrixUiState(supported = true, current = true),
            MatrixUiState(supported = false, current = true, pressed = setOf(80)))
        states.forEach { state ->
            for (index in 0..80) assertNull(state.active(index))
            assertTrue(state.observedCoordinates.isEmpty())
        }
    }

    @Test fun resultDoesNotRefreshOrEraseTheLatestObservation() {
        val tracker = MatrixStateTracker()
        tracker.accept(status(1), 100)
        assertFalse(tracker.accept(result(2), 900))
        assertEquals(setOf(0, 80), tracker.state(1099, true).pressed)
        val stale = tracker.state(1100, true)
        assertFalse(stale.current); assertNull(stale.pressed)
        assertNull(stale.active(80)); assertTrue(stale.message.contains("überholt"))
    }

    @Test fun duplicateOlderAndSupersededSamplesCannotRefreshOrOverwriteMatrix() {
        val tracker = MatrixStateTracker()
        tracker.accept(status(1), 10)
        assertFalse(tracker.accept(result(4), 500))
        assertFalse(tracker.accept(status(3, MatrixStatus(true, true, false, setOf(5))), 700))
        assertFalse(tracker.accept(status(1), 900))
        assertEquals(setOf(0, 80), tracker.state(1009, true).pressed)
        assertNull(tracker.state(1010, true).pressed)
        assertTrue(tracker.accept(status(5, MatrixStatus(true, true, false, setOf(5))), 1100))
        assertEquals(setOf(5), tracker.state(1100, true).pressed)
    }

    @Test fun partialNotificationsCannotKeepThePreviousMatrixLive() {
        val tracker = MatrixStateTracker()
        val assembler = LedFrameAssembler()
        tracker.accept(status(1), 100)
        val frames = LedFrames.encode(ByteArray(82), 1)
        frames.dropLast(1).forEachIndexed { index, frame ->
            val complete = assembler.accept(frame, 900 + index.toLong())
            assertNull(complete)
            // The session delivers samples to the tracker only after full frame assembly.
            if (complete != null) tracker.accept(status(2), 900 + index.toLong())
        }
        assertNull(tracker.state(1100, true).pressed)
        assertNotNull(assembler.accept(frames.last(), 1101))
        assertTrue(tracker.accept(status(2), 1101))
        assertEquals(setOf(0, 80), tracker.state(1101, true).pressed)
    }

    @Test fun unsupportedFirmwareAndReadErrorsClearContactsWhileAmbiguityPreservesRawObservation() {
        val tracker = MatrixStateTracker()
        assertTrue(tracker.state(0, true).message.contains("Warte"))
        tracker.accept(status(1), 0)
        tracker.accept(status(2, null), 100)
        val oldFirmware = tracker.state(100, true)
        assertFalse(oldFirmware.supported); assertFalse(oldFirmware.current)
        assertNull(oldFirmware.active(80)); assertTrue(oldFirmware.message.contains("keine Matrixdaten"))
        tracker.accept(status(3, MatrixStatus(false, false, false, emptySet())), 200)
        assertFalse(tracker.state(200, true).supported)
        tracker.accept(status(4, MatrixStatus(true, false, false, emptySet())), 300)
        val failed = tracker.state(300, true)
        assertTrue(failed.supported); assertFalse(failed.current); assertNull(failed.pressed)
        assertTrue(failed.message.contains("Lesefehler"))
        tracker.accept(status(5, MatrixStatus(true, true, true, setOf(0, 1, 9, 10))), 400)
        val ambiguous = tracker.state(400, true)
        assertTrue(ambiguous.current); assertTrue(ambiguous.ambiguous)
        assertEquals("Mehrfachkontakt – Zuordnung unklar", ambiguous.message)
        assertEquals(true, ambiguous.active(0)); assertEquals(false, ambiguous.active(80))
        assertEquals(setOf(0, 1, 9, 10), ambiguous.pressed)
        assertEquals(listOf("A1", "B1", "A2", "B2"), ambiguous.observedCoordinates)
        assertNull(tracker.state(1400, true).pressed)
        tracker.accept(status(6, MatrixStatus(true, true, false, emptySet())), 500)
        assertFalse(tracker.state(500, true).ambiguous)
        assertEquals(false, tracker.state(500, true).active(80))
    }

    @Test fun disconnectClearAndClockRegressionInvalidateEvenTheLastContact() {
        val tracker = MatrixStateTracker()
        tracker.accept(status(1), 100)
        assertNull(tracker.state(99, true).pressed)
        assertNull(tracker.state(100, false).pressed)
        assertTrue(tracker.state(100, false).message.contains("Getrennt"))
        val disconnected = LedUiState(authenticated = true, matrix = tracker.state(100, true)).disconnected()
        assertFalse(disconnected.authenticated); assertNull(disconnected.matrix.pressed)
        assertNull(disconnected.matrix.active(80))
        tracker.clear()
        assertNull(tracker.state(100, true).pressed)
        assertTrue(tracker.accept(status(1), 200))
    }

    @Test fun acceptedContactSetsDoNotChangeWhenTheCallerMutatesItsCopy() {
        val tracker = MatrixStateTracker()
        val contacts = mutableSetOf(0, 80)
        tracker.accept(status(1, MatrixStatus(true, true, false, contacts)), 0)
        contacts.clear()
        assertEquals(setOf(0, 80), tracker.state(0, true).pressed)
        assertEquals(listOf("A1", "I9"), tracker.state(0, true).observedCoordinates)
    }
}
