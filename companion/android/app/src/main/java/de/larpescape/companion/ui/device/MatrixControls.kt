package de.larpescape.companion.ui.device

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.TextButton
import androidx.compose.material3.AlertDialog
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.unit.dp
import de.larpescape.companion.data.device.MatrixUiState
import de.larpescape.companion.data.device.matrixCoordinate
import de.larpescape.companion.data.device.MatrixCollection
import de.larpescape.companion.data.device.MatrixCollectionRecorder
import de.larpescape.companion.data.device.MatrixRunPhase

@Composable
fun MatrixControls(state: MatrixUiState, collection: MatrixCollection = MatrixCollection(),
                   action: (String) -> Unit = {}, export: () -> Unit = {}) {
    var confirm by remember { mutableStateOf(false) }
    if (confirm) AlertDialog(onDismissRequest = { confirm = false },
        title = { Text("Unbestückte Koordinaten deaktivieren?") },
        text = { Text("Ich habe alle vorhandenen Kontakte durchprobiert und jeder hat einen gespeicherten Treffer erzeugt. " +
            "Die übrigen ${collection.unseen.size} Koordinaten besitzen keinen Kontakt. " +
            "Falls ein vorhandener Kontakt ohne Reaktion blieb, stattdessen nachprüfen.") },
        confirmButton = { TextButton(onClick = { confirm = false; action("confirm") }) { Text("Gesamtbestätigung speichern") } },
        dismissButton = { TextButton(onClick = { confirm = false }) { Text("Noch offen") } })
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("Tastenmatrix · Sammeldurchgang", style = MaterialTheme.typography.titleLarge)
            Text(state.message)
            Text(collection.notice)
            Text("Alle vorhandenen Kontakte nacheinander mindestens 1 Sekunde halten, dann mindestens 1 Sekunde lösen. " +
                "Reihenfolge und Wiederholungen sind egal. Zum Schluss einmal Bescheid geben.",
                style = MaterialTheme.typography.bodyMedium)
            Text("Gesammelt: ${collection.hits.size} von 81 Koordinaten · ${collection.hits.values.sumOf { it.count }} Betätigungen",
                style = MaterialTheme.typography.titleMedium)
            when (collection.phase) {
                MatrixRunPhase.IDLE, MatrixRunPhase.CONFIRMED -> Button(onClick = { action("start") },
                    enabled = collection.device.isNotEmpty() && MatrixCollectionRecorder.atRest(state)) { Text("Neuen Sammeldurchgang starten") }
                MatrixRunPhase.RECORDING -> Button(onClick = { action("finish") }) { Text("Durchgang abschließen") }
                MatrixRunPhase.PAUSED -> {
                    Button(onClick = { action("resume") }, enabled = MatrixCollectionRecorder.atRest(state)) { Text("Sammlung fortsetzen") }
                    OutlinedButton(onClick = { action("finish") }) { Text("Durchgang abschließen") }
                }
                MatrixRunPhase.REVIEW -> {
                    Text("Noch offen: ${collection.unseen.sorted().joinToString(", ", transform = ::matrixCoordinate)}")
                    Button(onClick = { confirm = true }, enabled = collection.hits.isNotEmpty()) { Text("Übrige sind unbestückt") }
                    OutlinedButton(onClick = { action("start") }, enabled = MatrixCollectionRecorder.atRest(state)) {
                        Text("Neuen Lauf starten · bisherigen aufbewahren")
                    }
                }
            }
            if (collection.phase in setOf(MatrixRunPhase.REVIEW, MatrixRunPhase.CONFIRMED)) {
                OutlinedButton(onClick = { action("reopen") }) { Text("Kontakte nachprüfen") }
            }
            if (collection.run.isNotEmpty()) {
                OutlinedButton(onClick = export) { Text("Sammelbericht exportieren") }
                Text("Gespeicherte Treffer: ${collection.coordinates.ifEmpty { "keine" }}")
                Text("Unterbrechungen: ${collection.interruptions} · übersprungene Mehrfachkontakte: ${collection.rejectedSamples}")
            }
            MatrixGrid(state, collection)
            val active = state.observedCoordinates
            Text(when {
                !state.current || !state.supported || state.pressed == null -> "Beobachtete Kontakte: unbekannt"
                state.ambiguous -> "Beobachtete Kontakte: ${active.joinToString(", ")} (mehrdeutig)"
                active.isEmpty() -> "Beobachtete Kontakte: keine"
                else -> "Beobachtete Kontakte: ${active.joinToString(", ")}"
            }, style = MaterialTheme.typography.titleMedium)
            Text("● gerade aktiv · ✓ gespeichert · × deaktiviert · – noch offen · ? live unbekannt. Seitlich wischen für weitere Spalten.",
                style = MaterialTheme.typography.bodySmall)
            Text("Zeilen 1–9, Spalten A–I. Elektrische Pinzuordnung und Zuordnung zu den physischen Tasten " +
                "sind vorläufig und werden im gemeinsamen Test geprüft.", style = MaterialTheme.typography.bodySmall)
        }
    }
}

@Composable
private fun MatrixGrid(state: MatrixUiState, collection: MatrixCollection) {
    val density = LocalDensity.current
    BoxWithConstraints(Modifier.fillMaxWidth()) {
        // Preserve legibility on a phone and with large system text; larger screens use their width.
        val minimum = 40.dp * density.fontScale.coerceAtLeast(1f)
        val cellSize = ((maxWidth - 36.dp) / 10).coerceAtLeast(minimum)
        Row(Modifier.horizontalScroll(rememberScrollState())) {
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    Box(Modifier.size(cellSize))
                    ('A'..'I').forEach { label ->
                        Box(Modifier.size(cellSize), contentAlignment = Alignment.Center) {
                            Text(label.toString(), style = MaterialTheme.typography.titleSmall)
                        }
                    }
                }
                repeat(9) { row ->
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        Box(Modifier.size(cellSize), contentAlignment = Alignment.Center) {
                            Text((row + 1).toString(), style = MaterialTheme.typography.titleSmall)
                        }
                        repeat(9) { column ->
                            val index = row * 9 + column
                            val active = state.active(index)
                            val seen = index in collection.hits
                            val disabled = collection.phase == MatrixRunPhase.CONFIRMED && index in collection.disabled
                            val colors = MaterialTheme.colorScheme
                            Surface(
                                modifier = Modifier.size(cellSize).clearAndSetSemantics {
                                    contentDescription = "Matrix ${matrixCoordinate(index)}"
                                    stateDescription = when {
                                        disabled -> "Deaktiviert · unbestückt bestätigt"
                                        seen -> "Im Sammeldurchgang gespeichert · ${collection.hits[index]?.count} Betätigungen"
                                        else -> when (active) {
                                        true -> if (state.ambiguous) "Kontakt beobachtet · Zuordnung unklar" else "Kontakt beobachtet"
                                        false -> "Kein Kontakt beobachtet"
                                        null -> "Unbekannt"
                                        }
                                    }
                                },
                                shape = RoundedCornerShape(6.dp),
                                color = when {
                                    disabled -> colors.surfaceContainerHighest
                                    active == true -> colors.primaryContainer
                                    seen -> colors.tertiaryContainer
                                    else -> colors.surfaceContainerLow
                                },
                                contentColor = if (active == true) colors.onPrimaryContainer else colors.onSurface,
                                border = BorderStroke(if (active == true) 2.dp else 1.dp,
                                    if (active == true) colors.primary else colors.outline),
                            ) {
                                Box(contentAlignment = Alignment.Center) {
                                    Text(when { disabled -> "×"; active == true -> "●"; seen -> "✓"; active == null -> "?"; else -> "–" },
                                        style = MaterialTheme.typography.titleMedium)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
