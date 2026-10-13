package de.larpescape.companion.ui.scenario

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import de.larpescape.companion.data.scenario.ScenarioDraftRepository
import de.larpescape.protocol.*
import java.util.UUID
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

data class ScenarioUiState(val draft: ScenarioDraft? = null, val saved: Boolean = false,
    val loading: Boolean = true, val busy: Boolean = false, val notice: String? = null, val preview: Boolean = false)

class ScenarioViewModel(private val repository: ScenarioDraftRepository) : ViewModel() {
    private val mutable = MutableStateFlow(ScenarioUiState())
    val state = mutable.asStateFlow()
    private var revision = 0L
    private var pendingExport: ByteArray? = null

    init { viewModelScope.launch {
        try {
            val loaded = withContext(Dispatchers.IO) { repository.load() }
            mutable.value = ScenarioUiState(loaded ?: fresh(), loaded != null, loading = false)
        } catch (error: Exception) {
            mutable.value = ScenarioUiState(loading = false,
                notice = "Gespeicherter Entwurf konnte nicht geöffnet werden: ${error.message}")
        }
    } }

    private fun fresh() = ScenarioDraft(UUID.randomUUID().toString())
    fun newDraft() { revision++; mutable.value = ScenarioUiState(fresh(), loading = false,
        notice = "Neuer Entwurf geöffnet. Bisheriger Speicherstand bleibt bis zum Speichern erhalten.") }
    fun showPreview(value: Boolean) { mutable.value = mutable.value.copy(preview = value) }
    private fun edit(next: ScenarioDraft) {
        try { ScenarioDraftCodec.validate(next); revision++; mutable.value = mutable.value.copy(draft = next, saved = false, notice = null) }
        catch (error: Exception) { mutable.value = mutable.value.copy(notice = error.message) }
    }
    fun title(value: String) { state.value.draft?.let { edit(it.copy(title = value)) } }
    fun addScene() { state.value.draft?.let { edit(it.copy(scenes = it.scenes + ScenarioScene(UUID.randomUUID().toString()))) } }
    fun scene(id: String, update: (ScenarioScene) -> ScenarioScene) { state.value.draft?.let { draft ->
        edit(draft.copy(scenes = draft.scenes.map { if (it.id == id) update(it) else it }))
    } }
    fun remove(id: String) { state.value.draft?.let { edit(it.copy(scenes = it.scenes.filterNot { scene -> scene.id == id })) } }
    fun move(id: String, offset: Int) { state.value.draft?.let { draft ->
        val index = draft.scenes.indexOfFirst { it.id == id }; val target = index + offset
        if (index >= 0 && target in draft.scenes.indices) edit(draft.copy(scenes = draft.scenes.toMutableList().apply { add(target, removeAt(index)) }))
    } }
    fun import(bytes: ByteArray) {
        try { edit(ScenarioDraftCodec.parse(bytes)); mutable.value = mutable.value.copy(notice = "Entwurf geladen. Zum Behalten bitte speichern.") }
        catch (error: Exception) { mutable.value = mutable.value.copy(notice = "Import fehlgeschlagen: ${error.message}") }
    }
    fun exportBytes(): ByteArray? = try { state.value.draft?.let { ScenarioDraftCodec.serialize(it) } }
        catch (error: Exception) { mutable.value = mutable.value.copy(notice = error.message); null }
    fun beginExport(): Boolean { pendingExport = exportBytes(); return pendingExport != null }
    fun takeExport(): ByteArray? = pendingExport.also { pendingExport = null }
    fun notice(message: String) { mutable.value = mutable.value.copy(notice = message) }
    fun save() {
        val draft = state.value.draft ?: return
        if (state.value.busy) return
        val bytes = try { ScenarioDraftCodec.serialize(draft) } catch (error: Exception) {
            mutable.value = mutable.value.copy(notice = error.message); return
        }
        val captured = revision
        mutable.value = mutable.value.copy(busy = true)
        viewModelScope.launch {
            try {
                withContext(Dispatchers.IO) { repository.save(ScenarioDraftCodec.parse(bytes)) }
                mutable.value = mutable.value.copy(busy = false, saved = captured == revision,
                    notice = if (captured == revision) "Entwurf auf diesem Gerät gespeichert." else "Änderungen nach dem Speichern sind noch ungespeichert.")
            } catch (error: Exception) {
                mutable.value = mutable.value.copy(busy = false, saved = false,
                    notice = "Speichern fehlgeschlagen: ${error.message}")
            }
        }
    }
}
