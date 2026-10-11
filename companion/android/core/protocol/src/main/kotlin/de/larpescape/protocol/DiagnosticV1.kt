package de.larpescape.protocol

import com.google.gson.*
import java.time.Instant

internal fun JsonObject.text(key: String): String = get(key).asString
internal fun JsonObject.number(key: String): Long = get(key).asLong
internal val diagnosticCorrelation = listOf("runId", "stepId", "deviceId", "bootId", "grantId", "operation")
internal val diagnosticInterruptions = setOf("timeout", "cancelled", "disconnected", "app_restarted", "device_restarted")

/** Detached document: parsing or a decoded grant never grants device access. */
class DiagnosticReportV1 internal constructor(private val document: JsonObject) {
    fun snapshot(): JsonObject = document.deepCopy()
    val evidence: String get() = document.text("evidence")
    val verdicts: List<String> get() = document.getAsJsonArray("steps").map { it.asJsonObject.text("verdict") }
}

object DiagnosticReportCodecV1 {
    fun import(bytes: ByteArray): DiagnosticReportV1 {
        val envelope = DiagnosticJsonV1.parse(bytes).asJsonObject
        require(envelope.keySet() == setOf("report", "sha256"))
        require(envelope["sha256"].isJsonPrimitive && envelope["sha256"].asJsonPrimitive.isString)
        val report = envelope.getAsJsonObject("report")
        require(envelope.text("sha256") == DiagnosticJsonV1.sha256(DiagnosticJsonV1.canonical(report))) { "Diagnostic checksum mismatch" }
        return fromDocument(report)
    }
    fun fromDocument(document: JsonObject): DiagnosticReportV1 {
        // Reparse canonical bytes to reject values injected through Gson's mutable API.
        val copy = DiagnosticJsonV1.parse(DiagnosticJsonV1.canonical(document)).asJsonObject
        validate(copy)
        return DiagnosticReportV1(copy)
    }
    fun export(report: DiagnosticReportV1): ByteArray {
        val document = report.snapshot()
        validate(document)
        val envelope = JsonObject().apply { add("report", document); addProperty("sha256", DiagnosticJsonV1.sha256(DiagnosticJsonV1.canonical(document))) }
        return DiagnosticJsonV1.canonical(envelope) + byteArrayOf(10)
    }
    internal fun validate(report: JsonObject) {
        DiagnosticSchemaV1.validate(report)
        val grant = report.getAsJsonObject("grant")
        require(grant["runId"] == report["runId"])
        require(grant.number("expiresDeviceMs") - grant.number("issuedDeviceMs") in 1..60000)
        val firmware = report.getAsJsonArray("firmware")
        require(firmware[0].asJsonObject.text("role") == "box" && firmware[1].asJsonObject.text("role") == "receiver")
        require(firmware[0].asJsonObject["deviceId"] == report["deviceId"])
        firmware.forEach { fw -> if (fw.asJsonObject.text("source") == "unavailable") require(fw.asJsonObject["deviceId"].isJsonNull && fw.asJsonObject["version"].isJsonNull) }
        if (report.text("evidence") == "device_and_user") {
            require(firmware[0].asJsonObject.text("source") == "device")
            require(report.getAsJsonObject("capabilities").text("source") == "device")
            require(report.getAsJsonObject("clock").text("appUtcSource") == "system_clock")
            require(firmware.none { it.asJsonObject.text("source") == "synthetic" })
        }
        val start = Instant.parse(report.text("startedUtc")); val end = Instant.parse(report.text("endedUtc"))
        require(start <= end)
        val media = report.getAsJsonArray("media")
        val mediaIds = media.map { it.asJsonObject.text("id") }.toSet()
        require(mediaIds.size == media.size())
        // Media IO is outside this first host consumer; fail closed instead of claiming checked media.
        require(media.size() == 0) { "Media byte verification is not implemented" }
        var previous = 0L
        report.getAsJsonArray("steps").forEach { item ->
            val step = item.asJsonObject; val request = step.getAsJsonObject("request")
            require(request.number("stepId") > previous); previous = request.number("stepId")
            require(listOf("runId", "deviceId", "bootId").all { request[it] == report[it] })
            require(request["grantId"] == grant["grantId"])
            require(step.text("state") != "pending" && !step["endedUtc"].isJsonNull)
            val stepStart = Instant.parse(step.text("startedUtc")); val stepEnd = Instant.parse(step.text("endedUtc"))
            require(start <= stepStart && stepStart <= stepEnd && stepEnd <= end)
            require(!step["deviceReply"].isJsonNull == (step.text("state") == "finished"))
            if (!step["deviceReply"].isJsonNull) validateReply(request, step.getAsJsonObject("deviceReply"), grant)
            val obs = step.getAsJsonObject("observation")
            require(obs.getAsJsonArray("mediaIds").all { it.asString in mediaIds })
            if (obs.text("result") != "not_observed") require(!obs["recordedUtc"].isJsonNull && obs.text("expected").isNotEmpty() && obs.text("actual").isNotEmpty())
            if (!obs["recordedUtc"].isJsonNull) require(Instant.parse(obs.text("recordedUtc")) in start..end)
            require(step.text("verdict") == verdict(step)) { "Diagnostic verdict disagrees with evidence" }
        }
    }
    internal fun validateReply(request: JsonObject, reply: JsonObject, grant: JsonObject) {
        DiagnosticSchemaV1.validate(reply, "reply")
        require(diagnosticCorrelation.all { request[it] == reply[it] }) { "Diagnostic correlation mismatch" }
        val start = reply.number("startedDeviceMs"); val end = reply.number("endedDeviceMs")
        require(grant.number("issuedDeviceMs") <= start && start <= end && end < grant.number("expiresDeviceMs"))
        require(end - start < request.number("timeoutMs"))
        if (request.text("operation") == "led_single" && reply.text("outcome") == "completed") require(end - start >= 500)
    }
    internal fun verdict(step: JsonObject): String {
        if (step.text("state") == "not_tested") return "not_tested"
        if (step.text("state") in diagnosticInterruptions || step.text("state") == "pending") return "unclear"
        if (step["deviceReply"].isJsonNull) return "unclear"
        val reply = step.getAsJsonObject("deviceReply")
        if (reply.text("outcome") in setOf("cancelled", "rejected")) return "unclear"
        if (reply.text("outcome") == "failed" || !telemetryMatches(reply)) return "failed"
        return when (step.getAsJsonObject("observation").text("result")) { "matches" -> "passed"; "mismatch" -> "failed"; else -> "unclear" }
    }
    private fun telemetryMatches(reply: JsonObject): Boolean {
        if (reply["telemetry"].isJsonNull) return false
        val t = reply.getAsJsonObject("telemetry")
        if (reply.text("operation") == "led_single") return t.number("renderCalls") == 2L && t["restoredOff"].asBoolean
        val edges = t.getAsJsonArray("edges")
        if (t["overflow"].asBoolean || edges.size() != 2) return false
        val a = edges[0].asJsonObject; val b = edges[1].asJsonObject
        return a.number("channel") == 1L && b.number("channel") == 1L && a["active"].asBoolean && !b["active"].asBoolean &&
            edges.all { it.asJsonObject.let { e -> e["active"].asBoolean == (e["rawHigh"].asBoolean == (t.text("activeLevel") == "high")) } } &&
            reply.number("startedDeviceMs") <= a.number("deviceMs") && a.number("deviceMs") < b.number("deviceMs") && b.number("deviceMs") <= reply.number("endedDeviceMs")
    }
}
