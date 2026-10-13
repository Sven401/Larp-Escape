package de.larpescape.companion.data.diagnostic_observation

import com.google.gson.Gson
import com.google.gson.JsonParser
import de.larpescape.protocol.DiagnosticDevelopmentRawStepReplyV1
import java.io.File
import java.io.FileOutputStream
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.time.Instant

/** Exploratory observations only. Neither a register value nor a user report qualifies a wire. */
data class DevelopmentRawObservationV1(
    val key: String,
    val recordedUtc: String,
    val deviceId: String,
    val bootId: String,
    val runId: String,
    val stepId: Long,
    val message: Long,
    val outcome: String,
    val i2cStatus: String,
    val rawByte: Int?,
    val rawHigh: Boolean?,
    val busCode: Int,
    val rejection: String,
    val captureStage: String? = null,
    val userObservation: String? = null,
    val userObservedUtc: String? = null,
    val physicalChannelQualified: Boolean = false,
)

data class DevelopmentRawObservationReportV1(
    val schema: String = "development_raw_observation_v1",
    val observations: List<DevelopmentRawObservationV1> = emptyList(),
    val physicalResult: String = "UNVERIFIED",
    val wake2CandidateSignal: String? = "NOT_TESTED",
    val exportedUtc: String? = null,
)

/** Durable record of correlated device replies and explicit user observations. */
class DevelopmentRawObservationJournalV1(private val directory: File) {
    private val gson = Gson()
    private val file get() = File(directory, "development-raw-observation-v1.json")

    @Synchronized
    fun load(): DevelopmentRawObservationReportV1 {
        if (!file.exists()) return DevelopmentRawObservationReportV1()
        require(file.length() in 1..MAX_BYTES.toLong()) { "Raw observation report too large" }
        val json = JsonParser.parseString(file.readText(Charsets.UTF_8)).asJsonObject
        require(json["schema"]?.asString == "development_raw_observation_v1")
        val report = gson.fromJson(json, DevelopmentRawObservationReportV1::class.java)
        validate(report)
        return report.copy(wake2CandidateSignal = candidateSignal(report.observations))
    }

    @Synchronized
    fun record(reply: DiagnosticDevelopmentRawStepReplyV1, stage: String? = null): DevelopmentRawObservationV1 {
        require(stage == null || stage in CAPTURE_STAGES)
        val request = reply.request
        val key = "${request.deviceId}:${request.bootId}:${request.runId}:${request.stepId}:${request.message}"
        val previous = load()
        previous.observations.firstOrNull { it.key == key }?.let { return it }
        check(previous.observations.size < MAX_OBSERVATIONS) { "Raw observation report capacity reached" }
        val item = DevelopmentRawObservationV1(key, Instant.now().toString(), request.deviceId,
            request.bootId, request.runId, request.stepId, request.message, reply.outcome.name,
            reply.i2cStatus.name, reply.rawByte, reply.rawHigh, reply.busCode, reply.rejection.name,
            captureStage = stage)
        val observations = previous.observations + item
        write(previous.copy(observations = observations,
            wake2CandidateSignal = candidateSignal(observations)))
        return item
    }

    @Synchronized
    fun observe(key: String, observation: String): DevelopmentRawObservationV1 {
        require(observation in OBSERVATIONS)
        val previous = load()
        val index = previous.observations.indexOfFirst { it.key == key }
        require(index >= 0) { "Raw step result is no longer available" }
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

    private fun validate(report: DevelopmentRawObservationReportV1) {
        require(report.schema == "development_raw_observation_v1" && report.physicalResult == "UNVERIFIED" &&
            report.observations.size <= MAX_OBSERVATIONS &&
            (report.wake2CandidateSignal == null ||
                report.wake2CandidateSignal == candidateSignal(report.observations)))
        require(report.observations.map { it.key }.distinct().size == report.observations.size)
        report.observations.forEach { item ->
            require(item.key == "${item.deviceId}:${item.bootId}:${item.runId}:${item.stepId}:${item.message}" &&
                item.recordedUtc.length in 1..80 && item.stepId in 1..0xffff_ffffL &&
                item.message in 1..0xffff_ffffL && !item.physicalChannelQualified &&
                (item.captureStage == null || item.captureStage in CAPTURE_STAGES) &&
                (item.rawByte == null || item.rawByte in 0..255) && item.busCode in 0..255 &&
                (item.userObservation == null && item.userObservedUtc == null ||
                    item.userObservation in OBSERVATIONS && item.userObservedUtc?.length in 1..80))
        }
    }

    private fun write(report: DevelopmentRawObservationReportV1) {
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
        val OBSERVATIONS = setOf("CHANGE_SEEN", "NO_CHANGE_SEEN", "UNCLEAR")
        val CAPTURE_STAGES = setOf("REST", "PRESSED", "RELEASED")

        /** Signal evidence only; never qualifies the physical Wake-2 wiring. */
        fun candidateSignal(items: List<DevelopmentRawObservationV1>): String {
            val triple = items.filter { it.captureStage != null }.takeLast(3)
            if (triple.map { it.captureStage } != listOf("REST", "PRESSED", "RELEASED"))
                return "NOT_TESTED"
            if (triple.map { it.deviceId }.distinct().size != 1 ||
                triple.map { it.bootId }.distinct().size != 1 ||
                triple.map { it.runId }.distinct().size != 1 ||
                triple[1].stepId != triple[0].stepId + 1 ||
                triple[2].stepId != triple[1].stepId + 1)
                return "NOT_TESTED"
            if (triple.any { it.outcome != "REGISTER_OBSERVED" ||
                    it.i2cStatus != "VALID" || it.rawHigh == null })
                return "READ_FAILED"
            return if (triple[0].rawHigh != triple[1].rawHigh &&
                triple[0].rawHigh == triple[2].rawHigh) "RAW_PRESS_RELEASE_OBSERVED"
                else "RAW_PATTERN_NOT_OBSERVED"
        }
    }
}
