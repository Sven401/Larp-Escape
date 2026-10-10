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
)

class DiagnosticViewModel(
    private val store: DiagnosticRunStore,
    private val utc: () -> String = { Instant.now().truncatedTo(ChronoUnit.MILLIS).toString().let { if (it.length == 20) it.replace("Z", ".000Z") else it } },
    private val monotonic: () -> Long = { System.nanoTime() / 1_000_000 },
    private val autoTimers: Boolean = true,
    private val scheduler: ((Long, () -> Unit) -> Unit)? = null,
) : ViewModel() {
    private val mutable = MutableStateFlow(DiagnosticUiState())
    val state = mutable.asStateFlow()
    private var repository: FakeDiagnosticRepositoryV1? = null
    private var request: JsonObject? = null
    private var wireExchange: SyntheticDiagnosticStepV1? = null
    private var startMs = 0L
    private var submittedMs = 0L
    private var export: ByteArray? = null
    init {
        action {
            store.latest()?.let { repository = FakeDiagnosticRepositoryV1.recover(it, utc()); refresh(false) }
        }
        if (autoTimers) viewModelScope.launch {
            while (true) {
                delay(100)
                if (mutable.value.running && mutable.value.pending) action {
                    repository!!.tick(monotonic(), utc())
                    val current = repository!!.snapshot().getAsJsonArray("steps").last().asJsonObject
                    if (current["state"].asString != "pending") refresh(false) else refresh(true)
                }
                if (mutable.value.running && monotonic() - startMs >= 60000) stop("timeout")
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
        mutable.value = mutable.value.copy(document = doc, running = running, pending = pending, busy = false)
    }
    fun newRun() = action {
        if (mutable.value.running) stop("cancelled")
        startMs = monotonic()
        wireExchange?.retire()
        repository = FakeDiagnosticRepositoryV1.start(SyntheticDiagnosticV1.newRun("android-debug-simulation-v1", utc()), store.create(), SyntheticDiagnosticTransportV1 {
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
    fun skip(operation: String) = action { repository!!.skip(operation, utc()); refresh() }
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
        repository!!.observe(stepId, result, expected, actual, utc()); refresh()
    }
    fun stop(reason: String = "cancelled") = action {
        if (mutable.value.running) { repository!!.interrupt(reason, utc()); wireExchange?.retire(); refresh(false) }
    }
    fun beginExport(): Boolean {
        export = null
        action {
            if (mutable.value.pending) repository!!.interrupt("cancelled", utc())
            wireExchange?.retire()
            export = DiagnosticReportCodecV1.export(repository!!.finish(utc())); refresh(false)
            mutable.value = mutable.value.copy(notice = "Lauf abgeschlossen. Simulationsbericht für Export bereit.", finalized = true)
        }
        return export != null
    }
    fun takeExport(): ByteArray? = export?.copyOf().also { export = null }
    fun notice(text: String) { mutable.value = mutable.value.copy(notice = text) }
    fun backgroundEnded() { if (mutable.value.running) stop("disconnected") }
}
