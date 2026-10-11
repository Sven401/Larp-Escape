package de.larpescape.companion.ui.nfc

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import de.larpescape.protocol.*

/** Validate the editor without truncating or normalizing out-of-range values. */
fun editedNfcData(hash: String, flags: String, history: String, timestamp: Long): NfcTagData {
    val visits = history.lineSequence().filter { it.isNotBlank() }.map { line ->
        val fields = line.split(',').map { it.trim() }
        require(fields.size == 3) { "Pro Besuch: Station, Ergebnis, Minuten" }
        NfcNodeVisit(fields[0].toInt(), fields[1].toInt(), fields[2].toInt())
    }.toList()
    return NfcTagData(hash.trim().toLong(), flags.trim().toInt(), timestamp, visits).also { NfcTagDataCodec.encode(it) }
}

@Composable
fun NfcScreen(state: NfcUiState, prepare: (NfcTagData, Long) -> Unit, cancel: () -> Unit) {
    Text("NFC-Tags", style = MaterialTheme.typography.titleLarge)
    Text(state.message)
    Text("Testformat: GM-Companion. Der gemeinsame Spielobjekt-Vertrag mit dem ESP steht noch aus.", style = MaterialTheme.typography.bodySmall)
    Text("Tag zum Lesen ruhig an die Rückseite des Handys halten. Zum Schreiben anschließend entfernen und erneut anhalten.")
    if (!state.selected) return
    val original = state.data ?: NfcTagData()
    var hash by remember(state.tagId, state.data) { mutableStateOf(original.uidHash.toString()) }
    var flags by remember(state.tagId, state.data) { mutableStateOf(original.flags.toString()) }
    var history by remember(state.tagId, state.data) { mutableStateOf(original.visits.joinToString("\n") { "${it.nodeId}, ${it.result}, ${it.deltaMinutes}" }) }
    var error by remember(state.tagId, state.data) { mutableStateOf<String?>(null) }
    var confirmation by remember(state.selectionId, state.listening) { mutableStateOf<NfcTagData?>(null) }
    Text("Tag ${state.tagId} · ${state.capacity} Byte verfügbar")
    val editing = state.canWrite && !state.busy && !state.armed
    OutlinedTextField(hash, { hash = it }, label = { Text("Kennwert (0–4294967295)") }, enabled = editing, singleLine = true, modifier = Modifier.fillMaxWidth())
    OutlinedTextField(flags, { flags = it }, label = { Text("Flags (0–65535)") }, enabled = editing, singleLine = true, modifier = Modifier.fillMaxWidth())
    OutlinedTextField(history, { history = it }, label = { Text("Stationsbesuche") }, supportingText = { Text("Eine Zeile je Besuch: Station, Ergebnis, Minuten\nStation und Ergebnis: 0–255 · Minuten: 0–65535") }, enabled = editing, minLines = 3, modifier = Modifier.fillMaxWidth())
    Text(if (state.data == null) "Leerer Tag wird im Format des Referenzprojekts angelegt." else "Der vorhandene Zeitstempel bleibt erhalten.", style = MaterialTheme.typography.bodySmall)
    error?.let { Text(it, color = MaterialTheme.colorScheme.error) }
    if (state.armed) OutlinedButton(onClick = cancel) { Text("Schreibauftrag abbrechen") }
    else Button(onClick = {
        try { confirmation = editedNfcData(hash, flags, history, original.timestampBinary); error = null }
        catch (_: NumberFormatException) { error = "Bitte ganze Zahlen innerhalb der angegebenen Bereiche eingeben" }
        catch (e: IllegalArgumentException) { error = e.message }
    }, enabled = editing) { Text("Schreiben vorbereiten") }
    confirmation?.let { data ->
        AlertDialog(onDismissRequest = { confirmation = null }, title = { Text("Tagdaten schreiben?") },
            text = { Text("Die Spiel-Daten auf Tag ${state.tagId} werden durch diese Eingaben ersetzt. Andere Datensätze bleiben erhalten. Halte danach denselben Tag erneut an.") },
            confirmButton = { TextButton(onClick = { confirmation = null; prepare(data, state.selectionId) }) { Text("Schreibauftrag vorbereiten") } },
            dismissButton = { TextButton(onClick = { confirmation = null }) { Text("Abbrechen") } })
    }
}
