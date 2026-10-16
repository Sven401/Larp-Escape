package de.larpescape.companion.ui.device

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import de.larpescape.companion.data.device.AudioUiState

@Composable
fun AudioControls(state: AudioUiState, selectTrack: (String) -> Unit, selectVolume: (String) -> Unit,
    start: () -> Unit, stop: () -> Unit, action: (Int) -> Unit) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("Audio testen", style = MaterialTheme.typography.titleLarge)
            Text(state.message)
            OutlinedTextField(value = state.trackText,
                onValueChange = { if (it.length <= 3 && it.all(Char::isDigit)) selectTrack(it) },
                label = { Text("Tracknummer · 1 bis 255") }, singleLine = true,
                isError = state.selectedTrack == null,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number), modifier = Modifier.fillMaxWidth())
            OutlinedTextField(value = state.volumeText,
                onValueChange = { if (it.length <= 2 && it.all(Char::isDigit)) selectVolume(it) },
                label = { Text("Testlautstärke · 1 bis 10") }, singleLine = true,
                isError = state.selectedVolume == null,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number), modifier = Modifier.fillMaxWidth())
            Text("Datei im Ordner /mp3 mit vierstelliger Nummer, z. B. /mp3/0100… · Inhalt beim Test bestätigen.",
                style = MaterialTheme.typography.bodySmall)
            Text("Auswahl allein sendet nichts. Nach spätestens 30 Sekunden fordert die Box Stop an.",
                style = MaterialTheme.typography.bodySmall)
            Button(onClick = start, enabled = state.canStart, modifier = Modifier.fillMaxWidth()) { Text("Play · Titel starten") }
            OutlinedButton(onClick = stop, enabled = state.canStop, modifier = Modifier.fillMaxWidth()) { Text("Stop · Audio") }
            HorizontalDivider()
            Text("Boxmeldung und Höreindruck", style = MaterialTheme.typography.titleMedium)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = { action(3) }, enabled = state.canPause) { Text("Pause") }
                OutlinedButton(onClick = { action(4) }, enabled = state.canResume) { Text("Fortsetzen") }
            }
            OutlinedButton(onClick = { action(5) }, enabled = state.canSetVolume) { Text("Lautstärke übernehmen") }
            if (state.paused) Text("Box meldet: Pauseauftrag übertragen · Hörposition prüfen")
            Text(when (state.active) {
                true -> "Box meldet: Startauftrag aktiv · Track ${state.observedTrack ?: "unbekannt"} · Lautstärke ${state.observedVolume ?: "unbekannt"}"
                false -> "Box meldet: kein aktiver Startauftrag"
                null -> "Startauftrag: nicht aktuell bestätigt"
            })
            Text(when (state.busyLow) {
                true -> "BUSY-Eingang: LOW · Rohpegel"
                false -> "BUSY-Eingang: HIGH · Rohpegel"
                null -> "BUSY: unbekannt oder nicht aktuell"
            })
            Text("Der Pegel allein bestätigt keinen hörbaren Ton.", style = MaterialTheme.typography.bodySmall)
            Text("Höreindruck: offen · Ob der richtige Titel hörbar ist, prüfen wir gemeinsam am Lautsprecher.",
                style = MaterialTheme.typography.bodySmall)
        }
    }
}
