package de.larpescape.companion.data.nfc

import com.google.gson.Gson
import com.google.gson.JsonParser
import de.larpescape.protocol.NfcCapabilitySnapshotV1
import java.io.File
import java.io.FileOutputStream
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.time.Instant

data class NfcCapabilityEventEvidenceV1(val kind: String, val recordedUtc: String)

/** Phone-local history. Device correlation cannot be set by this module. */
data class NfcCapabilityEvidenceV1(
    val schema: String = SCHEMA,
    val appBuild: String,
    val startedUtc: String,
    val endedUtc: String?,
    val runId: String,
    val hceFeatureObserved: Boolean,
    val nfcEnabledObserved: Boolean,
    val selectReceived: Boolean,
    val challengeReceived: Boolean,
    val deactivated: Boolean,
    val timedOut: Boolean,
    val challenge: String?,
    val localStatus: String,
    val assessment: String = "UNCLEAR",
    val deviceCorrelation: String = "UNVERIFIED",
    val events: List<NfcCapabilityEventEvidenceV1>,
    val exportedUtc: String? = null,
) {
    companion object { const val SCHEMA = "nfc_capability_v1"; const val MAX_EVENTS = 12 }
}

class NfcCapabilityEvidenceJournalV1(private val directory: File, private val now: () -> Instant = Instant::now) {
    private val gson = Gson()
    private val file get() = File(directory, "nfc-capability-v1.json")

    fun fromSnapshot(snapshot: NfcCapabilitySnapshotV1, appBuild: String, startUtc: Instant,
                     feature: Boolean, enabled: Boolean): NfcCapabilityEvidenceV1 {
        val startedElapsed = requireNotNull(snapshot.startedElapsedMs)
        val events = snapshot.events.map {
            NfcCapabilityEventEvidenceV1(it.kind, startUtc.plusMillis(it.elapsedMs - startedElapsed).toString())
        }
        val terminal = snapshot.status !in setOf("ARMED", "SELECTED")
        return NfcCapabilityEvidenceV1(appBuild = appBuild, startedUtc = startUtc.toString(),
            endedUtc = if (terminal) events.lastOrNull()?.recordedUtc ?: now().toString() else null,
            runId = requireNotNull(snapshot.runId), hceFeatureObserved = feature,
            nfcEnabledObserved = enabled, selectReceived = events.any { it.kind == "SELECT" },
            challengeReceived = events.any { it.kind == "CHALLENGE_ECHO" },
            deactivated = events.any { it.kind == "DEACTIVATED" },
            timedOut = events.any { it.kind == "TIMEOUT" }, challenge = snapshot.challenge,
            localStatus = snapshot.status, events = events)
    }

    /** Recovery closes an interrupted arm as evidence; it never re-arms HCE. */
    fun loadRecovered(): NfcCapabilityEvidenceV1? {
        if (!file.exists()) return null
        require(file.length() in 1..MAX_BYTES.toLong())
        val bytes = file.inputStream().use { it.readNBytes(MAX_BYTES + 1) }
        require(bytes.size <= MAX_BYTES)
        val json = JsonParser.parseString(String(bytes, Charsets.UTF_8)).asJsonObject
        require(json["schema"]?.asString == NfcCapabilityEvidenceV1.SCHEMA)
        val saved = gson.fromJson(json, NfcCapabilityEvidenceV1::class.java)
        validate(saved)
        if (saved.localStatus !in setOf("ARMED", "SELECTED")) return saved
        val closed = saved.copy(localStatus = "APP_RESTARTED", endedUtc = now().toString(),
            events = (saved.events + NfcCapabilityEventEvidenceV1("APP_RESTARTED", now().toString()))
                .take(NfcCapabilityEvidenceV1.MAX_EVENTS))
        save(closed)
        return closed
    }

    fun save(report: NfcCapabilityEvidenceV1) {
        validate(report)
        val bytes = gson.toJson(report.copy(exportedUtc = null)).toByteArray(Charsets.UTF_8)
        require(bytes.size in 1..MAX_BYTES)
        check(directory.isDirectory || directory.mkdirs())
        val temp = File(directory, file.name + ".tmp")
        try {
            FileOutputStream(temp).use { it.write(bytes); it.fd.sync() }
            Files.move(temp.toPath(), file.toPath(), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING)
        } finally { temp.delete() }
    }

    fun exportBytes(report: NfcCapabilityEvidenceV1): ByteArray {
        validate(report)
        return gson.toJson(report.copy(exportedUtc = now().toString())).toByteArray(Charsets.UTF_8).also {
            require(it.size in 1..MAX_BYTES)
        }
    }

    private fun validate(report: NfcCapabilityEvidenceV1) {
        require(report.schema == NfcCapabilityEvidenceV1.SCHEMA && report.appBuild.length in 1..80 &&
            HEX16.matches(report.runId) && report.events.size in 1..NfcCapabilityEvidenceV1.MAX_EVENTS &&
            report.assessment == "UNCLEAR" && report.deviceCorrelation == "UNVERIFIED" &&
            report.localStatus in STATUS && (report.challenge == null || HEX16.matches(report.challenge)))
        Instant.parse(report.startedUtc)
        report.endedUtc?.let(Instant::parse)
        report.exportedUtc?.let(Instant::parse)
        report.events.forEach { require(it.kind in EVENTS); Instant.parse(it.recordedUtc) }
        require(report.selectReceived == report.events.any { it.kind == "SELECT" } &&
            report.challengeReceived == report.events.any { it.kind == "CHALLENGE_ECHO" } &&
            report.deactivated == report.events.any { it.kind == "DEACTIVATED" } &&
            report.timedOut == report.events.any { it.kind == "TIMEOUT" } &&
            (!report.challengeReceived || report.challenge != null))
    }

    companion object {
        const val MAX_BYTES = 8192
        private val HEX16 = Regex("[0-9A-F]{32}")
        private val STATUS = setOf("ARMED", "SELECTED", "LOCAL_ECHO_ONLY", "STOPPED", "TIMEOUT", "DEACTIVATED", "APP_RESTARTED")
        private val EVENTS = setOf("ARMED", "SELECT", "CHALLENGE_ECHO", "STOPPED", "TIMEOUT", "DEACTIVATED", "APP_RESTARTED")
    }
}
