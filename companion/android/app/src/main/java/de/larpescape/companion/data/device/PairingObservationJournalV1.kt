package de.larpescape.companion.data.device

import com.google.gson.Gson
import com.google.gson.JsonParser
import java.io.File
import java.io.FileOutputStream
import java.nio.file.Files
import java.nio.file.StandardCopyOption

/** One app-private, bounded report. Disk state is evidence, never a live link or grant. */
class PairingObservationJournalV1(private val directory: File) {
    private val gson = Gson()
    private val reportFile get() = File(directory, "ble-pairing-observation-v1.json")
    private val pendingExportFile get() = File(directory, "ble-pairing-export-pending-v1.json")

    fun loadRecovered(utc: String): PairingObservationReportV1? {
        val file = reportFile
        if (!file.exists()) return null
        val bytes = boundedRead(file)
        val json = JsonParser.parseString(String(bytes, Charsets.UTF_8)).asJsonObject
        require(json["storageSchema"]?.asString == STORAGE_SCHEMA) { "Unbekannte Kopplungsbericht-Version" }
        val report = gson.fromJson(json["report"], PairingObservationReportV1::class.java)
        validate(report)
        val closed = report.closeRecoveredSession(utc)
        save(closed)
        return closed
    }

    fun save(report: PairingObservationReportV1) {
        validate(report)
        val bytes = gson.toJson(mapOf("storageSchema" to STORAGE_SCHEMA, "report" to report)).toByteArray(Charsets.UTF_8)
        atomicWrite(reportFile, bytes)
    }

    fun prepareExport(report: PairingObservationReportV1, utc: String): ByteArray {
        validate(report)
        val bytes = report.export(utc)
        atomicWrite(pendingExportFile, bytes)
        return bytes
    }

    fun pendingExport(): ByteArray? = pendingExportFile.takeIf { it.exists() }?.let { file ->
        boundedRead(file).also { bytes ->
            val json = JsonParser.parseString(String(bytes, Charsets.UTF_8)).asJsonObject
            require(json["schema"]?.asString == "ble_pairing_observation_v1" &&
                json["reportId"]?.asString?.isNotBlank() == true &&
                json["exportedUtc"]?.asString?.isNotBlank() == true &&
                json.has("androidAssessment")) { "Gesicherter Export ungültig" }
        }
    }

    fun clearPendingExport() {
        Files.deleteIfExists(pendingExportFile.toPath())
    }

    private fun validate(report: PairingObservationReportV1) {
        require(report.schema == "ble_pairing_observation_v1") { "Unbekanntes Berichtsformat" }
        require(report.reportId.length in 1..80 && report.startedUtc.length in 1..80) { "Ungültiger Berichtskopf" }
        require(report.events.size <= PairingObservationReportV1.MAX_EVENTS) { "Bericht zu groß" }
        report.events.forEachIndexed { index, event ->
            require(event.sequence == index + 1 && event.recordedUtc.length <= 80 &&
                event.source.length <= 100 && event.kind.length <= 100 && event.value.length <= 256 &&
                (event.targetAddress?.length ?: 0) <= 40) { "Ungültiges Ereignis" }
        }
    }

    private fun boundedRead(file: File): ByteArray {
        require(file.length() in 1..MAX_BYTES.toLong()) { "Gespeicherter Bericht zu groß" }
        return file.inputStream().use { it.readNBytes(MAX_BYTES + 1) }.also {
            require(it.size <= MAX_BYTES) { "Gespeicherter Bericht zu groß" }
        }
    }

    private fun atomicWrite(file: File, bytes: ByteArray) {
        require(bytes.size in 1..MAX_BYTES) { "Bericht zu groß" }
        check(directory.isDirectory || directory.mkdirs()) { "Berichtsverzeichnis fehlt" }
        val temp = File(directory, file.name + ".tmp")
        try {
            FileOutputStream(temp).use { it.write(bytes); it.fd.sync() }
            Files.move(temp.toPath(), file.toPath(), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING)
        } finally { temp.delete() }
    }

    companion object {
        const val STORAGE_SCHEMA = "ble_pairing_journal_v1"
        const val MAX_BYTES = 131072
    }
}
