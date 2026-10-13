package de.larpescape.companion.data.diagnostic_observation

import com.google.gson.Gson
import com.google.gson.JsonParser
import de.larpescape.protocol.DiagnosticDevelopmentLedStepReplyV1
import java.io.File
import java.io.FileOutputStream
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.time.Instant

/** Device replies and user observations for the LED development step; no electrical qualification. */
data class DevelopmentLedObservationV1(
    val key: String,
    val recordedUtc: String,
    val deviceId: String,
    val bootId: String,
    val runId: String,
    val stepId: Long,
    val message: Long,
    val index: Int,
    val red: Int,
    val green: Int,
    val blue: Int,
    val brightness: Int,
    val durationMs: Int,
    val outcome: String,
    val rejection: String,
    val renderCalls: Int,
    val blackSubmitted: Boolean,
    val timerFired: Boolean,
    val userObservation: String? = null,
    val userObservedUtc: String? = null,
    val physicalOutputConfirmed: Boolean = false,
)

data class DevelopmentLedObservationReportV1(
    val schema: String = "development_led_observation_v1",
    val observations: List<DevelopmentLedObservationV1> = emptyList(),
    val physicalResult: String = "UNVERIFIED",
    val exportedUtc: String? = null,
)

class DevelopmentLedObservationJournalV1(private val directory: File) {
    private val gson = Gson()
    private val file get() = File(directory, "development-led-observation-v1.json")

    @Synchronized
    fun load(): DevelopmentLedObservationReportV1 {
        if (!file.exists()) return DevelopmentLedObservationReportV1()
        require(file.length() in 1..MAX_BYTES.toLong()) { "LED observation report too large" }
        val json = JsonParser.parseString(file.readText(Charsets.UTF_8)).asJsonObject
        require(json["schema"]?.asString == "development_led_observation_v1")
        return gson.fromJson(json, DevelopmentLedObservationReportV1::class.java).also(::validate)
    }

    @Synchronized
    fun record(reply: DiagnosticDevelopmentLedStepReplyV1): DevelopmentLedObservationV1 {
        val request = reply.request
        val key = "${request.deviceId}:${request.bootId}:${request.runId}:${request.stepId}:${request.message}"
        val previous = load()
        previous.observations.firstOrNull { it.key == key }?.let { return it }
        check(previous.observations.size < MAX_OBSERVATIONS) { "LED observation report capacity reached" }
        val item = DevelopmentLedObservationV1(key, Instant.now().toString(), request.deviceId,
            request.bootId, request.runId, request.stepId, request.message, request.index,
            request.red, request.green, request.blue, request.brightness, request.durationMs,
            reply.outcome.name, reply.rejection.name, reply.renderCalls, reply.blackSubmitted,
            reply.timerFired)
        write(previous.copy(observations = previous.observations + item))
        return item
    }

    @Synchronized
    fun observe(key: String, observation: String): DevelopmentLedObservationV1 {
        require(observation in OBSERVATIONS)
        val previous = load()
        val index = previous.observations.indexOfFirst { it.key == key }
        require(index >= 0) { "LED step result is no longer available" }
        val existing = previous.observations[index]
        require(existing.userObservation == null) { "Observation already recorded" }
        val item = existing.copy(userObservation = observation, userObservedUtc = Instant.now().toString())
        write(previous.copy(observations = previous.observations.toMutableList().also { it[index] = item }))
        return item
    }

    @Synchronized
    fun exportBytes(): ByteArray? {
        val report = load()
        if (report.observations.isEmpty()) return null
        return gson.toJson(report.copy(exportedUtc = Instant.now().toString())).toByteArray(Charsets.UTF_8)
            .also { require(it.size <= MAX_BYTES) }
    }

    private fun validate(report: DevelopmentLedObservationReportV1) {
        require(report.schema == "development_led_observation_v1" &&
            report.physicalResult == "UNVERIFIED" && report.observations.size <= MAX_OBSERVATIONS)
        require(report.observations.map { it.key }.distinct().size == report.observations.size)
        report.observations.forEach { item ->
            require(item.key == "${item.deviceId}:${item.bootId}:${item.runId}:${item.stepId}:${item.message}" &&
                item.recordedUtc.length in 1..80 && item.stepId in 1..0xffff_ffffL &&
                item.message in 1..0xffff_ffffL && item.index in 0..22 &&
                item.red in 0..32 && item.green in 0..32 && item.blue in 0..32 &&
                item.brightness in 1..32 && item.durationMs in 100..250 &&
                item.renderCalls in 0..2 && !item.physicalOutputConfirmed &&
                (item.userObservation == null && item.userObservedUtc == null ||
                    item.userObservation in OBSERVATIONS && item.userObservedUtc?.length in 1..80))
        }
    }

    private fun write(report: DevelopmentLedObservationReportV1) {
        validate(report)
        val bytes = gson.toJson(report).toByteArray(Charsets.UTF_8)
        require(bytes.size in 1..MAX_BYTES)
        check(directory.isDirectory || directory.mkdirs())
        val staged = File(directory, file.name + ".tmp")
        try {
            FileOutputStream(staged).use { it.write(bytes); it.fd.sync() }
            Files.move(staged.toPath(), file.toPath(), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING)
        } finally { staged.delete() }
    }

    companion object {
        const val MAX_OBSERVATIONS = 128
        const val MAX_BYTES = 131072
        val OBSERVATIONS = setOf("LIGHT_SEEN", "NO_LIGHT_SEEN", "UNEXPECTED_LIGHT_SEEN", "UNCLEAR")
    }
}
