package de.larpescape.companion.ui.diagnostic

import de.larpescape.companion.data.diagnostic.DiagnosticRunStore
import de.larpescape.protocol.*
import org.junit.Assert.*
import org.junit.Test

class DiagnosticViewModelTest {
    private class Store : DiagnosticRunStore {
        val journals = mutableListOf<DiagnosticJournalV1>()
        val positions = mutableMapOf<String, Int>()
        var rejectPosition = false
        override fun latest() = journals.lastOrNull()
        override fun create() = MemoryDiagnosticJournalV1().also { journals.add(it) }
        override fun loadPosition(runId: String) = positions[runId]
        override fun savePosition(runId: String, index: Int) {
            check(!rejectPosition) { "Positionsspeicher nicht verfügbar" }
            positions[runId] = index
        }
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
    @Test fun delayedFakeCallbackCannotEnterNewRunOrRetiredStep() {
        val callbacks = mutableListOf<() -> Unit>()
        val model = DiagnosticViewModel(Store(), ::utc, { time }, false) { _, block -> callbacks.add(block) }
        model.newRun(); model.begin("wake_capture"); model.simulateReply(false)
        model.stop(); model.newRun(); model.begin("led_single")
        time += 500; callbacks.single().invoke()
        assertEquals("pending", last(model)["state"].asString); assertTrue(last(model)["deviceReply"].isJsonNull)
        assertEquals(0, last(model).getAsJsonArray("ignoredReplies").size())
        model.simulateReply(false); time += 500; callbacks.last().invoke()
        assertEquals("finished", last(model)["state"].asString)
    }
    @Test fun delayedFakeCallbackAtAbsoluteDeadlineLeavesUnclearReport() {
        var deliver: (() -> Unit)? = null
        val model = DiagnosticViewModel(Store(), ::utc, { time }, false) { _, block -> deliver = block }
        model.newRun(); model.begin("led_single"); model.simulateReply(false)
        time = 10000; deliver!!.invoke()
        assertEquals("timeout", last(model)["state"].asString); assertTrue(last(model)["deviceReply"].isJsonNull)
        assertFalse(model.state.value.running); assertFalse(model.state.value.busy)
        assertTrue(model.beginExport()); assertEquals(listOf("unclear"), DiagnosticReportCodecV1.import(model.takeExport()!!).verdicts)
    }
    @Test fun guidedPlanRequiresStartResultAndExplicitObservationBeforeNext() {
        val model = model(); model.newRun()
        assertEquals(0, model.state.value.planIndex)
        assertNull(model.state.value.planStep())
        model.nextStep(); assertEquals(0, model.state.value.planIndex)
        model.startCurrent(); assertEquals("wake_capture", last(model).getAsJsonObject("request")["operation"].asString)
        model.startCurrent(); assertEquals(1, model.state.value.document!!.getAsJsonArray("steps").size())
        model.nextStep(); assertEquals(0, model.state.value.planIndex)
        model.observe(1, "matches", "Vorzeitig"); assertTrue(last(model).getAsJsonObject("observation")["recordedUtc"].isJsonNull)
        model.simulateReply(false); assertFalse(model.state.value.canAdvance())
        model.observe(1, "not_observed", ""); assertTrue(model.state.value.canAdvance())
        model.nextStep(); assertEquals(1, model.state.value.planIndex); assertNull(model.state.value.planStep())
        model.startCurrent(); model.simulateReply(false); model.observe(2, "matches", "Rot und aus simuliert")
        model.nextStep(); assertEquals(2, model.state.value.planIndex)
        assertTrue(model.beginExport())
        assertEquals(listOf("unclear", "passed"), DiagnosticReportCodecV1.import(model.takeExport()!!).verdicts)
    }
    @Test fun guidedSkipIsExplicitNotTestedAndReviewCannotRepeatEitherStep() {
        val model = model(); model.newRun(); model.skipCurrent()
        assertEquals("not_tested", last(model)["verdict"].asString); assertTrue(last(model)["deviceReply"].isJsonNull)
        model.nextStep(); model.skipCurrent(); model.nextStep()
        assertEquals(2, model.state.value.planIndex)
        model.reviewStep(0); model.startCurrent(); model.skipCurrent()
        assertEquals(2, model.state.value.document!!.getAsJsonArray("steps").size())
        model.nextStep(); model.nextStep(); assertTrue(model.beginExport())
        assertEquals(listOf("not_tested", "not_tested"), DiagnosticReportCodecV1.import(model.takeExport()!!).verdicts)
    }
    @Test fun unsavedObservationBlocksNextReviewExportAndReplacementRun() {
        val store = Store(); val model = model(store); model.newRun(); model.startCurrent(); model.simulateReply(false)
        model.observe(1, "matches", "Gespeichert")
        model.draft(1, "Noch nicht gespeichert"); assertTrue(model.state.value.hasDraft())
        model.nextStep(); model.reviewStep(0); assertEquals(0, model.state.value.planIndex)
        assertFalse(model.beginExport()); model.newRun(); assertEquals(1, store.journals.size)
        model.observe(1, "mismatch", model.state.value.drafts[1]!!)
        assertFalse(model.state.value.hasDraft()); assertEquals("failed", last(model)["verdict"].asString)
        model.draft(1, "Verwerfen"); model.discardDraft(1); assertTrue(model.state.value.canAdvance())
    }
    @Test fun savedNavigationReopensAtNextStepWithoutResendingAndKeepsCompletion() {
        val store = Store(); val first = model(store); first.newRun(); first.skipCurrent(); first.nextStep()
        val second = model(store)
        assertEquals(1, second.state.value.planIndex); assertFalse(second.state.value.running)
        second.startCurrent(); assertEquals(1, second.state.value.document!!.getAsJsonArray("steps").size())
        first.skipCurrent(); first.nextStep()
        val third = model(store); assertEquals(2, third.state.value.planIndex)
        assertTrue(third.beginExport()); assertEquals(listOf("not_tested", "not_tested"), DiagnosticReportCodecV1.import(third.takeExport()!!).verdicts)
    }
    @Test fun exportedPilotReopensReadOnlyAndProducesSameReport() {
        val store = Store(); val first = model(store)
        first.newRun(); first.skipCurrent(); first.nextStep(); first.skipCurrent(); first.nextStep()
        assertTrue(first.beginExport())
        val firstExport = first.takeExport()!!
        time += 1000
        val reopened = model(store)
        assertTrue(reopened.state.value.finalized)
        reopened.observe(1, "matches", "Changed after export")
        assertTrue(reopened.state.value.notice!!.startsWith("Fehler:"))
        assertTrue(reopened.beginExport())
        assertArrayEquals(firstExport, reopened.takeExport())
    }
    @Test fun navigationStorageFailureDoesNotClaimProgress() {
        val store = Store(); val model = model(store); model.newRun(); model.skipCurrent()
        store.rejectPosition = true; model.nextStep()
        assertEquals(0, model.state.value.planIndex); assertTrue(model.state.value.notice!!.contains("Positionsspeicher"))
    }
    @Test fun visibleCountdownExpiresAtTenSecondsAndKeepsUnknownOutcome() {
        val model = model(); model.newRun(); model.startCurrent()
        assertEquals(10000L, model.state.value.stepRemainingMs)
        time = 9999; model.tick(); assertEquals(1L, model.state.value.stepRemainingMs); assertTrue(model.state.value.pending)
        time = 10000; model.tick(); assertFalse(model.state.value.pending); assertFalse(model.state.value.running)
        assertEquals("timeout", last(model)["state"].asString); assertEquals("unclear", last(model)["verdict"].asString)
        model.observe(1, "not_observed", ""); model.nextStep(); model.startCurrent()
        assertNull(model.state.value.planStep()); assertEquals(1, model.state.value.document!!.getAsJsonArray("steps").size())
    }
}
