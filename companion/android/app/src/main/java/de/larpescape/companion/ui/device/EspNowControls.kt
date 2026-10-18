package de.larpescape.companion.ui.device

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import de.larpescape.companion.data.device.EspNowUiState

@Composable
fun EspNowControls(state: EspNowUiState, selectPeer: (String) -> Unit, send: () -> Unit, measure: (Boolean) -> Unit, stop: () -> Unit) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("ESP-NOW testen", style = MaterialTheme.typography.titleLarge)
            Text("Ein Testpaket an den zweiten ESP senden und seine Rückantwort prüfen.")
            OutlinedTextField(value = state.peerText, onValueChange = selectPeer,
                enabled = !state.busy && !state.measuring, singleLine = true, modifier = Modifier.fillMaxWidth(),
                label = { Text("MAC des Empfängers") }, placeholder = { Text("AA:BB:CC:DD:EE:FF") },
                isError = state.peerText.isNotBlank() && state.peer == null,
                supportingText = { Text("Die Empfänger-Prüffirmware zeigt diese Adresse beim Start per USB.") })
            Button(onClick = send, enabled = state.canSend, modifier = Modifier.fillMaxWidth()) {
                Text(if (state.waiting) "Warte auf Empfangsbestätigung …" else "Testpaket senden")
            }
            Text(state.message)
            HorizontalDivider()
            Text("Signalstärke und Funkeln", style = MaterialTheme.typography.titleMedium)
            Text("Misst die Rückantwort des zweiten ESPs an der Box. Keine Entfernungsmessung in Metern.")
            Button(onClick = { measure(false) }, enabled = state.canSend && state.rssiSupported) { Text("Signalstärke messen") }
            Button(onClick = { measure(true) }, enabled = state.canSend && state.rssiSupported) { Text("Messen und Funkeln steuern") }
            Button(onClick = stop, enabled = state.measuring) { Text("Messung beenden") }
            if (!state.rssiSupported) Text("Für RSSI wird die passende neue Box-Firmware benötigt.")
            if (state.hasJournal) Text("Gespeicherte Messpunkte: ${state.savedSamples} · Protokoll bleibt auf dem Handy erhalten")
            if (state.measuring) Text("Messung läuft · neue Probe etwa alle 0,5 Sekunden")
            state.rssiResult?.let { result ->
                if (state.rssiCurrent && result.rssi != null) {
                    Text("Signalstärke: ${result.rssi} dBm")
                    Text("Geglättet: ${result.filteredRssi} dBm")
                    Text("Messwertalter am Handy: ${state.rssiAgeMillis ?: 0} ms")
                    Text("Funkel-Zielintensität: ${result.intensity} % der bisherigen Helligkeitsgrenze")
                    Text("Messauftrag #${result.led.request} · Empfangszähler ${result.receiverCount}")
                } else Text("Signalstärke unbekannt · keine aktuelle passende Rückantwort")
            }
            Text("Stärkeres Signal = heller. Zwischen −90 und −40 dBm steigt die Intensität von 20 auf 100 %. Körper, Wände und Ausrichtung beeinflussen das Signal. Ohne frische Antwort fällt gesteuertes Funkeln auf 20 % zurück.")
            state.result?.let { result ->
                Text("Letzter abgeschlossener Test #${result.led.request}", style = MaterialTheme.typography.titleMedium)
                Text("Senden: ${if (result.accepted) "von der Box an den Funk übergeben" else "nicht bestätigt"}")
                Text("Funkzustellung: ${when(result.link) { 1 -> "bestätigt"; 2 -> "fehlgeschlagen"; else -> "unbekannt" }}")
                Text("Empfang: ${if (result.received) "passende Rückantwort erhalten" else "nicht bestätigt"}")
                if (result.received) Text("Empfangszähler des zweiten ESP: ${result.receiverCount}")
            }
            Text("Die Einzelsendetaste sendet genau ein Testpaket. Eine laufende Messung sendet weitere Proben bis zum Beenden.",
                style = MaterialTheme.typography.bodySmall)
        }
    }
}
