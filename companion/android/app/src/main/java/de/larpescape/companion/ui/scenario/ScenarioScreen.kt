package de.larpescape.companion.ui.scenario

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import de.larpescape.protocol.ScenarioDraftCodec

@Composable
fun ScenarioScreen(state: ScenarioUiState, model: ScenarioViewModel, importFile: () -> Unit, exportFile: () -> Unit) {
    Text("Lokales Szenario", style = MaterialTheme.typography.titleLarge)
    Text("Nur auf diesem Handy. Keine Übertragung zum Spielgerät und keine Gerätefreigabe.", style = MaterialTheme.typography.bodySmall)
    if (state.loading) { CircularProgressIndicator(); return }
    val draft = state.draft
    if (draft == null) {
        state.notice?.let { Text(it, color = MaterialTheme.colorScheme.error) }
        Button(onClick = model::newDraft) { Text("Neuen Entwurf beginnen") }
        return
    }
    Text(if (state.saved) "Gespeichert" else "Ungespeichert", style = MaterialTheme.typography.labelLarge)
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        FilterChip(selected = !state.preview, onClick = { model.showPreview(false) }, label = { Text("Bearbeiten") })
        FilterChip(selected = state.preview, onClick = { model.showPreview(true) }, label = { Text("Leseprobe") })
    }
    if (state.preview) {
        val preview = ScenarioDraftCodec.preview(draft)
        Text(preview.title, style = MaterialTheme.typography.headlineSmall)
        Text("${preview.sceneCount} ${if (preview.sceneCount == 1) "Szene" else "Szenen"} · ${preview.noteCount} ${if (preview.noteCount == 1) "offene Angabe" else "offene Angaben"}")
        Text("Redaktionelle Leseprobe · kein Gerätetest", style = MaterialTheme.typography.bodySmall)
        preview.scenes.forEach { scene ->
            Card(Modifier.fillMaxWidth()) { Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("${scene.number}. ${scene.title}", style = MaterialTheme.typography.titleMedium)
                Text("Ziel: ${scene.goal ?: "Noch nicht beschrieben"}")
                Text("Hinweis: ${scene.clue ?: "Noch nicht beschrieben"}")
                if (scene.missing.isNotEmpty()) Text("Offen: ${scene.missing.joinToString(" und ")}")
            } }
        }
    } else {
        OutlinedTextField(draft.title, model::title, label = { Text("Szenarioname") }, modifier = Modifier.fillMaxWidth(),
            supportingText = { Text("Höchstens 80 Zeichen") }, singleLine = true)
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("Szenen ${draft.scenes.size}/24", style = MaterialTheme.typography.titleMedium)
            Button(onClick = model::addScene, enabled = draft.scenes.size < 24) { Text("Szene hinzufügen") }
        }
        draft.scenes.forEachIndexed { index, scene ->
            var confirmRemove by remember(scene.id) { mutableStateOf(false) }
            Card(Modifier.fillMaxWidth()) { Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Szene ${index + 1}", style = MaterialTheme.typography.titleMedium)
                OutlinedTextField(scene.title, { model.scene(scene.id) { old -> old.copy(title = it) } },
                    label = { Text("Titel") }, modifier = Modifier.fillMaxWidth(), singleLine = true)
                OutlinedTextField(scene.goal, { model.scene(scene.id) { old -> old.copy(goal = it) } },
                    label = { Text("Ziel der Szene") }, modifier = Modifier.fillMaxWidth(), minLines = 2)
                OutlinedTextField(scene.clue, { model.scene(scene.id) { old -> old.copy(clue = it) } },
                    label = { Text("Hinweis für Spielende") }, modifier = Modifier.fillMaxWidth(), minLines = 2)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    TextButton(onClick = { model.move(scene.id, -1) }, enabled = index > 0) { Text("Nach oben") }
                    TextButton(onClick = { model.move(scene.id, 1) }, enabled = index < draft.scenes.lastIndex) { Text("Nach unten") }
                    TextButton(onClick = { confirmRemove = true }) { Text("Entfernen") }
                }
            } }
            if (confirmRemove) AlertDialog(onDismissRequest = { confirmRemove = false },
                title = { Text("Szene entfernen?") }, text = { Text("Die Szene wird aus dem aktuellen Entwurf entfernt.") },
                confirmButton = { TextButton(onClick = { model.remove(scene.id); confirmRemove = false }) { Text("Entfernen") } },
                dismissButton = { TextButton(onClick = { confirmRemove = false }) { Text("Abbrechen") } })
        }
    }
    state.notice?.let { Text(it, color = MaterialTheme.colorScheme.primary) }
    Button(onClick = model::save, enabled = !state.busy, modifier = Modifier.fillMaxWidth()) { Text(if (state.busy) "Speichert …" else "Speichern") }
    OutlinedButton(onClick = exportFile, modifier = Modifier.fillMaxWidth()) { Text("JSON exportieren") }
    OutlinedButton(onClick = importFile, modifier = Modifier.fillMaxWidth()) { Text("JSON importieren") }
}
