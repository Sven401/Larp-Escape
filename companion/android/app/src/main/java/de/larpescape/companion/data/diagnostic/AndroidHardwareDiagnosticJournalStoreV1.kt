package de.larpescape.companion.data.diagnostic

import android.content.Context
import android.util.AtomicFile
import de.larpescape.protocol.DiagnosticJournalV1
import java.io.File
import java.util.UUID

/** Private, atomic storage for a hardware report draft; the guided UI is not wired yet. */
class AndroidHardwareDiagnosticJournalStoreV1(context: Context) {
    private val directory = File(context.filesDir, "hardware-diagnostic-v1")

    fun journal(runId: String): DiagnosticJournalV1 {
        require(UUID.fromString(runId).toString() == runId) { "Canonical run ID required" }
        val file = AtomicFile(File(directory, "$runId.json"))
        return object : DiagnosticJournalV1 {
            override fun load(): ByteArray? {
                if (!file.baseFile.exists() && !File(file.baseFile.path + ".bak").exists()) return null
                return file.openRead().use { input ->
                    input.readNBytes(MAX_BYTES + 1).also { require(it.size <= MAX_BYTES) { "Hardware diagnostic journal exceeds limit" } }
                }
            }

            override fun replace(bytes: ByteArray) {
                require(bytes.size <= MAX_BYTES) { "Hardware diagnostic journal exceeds limit" }
                check(directory.isDirectory || directory.mkdirs()) { "Cannot create diagnostic journal directory" }
                val stream = file.startWrite()
                try {
                    stream.write(bytes)
                    file.finishWrite(stream)
                } catch (error: Exception) {
                    file.failWrite(stream)
                    throw error
                }
            }
        }
    }

    companion object { const val MAX_BYTES = 16 * 1024 * 1024 }
}
