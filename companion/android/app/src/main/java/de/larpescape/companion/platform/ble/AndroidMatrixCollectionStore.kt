package de.larpescape.companion.platform.ble

import android.content.Context
import android.util.AtomicFile
import de.larpescape.companion.data.device.*
import java.io.File
import java.io.FileNotFoundException

/** Private, crash-safe files. A new run creates a new file; older evidence is retained. */
internal class AndroidMatrixCollectionStore(context: Context) : MatrixCollectionStore {
    private val directory = File(context.noBackupFilesDir, "matrix-collection")
    private fun atomic(name: String) = AtomicFile(File(directory, name))
    private fun read(file: AtomicFile): String? = try {
        file.openRead().use { stream ->
            val bytes = stream.readNBytes(32769)
            check(bytes.size <= 32768)
            bytes.toString(Charsets.UTF_8)
        }
    } catch (_: FileNotFoundException) { null }

    override fun load(device: String): MatrixCollection? {
        val run = read(atomic("$device-current")) ?: return null
        check(run.matches(Regex("[0-9a-f-]{36}")))
        val value = MatrixCollectionCodec.decode(read(atomic("$run.properties")) ?: error("Lauf fehlt"))
        check(value.device == device && value.run == run)
        return value
    }

    private fun write(file: AtomicFile, text: String) {
        val stream = file.startWrite()
        try { stream.write(text.toByteArray(Charsets.UTF_8)); file.finishWrite(stream) }
        catch (error: Exception) { file.failWrite(stream); throw error }
    }

    override fun save(value: MatrixCollection) {
        check(directory.isDirectory || directory.mkdirs())
        write(atomic("${value.run}.properties"), MatrixCollectionCodec.encode(value))
        val pointer = atomic("${value.device}-current")
        if (read(pointer) != value.run) write(pointer, value.run)
    }
}
