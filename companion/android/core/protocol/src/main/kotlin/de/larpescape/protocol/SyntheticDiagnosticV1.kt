package de.larpescape.protocol

import com.google.gson.*
import java.util.UUID

/** Synthetic metadata and replies for the explicit debug UI, never a hardware profile. */
object SyntheticDiagnosticV1 {
    fun newRun(appVersion: String, utc: String): DiagnosticReportV1 {
        val device = "0123456789ABCDEF0123456789ABCDEF"
        val run = UUID.randomUUID().toString(); val grant = UUID.randomUUID().toString()
        val doc = DiagnosticJsonV1.parse("""{
            "schemaVersion":1,"kind":"diagnostic_report","evidence":"synthetic",
            "runId":"$run","deviceId":"$device","bootId":"0123456789ABCDEF",
            "appVersion":"synthetic","startedUtc":"$utc","endedUtc":"$utc",
            "firmware":[{"role":"box","deviceId":"$device","version":"fake-v1","source":"synthetic"},{"role":"receiver","deviceId":null,"version":null,"source":"unavailable"}],
            "clock":{"appUtcSource":"synthetic","deviceTime":"boot_monotonic_ms","uncertaintyMs":null},
            "capabilities":{"operations":["wake_capture","led_single"],"wakeChannels":[1],"ledIndices":[0],"maxGrantMs":60000,"maxStepMs":10000,"hardwareProfileRevision":0,"source":"synthetic","polaritySource":"synthetic"},
            "grant":{"runId":"$run","grantId":"$grant","issuedDeviceMs":0,"expiresDeviceMs":60000,"endedReason":"completed"},
            "tools":[{"name":"voltmeter","available":false,"detail":"Simulation: no supply measurement"}],"media":[],"steps":[]
        }""".toByteArray()).asJsonObject
        doc.addProperty("appVersion", appVersion)
        return DiagnosticReportCodecV1.fromDocument(doc)
    }
    fun reply(request: JsonObject, startedDeviceMs: Long, failed: Boolean = false): ByteArray {
        DiagnosticSchemaV1.validate(request, "request")
        val response = JsonObject().apply {
            addProperty("schemaVersion", 1); addProperty("kind", "diagnostic_reply")
            diagnosticCorrelation.forEach { add(it, request[it].deepCopy()) }
            addProperty("outcome", if (failed) "failed" else "completed"); addProperty("reason", if (failed) "owner_error" else "none")
            addProperty("startedDeviceMs", startedDeviceMs); addProperty("endedDeviceMs", startedDeviceMs + 500)
            add("telemetry", if (request.text("operation") == "led_single") JsonObject().apply {
                addProperty("renderCalls", if (failed) 0 else 2); addProperty("restoredOff", !failed); add("supplyMillivolts", JsonNull.INSTANCE)
            } else JsonObject().apply {
                addProperty("overflow", false); addProperty("activeLevel", "high")
                add("edges", JsonArray().apply { listOf(true, false).forEachIndexed { i, active -> add(JsonObject().apply {
                    addProperty("channel", 1); addProperty("rawHigh", active); addProperty("active", active); addProperty("deviceMs", startedDeviceMs + 100 * (i + 1))
                }) } })
            })
        }
        DiagnosticSchemaV1.validate(response, "reply")
        return DiagnosticJsonV1.canonical(response)
    }
}
