package de.larpescape.companion.data.diagnostic

import android.content.Context
import android.util.AtomicFile
import de.larpescape.protocol.DiagnosticJournalV1
import java.io.File
import java.util.UUID

interface DiagnosticRunStore {
    fun latest(): DiagnosticJournalV1?
    fun create(): DiagnosticJournalV1
}

/** Each explicit run has its own journal; selecting a new run preserves previous files. */
class AndroidDiagnosticRunStore(context: Context) : DiagnosticRunStore {
    private val directory = File(context.filesDir, "diagnostic-simulation-v1")
    private val pointer = AtomicFile(File(directory, "latest.txt"))
    private fun read(file: AtomicFile, limit: Int): ByteArray? {
        if (!file.baseFile.exists() && !File(file.baseFile.path + ".bak").exists()) return null
        return file.openRead().use { it.readNBytes(limit + 1) }.also { require(it.size <= limit) { "Diagnosedatei ist zu groß" } }
    }
    private fun write(file: AtomicFile, bytes: ByteArray) {
        directory.mkdirs()
        val stream = file.startWrite()
        try { stream.write(bytes); file.finishWrite(stream) } catch (error: Exception) { file.failWrite(stream); throw error }
    }
    private fun journal(name: String, select: Boolean): DiagnosticJournalV1 {
        require(Regex("[0-9a-f-]{36}\\.json").matches(name))
        val file = AtomicFile(File(directory, name))
        var selected = !select
        return object : DiagnosticJournalV1 {
            override fun load() = read(file, 1024 * 1024)
            override fun replace(bytes: ByteArray) {
                require(bytes.size <= 1024 * 1024)
                write(file, bytes)
                if (!selected) { write(pointer, name.toByteArray(Charsets.US_ASCII)); selected = true }
            }
        }
    }
    override fun latest(): DiagnosticJournalV1? = read(pointer, 64)?.let { journal(it.toString(Charsets.US_ASCII), false) }
    override fun create(): DiagnosticJournalV1 = journal(UUID.randomUUID().toString() + ".json", true)
}
