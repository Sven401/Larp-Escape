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

@Composable
fun MatrixControls(state: MatrixUiState) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("Tastenmatrix · gemeinsamer Test", style = MaterialTheme.typography.titleLarge)
            Text(state.message)
            Text("Jeweils eine einzelne Taste mindestens 1 Sekunde halten, dann loslassen. " +
                "Die angezeigte Koordinate zusammen mit der Taste an der Box notieren.",
                style = MaterialTheme.typography.bodyMedium)
            MatrixGrid(state)
            val active = state.observedCoordinates
            Text(when {
                !state.current || !state.supported || state.pressed == null -> "Beobachtete Kontakte: unbekannt"
                state.ambiguous -> "Beobachtete Kontakte: ${active.joinToString(", ")} (mehrdeutig)"
                active.isEmpty() -> "Beobachtete Kontakte: keine"
                else -> "Beobachtete Kontakte: ${active.joinToString(", ")}"
            }, style = MaterialTheme.typography.titleMedium)
            Text("● Kontakt · – frei · ? unbekannt. Bei Bedarf seitlich durch das Raster wischen.",
                style = MaterialTheme.typography.bodySmall)
            Text("Zeilen 1–9, Spalten A–I. Elektrische Pinzuordnung und Zuordnung zu den physischen Tasten " +
                "sind vorläufig und werden im gemeinsamen Test geprüft.", style = MaterialTheme.typography.bodySmall)
        }
    }
}

@Composable
private fun MatrixGrid(state: MatrixUiState) {
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
                            val colors = MaterialTheme.colorScheme
                            Surface(
                                modifier = Modifier.size(cellSize).clearAndSetSemantics {
                                    contentDescription = "Matrix ${matrixCoordinate(index)}"
                                    stateDescription = when (active) {
                                        true -> if (state.ambiguous) "Kontakt beobachtet · Zuordnung unklar" else "Kontakt beobachtet"
                                        false -> "Kein Kontakt beobachtet"
                                        null -> "Unbekannt"
                                    }
                                },
                                shape = RoundedCornerShape(6.dp),
                                color = when (active) {
                                    true -> colors.primaryContainer
                                    false -> colors.surfaceContainerLow
                                    null -> colors.surfaceContainerHighest
                                },
                                contentColor = if (active == true) colors.onPrimaryContainer else colors.onSurface,
                                border = BorderStroke(if (active == true) 2.dp else 1.dp,
                                    if (active == true) colors.primary else colors.outline),
                            ) {
                                Box(contentAlignment = Alignment.Center) {
                                    Text(when (active) { true -> "●"; false -> "–"; null -> "?" },
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
