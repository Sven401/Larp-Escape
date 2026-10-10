package de.larpescape.companion.ui.diagnostic

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@Composable
fun DiagnosticScreen(state: DiagnosticUiState, model: DiagnosticViewModel, exportFile: () -> Unit) {
    Text("Hardwarediagnose · Simulation", style = MaterialTheme.typography.titleLarge)
    Text("Debugablauf mit Fake-Antworten. Die Box wird nicht angesprochen. Jede Einstufung ist simuliert; eine physische Prüfung bleibt offen.")
    state.notice?.let { Text(it) }
    Button(onClick = model::newRun, enabled = !state.running, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Neue Simulation ausdrücklich starten") }
    val document = state.document ?: return
    Text("Lauf: ${document["runId"].asString}", style = MaterialTheme.typography.bodySmall)
    Text(if (state.running) "Simulation aktiv · maximal 60 Sekunden" else "Lauf beendet / wiederhergestellt · kein erneutes Senden")
    if (state.running && !state.pending) {
        Button(onClick = { model.begin("wake_capture") }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Auftrag: Wake-Eingang 1") }
        Button(onClick = { model.begin("led_single") }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Auftrag: LED 0 einzeln") }
        OutlinedButton(onClick = { model.skip("led_single") }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("LED-Schritt als nicht geprüft überspringen") }
    }
    document.getAsJsonArray("steps").forEach { item ->
        val step = item.asJsonObject; val request = step.getAsJsonObject("request"); val id = request["stepId"].asLong
        val wake = request["operation"].asString == "wake_capture"
        val verdict = when (step["verdict"].asString) { "passed" -> "bestanden (simuliert)"; "failed" -> "fehlgeschlagen (simuliert)"; "not_tested" -> "nicht geprüft"; else -> "unklar" }
        Card(Modifier.fillMaxWidth()) { Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Schritt $id · ${if (wake) "Wake 1" else "LED 0"}", style = MaterialTheme.typography.titleMedium)
            Text("Auftrag: ${if (wake) "Taste 1 einmal drücken und loslassen; keine Nebenauslösung" else "Nur LED 0 rot und anschließend aus"}")
            Text("Zustand: ${step["state"].asString} · Einstufung: $verdict")
            Text("Gerätetelemetrie (Fake)", style = MaterialTheme.typography.titleSmall)
            if (step["deviceReply"].isJsonNull) Text("Keine bestätigte Endantwort.") else {
                val reply = step.getAsJsonObject("deviceReply"); val telemetry = reply.getAsJsonObject("telemetry")
                Text("Fake-Endantwort: ${reply["outcome"].asString} · ${reply["reason"].asString}")
                if (wake) telemetry.getAsJsonArray("edges").forEach { edge -> edge.asJsonObject.let {
                    Text("Eingang ${it["channel"].asInt}: ${if (it["active"].asBoolean) "aktiv" else "inaktiv"} · Bootzeit ${it["deviceMs"].asLong} ms · Rohpegel ${if (it["rawHigh"].asBoolean) "High" else "Low"}")
                } } else Text("Renderaufrufe: ${telemetry["renderCalls"].asInt} · Rückkehr zu aus: ${telemetry["restoredOff"].asBoolean} · Versorgung ungemessen")
            }
            Text("Ignorierte Antworten: ${step.getAsJsonArray("ignoredReplies").size()}")
            if (step["state"].asString == "pending" && state.running) {
                Button(onClick = { model.simulateReply(false) }, enabled = !state.busy, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Passende Geräteantwort simulieren") }
                OutlinedButton(onClick = { model.simulateReply(true) }, enabled = !state.busy, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Gerätefehler simulieren") }
            }
            Text("Unabhängige Nutzerbeobachtung", style = MaterialTheme.typography.titleSmall)
            Text("Gespeichert: ${step.getAsJsonObject("observation")["result"].asString} · ${step.getAsJsonObject("observation")["actual"].asString}")
            var actual by remember(document["runId"].asString, id) { mutableStateOf(step.getAsJsonObject("observation")["actual"].asString) }
            OutlinedTextField(actual, { if (it.length <= 256) actual = it }, label = { Text("Beobachtung dieser Simulation") }, modifier = Modifier.fillMaxWidth())
            Button(onClick = { model.observe(id, "matches", actual) }, enabled = actual.isNotBlank() && !state.finalized, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Passt zum Soll (Simulation)") }
            OutlinedButton(onClick = { model.observe(id, "mismatch", actual) }, enabled = actual.isNotBlank() && !state.finalized, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Abweichung festhalten") }
            TextButton(onClick = { model.observe(id, "not_observed", "") }, enabled = !state.finalized, modifier = Modifier.heightIn(min = 48.dp)) { Text("Nicht beobachtet") }
        } }
    }
    if (state.running) {
        OutlinedButton(onClick = { model.stop("cancelled") }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Lauf abbrechen · Ausgang offen halten") }
        OutlinedButton(onClick = { model.stop("disconnected") }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Verbindungsverlust simulieren") }
        OutlinedButton(onClick = { model.stop("device_restarted") }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Geräteneustart simulieren") }
    }
    Button(onClick = exportFile, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Lauf abschließen und Bericht exportieren") }
    Text("Fehlendes Testmittel: keine Spannungsmessung. Firmware und Transport zur echten Box sowie physische Abnahme sind offen.", style = MaterialTheme.typography.bodySmall)
}
