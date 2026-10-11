package de.larpescape.companion.data.scenario

import android.content.Context
import android.util.AtomicFile
import de.larpescape.protocol.ScenarioDraft
import de.larpescape.protocol.ScenarioDraftCodec
import java.io.File

interface ScenarioDraftRepository {
    fun load(): ScenarioDraft?
    fun save(draft: ScenarioDraft)
}

class LocalScenarioDraftRepository(context: Context) : ScenarioDraftRepository {
    private val file = AtomicFile(File(context.filesDir, "scenario-draft-v1.json"))

    override fun load(): ScenarioDraft? {
        if (!file.baseFile.exists() && !File(file.baseFile.path + ".bak").exists()) return null
        val bytes = file.openRead().use { input ->
            val bounded = input.readNBytes(ScenarioDraftCodec.MAX_BYTES + 1)
            require(bounded.size <= ScenarioDraftCodec.MAX_BYTES) { "Gespeicherter Entwurf ist zu groß." }
            bounded
        }
        return ScenarioDraftCodec.parse(bytes)
    }

    override fun save(draft: ScenarioDraft) {
        val bytes = ScenarioDraftCodec.serialize(draft)
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
