package de.larpescape.companion.ui.device

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import de.larpescape.companion.data.device.*

@Composable
fun LedControls(state: LedUiState, select: (LedEffect) -> Unit, start: () -> Unit, stop: () -> Unit, importAccess: () -> Unit) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("LEDs", style = MaterialTheme.typography.titleLarge)
            state.accessLabel?.let { Text("Eingerichtete Box: $it") }
            if (state.ledOnly) Text("LED-Lichtsteuerung · Spielintegration folgt getrennt")
            Text(state.message)
            if (state.authenticated && state.current) {
                Text("Box meldet: ${state.active?.label ?: "LEDs aus"}", style = MaterialTheme.typography.titleMedium)
            }
            Column(Modifier.selectableGroup()) {
                LedEffect.entries.forEach { effect ->
                    Row(Modifier.fillMaxWidth().selectable(selected = state.selected == effect,
                        onClick = { select(effect) }, role = Role.RadioButton).padding(vertical = 8.dp),
                        horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        RadioButton(selected = state.selected == effect, onClick = null)
                        Column { Text(effect.label); Text(effect.description, style = MaterialTheme.typography.bodySmall) }
                    }
                }
            }
            Text("Auswahl allein ändert das Licht nicht. Start ersetzt den laufenden Effekt.", style = MaterialTheme.typography.bodySmall)
            if (state.selected == LedEffect.RESONANCE) {
                Text(if (!state.resonanceSupported) "Der Resonanzkreis benötigt die passende neue Box-Firmware."
                    else "Aktive Wake-Bereiche leuchten. Bei vier aktiven Eingängen wird das Licht gemeinsam ruhig. Unbekannte Eingänge bleiben dunkel.",
                    style = MaterialTheme.typography.bodySmall)
            }
            Button(onClick = start, enabled = state.canStart, modifier = Modifier.fillMaxWidth()) { Text("Start") }
            OutlinedButton(onClick = stop, enabled = state.canStop, modifier = Modifier.fillMaxWidth()) { Text("Stop · LEDs aus") }
            Text("Die Box bestätigt die Ausführung. Das sichtbare Licht kannst du am Streifen prüfen.", style = MaterialTheme.typography.bodySmall)
            HorizontalDivider()
            OutlinedButton(onClick = importAccess, modifier = Modifier.fillMaxWidth()) { Text("Box-Zugang importieren") }
            Text("Einrichtungsdatei deiner Box auswählen. Der Zugang bleibt geschützt auf diesem Handy.", style = MaterialTheme.typography.bodySmall)
        }
    }
}
