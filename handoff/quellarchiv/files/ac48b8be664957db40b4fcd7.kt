package de.larpescape.companion.ui.diagnostic

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.background
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.saveable.rememberSaveableStateHolder
import androidx.compose.ui.Modifier
import androidx.compose.ui.Alignment
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp

private data class ComponentEntry(val title: String, val group: String, val scope: String)

private val components = listOf(
    ComponentEntry("23 LEDs", "Ausgaben", "Index/Gruppe, Farbe, Helligkeit, Effekte, Stop"),
    ComponentEntry("Audio / SD", "Ausgaben", "Clip oder Tracknummer, Lautstärke, Play/Stop"),
    ComponentEntry("Vier Wake-Eingänge", "Eingaben", "Pegel, Flanken, Einzel- und Kombinationstest"),
    ComponentEntry("9×9-Keymatrix", "Eingaben", "Rohpunkte, Drehung, Spiegelung, 81-Punkt-Prüfung"),
    ComponentEntry("NFC / Test-Tag", "Eingaben", "Handy → Box und Box → Handy mit Rücklesung"),
    ComponentEntry("ESP-NOW / Empfänger", "Funk", "Kurze und Einsatzentfernung mit Empfängerbeleg"),
    ComponentEntry("Abschluss", "Gesamttest", "Drei Kaltstarts und gemeinsamer Lauf"),
)

@Composable
fun DiagnosticWorkbenchScreen(state: DiagnosticUiState, model: DiagnosticViewModel, exportFile: () -> Unit) {
    var page by rememberSaveable { mutableStateOf("list") }
    var selected by rememberSaveable { mutableStateOf("23 LEDs") }
    val savedPages = rememberSaveableStateHolder()
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("DIAGNOSE", style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
                Text(when (page) { "list" -> "Baugruppen"; "detail" -> selected; "guided" -> "Geführter Lauf"; else -> "Berichte" },
                    style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.SemiBold)
            }
            Surface(shape = RoundedCornerShape(16.dp), color = MaterialTheme.colorScheme.secondaryContainer) {
                Text("DEMO", Modifier.padding(horizontal = 10.dp, vertical = 6.dp), style = MaterialTheme.typography.labelMedium)
            }
        }
        Surface(shape = RoundedCornerShape(16.dp), color = MaterialTheme.colorScheme.errorContainer) {
            Column(Modifier.fillMaxWidth().padding(12.dp), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                Text("Keine Box verbunden", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.SemiBold)
                Text("Alle Antworten sind simuliert. Geräteaktionen bleiben gesperrt.", style = MaterialTheme.typography.bodySmall)
            }
        }
        when (page) {
            "list" -> {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    FilledTonalButton(onClick = { page = "guided" }, modifier = Modifier.weight(1f).heightIn(min = 48.dp)) { Text("Gesamttest") }
                    OutlinedButton(onClick = { page = "report" }, modifier = Modifier.weight(1f).heightIn(min = 48.dp)) { Text("Berichte") }
                }
                Column(Modifier.weight(1f).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    Text("Einzelprüfung", style = MaterialTheme.typography.titleLarge)
                    Text("Baugruppe wählen, Prüfziel ansehen und einen lokalen Entwurf vorbereiten.", style = MaterialTheme.typography.bodyMedium)
                    components.groupBy { it.group }.forEach { (group, entries) ->
                        Text(group.uppercase(), style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.primary)
                        entries.forEach { item ->
                            OutlinedCard(Modifier.fillMaxWidth()) {
                                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                                        Text(item.title, Modifier.weight(1f), style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
                                        Text(if (item.title == "23 LEDs") "VORSCHAU" else "GESPERRT", style = MaterialTheme.typography.labelSmall,
                                            color = if (item.title == "23 LEDs") MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.error)
                                    }
                                    Text(item.scope, style = MaterialTheme.typography.bodyMedium)
                                    Text(if (item.title == "23 LEDs") "Lokaler Entwurf und Fake-Antwort" else "Gerätevertrag und Freigabe ausstehend", style = MaterialTheme.typography.bodySmall)
                                    TextButton(onClick = { selected = item.title; page = "detail" }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Prüfung ansehen →") }
                                }
                            }
                        }
                    }
                }
            }
            "detail" -> {
                TextButton(onClick = { page = "list" }, modifier = Modifier.heightIn(min = 48.dp)) { Text("← Zur Baugruppenliste") }
                if (selected == "23 LEDs") savedPages.SaveableStateProvider("led-preview") { LedIndividualPreview(Modifier.weight(1f)) }
                else Column(Modifier.weight(1f).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    Text(selected, style = MaterialTheme.typography.titleLarge)
                    Text(components.first { it.title == selected }.scope)
                    Text("Geräteaktion gesperrt: Fähigkeit, Berechtigung und Komponentenbesitz sind noch nicht live bestätigt.")
                    Text("Nächster Schritt: Gerätevertrag und Fake-/Firmwarepfad für diese Baugruppe prüfen.")
                }
            }
            "guided" -> {
                TextButton(onClick = { page = "list" }, modifier = Modifier.heightIn(min = 48.dp)) { Text("← Zur Baugruppenliste") }
                DiagnosticScreen(state, model, exportFile, Modifier.weight(1f))
            }
            "report" -> {
                TextButton(onClick = { page = "list" }, modifier = Modifier.heightIn(min = 48.dp)) { Text("← Zur Baugruppenliste") }
                Column(Modifier.weight(1f).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    Text("Berichte", style = MaterialTheme.typography.titleLarge)
                    Text("Der geführte Wake-/LED-Pilot hat ein gespeichertes Simulationsjournal.")
                    Text(if (state.document == null) "Noch kein Pilotlauf gespeichert." else "Pilotlauf vorhanden · ${state.document["runId"].asString}")
                    Button(onClick = exportFile, enabled = state.document != null && !state.hasDraft(), modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Pilotbericht exportieren") }
                    Text("Einzelprüfung: Die lokale LED-Vorschau wird noch nicht im Bericht gespeichert. Sie belegt keine Hardwarefunktion.")
                }
            }
        }
    }
}

@Composable
internal fun LedActionSummary(index: Int, red: Int, green: Int, blue: Int, brightness: Int, seconds: Int) {
    Text("Prüfkonfiguration · Entwurf auf diesem Handy", style = MaterialTheme.typography.titleMedium)
    Text("LED $index · RGB $red/$green/$blue · Helligkeit $brightness % · $seconds s")
    Text("Pinprofil, Grenzwerte und sicherer Auszustand: noch nicht vom Gerät bestätigt.")
}

@Composable
private fun LedIndividualPreview(modifier: Modifier = Modifier) {
    var index by rememberSaveable { mutableIntStateOf(0) }
    var red by rememberSaveable { mutableIntStateOf(255) }
    var green by rememberSaveable { mutableIntStateOf(0) }
    var blue by rememberSaveable { mutableIntStateOf(0) }
    var brightness by rememberSaveable { mutableIntStateOf(25) }
    var seconds by rememberSaveable { mutableIntStateOf(3) }
    var phase by rememberSaveable { mutableStateOf("prepared") }
    var observation by rememberSaveable { mutableStateOf("") }
    var assessment by rememberSaveable { mutableStateOf("Nicht beobachtet") }
    Column(modifier, verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Column(Modifier.weight(1f).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(16.dp)) {
            Text("LED-Einzelprüfung", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)
            Text("Prüfziel: Eine gewählte LED in der Vorschau farbig ansteuern und den Ausgang getrennt bewerten.")
            Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.primaryContainer)) {
                Row(Modifier.fillMaxWidth().padding(16.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                    Box(Modifier.size(56.dp).background(Color(red, green, blue), RoundedCornerShape(16.dp))
                        .semantics { contentDescription = "Vorschaufarbe RGB $red, $green, $blue" })
                    Column {
                        Text("Farbvorschau", style = MaterialTheme.typography.titleSmall)
                        Text("LED $index · $brightness % · $seconds s", style = MaterialTheme.typography.bodyMedium)
                    }
                }
            }
            Card {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    LedActionSummary(index, red, green, blue, brightness, seconds)
                    SliderField("LED-Index", index, 22) { index = it; phase = "prepared" }
                    SliderField("Rot", red, 255) { red = it; phase = "prepared" }
                    SliderField("Grün", green, 255) { green = it; phase = "prepared" }
                    SliderField("Blau", blue, 255) { blue = it; phase = "prepared" }
                    SliderField("Helligkeit in Prozent", brightness, 100) { brightness = it; phase = "prepared" }
                    SliderField("Dauer in Sekunden", seconds, 10, 1) { seconds = it; phase = "prepared" }
                }
            }
            OutlinedCard {
                Column(Modifier.fillMaxWidth().padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Bevorstehender Auftrag", style = MaterialTheme.typography.titleMedium)
                    Text("LED $index · RGB $red/$green/$blue · $brightness % · begrenzt auf $seconds s")
                    Text("Nur Simulation auf diesem Handy. Kein Befehl an die Box.", style = MaterialTheme.typography.bodySmall)
                }
            }
            if (phase == "requested") {
                Text("Fake-Auftrag angefordert · noch keine Endantwort", style = MaterialTheme.typography.titleMedium)
                OutlinedButton(onClick = { phase = "unclear" }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Stop simulieren · Auszustand unklar") }
                OutlinedButton(onClick = { phase = "unclear" }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Timeout simulieren · Ausgang unklar") }
            }
            if (phase == "fake" || phase == "unclear") {
                Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)) {
                    Column(Modifier.fillMaxWidth().padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text("Ergebnis der Vorschau", style = MaterialTheme.typography.titleMedium)
                        Text(if (phase == "fake") "Fake-Endantwort: simuliert abgeschlossen. Sicherer Auszustand nicht bestätigt." else "Keine Endantwort · Ausgang unklar. Kein automatischer Neustart.")
                        Text("Quelle: lokale Simulation · keine Gerätetelemetrie", style = MaterialTheme.typography.bodySmall)
                    }
                }
                OutlinedTextField(observation, { observation = it.take(256) }, label = { Text("Eigene Beobachtung") }, modifier = Modifier.fillMaxWidth())
                Text("Eigene Einstufung", style = MaterialTheme.typography.titleMedium)
                listOf("Nicht beobachtet", "Passt zum Soll", "Abweichung").forEach { choice ->
                    OutlinedButton(onClick = { assessment = choice }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) {
                        Text("${if (assessment == choice) "✓ " else ""}$choice")
                    }
                }
                Text("Vorschauergebnis: $assessment · ${if (phase == "unclear") "Geräteausgang unklar" else "nur Fake-Endantwort"}")
            }
            Text("Gruppen, Effekte und eine echte Stop-Bestätigung folgen mit dem Gerätevertrag.", style = MaterialTheme.typography.bodySmall)
        }
        HorizontalDivider()
        Button(onClick = { phase = when (phase) { "prepared" -> "requested"; "requested" -> "fake"; else -> "prepared" } },
            modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) {
            Text(when (phase) { "prepared" -> "Fake-Auftrag starten"; "requested" -> "Fake-Endantwort zeigen"; else -> "Neuen Fake-Auftrag vorbereiten" })
        }
    }
}

@Composable
private fun SliderField(label: String, value: Int, max: Int, min: Int = 0, onChange: (Int) -> Unit) {
    Text("$label: $value")
    Slider(value = value.toFloat(), onValueChange = { onChange(it.toInt().coerceIn(min, max)) },
        modifier = Modifier.semantics { contentDescription = "$label: $value" }, valueRange = min.toFloat()..max.toFloat())
}
