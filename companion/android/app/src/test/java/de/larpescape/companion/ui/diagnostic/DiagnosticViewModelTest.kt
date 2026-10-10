package de.larpescape.companion.ui.diagnostic

import de.larpescape.companion.data.diagnostic.DiagnosticRunStore
import de.larpescape.protocol.*
import org.junit.Assert.*
import org.junit.Test

class DiagnosticViewModelTest {
    private class Store : DiagnosticRunStore {
        val journals = mutableListOf<DiagnosticJournalV1>()
        override fun latest() = journals.lastOrNull()
        override fun create() = MemoryDiagnosticJournalV1().also { journals.add(it) }
    }
    private var time = 0L
    private fun utc() = "2026-10-10T12:00:${(time / 1000).toString().padStart(2, '0')}.${(time % 1000).toString().padStart(3, '0')}Z"
    private fun model(store: Store = Store()) = DiagnosticViewModel(store, ::utc, { time }, false) { delay, block -> time += delay; block() }
    private fun last(model: DiagnosticViewModel) = model.state.value.document!!.getAsJsonArray("steps").last().asJsonObject

    @Test fun explicitTwoStepSimulationExportsSeparateEvidence() {
        val model = model(); assertNull(model.state.value.document)
        model.newRun(); model.begin("wake_capture")
        assertTrue(model.state.value.pending); assertEquals("unclear", last(model)["verdict"].asString)
        model.simulateReply(false); assertFalse(model.state.value.pending)
        assertEquals("unclear", last(model)["verdict"].asString)
        model.observe(1, "matches", "Synthetische Taste 1 aktiv und inaktiv")
        model.begin("led_single"); model.simulateReply(false)
        model.observe(2, "matches", "Synthetische LED 0 rot und aus")
        assertTrue(model.beginExport())
        val report = DiagnosticReportCodecV1.import(model.takeExport()!!)
        assertEquals("synthetic", report.evidence); assertEquals(listOf("passed", "passed"), report.verdicts)
    }
    @Test fun reopenPreservesPendingAsUnclearAndCannotResume() {
        val store = Store(); val first = model(store)
        first.newRun(); first.begin("led_single"); time += 1000
        val second = model(store)
        assertFalse(second.state.value.running); assertFalse(second.state.value.pending)
        assertEquals("app_restarted", last(second)["state"].asString)
        second.begin("led_single")
        assertTrue(second.state.value.notice!!.startsWith("Fehler:"))
        assertTrue(second.beginExport()); assertEquals(listOf("unclear"), DiagnosticReportCodecV1.import(second.takeExport()!!).verdicts)
    }
    @Test fun backgroundRetiresGrantButUserCanRecordWhatTheyObserved() {
        val model = model(); model.newRun(); model.begin("wake_capture")
        model.backgroundEnded(); model.observe(1, "matches", "Taste beobachtet, Antwort verloren")
        assertEquals("disconnected", last(model)["state"].asString)
        assertEquals("unclear", last(model)["verdict"].asString)
        assertTrue(model.beginExport())
    }
    @Test fun newRunPreservesOldJournalAndSkipDoesNotCreateReply() {
        val store = Store(); val model = model(store)
        model.newRun(); model.skip("led_single"); assertTrue(model.beginExport())
        val previous = store.journals.single().load()!!
        model.newRun()
        assertEquals(2, store.journals.size); assertArrayEquals(previous, store.journals[0].load())
        assertEquals(0, model.state.value.document!!.getAsJsonArray("steps").size())
    }
    @Test fun failedSimulatedDeviceResultNeverBecomesPassed() {
        val model = model(); model.newRun(); model.begin("led_single"); model.simulateReply(true)
        model.observe(1, "matches", "Beobachtung passt, Gerätefehler bleibt")
        assertEquals("failed", last(model)["verdict"].asString)
        assertTrue(model.beginExport()); assertEquals(listOf("failed"), DiagnosticReportCodecV1.import(model.takeExport()!!).verdicts)
    }
}
