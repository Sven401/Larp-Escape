package de.larpescape.companion.data.diagnostic_observation

import com.google.gson.Gson
import com.google.gson.JsonParser
import java.io.File
import java.io.FileOutputStream
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.time.Instant

/** Transport and correlated wire observations. None is a user or physical-device verdict. */
data class DiagnosticConnectionEventV1(
    val sequence: Int,
    val recordedUtc: String,
    val connectionGeneration: Int,
    val targetAddress: String,
    val kind: Kind,
    val message: Long?,
    val detail: String,
) {
    enum class Kind { CCCD_WRITE_ACK, TRANSPORT_PROBE, TRANSPORT_PROBE_COMPLETE, WRITE_ACK, CONTROL_REPLY, STEP_REPLY, TRANSPORT_ERROR }
}

data class DiagnosticConnectionEvidenceV1(
    val schema: String = SCHEMA,
    val connectionGeneration: Int,
    val targetAddress: String,
    val events: List<DiagnosticConnectionEventV1> = emptyList(),
    val originalAuthority: String = "OPEN",
    val physicalResult: String = "UNVERIFIED",
    /** Time of this export snapshot; the private journal may receive later events. */
    val exportedUtc: String? = null,
) {
    companion object {
        const val SCHEMA = "diagnostic_connection_evidence_v1"
        const val MAX_EVENTS = 128
    }
}

/** One connection per file. Recovery yields history only; it never recreates pending writes. */
class DiagnosticConnectionEvidenceJournalV1(private val directory: File) {
    private val gson = Gson()
    private val file get() = File(directory, "diagnostic-connection-evidence-v1.json")
    private var current: DiagnosticConnectionEvidenceV1? = null

    fun loadRecovered(): DiagnosticConnectionEvidenceV1? {
        if (!file.exists()) return null
        require(file.length() in 1..MAX_BYTES.toLong()) { "Connection evidence too large" }
        val bytes = file.inputStream().use { it.readNBytes(MAX_BYTES + 1) }
        require(bytes.size <= MAX_BYTES) { "Connection evidence too large" }
        val json = JsonParser.parseString(String(bytes, Charsets.UTF_8)).asJsonObject
        require(json["schema"]?.asString == DiagnosticConnectionEvidenceV1.SCHEMA)
        val saved = gson.fromJson(json, DiagnosticConnectionEvidenceV1::class.java)
        validate(saved)
        // Deliberately do not set current. A process restart has no live GATT owner or pending write.
        return saved
    }

    /** A validated, bounded historical snapshot. Reading it never resumes a GATT session. */
    fun exportBytes(): ByteArray? {
        val saved = loadRecovered() ?: return null
        return gson.toJson(saved.copy(exportedUtc = Instant.now().toString())).toByteArray(Charsets.UTF_8).also {
            require(it.size in 1..MAX_BYTES) { "Connection evidence too large" }
        }
    }

    fun begin(generation: Int, address: String) {
        val fresh = DiagnosticConnectionEvidenceV1(connectionGeneration = generation, targetAddress = address)
        validate(fresh)
        write(fresh)
        current = fresh
    }

    fun append(event: DiagnosticConnectionEventV1): Boolean {
        val previous = current ?: return false
        if (previous.events.size >= DiagnosticConnectionEvidenceV1.MAX_EVENTS ||
            event.sequence != previous.events.size + 1 || event.connectionGeneration != previous.connectionGeneration ||
            event.targetAddress != previous.targetAddress) return false
        val next = previous.copy(events = previous.events + event)
        validate(next)
        write(next)
        current = next
        return true
    }

    fun nextSequence(): Int? = current?.events?.size?.plus(1)

    private fun validate(saved: DiagnosticConnectionEvidenceV1) {
        require(saved.schema == DiagnosticConnectionEvidenceV1.SCHEMA && saved.connectionGeneration > 0 &&
            ADDRESS.matches(saved.targetAddress) && saved.originalAuthority == "OPEN" &&
            saved.physicalResult == "UNVERIFIED" && saved.events.size <= DiagnosticConnectionEvidenceV1.MAX_EVENTS)
        saved.events.forEachIndexed { index, event ->
            require(event.sequence == index + 1 && event.connectionGeneration == saved.connectionGeneration &&
                event.targetAddress == saved.targetAddress && event.recordedUtc.length in 1..80 &&
                event.detail.length in 1..256 && (event.message == null || event.message in 1..0xffff_ffffL))
        }
    }

    private fun write(snapshot: DiagnosticConnectionEvidenceV1) {
        val bytes = gson.toJson(snapshot).toByteArray(Charsets.UTF_8)
        require(bytes.size in 1..MAX_BYTES)
        check(directory.isDirectory || directory.mkdirs())
        val temp = File(directory, file.name + ".tmp")
        try {
            FileOutputStream(temp).use { it.write(bytes); it.fd.sync() }
            Files.move(temp.toPath(), file.toPath(), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING)
        } finally { temp.delete() }
    }

    companion object {
        const val MAX_BYTES = 65536
        private val ADDRESS = Regex("[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}")
    }
}
