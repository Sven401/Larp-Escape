package de.larpescape.protocol

import com.google.gson.*
import java.time.Instant

/** The future Android adapter must atomically replace bytes or throw without changing them. */
interface DiagnosticJournalV1 {
    fun load(): ByteArray?
    fun replace(bytes: ByteArray)
}

/** Shared across repository instances in host tests. No claim of process durable Android IO. */
class MemoryDiagnosticJournalV1 : DiagnosticJournalV1 {
    private var bytes: ByteArray? = null
    override fun load(): ByteArray? = bytes?.copyOf()
    override fun replace(bytes: ByteArray) { this.bytes = bytes.copyOf() }
}

/** All replies from this transport are synthetic; write success cannot become device evidence. */
fun interface SyntheticDiagnosticTransportV1 {
    fun send(request: JsonObject)
}

/** Explicit actions only. A saved pending intent is retired on reopen and is never replayed. */
class FakeDiagnosticRepositoryV1 private constructor(
    private var document: JsonObject,
    private val journal: DiagnosticJournalV1,
    private val transport: SyntheticDiagnosticTransportV1,
    private val originalBinding: Long,
    private var deadline: Long? = null,
    private var closed: Boolean = false,
    private var finalized: Boolean = false,
    val isLegacyLocked: Boolean = false,
) {
    val isFinalized: Boolean get() = finalized
    fun snapshot(): JsonObject = document.deepCopy()
    private fun mutate(action: (JsonObject) -> Unit) {
        require(!finalized)
        val next = document.deepCopy()
        action(next)
        DiagnosticSchemaV1.validate(next)
        journal.replace(journalBytes(next, false)) // Commit intent before exposing or sending it.
        document = next
    }
    private fun pending(report: JsonObject = document): JsonObject? = report.getAsJsonArray("steps").lastOrNull()?.asJsonObject?.takeIf { it.text("state") == "pending" }
    fun begin(operation: String, utc: String, nowMs: Long, timeoutMs: Int = 2000): JsonObject = append(operation, utc, nowMs, timeoutMs, true)
    fun skip(operation: String, utc: String): JsonObject = append(operation, utc, 0, 2000, false)
    private fun append(operation: String, utc: String, nowMs: Long, timeoutMs: Int, tested: Boolean): JsonObject {
        require(!closed && pending() == null)
        require(Instant.parse(utc) >= Instant.parse(document.text("startedUtc")))
        require(nowMs >= 0 && timeoutMs in 1000..10000 && nowMs <= Long.MAX_VALUE - timeoutMs)
        val nextId = (document.getAsJsonArray("steps").lastOrNull()?.asJsonObject?.getAsJsonObject("request")?.number("stepId") ?: 0) + 1
        val request = JsonObject().apply {
            addProperty("schemaVersion", 1); addProperty("kind", "diagnostic_request")
            listOf("runId", "deviceId", "bootId").forEach { add(it, document[it].deepCopy()) }
            add("grantId", document.getAsJsonObject("grant")["grantId"].deepCopy())
            addProperty("stepId", nextId); addProperty("operation", operation); addProperty("timeoutMs", timeoutMs)
            add("parameters", JsonObject().apply { when (operation) {
                "wake_capture" -> addProperty("channel", 1)
                "led_single" -> { addProperty("index", 0); add("rgb", JsonArray().apply { add(16); add(0); add(0) }); addProperty("holdMs", 500) }
                else -> throw IllegalArgumentException("Unsupported diagnostic operation")
            } })
        }
        DiagnosticSchemaV1.validate(request, "request")
        require(document.getAsJsonArray("steps").size() < 128)
        mutate { next -> next.getAsJsonArray("steps").add(JsonObject().apply {
            add("request", request.deepCopy()); addProperty("state", if (tested) "pending" else "not_tested"); addProperty("startedUtc", utc)
            add("endedUtc", if (tested) JsonNull.INSTANCE else JsonPrimitive(utc)); add("deviceReply", JsonNull.INSTANCE); add("ignoredReplies", JsonArray())
            add("observation", JsonObject().apply { addProperty("source", "user"); addProperty("result", "not_observed"); addProperty("expected", ""); addProperty("actual", ""); add("recordedUtc", JsonNull.INSTANCE); add("mediaIds", JsonArray()) })
            addProperty("verdict", if (tested) "unclear" else "not_tested")
        }) }
        if (!tested) return request.deepCopy()
        deadline = nowMs + timeoutMs
        try { transport.send(request.deepCopy()) } catch (e: Exception) {
            interrupt("disconnected", utc)
            throw e
        }
        return request.deepCopy()
    }
    fun receive(bytes: ByteArray, binding: Long, utc: String, nowMs: Long): Boolean {
        if (closed) return false
        require(nowMs >= 0 && Instant.parse(utc) >= Instant.parse(document.text("startedUtc")))
        tick(nowMs, utc)
        val reply = DiagnosticJsonV1.parse(bytes).asJsonObject
        DiagnosticSchemaV1.validate(reply, "reply")
        val active = pending()
        val valid = !closed && binding == originalBinding && active != null && runCatching {
            DiagnosticReportCodecV1.validateReply(active.getAsJsonObject("request"), reply, document.getAsJsonObject("grant"))
        }.isSuccess
        if (!valid) {
            val last = document.getAsJsonArray("steps").lastOrNull()?.asJsonObject ?: return false
            if (last.getAsJsonArray("ignoredReplies").size() >= 16) {
                interrupt("disconnected", utc)
                closed = true
                throw IllegalStateException("Ignored reply budget exceeded; full transport evidence required")
            }
            mutate { it.getAsJsonArray("steps").last().asJsonObject.getAsJsonArray("ignoredReplies").add(reply.deepCopy()) }
            return false
        }
        mutate { next -> pending(next)!!.apply {
            add("deviceReply", reply.deepCopy()); addProperty("state", "finished"); addProperty("endedUtc", utc)
            addProperty("verdict", DiagnosticReportCodecV1.verdict(this))
        } }
        deadline = null
        return true
    }
    fun observe(stepId: Long, result: String, expected: String, actual: String, utc: String) {
        require(result in setOf("matches", "mismatch", "not_observed"))
        require(result == "not_observed" || (expected.isNotEmpty() && actual.isNotEmpty()))
        require(!finalized)
        require(Instant.parse(utc) >= Instant.parse(document.text("startedUtc")))
        mutate { next ->
            val step = next.getAsJsonArray("steps").single { it.asJsonObject.getAsJsonObject("request").number("stepId") == stepId }.asJsonObject
            step.getAsJsonObject("observation").apply { addProperty("result", result); addProperty("expected", expected); addProperty("actual", actual); addProperty("recordedUtc", utc) }
            DiagnosticSchemaV1.validate(step, "step")
            step.addProperty("verdict", DiagnosticReportCodecV1.verdict(step))
        }
    }
    fun tick(nowMs: Long, utc: String) { if (deadline?.let { nowMs >= it } == true) interrupt("timeout", utc) }
    fun interrupt(reason: String, utc: String) {
        require(!finalized)
        require(reason in diagnosticInterruptions)
        require(Instant.parse(utc) >= Instant.parse(document.text("startedUtc")))
        mutate { next ->
            pending(next)?.apply { addProperty("state", reason); addProperty("endedUtc", utc); addProperty("verdict", "unclear") }
            next.getAsJsonObject("grant").addProperty("endedReason", if (reason == "timeout") "expired" else reason)
        }
        deadline = null
        // A timeout retires the uncertain grant too; new execution needs a new explicitly created run.
        closed = true
    }
    fun finish(utc: String): DiagnosticReportV1 {
        if (finalized) return DiagnosticReportCodecV1.fromDocument(document)
        require(pending() == null)
        val next = document.deepCopy().apply { addProperty("endedUtc", utc); if (!closed) getAsJsonObject("grant").addProperty("endedReason", "completed") }
        val report = DiagnosticReportCodecV1.fromDocument(next)
        // Keep the private completion marker in the same atomic replacement as the report.
        journal.replace(journalBytes(next, true)); document = next; closed = true; finalized = true
        return report
    }
    companion object {
        private fun journalBytes(report: JsonObject, finalized: Boolean, legacyLocked: Boolean = false): ByteArray = DiagnosticJsonV1.canonical(JsonObject().apply {
            add("report", report.deepCopy()); addProperty("finalized", finalized)
            if (legacyLocked) addProperty("legacyLocked", true)
        })
        fun start(metadata: DiagnosticReportV1, journal: DiagnosticJournalV1, transport: SyntheticDiagnosticTransportV1, binding: Long): FakeDiagnosticRepositoryV1 {
            require(journal.load() == null) { "Existing journal must be recovered, never overwritten" }
            val document = metadata.snapshot()
            require(metadata.evidence == "synthetic" && document.getAsJsonArray("steps").size() == 0)
            require(document.getAsJsonObject("capabilities").text("source") == "synthetic")
            journal.replace(journalBytes(document, false))
            return FakeDiagnosticRepositoryV1(document, journal, transport, binding)
        }
        fun recover(journal: DiagnosticJournalV1, utc: String): FakeDiagnosticRepositoryV1 {
            val saved = DiagnosticJsonV1.parse(checkNotNull(journal.load())).asJsonObject
            val marked = saved.has("finalized")
            if (marked) {
                require(saved.keySet() == setOf("report", "finalized") || saved.keySet() == setOf("report", "finalized", "legacyLocked"))
                require(saved["finalized"].isJsonPrimitive && saved["finalized"].asJsonPrimitive.isBoolean)
                if (saved.has("legacyLocked")) require(saved["legacyLocked"].isJsonPrimitive && saved["legacyLocked"].asJsonPrimitive.isBoolean && saved["legacyLocked"].asBoolean && saved["finalized"].asBoolean)
            }
            val document = if (marked) saved.getAsJsonObject("report") else saved
            val finalized = marked && saved["finalized"].asBoolean
            require(document.text("evidence") == "synthetic")
            DiagnosticSchemaV1.validate(document)
            val pendingSteps = document.getAsJsonArray("steps").filter { it.asJsonObject.text("state") == "pending" }
            require(pendingSteps.size <= 1 && (pendingSteps.isEmpty() || pendingSteps.single() == document.getAsJsonArray("steps").last()))
            val validation = document.deepCopy().apply {
                addProperty("endedUtc", utc)
                getAsJsonArray("steps").forEach { step -> if (step.asJsonObject.text("state") == "pending") step.asJsonObject.apply {
                    addProperty("state", "app_restarted"); addProperty("endedUtc", utc)
                } }
            }
            DiagnosticReportCodecV1.validate(if (finalized) document else validation)
            val legacyLocked = (marked && saved.has("legacyLocked")) || (!marked && pendingSteps.isEmpty())
            // A raw journal without a pending request may already have been exported.
            // Freeze its evidence; preserve a valid original report byte-for-byte.
            val recovered = if (legacyLocked && runCatching { DiagnosticReportCodecV1.fromDocument(document) }.isFailure) validation else document
            if (!marked && legacyLocked) journal.replace(journalBytes(recovered, true, true))
            val repository = FakeDiagnosticRepositoryV1(recovered, journal, SyntheticDiagnosticTransportV1 { error("Recovered run cannot send") }, -1,
                closed = true, finalized = finalized || legacyLocked, isLegacyLocked = legacyLocked)
            if (repository.pending() != null) repository.interrupt("app_restarted", utc)
            // Preserve completed results; no grant, binding or command becomes live after recovery.
            return repository
        }
    }
}
