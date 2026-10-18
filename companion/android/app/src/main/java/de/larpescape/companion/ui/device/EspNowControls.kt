package de.larpescape.companion.ui.device

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import de.larpescape.companion.data.device.EspNowUiState

@Composable
fun EspNowControls(state: EspNowUiState, selectPeer: (String) -> Unit, send: () -> Unit) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("ESP-NOW testen", style = MaterialTheme.typography.titleLarge)
            Text("Ein Testpaket an den zweiten ESP senden und seine Rückantwort prüfen.")
            OutlinedTextField(value = state.peerText, onValueChange = selectPeer,
                enabled = !state.busy, singleLine = true, modifier = Modifier.fillMaxWidth(),
                label = { Text("MAC des Empfängers") }, placeholder = { Text("AA:BB:CC:DD:EE:FF") },
                isError = state.peerText.isNotBlank() && state.peer == null,
                supportingText = { Text("Die Empfänger-Prüffirmware zeigt diese Adresse beim Start per USB.") })
            Button(onClick = send, enabled = state.canSend, modifier = Modifier.fillMaxWidth()) {
                Text(if (state.waiting) "Warte auf Empfangsbestätigung …" else "Testpaket senden")
            }
            Text(state.message)
            state.result?.let { result ->
                Text("Letzter abgeschlossener Test #${result.led.request}", style = MaterialTheme.typography.titleMedium)
                Text("Senden: ${if (result.accepted) "von der Box an den Funk übergeben" else "nicht bestätigt"}")
                Text("Funkzustellung: ${when(result.link) { 1 -> "bestätigt"; 2 -> "fehlgeschlagen"; else -> "unbekannt" }}")
                Text("Empfang: ${if (result.received) "passende Rückantwort erhalten" else "nicht bestätigt"}")
                if (result.received) Text("Empfangszähler des zweiten ESP: ${result.receiverCount}")
            }
            Text("Jeder Tastendruck startet genau einen Test. Keine automatische Wiederholung.",
                style = MaterialTheme.typography.bodySmall)
        }
    }
}
