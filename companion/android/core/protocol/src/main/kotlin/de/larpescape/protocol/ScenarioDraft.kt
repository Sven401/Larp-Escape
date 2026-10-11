package de.larpescape.protocol

import com.google.gson.JsonParser
import com.google.gson.JsonObject
import com.google.gson.JsonParseException
import java.nio.charset.CodingErrorAction
import java.nio.ByteBuffer
import java.nio.charset.StandardCharsets

data class ScenarioScene(val id: String, val title: String = "Neue Szene", val goal: String = "", val clue: String = "")
data class ScenarioDraft(val id: String, val title: String = "Neues Szenario", val scenes: List<ScenarioScene> = emptyList(), val version: Int = 1)
data class PreviewScene(val id: String, val number: Int, val title: String, val goal: String?, val clue: String?, val missing: List<String>)
data class ScenarioPreview(val title: String, val sceneCount: Int, val noteCount: Int, val scenes: List<PreviewScene>)

object ScenarioDraftCodec {
    const val MAX_BYTES = 128 * 1024
    fun validate(draft: ScenarioDraft, complete: Boolean = false): ScenarioDraft {
        require(draft.version == 1) { "Diese Entwurfsversion wird nicht unterstützt." }
        require(draft.id.isNotBlank() && draft.id.length <= 100) { "Entwurfs-ID muss 1 bis 100 Zeichen enthalten." }
        require(draft.title.length <= 80 && (!complete || draft.title.isNotBlank())) { "Szenarioname muss 1 bis 80 Zeichen enthalten." }
        require(draft.scenes.size <= 24) { "Ein Entwurf darf höchstens 24 Szenen enthalten." }
        require(draft.scenes.map { it.id }.toSet().size == draft.scenes.size) { "Szenen-IDs müssen eindeutig sein." }
        draft.scenes.forEach { scene ->
            require(scene.id.isNotBlank() && scene.id.length <= 100) { "Szenen-ID muss 1 bis 100 Zeichen enthalten." }
            require(scene.title.length <= 80 && (!complete || scene.title.isBlank().not())) { "Szenentitel muss 1 bis 80 Zeichen enthalten." }
            require(scene.goal.length <= 600) { "goal darf höchstens 600 Zeichen enthalten." }
            require(scene.clue.length <= 600) { "clue darf höchstens 600 Zeichen enthalten." }
        }
        return draft
    }

    private fun JsonObject.text(name: String): String {
        val value = get(name)
        require(value != null && value.isJsonPrimitive && value.asJsonPrimitive.isString) { "$name fehlt oder ist ungültig." }
        return value.asString
    }

    fun parse(bytes: ByteArray): ScenarioDraft {
        require(bytes.size <= MAX_BYTES) { "Entwurfsdatei ist zu groß." }
        val json = try { StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
            .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes)).toString() }
            catch (_: Exception) { throw IllegalArgumentException("Ungültiges UTF-8 im Entwurf.") }
        val root = try { JsonParser.parseString(json) }
            catch (_: JsonParseException) { throw IllegalArgumentException("Ungültiges Entwurfs-JSON.") }
        require(root.isJsonObject) { "Ungültiger Entwurf." }
        val obj = root.asJsonObject
        val version = obj.get("version")
        require(version != null && version.isJsonPrimitive && version.asJsonPrimitive.isNumber && version.asString == "1") { "Diese Entwurfsversion wird nicht unterstützt." }
        val array = obj.get("scenes")
        require(array != null && array.isJsonArray && array.asJsonArray.size() <= 24) { "Ungültige Szenenliste." }
        val scenes = array.asJsonArray.map { item ->
            require(item.isJsonObject) { "Ungültige Szene." }
            item.asJsonObject.let { ScenarioScene(it.text("id"), it.text("title"), it.text("goal"), it.text("clue")) }
        }
        return validate(ScenarioDraft(obj.text("id"), obj.text("title"), scenes), complete = true)
    }

    fun serialize(draft: ScenarioDraft): ByteArray {
        validate(draft, complete = true)
        val root = JsonObject().apply {
            addProperty("version", 1); addProperty("id", draft.id); addProperty("title", draft.title)
            add("scenes", com.google.gson.JsonArray().apply {
                draft.scenes.forEach { scene -> add(JsonObject().apply {
                    addProperty("id", scene.id); addProperty("title", scene.title)
                    addProperty("goal", scene.goal); addProperty("clue", scene.clue)
                }) }
            })
        }
        return (com.google.gson.GsonBuilder().setPrettyPrinting().create().toJson(root) + "\n").toByteArray(Charsets.UTF_8)
            .also { require(it.size <= MAX_BYTES) { "Entwurfsdatei ist zu groß." } }
    }

    fun preview(draft: ScenarioDraft): ScenarioPreview {
        validate(draft)
        val scenes = draft.scenes.mapIndexed { index, scene ->
            val title = scene.title.trim(); val goal = scene.goal.trim(); val clue = scene.clue.trim()
            PreviewScene(scene.id, index + 1, title.ifEmpty { "Szene ${index + 1} ohne Titel" },
                goal.ifEmpty { null }, clue.ifEmpty { null },
                listOfNotNull(if (title.isEmpty()) "Titel" else null, if (goal.isEmpty()) "Ziel" else null, if (clue.isEmpty()) "Hinweis" else null))
        }
        return ScenarioPreview(draft.title.trim().ifEmpty { "Unbenanntes Szenario" }, scenes.size,
            (if (draft.title.isBlank()) 1 else 0) + scenes.sumOf { it.missing.size }, scenes)
    }
}
