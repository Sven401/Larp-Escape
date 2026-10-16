package de.larpescape.companion.data.device

import org.junit.Assert.*
import org.junit.Test

class MatrixCollectionTest {
    private class Store : MatrixCollectionStore {
        val runs = mutableMapOf<String, String>()
        val current = mutableMapOf<String, String>()
        var fail = false
        override fun load(device: String) = current[device]?.let { MatrixCollectionCodec.decode(runs.getValue(it)) }
        override fun save(value: MatrixCollection) {
            check(!fail)
            runs[value.run] = MatrixCollectionCodec.encode(value)
            current[value.device] = value.run
        }
    }
    private val device = "01".repeat(16)
    private fun live(vararg contacts: Int) = MatrixUiState(supported = true, current = true, pressed = contacts.toSet())
    private fun ready(store: Store = Store()) = MatrixCollectionRecorder(store).apply {
        selectDevice(device)
        assertTrue(start(live(), 100))
    }

    @Test fun all81ContactsLatchWithoutQuestionsAndCountsRequireRelease() {
        val r = ready()
        for (index in 0..80) {
            r.observe(live(index), 200L + index)
            repeat(4) { r.observe(live(index), 300L + index) }
            r.observe(live(), 400L + index)
        }
        assertEquals((0..80).toSet(), r.state.hits.keys)
        assertTrue(r.state.hits.values.all { it.count == 1 })
        r.observe(live(80), 1000)
        assertEquals(MatrixHit(2, 280, 1000), r.state.hits[80])
        r.finish(1100)
        r.observe(live(0), 1200)
        assertEquals(1, r.state.hits[0]?.count)
        assertTrue(r.state.disabled.isEmpty())
        r.confirmUnpopulated()
        assertEquals(MatrixRunPhase.CONFIRMED, r.state.phase)
        assertTrue(r.state.disabled.isEmpty())
    }

    @Test fun absenceOnlyBecomesDisabledAfterOneExplicitGlobalConfirmation() {
        val store = Store()
        val r = ready(store)
        r.observe(live(0), 200); r.observe(live(), 300); r.observe(live(80), 400)
        r.finish(500)
        assertEquals(79, r.state.unseen.size)
        assertTrue(r.state.disabled.isEmpty())
        r.confirmUnpopulated()
        assertEquals((1..79).toSet(), r.state.disabled)
        val restored = MatrixCollectionRecorder(store).apply { selectDevice(device) }
        assertEquals(r.state, restored.state)
        assertTrue(restored.start(live(), 600))
        restored.observe(live(1), 700) // Disabled coordinates remain detectable in a new run.
        assertEquals(setOf(1), restored.state.hits.keys)
        assertEquals(2, store.runs.size)
        restored.finish(800); restored.reopen(); assertTrue(restored.resume(live()))
        restored.observe(live(80), 900)
        restored.finish(1000); restored.confirmUnpopulated()
        assertFalse(1 in restored.state.disabled)
        assertFalse(80 in restored.state.disabled)
    }

    @Test fun emptyRunCannotDisableTheEntireMatrixAndOldLiveCannotStart() {
        val r = MatrixCollectionRecorder(Store())
        r.selectDevice(device)
        assertFalse(r.start(MatrixUiState(), 100))
        assertFalse(r.start(live(0), 100))
        assertFalse(r.start(live().copy(ambiguous = true), 100))
        assertTrue(r.start(live(), 100))
        r.finish(200); r.confirmUnpopulated()
        assertEquals(MatrixRunPhase.REVIEW, r.state.phase)
        assertTrue(r.state.disabled.isEmpty())
    }

    @Test fun ambiguityDirectChangesAndInvalidSamplesCannotCreateFalseHits() {
        val r = ready()
        r.observe(live(0, 1), 200)
        r.observe(live(80), 300)
        assertTrue(r.state.hits.isEmpty())
        r.observe(live(), 400); r.observe(live(80), 500)
        r.observe(live(1), 600)
        assertEquals(setOf(80), r.state.hits.keys)
        r.observe(live().copy(current = false), 700)
        assertEquals(MatrixRunPhase.PAUSED, r.state.phase)
        r.observe(live(2), 800)
        assertFalse(r.resume(live(2)))
        assertTrue(r.resume(live()))
        r.observe(live(2).copy(ambiguous = true), 900)
        assertEquals(setOf(80), r.state.hits.keys)
    }

    @Test fun restartAndDeviceSwitchKeepEvidenceWithoutAutomaticallyRecording() {
        val store = Store()
        val first = ready(store)
        first.observe(live(0), 200)
        val next = MatrixCollectionRecorder(store)
        next.selectDevice(device)
        assertEquals(MatrixRunPhase.PAUSED, next.state.phase)
        assertEquals(setOf(0), next.state.hits.keys)
        assertEquals(1, next.state.interruptions)
        next.selectDevice("02".repeat(16))
        assertTrue(next.state.hits.isEmpty())
        next.selectDevice(device)
        assertEquals(setOf(0), next.state.hits.keys)
    }

    @Test fun failedPersistenceStopsCollectionAndCannotSilentlyAcceptAHit() {
        val store = Store()
        val r = ready(store)
        store.fail = true
        r.observe(live(0), 200)
        assertEquals(MatrixRunPhase.PAUSED, r.state.phase)
        assertTrue(r.state.hits.isEmpty())
        r.finish(300)
        assertEquals(MatrixRunPhase.PAUSED, r.state.phase)
        store.fail = false
        assertTrue(r.resume(live()))
        r.observe(live(0), 400)
        assertEquals(1, r.state.hits[0]?.count)
    }

    @Test fun codecRoundtripAndReportIncludeEveryStableCoordinate() {
        val r = ready()
        r.observe(live(80), 200)
        assertEquals(r.state, MatrixCollectionCodec.decode(MatrixCollectionCodec.encode(r.state)))
        val report = MatrixCollectionCodec.report(r.state)
        assertTrue(report.contains("I9;1;200;200;true;false"))
        assertTrue(report.contains("A1;0;;;false;false"))
        assertEquals(81, report.lines().count { it.matches(Regex("[A-I][1-9];.*")) })
        try { MatrixCollectionCodec.decode("schema=1\ndevice=../../outside") ; fail() } catch (_: Exception) {}
    }

    @Test fun knownFaultsRemainOpenWhileOnlyUnpopulatedCoordinatesAreDisabled() {
        val store = Store()
        val r = ready(store)
        val problems = matrixProblemIndices("C3, A8, G3, H3")!!
        assertEquals(setOf(20, 63, 24, 25), problems)
        r.observe(live(0), 200); r.finish(300)
        r.confirmUnpopulated(problems)
        assertEquals(problems, r.state.problems)
        assertEquals(r.state.unseen - problems, r.state.disabled)
        assertTrue((r.state.disabled intersect problems).isEmpty())
        assertEquals(r.state, store.load(device))
        assertTrue(MatrixCollectionCodec.report(r.state).contains("C3;0;;;false;false;true"))
        r.reopen(); assertTrue(r.resume(live()))
        r.observe(live(20), 400); r.finish(500)
        r.confirmUnpopulated(problems - 20)
        assertFalse(20 in r.state.problems)
        assertFalse(20 in r.state.disabled)
        assertTrue(20 in r.state.hits)
    }

    @Test fun invalidProblemListsCannotSilentlyConfirmAnEmptyExceptionList() {
        assertNull(matrixProblemIndices("C3, Z4"))
        assertNull(matrixProblemIndices("C3,"))
        assertEquals(emptySet<Int>(), matrixProblemIndices(" "))
        val r = ready()
        r.observe(live(0), 200); r.finish(300)
        try { r.confirmUnpopulated(setOf(0)); fail() } catch (_: IllegalArgumentException) {}
        assertEquals(MatrixRunPhase.REVIEW, r.state.phase)
        assertTrue(r.state.disabled.isEmpty())
    }

    @Test fun existingRunFormatLoadsWithoutLosingAnyHits() {
        val r = ready()
        r.observe(live(80), 200); r.finish(300)
        val legacy = MatrixCollectionCodec.encode(r.state).lines().filterNot { it.startsWith("problems=") }.joinToString("\n")
        assertEquals(r.state, MatrixCollectionCodec.decode(legacy))
    }
}
