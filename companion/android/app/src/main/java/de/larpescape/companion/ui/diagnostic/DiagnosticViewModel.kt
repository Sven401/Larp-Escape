package de.larpescape.companion.ui.diagnostic

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.google.gson.JsonObject
import de.larpescape.companion.data.diagnostic.DiagnosticRunStore
import de.larpescape.protocol.*
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.time.Instant
import java.time.temporal.ChronoUnit

data class DiagnosticUiState(
    val document: JsonObject? = null,
    val running: Boolean = false,
    val pending: Boolean = false,
    val busy: Boolean = false,
    val notice: String? = null,
    val blocked: Boolean = false,
    val finalized: Boolean = false,
    val legacyLocked: Boolean = false,
    val planIndex: Int = 0,
    val stepRemainingMs: Long = 0,
    val runRemainingMs: Long = 0,
    val drafts: Map<Long, String> = emptyMap(),
    val editingObservation: Long? = null,
)

val pilotOperations = listOf("wake_capture", "led_single")
val pilotTitles = listOf("Wake 1 · Eingang", "LED 0 · Einzelausgabe")

fun DiagnosticUiState.planStep(index: Int = planIndex): JsonObject? = document?.getAsJsonArray("steps")
    ?.lastOrNull { it.asJsonObject.getAsJsonObject("request")["operation"].asString == pilotOperations.getOrNull(index) }?.asJsonObject
fun DiagnosticUiState.canAdvance(): Boolean = planStep()?.let {
    it["state"].asString != "pending" && (it["state"].asString == "not_tested" || !it.getAsJsonObject("observation")["recordedUtc"].isJsonNull)
} == true && !pending && !busy && !hasDraft() && editingObservation == null
fun DiagnosticUiState.hasDraft(): Boolean = editingObservation != null || drafts.any { (id, text) ->
    document?.getAsJsonArray("steps")?.any { it.asJsonObject.getAsJsonObject("request")["stepId"].asLong == id &&
        it.asJsonObject.getAsJsonObject("observation")["actual"].asString != text } == true
}

class DiagnosticViewModel(
    private val store: DiagnosticRunStore,
    private val utc: () -> String = { Instant.now().truncatedTo(ChronoUnit.MILLIS).toString().let { if (it.length == 20) it.replace("Z", ".000Z") else it } },
    private val monotonic: () -> Long = { System.nanoTime() / 1_000_000 },
    private val autoTimers: Boolean = true,
    private val controlTransport: (String) -> SyntheticDiagnosticControlTransportV1 = { grant ->
        SyntheticDiagnosticControlTransportV1 { request -> SyntheticDiagnosticControlPeerV1.reply(request, grant) }
    },
    private val scheduler: ((Long, () -> Unit) -> Unit)? = null,
) : ViewModel() {
    private val mutable = MutableStateFlow(DiagnosticUiState())
    val state = mutable.asStateFlow()
    private var repository: FakeDiagnosticRepositoryV1? = null
    private var request: JsonObject? = null
    private var wireExchange: SyntheticDiagnosticStepV1? = null
    private var control: SyntheticDiagnosticControlSessionV1? = null
    private var startMs = 0L
    private var submittedMs = 0L
    private var export: ByteArray? = null
    init {
        action {
            store.latest()?.let {
                repository = FakeDiagnosticRepositoryV1.recover(it, utc()); refresh(false)
                val steps = mutable.value.document!!.getAsJsonArray("steps")
                val index = store.loadPosition(mutable.value.document!!["runId"].asString) ?: pilotOperations.indexOf(steps.lastOrNull()?.asJsonObject?.getAsJsonObject("request")?.get("operation")?.asString).coerceAtLeast(0)
                val finalized = repository!!.isFinalized
                val legacyLocked = repository!!.isLegacyLocked
                mutable.value = mutable.value.copy(planIndex = index, finalized = finalized, legacyLocked = legacyLocked,
                    notice = if (legacyLocked) "Älteres Journal ohne Abschlussmarker: Status nicht rekonstruierbar. Der vorhandene Stand ist gesperrt; kein Auftrag und keine Beobachtung werden ergänzt."
                    else if (finalized) "Abgeschlossener Bericht wieder geöffnet. Ergebnisse bleiben unverändert; kein Auftrag wird wiederholt."
                    else "Gespeicherter Lauf: Ergebnisse bleiben erhalten. Es wird kein Auftrag wiederholt.")
            }
        }
        if (autoTimers) viewModelScope.launch {
            while (true) {
                delay(100)
                tick()
            }
        }
    }
    private fun action(block: () -> Unit) {
        try { block() } catch (error: Exception) { mutable.value = mutable.value.copy(notice = "Fehler: ${error.message}", busy = false) }
    }
    private fun refresh(running: Boolean = mutable.value.running) {
        if (!running) wireExchange?.retire()
        val doc = repository?.snapshot()
        val pending = doc?.getAsJsonArray("steps")?.lastOrNull()?.asJsonObject?.get("state")?.asString == "pending"
        mutable.value = mutable.value.copy(document = doc, running = running, pending = pending, busy = false,
            stepRemainingMs = if (running && pending) (10000 - (monotonic() - submittedMs)).coerceAtLeast(0) else 0,
            runRemainingMs = if (running) (60000 - (monotonic() - startMs)).coerceAtLeast(0) else 0)
    }
    fun tick() = action {
        if (mutable.value.running) {
            val wasPending = mutable.value.pending
            repository!!.tick(monotonic(), utc())
            val timedOut = wasPending && repository!!.snapshot().getAsJsonArray("steps").last().asJsonObject["state"].asString == "timeout"
            val busy = mutable.value.busy
            refresh(!timedOut)
            if (mutable.value.pending) mutable.value = mutable.value.copy(busy = busy)
            if (mutable.value.running && monotonic() - startMs >= 60000) stop("timeout")
        }
    }
    fun newRun() = action {
        require(!mutable.value.hasDraft()) { "Ungespeicherte Beobachtung zuerst speichern oder verwerfen." }
        if (mutable.value.running) stop("cancelled")
        startMs = monotonic()
        wireExchange?.retire()
        val metadata = SyntheticDiagnosticV1.newRun("android-debug-simulation-v1", utc())
        val context = metadata.snapshot()
        val candidate = SyntheticDiagnosticControlSessionV1(context["deviceId"].asString, context["bootId"].asString,
            context["runId"].asString, context.getAsJsonObject("grant")["grantId"].asString,
            controlTransport(context.getAsJsonObject("grant")["grantId"].asString))
        require(candidate.start(startMs)) { "Synthetische Startantwort fehlt oder passt nicht. Kein Diagnoseauftrag wurde freigegeben." }
        control = candidate
        repository = FakeDiagnosticRepositoryV1.start(metadata, store.create(), SyntheticDiagnosticTransportV1 {
            request = it
            // Repository has atomically journaled pending before invoking this synthetic transport.
            wireExchange = SyntheticDiagnosticStepV1(it, submittedMs)
        }, 1)
        export = null; request = null; wireExchange = null
        mutable.value = DiagnosticUiState(notice = "Simulation gestartet. Keine Gerätefreigabe.")
        refresh(true)
    }
    fun begin(operation: String) = action {
        require(mutable.value.running && !mutable.value.pending && !mutable.value.busy)
        submittedMs = monotonic()
        require(submittedMs - startMs + 10000 <= 60000) { "Simulationsfreigabe zu kurz für einen weiteren Schritt. Neuen Lauf starten." }
        repository!!.begin(operation, utc(), submittedMs, 10000)
        refresh()
    }
    fun skip(operation: String) = action { require(mutable.value.running && !mutable.value.pending && !mutable.value.busy); repository!!.skip(operation, utc()); refresh() }
    fun startCurrent() = action {
        require(mutable.value.planIndex in pilotOperations.indices && mutable.value.planStep() == null)
        require(!mutable.value.hasDraft())
        begin(pilotOperations[mutable.value.planIndex])
    }
    fun skipCurrent() = action {
        require(mutable.value.planIndex in pilotOperations.indices && mutable.value.planStep() == null)
        require(!mutable.value.hasDraft())
        skip(pilotOperations[mutable.value.planIndex])
    }
    fun nextStep() = action {
        require(mutable.value.canAdvance()) { "Ergebnis abwarten und Beobachtung speichern oder ausdrücklich als nicht beobachtet festhalten." }
        val index = (mutable.value.planIndex + 1).coerceAtMost(pilotOperations.size)
        store.savePosition(mutable.value.document!!["runId"].asString, index)
        mutable.value = mutable.value.copy(planIndex = index)
    }
    fun reviewStep(index: Int) = action {
        require(index in pilotOperations.indices && !mutable.value.pending && !mutable.value.hasDraft())
        require(mutable.value.planStep(index) != null) { "Dieser Schritt wurde noch nicht begonnen." }
        store.savePosition(mutable.value.document!!["runId"].asString, index)
        mutable.value = mutable.value.copy(planIndex = index)
    }
    fun draft(stepId: Long, text: String) {
        if (text.length <= 256) mutable.value = mutable.value.copy(drafts = mutable.value.drafts + (stepId to text))
    }
    fun editObservation(stepId: Long) { mutable.value = mutable.value.copy(editingObservation = stepId) }
    fun discardDraft(stepId: Long) { mutable.value = mutable.value.copy(drafts = mutable.value.drafts - stepId, editingObservation = null) }
    fun simulateReply(failed: Boolean) = action {
        require(mutable.value.running && mutable.value.pending && !mutable.value.busy)
        checkNotNull(request)
        val savedRepository = checkNotNull(repository); val exchange = checkNotNull(wireExchange); val submitted = submittedMs
        val deviceStart = submitted - startMs
        mutable.value = mutable.value.copy(busy = true)
        val deliver = {
            action {
                // A delayed callback cannot enter a new run, completed step or retired repository.
                if (repository === savedRepository && wireExchange === exchange && mutable.value.running && mutable.value.pending) {
                    val now = monotonic()
                    savedRepository.tick(now, utc())
                    val pending = savedRepository.snapshot().getAsJsonArray("steps").last().asJsonObject["state"].asString == "pending"
                    if (pending) savedRepository.receive(exchange.respond(deviceStart, failed, now), 1, utc(), now)
                    else exchange.retire()
                    refresh(pending && mutable.value.running)
                }
            }
        }
        if (scheduler != null) scheduler.invoke(500, deliver) else viewModelScope.launch { delay(500); deliver() }
    }
    fun observe(stepId: Long, result: String, actual: String) = action {
        val step = repository!!.snapshot().getAsJsonArray("steps").single { it.asJsonObject.getAsJsonObject("request")["stepId"].asLong == stepId }.asJsonObject
        val expected = if (step.getAsJsonObject("request")["operation"].asString == "wake_capture") "Einmal Taste 1 drücken und loslassen, keine Nebenauslösung" else "Nur LED 0 rot, danach aus"
        require(step["state"].asString != "pending" && step["state"].asString != "not_tested") { "Endergebnis zuerst abwarten." }
        repository!!.observe(stepId, result, expected, actual, utc()); refresh()
        discardDraft(stepId)
    }
    fun stop(reason: String = "cancelled") = action {
        if (mutable.value.running) {
            repository!!.interrupt(reason, utc()); wireExchange?.retire(); refresh(false)
            runCatching { control?.end(monotonic()) }
            mutable.value = mutable.value.copy(notice = if (reason == "timeout") "Simulationsfrist abgelaufen. Ergebnisse und Beobachtungen bleiben gespeichert; keine weiteren Aufträge." else "Lauf beendet. Keine weiteren Aufträge; offene Ausgänge bleiben unklar.")
        }
    }
    fun beginExport(): Boolean {
        export = null
        action {
            require(!mutable.value.hasDraft()) { "Ungespeicherte Beobachtung zuerst speichern oder verwerfen." }
            if (mutable.value.pending) repository!!.interrupt("cancelled", utc())
            wireExchange?.retire()
            runCatching { control?.end(monotonic()) }
            export = DiagnosticReportCodecV1.export(repository!!.finish(utc())); refresh(false)
            mutable.value = mutable.value.copy(notice = "Lauf abgeschlossen. Simulationsbericht für Export bereit.", finalized = true)
        }
        return export != null
    }
    fun takeExport(): ByteArray? = export?.copyOf().also { export = null }
    fun notice(text: String) { mutable.value = mutable.value.copy(notice = text) }
    fun backgroundEnded() { if (mutable.value.running) stop("disconnected") }
}
