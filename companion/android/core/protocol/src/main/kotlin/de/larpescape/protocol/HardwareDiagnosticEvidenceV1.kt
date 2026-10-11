package de.larpescape.protocol

import com.google.gson.JsonArray
import com.google.gson.JsonElement
import com.google.gson.JsonNull
import com.google.gson.JsonObject
import com.google.gson.JsonPrimitive
import java.time.Instant
import java.util.UUID

enum class HardwareDiagnosticActionV1(val wireId: String, val writesDeviceState: Boolean) {
    LED_SELECT_INDICES("led_select_indices", true), LED_SELECT_GROUP("led_select_group", true),
    LED_SET_COLOR("led_set_color", true), LED_SET_BRIGHTNESS("led_set_brightness", true),
    LED_CONFIGURE_EFFECT("led_configure_effect", true), LED_START("led_start", true),
    LED_DURATION_EXPIRY("led_duration_expiry", false), LED_STOP("led_stop", true),
    LED_INDEPENDENT_OFF("led_independent_off", false),
    AUDIO_NAMED_REICHSTAG_CLIP("audio_named_reichstag_clip", true),
    AUDIO_NAMED_STAR_WARS_CLIP("audio_named_star_wars_clip", true),
    AUDIO_DIRECT_TRACK("audio_direct_track", true), AUDIO_PLAY("audio_play", true),
    AUDIO_STOP("audio_stop", true), AUDIO_REPEAT("audio_repeat", true),
    AUDIO_VOLUME("audio_volume", true), AUDIO_BUSY_STATUS("audio_busy_status", false),
    AUDIO_UART_STATUS("audio_uart_status", false), AUDIO_ERROR_STATUS("audio_error_status", false),
    NFC_PHONE_WRITE_BOX_READ("nfc_phone_write_box_read", false),
    NFC_PHONE_WRITE_REAPPLY("nfc_phone_write_reapply", false),
    NFC_PHONE_WRITE_BYTE_COMPARE("nfc_phone_write_byte_compare", false),
    NFC_BOX_WRITE_PHONE_READ("nfc_box_write_phone_read", true),
    NFC_EXPLICIT_WRITE_CONFIRMATION("nfc_explicit_write_confirmation", false),
    NFC_BOX_WRITE_REAPPLY("nfc_box_write_reapply", false),
    NFC_BOX_WRITE_BYTE_COMPARE("nfc_box_write_byte_compare", false),
    MATRIX_RAW_PRESS("matrix_raw_press", false), MATRIX_RAW_RELEASE("matrix_raw_release", false),
    MATRIX_LIVE_HIGHLIGHT("matrix_live_highlight", false), MATRIX_SCAN_ERROR("matrix_scan_error", false),
    MATRIX_VALIDATE_81("matrix_validate_81", false), MATRIX_ROTATION("matrix_rotation", false),
    MATRIX_MIRROR("matrix_mirror", false), MATRIX_ORIENTATION_SUGGEST("matrix_orientation_suggest", false),
    MATRIX_ORIENTATION_CONFIRM("matrix_orientation_confirm", false),
    WAKE_RAW_LEVEL("wake_raw_level", false), WAKE_EDGE("wake_edge", false),
    WAKE_SINGLE("wake_single", false), WAKE_COMBINATION("wake_combination", false),
    ESP_NOW_IDENTIFIED_PACKET("esp_now_identified_packet", true),
    ESP_NOW_SHORT_RANGE("esp_now_short_range", false),
    ESP_NOW_OPERATIONAL_RANGE("esp_now_operational_range", false),
    ESP_NOW_RECEIVER_REPORT("esp_now_receiver_report", false),
    COMPLETION_THREE_COLD_STARTS("completion_three_cold_starts", false),
    COMPLETION_ENDURANCE("completion_endurance", false),
    COMPLETION_OPEN_ERRORS("completion_open_errors", false),
    COMPLETION_USER_OBSERVATIONS("completion_user_observations", false),
    COMPLETION_REPORT_EXPORT("completion_report_export", false),
}

enum class DeviceAcceptanceV1 { ACCEPTED, REJECTED, UNCLEAR }
enum class DeviceExecutionV1 { EXECUTED, MEASURED, FAILED, UNCLEAR }
enum class UserObservationV1 { MATCHES, MISMATCH, NOT_OBSERVED }

data class DiagnosticRequestEvidenceV1(val correlationId: String, val requestedUtc: String)

/** A local claim is deliberately not a verified device reply. */
data class DiagnosticAcceptanceClaimV1(val state: DeviceAcceptanceV1, val deviceMs: Long?, val detail: String)
data class DiagnosticExecutionClaimV1(val state: DeviceExecutionV1, val deviceMs: Long?, val detail: String)
data class DiagnosticUserObservationV1(val state: UserObservationV1, val recordedUtc: String, val detail: String)
data class HardwareDiagnosticAttemptEvidenceV1(
    val request: DiagnosticRequestEvidenceV1,
    val acceptanceClaim: DiagnosticAcceptanceClaimV1?,
    val executionClaim: DiagnosticExecutionClaimV1?,
    val observation: DiagnosticUserObservationV1?,
)
data class HardwareDiagnosticActionEvidenceV1(
    val action: HardwareDiagnosticActionV1,
    val attempts: List<HardwareDiagnosticAttemptEvidenceV1>,
)

class HardwareDiagnosticReportV1 internal constructor(private val document: JsonObject) {
    fun snapshot(): JsonObject = document.deepCopy()
    val runId: String get() = document.text("runId")
    val actions: List<HardwareDiagnosticActionEvidenceV1> get() = document.getAsJsonArray("actions").map(::decodeAction)
}

/** Pure evidence model. Use DurableHardwareDiagnosticRunV1 before any productive send. */
class HardwareDiagnosticEvidenceLedgerV1(val runId: String, val startedUtc: String) {
    private data class MutableAttempt(
        val request: DiagnosticRequestEvidenceV1,
        var acceptanceClaim: DiagnosticAcceptanceClaimV1? = null,
        var executionClaim: DiagnosticExecutionClaimV1? = null,
        var observation: DiagnosticUserObservationV1? = null,
    )

    private val evidence = HardwareDiagnosticActionV1.entries.associateWith { ArrayList<MutableAttempt>() }
    private val correlations = HashSet<String>()
    private var finished: HardwareDiagnosticReportV1? = null

    init {
        UUID.fromString(runId)
        Instant.parse(startedUtc)
    }

    fun request(action: HardwareDiagnosticActionV1, correlationId: String, utc: String) {
        require(finished == null)
        UUID.fromString(correlationId)
        require(!Instant.parse(utc).isBefore(Instant.parse(startedUtc)))
        require(correlationId !in correlations) { "Correlation must be unique within the run" }
        val attempts = evidence.getValue(action)
        require(attempts.size < MAX_ATTEMPTS_PER_ACTION)
        attempts.lastOrNull()?.let { previous ->
            require(!Instant.parse(utc).isBefore(Instant.parse(previous.request.requestedUtc)))
            require(previous.acceptanceClaim != null &&
                (previous.acceptanceClaim!!.state != DeviceAcceptanceV1.ACCEPTED || previous.executionClaim != null)) {
                "Previous attempt has an unclear outcome"
            }
            if (action.writesDeviceState) {
                require(attempts.none {
                    it.acceptanceClaim?.state == DeviceAcceptanceV1.UNCLEAR ||
                        it.executionClaim?.state == DeviceExecutionV1.UNCLEAR
                }) { "An unclear device-state write cannot be repeated in this run" }
            }
        }
        attempts.add(MutableAttempt(DiagnosticRequestEvidenceV1(correlationId, utc)))
        correlations.add(correlationId)
    }

    /** Records only an unverified local claim; no caller can turn this into a device receipt. */
    fun recordAcceptanceClaim(
        action: HardwareDiagnosticActionV1, correlationId: String,
        state: DeviceAcceptanceV1, deviceMs: Long?, detail: String,
    ) {
        require(finished == null && detail.isNotBlank() && detail.length <= 512)
        require(deviceMs == null || deviceMs >= 0)
        val item = attempt(action, correlationId)
        require(item.acceptanceClaim == null)
        item.acceptanceClaim = DiagnosticAcceptanceClaimV1(state, deviceMs, detail)
    }

    /** Records only an unverified local claim; a future device receipt needs a separate type. */
    fun recordExecutionClaim(
        action: HardwareDiagnosticActionV1, correlationId: String,
        state: DeviceExecutionV1, deviceMs: Long?, detail: String,
    ) {
        require(finished == null && detail.isNotBlank() && detail.length <= 512)
        require(deviceMs == null || deviceMs >= 0)
        val item = attempt(action, correlationId)
        require(item.acceptanceClaim?.state == DeviceAcceptanceV1.ACCEPTED && item.executionClaim == null)
        if (deviceMs != null && item.acceptanceClaim?.deviceMs != null)
            require(deviceMs >= item.acceptanceClaim!!.deviceMs!!)
        item.executionClaim = DiagnosticExecutionClaimV1(state, deviceMs, detail)
    }

    fun observe(
        action: HardwareDiagnosticActionV1, correlationId: String,
        state: UserObservationV1, utc: String, detail: String,
    ) {
        require(finished == null && detail.isNotBlank() && detail.length <= 1024)
        val item = attempt(action, correlationId)
        require(item.observation == null && !Instant.parse(utc).isBefore(Instant.parse(item.request.requestedUtc)))
        item.observation = DiagnosticUserObservationV1(state, utc, detail)
    }

    fun finish(endedUtc: String): HardwareDiagnosticReportV1 {
        finished?.let { return it }
        return reportAt(endedUtc).also { finished = it }
    }

    /** A validated, non-final snapshot for transactional journal updates. */
    internal fun snapshotAt(utc: String): HardwareDiagnosticReportV1 = reportAt(utc)

    private fun reportAt(endedUtc: String): HardwareDiagnosticReportV1 {
        require(!Instant.parse(endedUtc).isBefore(Instant.parse(startedUtc)))
        val document = JsonObject().apply {
            addProperty("schema", "hardware_diagnostic_evidence_v1")
            addProperty("runId", runId)
            addProperty("startedUtc", startedUtc)
            addProperty("endedUtc", endedUtc)
            add("actions", JsonArray().apply {
                HardwareDiagnosticActionV1.entries.forEach { action ->
                    add(JsonObject().apply {
                        addProperty("id", action.wireId)
                        addProperty("writesDeviceState", action.writesDeviceState)
                        add("attempts", JsonArray().apply {
                            evidence.getValue(action).forEach { attempt -> add(encodeAttempt(attempt)) }
                        })
                    })
                }
            })
        }
        return HardwareDiagnosticReportCodecV1.fromDocument(document)
    }

    private fun attempt(action: HardwareDiagnosticActionV1, correlationId: String): MutableAttempt =
        evidence.getValue(action).single { it.request.correlationId == correlationId }

    private fun encodeAttempt(item: MutableAttempt) = JsonObject().apply {
        add("request", encodeRequest(item.request))
        add("acceptanceClaim", item.acceptanceClaim?.let(::encodeAcceptance) ?: JsonNull.INSTANCE)
        add("executionClaim", item.executionClaim?.let(::encodeExecution) ?: JsonNull.INSTANCE)
        add("observation", item.observation?.let(::encodeObservation) ?: JsonNull.INSTANCE)
    }

    companion object {
        const val MAX_ATTEMPTS_PER_ACTION = 128

        /** Rebuild only from a validated report. This never sends or retries a request. */
        internal fun restore(report: HardwareDiagnosticReportV1): HardwareDiagnosticEvidenceLedgerV1 {
            val document = report.snapshot()
            val ledger = HardwareDiagnosticEvidenceLedgerV1(report.runId, document.text("startedUtc"))
            report.actions.forEach { action ->
                action.attempts.forEach { attempt ->
                    val id = attempt.request.correlationId
                    ledger.request(action.action, id, attempt.request.requestedUtc)
                    attempt.acceptanceClaim?.let {
                        ledger.recordAcceptanceClaim(action.action, id, it.state, it.deviceMs, it.detail)
                    }
                    attempt.executionClaim?.let {
                        ledger.recordExecutionClaim(action.action, id, it.state, it.deviceMs, it.detail)
                    }
                    attempt.observation?.let {
                        ledger.observe(action.action, id, it.state, it.recordedUtc, it.detail)
                    }
                }
            }
            return ledger
        }
    }
}

object HardwareDiagnosticReportCodecV1 {
    fun export(report: HardwareDiagnosticReportV1): ByteArray {
        val document = report.snapshot()
        validate(document)
        val envelope = JsonObject().apply {
            add("report", document)
            addProperty("sha256", DiagnosticJsonV1.sha256(DiagnosticJsonV1.canonical(document)))
        }
        return DiagnosticJsonV1.canonical(envelope) + byteArrayOf(10)
    }

    fun import(bytes: ByteArray): HardwareDiagnosticReportV1 {
        val envelope = DiagnosticJsonV1.parse(bytes).asJsonObject
        require(envelope.keySet() == setOf("report", "sha256"))
        val report = envelope.getAsJsonObject("report")
        require(envelope.text("sha256") == DiagnosticJsonV1.sha256(DiagnosticJsonV1.canonical(report)))
        return fromDocument(report)
    }

    internal fun fromDocument(document: JsonObject): HardwareDiagnosticReportV1 {
        val copy = DiagnosticJsonV1.parse(DiagnosticJsonV1.canonical(document)).asJsonObject
        validate(copy)
        return HardwareDiagnosticReportV1(copy)
    }

    private fun validate(document: JsonObject) {
        require(document.keySet() == setOf("schema", "runId", "startedUtc", "endedUtc", "actions"))
        require(document.text("schema") == "hardware_diagnostic_evidence_v1")
        UUID.fromString(document.text("runId"))
        val started = Instant.parse(document.text("startedUtc"))
        val ended = Instant.parse(document.text("endedUtc"))
        require(started <= ended)
        val actions = document.getAsJsonArray("actions")
        require(actions.size() == HardwareDiagnosticActionV1.entries.size)
        val correlations = HashSet<String>()
        actions.forEachIndexed { index, value ->
            val action = HardwareDiagnosticActionV1.entries[index]
            val item = value.asJsonObject
            require(item.keySet() == setOf("id", "writesDeviceState", "attempts"))
            require(item.text("id") == action.wireId)
            require(item["writesDeviceState"].isJsonPrimitive && item["writesDeviceState"].asJsonPrimitive.isBoolean)
            require(item["writesDeviceState"].asBoolean == action.writesDeviceState)
            val attempts = item.getAsJsonArray("attempts")
            require(attempts.size() <= HardwareDiagnosticEvidenceLedgerV1.MAX_ATTEMPTS_PER_ACTION)
            var previous: JsonObject? = null
            attempts.forEach { attemptElement ->
                val attempt = attemptElement.asJsonObject
                require(attempt.keySet() == setOf("request", "acceptanceClaim", "executionClaim", "observation"))
                val request = attempt.getAsJsonObject("request").also(::validateRequest)
                require(correlations.add(request.text("correlationId")))
                val requestTime = Instant.parse(request.text("requestedUtc"))
                require(requestTime in started..ended)
                val acceptance = attempt["acceptanceClaim"].takeUnless { it.isJsonNull }?.asJsonObject?.also(::validateAcceptance)
                val execution = attempt["executionClaim"].takeUnless { it.isJsonNull }?.asJsonObject?.also(::validateExecution)
                val observation = attempt["observation"].takeUnless { it.isJsonNull }?.asJsonObject?.also(::validateObservation)
                require(execution == null || acceptance?.text("state") == DeviceAcceptanceV1.ACCEPTED.name.lowercase())
                if (execution != null && acceptance != null && !execution["deviceMs"].isJsonNull && !acceptance["deviceMs"].isJsonNull)
                    require(execution.number("deviceMs") >= acceptance.number("deviceMs"))
                observation?.let { require(Instant.parse(it.text("recordedUtc")) in requestTime..ended) }
                previous?.let { prior ->
                    val priorRequest = prior.getAsJsonObject("request")
                    require(requestTime >= Instant.parse(priorRequest.text("requestedUtc")))
                    val priorAcceptance = prior["acceptanceClaim"].takeUnless { it.isJsonNull }?.asJsonObject
                    val priorExecution = prior["executionClaim"].takeUnless { it.isJsonNull }?.asJsonObject
                    require(priorAcceptance != null &&
                        (priorAcceptance.text("state") != DeviceAcceptanceV1.ACCEPTED.name.lowercase() || priorExecution != null))
                    if (action.writesDeviceState) require(
                        priorAcceptance.text("state") != DeviceAcceptanceV1.UNCLEAR.name.lowercase() &&
                            priorExecution?.text("state") != DeviceExecutionV1.UNCLEAR.name.lowercase()
                    )
                }
                previous = attempt
            }
        }
    }
}

private fun encodeRequest(value: DiagnosticRequestEvidenceV1) = JsonObject().apply {
    addProperty("correlationId", value.correlationId); addProperty("requestedUtc", value.requestedUtc)
}
private fun encodeAcceptance(value: DiagnosticAcceptanceClaimV1) = JsonObject().apply {
    addProperty("state", value.state.name.lowercase()); add("deviceMs", value.deviceMs?.let(::JsonPrimitive) ?: JsonNull.INSTANCE)
    addProperty("detail", value.detail); addProperty("provenance", "unverified_local")
}
private fun encodeExecution(value: DiagnosticExecutionClaimV1) = JsonObject().apply {
    addProperty("state", value.state.name.lowercase()); add("deviceMs", value.deviceMs?.let(::JsonPrimitive) ?: JsonNull.INSTANCE)
    addProperty("detail", value.detail); addProperty("provenance", "unverified_local")
}
private fun encodeObservation(value: DiagnosticUserObservationV1) = JsonObject().apply {
    addProperty("state", value.state.name.lowercase()); addProperty("recordedUtc", value.recordedUtc); addProperty("detail", value.detail)
}
private fun validateRequest(value: JsonObject) {
    require(value.keySet() == setOf("correlationId", "requestedUtc"))
    UUID.fromString(value.text("correlationId")); Instant.parse(value.text("requestedUtc"))
}
private fun validateAcceptance(value: JsonObject) {
    require(value.keySet() == setOf("state", "deviceMs", "detail", "provenance"))
    DeviceAcceptanceV1.valueOf(value.text("state").uppercase()); validateLocalClaim(value)
}
private fun validateExecution(value: JsonObject) {
    require(value.keySet() == setOf("state", "deviceMs", "detail", "provenance"))
    DeviceExecutionV1.valueOf(value.text("state").uppercase()); validateLocalClaim(value)
}
private fun validateLocalClaim(value: JsonObject) {
    require(value.text("provenance") == "unverified_local")
    require(value["deviceMs"].isJsonNull || (value["deviceMs"].asJsonPrimitive.isNumber && value.number("deviceMs") >= 0))
    require(value.text("detail").isNotBlank() && value.text("detail").length <= 512)
}
private fun validateObservation(value: JsonObject) {
    require(value.keySet() == setOf("state", "recordedUtc", "detail"))
    UserObservationV1.valueOf(value.text("state").uppercase())
    Instant.parse(value.text("recordedUtc"))
    require(value.text("detail").isNotBlank() && value.text("detail").length <= 1024)
}
private fun decodeAction(value: JsonElement): HardwareDiagnosticActionEvidenceV1 {
    val item = value.asJsonObject
    val action = HardwareDiagnosticActionV1.entries.single { it.wireId == item.text("id") }
    return HardwareDiagnosticActionEvidenceV1(action, item.getAsJsonArray("attempts").map { attemptElement ->
        val attempt = attemptElement.asJsonObject
        val request = attempt.getAsJsonObject("request").let {
            DiagnosticRequestEvidenceV1(it.text("correlationId"), it.text("requestedUtc"))
        }
        val acceptance = attempt["acceptanceClaim"].takeUnless { it.isJsonNull }?.asJsonObject?.let {
            DiagnosticAcceptanceClaimV1(DeviceAcceptanceV1.valueOf(it.text("state").uppercase()),
                it["deviceMs"].takeUnless { n -> n.isJsonNull }?.asLong, it.text("detail"))
        }
        val execution = attempt["executionClaim"].takeUnless { it.isJsonNull }?.asJsonObject?.let {
            DiagnosticExecutionClaimV1(DeviceExecutionV1.valueOf(it.text("state").uppercase()),
                it["deviceMs"].takeUnless { n -> n.isJsonNull }?.asLong, it.text("detail"))
        }
        val observation = attempt["observation"].takeUnless { it.isJsonNull }?.asJsonObject?.let {
            DiagnosticUserObservationV1(UserObservationV1.valueOf(it.text("state").uppercase()),
                it.text("recordedUtc"), it.text("detail"))
        }
        HardwareDiagnosticAttemptEvidenceV1(request, acceptance, execution, observation)
    })
}
