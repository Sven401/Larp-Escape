package de.larpescape.companion.ui.device

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import de.larpescape.companion.data.device.*

@Composable
fun NearGlowControls(state: NearGlowUiState, importAccess: (Int) -> Unit,
    connect: (Int) -> Unit, disconnect: (Int) -> Unit,
    edit: (Int, String, String) -> Unit, applyBoth: () -> Unit) {
    Text("Näheglut live", style = MaterialTheme.typography.titleLarge)
    if (state.permissionMissing) Text("Bluetoothzugriff für Geräte in der Nähe erlauben.")
    state.stations.forEachIndexed { index, station ->
        var details by rememberSaveable(index, station.mac) { mutableStateOf(false) }
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    Text(station.label, style = MaterialTheme.typography.titleMedium, modifier = Modifier.weight(1f))
                    TextButton(onClick = { details = !details }, modifier = Modifier.heightIn(min = 48.dp)) {
                        Text(if (details) "Schließen" else if (station.mac == null) "Einrichten" else "Details")
                    }
                }
                val reading = station.reading
                if (station.authenticated && reading?.handsetFresh(state.now) == true) {
                    Text(when {
                        reading.fault != 0 || reading.configFault != 0 -> "Handy verbunden · Gerätefehler"
                        !reading.armed -> "Handy verbunden · Station nicht freigegeben"
                        reading.sourceFresh(state.now) -> "Handy verbunden · Partner aktuell"
                        else -> "Handy verbunden · Partnerfunk fehlt"
                    })
                    val rssi = if (reading.sourceFresh(state.now) && reading.rssi != null) "${reading.rssi} dBm" else "unbekannt"
                    Text("Funknähe $rssi · Glut-Ziel ${reading.level / 10f} %")
                    if (reading.outputKnown) {
                        val color = Color(reading.rgb[0] / 32f, reading.rgb[1] / 32f, reading.rgb[2] / 32f)
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            Canvas(Modifier.width(48.dp).height(24.dp).semantics { contentDescription = "Gemeldete LED-Farbe" }) { drawRect(color) }
                            Text("LED-Ausgabe ${reading.rgb.joinToString("/")}")
                        }
                    } else Text("LED-Ausgabe unbekannt")
                    if (reading.fault != 0) Text("Gerätefehler: ${reading.fault}", color = MaterialTheme.colorScheme.error)
                    if (reading.configFault != 0) Text("Konfigurationsfehler: ${reading.configFault}", color = MaterialTheme.colorScheme.error)
                } else {
                    Text(if (station.mac == null) "Noch nicht eingerichtet" else if (station.authenticated) "Handy verbunden · Daten überholt" else station.message)
                    Text("Funknähe / Glut-Ziel unbekannt")
                    Text("LED-Ausgabe unbekannt")
                }
                if (station.change != NearGlowChange.DRAFT) Text(station.changeMessage, style = MaterialTheme.typography.bodySmall)
                else if (station.confirmed != null && station.confirmed != state.draft.curve) Text("Entwurf noch nicht übernommen", style = MaterialTheme.typography.bodySmall)
                if (station.mac != null && !station.connected) {
                    Button(onClick = { connect(index) }, enabled = state.stations.all { it.mac != null } && !station.busy,
                        modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Verbinden") }
                }
                if (details) {
                    HorizontalDivider()
                    Text("LED-Farbe und Glut-Ziel sind Gerätemeldungen, keine optische Helligkeitsmessung.", style = MaterialTheme.typography.bodySmall)
                    Text(station.mac ?: "Noch keine Station zugeordnet")
                    station.peer?.let { Text("Partner: $it") }
                    if (station.authenticated && reading?.handsetFresh(state.now) == true) {
                        Text(if (reading.armed) "Station freigegeben" else "Station nicht freigegeben")
                        Text("Handyempfang vor ${state.now - reading.receivedAt} ms")
                        Text("Konfigurationsrevision: ${reading.configurationRevision ?: "ungeklärt"} · Ausgaberevision: ${reading.outputRevision ?: "unbekannt"}")
                        if (reading.outputPending) Text("LED-Ausgabe wird aktualisiert …")
                    }
                    station.revision?.let { Text("Dauerhafte Revision: $it") }
                    station.confirmed?.let { curve -> Text("Gelesene Kennlinie: " + curve.points.joinToString(" · ") { "${it.rssi} dBm → ${it.percent} %" }) }
                    Text(station.changeMessage)
                    OutlinedButton(onClick = { importAccess(index) }, enabled = !station.connected && !station.busy,
                        modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Stationszugang importieren") }
                    if (station.connected || station.busy) OutlinedButton(onClick = { disconnect(index) },
                        modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Handyverbindung trennen") }
                }
            }
        }
    }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("Nähe → Helligkeit", style = MaterialTheme.typography.titleLarge)
            Text("Entwurf · stärkeres Signal darf die Glut heller machen. dBm beschreibt Funknähe, keine Meterentfernung.")
            state.draft.points.forEachIndexed { index, point ->
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(point.rssi, { edit(index, it, point.percent) },
                        label = { Text("Punkt ${index + 1} · dBm") }, singleLine = true,
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Ascii),
                        modifier = Modifier.weight(1f))
                    OutlinedTextField(point.percent, { edit(index, point.rssi, it) },
                        label = { Text("Helligkeit %") }, singleLine = true,
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        modifier = Modifier.weight(1f))
                }
            }
            state.draft.error?.let { Text(it, color = MaterialTheme.colorScheme.error) }
            state.draft.curve?.let { curve ->
                val color = MaterialTheme.colorScheme.primary
                val description = curve.points.joinToString(", ") { "${it.rssi} dBm: ${it.percent} Prozent" }
                Canvas(Modifier.fillMaxWidth().height(96.dp).semantics { contentDescription = "Kennlinienvorschau: $description" }) {
                    val first = curve.points.first().rssi
                    val span = curve.points.last().rssi - first
                    val positions = curve.points.map {
                        Offset((it.rssi - first).toFloat() / span * size.width,
                            (1f - it.percent / 100f) * size.height)
                    }
                    positions.zipWithNext().forEach { (a, b) -> drawLine(color, a, b, 3.dp.toPx()) }
                    positions.forEach { drawCircle(color, 4.dp.toPx(), it) }
                }
            }
            Button(onClick = applyBoth, enabled = state.canApply, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) {
                Text("Auf beide Stationen übernehmen")
            }
            Text("Beide Stationen speichern getrennt. Der Status jeder Station zeigt, was dauerhaft bestätigt ist. Bei einer Unterbrechung wird nichts automatisch erneut gesendet.",
                style = MaterialTheme.typography.bodySmall)
        }
    }
}
