package de.larpescape.companion.ui.diagnostic

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

private fun stateLabel(value: String) = when (value) {
    "pending" -> "Antwort ausstehend"
    "finished" -> "Endantwort erhalten"
    "not_tested" -> "Bewusst übersprungen · nicht geprüft"
    "timeout" -> "Frist abgelaufen · Ausgang unklar"
    "cancelled" -> "Abgebrochen · Ausgang unklar"
    "disconnected" -> "Verbindung beendet · Ausgang unklar"
    "app_restarted" -> "App neu geöffnet · Ausgang unklar"
    "device_restarted" -> "Geräteneustart · Ausgang unklar"
    else -> "Unklar"
}
private fun verdictLabel(value: String) = when (value) {
    "passed" -> "Bestanden (simuliert)"
    "failed" -> "Fehlgeschlagen (simuliert)"
    "not_tested" -> "Nicht geprüft"
    else -> "Unklar"
}
private fun observationLabel(value: String) = when (value) {
    "matches" -> "Passt zum Soll"
    "mismatch" -> "Abweichung"
    else -> "Nicht beobachtet"
}

@Composable
fun DiagnosticScreen(state: DiagnosticUiState, model: DiagnosticViewModel, exportFile: () -> Unit) {
    val document = state.document
    val index = state.planIndex
    val step = state.planStep()
    val id = step?.getAsJsonObject("request")?.get("stepId")?.asLong
    val observation = step?.getAsJsonObject("observation")
    val saved = observation?.get("recordedUtc")?.isJsonNull == false
    val actual = state.drafts[id] ?: observation?.get("actual")?.asString.orEmpty()
    var result by remember(document?.get("runId")?.asString, id) { mutableStateOf(if (saved) observation!!["result"].asString else "matches") }
    var showPlan by remember { mutableStateOf(false) }
    var showFuture by remember { mutableStateOf(false) }
    var skipConfirmation by remember { mutableStateOf(false) }
    var showTools by remember { mutableStateOf(false) }
    val pending = step?.get("state")?.asString == "pending"
    val skipped = step?.get("state")?.asString == "not_tested"
    val editing = step != null && !pending && !skipped && !state.finalized && !state.canAdvance()

    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text("Larp Diagnose Vorschau", style = MaterialTheme.typography.titleLarge)
        Text(if (document == null) "Pilotplan · Wake 1 → LED 0" else if (index < 2) "Schritt ${index + 1} von 2 · ${pilotTitles[index]}" else "Pilotplan beendet · 2 von 2", style = MaterialTheme.typography.titleMedium)
        if (document != null) Text(pilotOperations.indices.joinToString("\n") { position ->
            val savedStep = state.planStep(position)
            val status = when {
                savedStep == null -> "Noch offen"
                savedStep["state"].asString == "pending" -> "Läuft"
                savedStep["state"].asString == "not_tested" -> "Nicht geprüft"
                savedStep.getAsJsonObject("observation")["recordedUtc"].isJsonNull -> "Beobachtung offen"
                else -> verdictLabel(savedStep["verdict"].asString)
            }
            "${position + 1}. ${if (position == 0) "Wake 1" else "LED 0"} · $status"
        }, style = MaterialTheme.typography.bodyMedium)
        if (state.running) Text(if (pending) "Auftrag läuft · noch ${(state.stepRemainingMs + 999) / 1000} von 10 s" else "Simulationslauf · noch ${(state.runRemainingMs + 999) / 1000} s")
        if (document != null && !state.running) OutlinedButton(onClick = model::newRun, enabled = !state.hasDraft(), modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Neuen Pilotlauf starten") }
        HorizontalDivider()
        val scroll = rememberScrollState()
        LaunchedEffect(document?.get("runId")?.asString, index, pending, saved) { scroll.scrollTo(0) }
        Column(Modifier.weight(1f).verticalScroll(scroll), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("Simulation ohne Box. Fake-Endantwort und deine Beobachtung bleiben getrennt.")
            if (document == null) {
                Text("1. Wake 1: Drücken und Loslassen simulieren\n2. LED 0: Rot und anschließend aus simulieren\nDanach den Bericht exportieren.")
                Text("„Pilotplan starten“ bereitet den Lauf vor. Jeder Auftrag beginnt anschließend mit seinem eigenen Startknopf.")
            } else if (index < 2) {
                if (!state.running) Text("Lauf beendet · gespeicherte Ergebnisse ansehen und exportieren. Kein erneutes Senden.")
                if (step == null) {
                    Text("Noch nicht geprüft. Der Auftrag beginnt erst mit „Schritt starten“.")
                    Text(if (index == 0) "Soll: Eingang 1 einmal aktiv und wieder inaktiv; kein weiterer Eingang." else "Soll: nur LED 0 rot, danach aus. Keine Versorgung gemessen.")
                    OutlinedButton(onClick = { skipConfirmation = true }, enabled = state.running && !state.pending && !state.hasDraft(), modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Schritt überspringen · nicht geprüft") }
                    if (!state.running) Text("Dieser Schritt wurde nicht begonnen. Für neue Aufträge ist ein neuer Pilotlauf nötig.")
                } else {
                    Text(stateLabel(step["state"].asString))
                    if (pending) {
                        Text(if (index == 0) "Bedienauftrag der Vorschau: Drücken und Loslassen von Wake 1 simulieren." else "Bedienauftrag der Vorschau: LED 0 rot und anschließend aus simulieren.")
                        Text("Wähle unten die passende Fake-Endantwort. Ohne Antwort läuft die Frist nach 10 Sekunden ab; der Ausgang bleibt unklar.")
                        OutlinedButton(onClick = { model.simulateReply(true) }, enabled = !state.busy, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Fehlerantwort simulieren") }
                    } else {
                        Text("Fake-Endantwort: ${if (step["deviceReply"].isJsonNull) "keine" else if (step.getAsJsonObject("deviceReply")["outcome"].asString == "completed") "abgeschlossen" else "Fehler"}")
                        Text("Einstufung: ${verdictLabel(step["verdict"].asString)}")
                        if (!skipped) {
                            Text("Deine Beobachtung der Simulation", style = MaterialTheme.typography.titleMedium)
                            Text(if (saved) "Gespeichert: ${observationLabel(observation!!["result"].asString)} · ${observation["actual"].asString}" else "Noch keine Beobachtung gespeichert. Fake-Endantwort allein bestätigt keinen Erfolg.")
                            if (editing) {
                                if (state.hasDraft()) Text("Ungespeicherter Beobachtungstext", color = MaterialTheme.colorScheme.error)
                                OutlinedTextField(actual, { model.draft(id!!, it) }, label = { Text("Was hast du in der Vorschau gesehen?") }, modifier = Modifier.fillMaxWidth())
                                var choose by remember { mutableStateOf(false) }
                                Box {
                                    OutlinedButton(onClick = { choose = true }, modifier = Modifier.heightIn(min = 48.dp)) { Text("Bewertung: ${observationLabel(result)} ▾") }
                                    DropdownMenu(expanded = choose, onDismissRequest = { choose = false }) {
                                        listOf("matches", "mismatch", "not_observed").forEach { choice -> DropdownMenuItem(text = { Text(observationLabel(choice)) }, onClick = { result = choice; choose = false }) }
                                    }
                                }
                                if (result == "not_observed") Text("Mit „Beobachtung speichern“ wird ausdrücklich „nicht beobachtet“ festgehalten. Die Einstufung bleibt unklar.")
                                if (state.hasDraft() || state.editingObservation != null) TextButton(onClick = { model.discardDraft(id!!) }, modifier = Modifier.heightIn(min = 48.dp)) { Text("Bearbeitung verwerfen") }
                            } else if (!state.finalized) TextButton(onClick = { model.editObservation(id!!) }, modifier = Modifier.heightIn(min = 48.dp)) { Text("Beobachtung bearbeiten") }
                        }
                    }
                    var details by remember(document["runId"].asString, id) { mutableStateOf(false) }
                    TextButton(onClick = { details = !details }, modifier = Modifier.heightIn(min = 48.dp)) { Text(if (details) "Technische Details schließen" else "Technische Details ansehen") }
                    if (details) {
                        Text("Rohdaten der Simulation · keine echte Gerätetelemetrie", style = MaterialTheme.typography.titleSmall)
                        Text(step.toString(), style = MaterialTheme.typography.bodySmall)
                    }
                }
            } else {
                pilotOperations.indices.forEach { position -> Text("${position + 1}. ${pilotTitles[position]}: ${state.planStep(position)?.let { verdictLabel(it["verdict"].asString) } ?: "Nicht geprüft"}") }
                Text("Dieser Bericht enthält ausschließlich Simulationsergebnisse. Die physische Hardwareprüfung bleibt offen.")
            }
            state.notice?.let { Text(it) }
            TextButton(onClick = { showPlan = !showPlan }, modifier = Modifier.heightIn(min = 48.dp)) { Text(if (showPlan) "Pilotübersicht schließen" else "Pilotübersicht und Rückblick") }
            if (showPlan) pilotOperations.indices.forEach { position ->
                val savedStep = state.planStep(position)
                Text("${position + 1}. ${pilotTitles[position]} · ${savedStep?.let { stateLabel(it["state"].asString) + " · " + verdictLabel(it["verdict"].asString) } ?: "Noch nicht geprüft"}")
                if (savedStep != null) TextButton(onClick = { model.reviewStep(position) }, enabled = !state.pending && !state.hasDraft(), modifier = Modifier.heightIn(min = 48.dp)) { Text("Schritt ${position + 1} ansehen · kein erneuter Auftrag") }
            }
            TextButton(onClick = { showFuture = !showFuture }, modifier = Modifier.heightIn(min = 48.dp)) { Text(if (showFuture) "Gesamtplan-Vorschau schließen" else "Gesamtplan-Vorschau · gesperrt") }
            if (showFuture) {
                Text("Spätere Hardwareprüfung · nicht ausführbar", style = MaterialTheme.typography.titleMedium)
                Text("Alle Bereiche sind gesperrt. Die Vorschau gibt keine Box frei und sendet keinen Gerätebefehl.")
                listOf("Vier Aufweckeingänge · je 3 Versuche und Kombinationen", "9×9-Matrix · 81 Felder drücken und loslassen", "23 LEDs · Position, Farbe und Versorgung", "PN532 / Test-Tag · geschütztes Schreiben und Rücklesen", "DFPlayer / SD / Lautsprecher · Tracks und hörbarer Ton", "ESP-NOW · kurzer und entfernter Empfang mit Empfängerbericht", "Abschluss · 3 Kaltstarts und gemeinsamer Dauerlauf").forEach { Text("Gesperrt: $it") }
            }
            if (document != null) {
                TextButton(onClick = { showTools = !showTools }, modifier = Modifier.heightIn(min = 48.dp)) { Text(if (showTools) "Weitere Aktionen schließen" else "Abbruch, Export und neuer Lauf") }
                if (showTools) {
                    if (state.running) {
                        OutlinedButton(onClick = { model.stop() }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Lauf abbrechen · Ausgang unklar") }
                        TextButton(onClick = { model.stop("disconnected") }, modifier = Modifier.heightIn(min = 48.dp)) { Text("Verbindungsverlust simulieren") }
                        TextButton(onClick = { model.stop("device_restarted") }, modifier = Modifier.heightIn(min = 48.dp)) { Text("Geräteneustart simulieren") }
                    }
                    OutlinedButton(onClick = exportFile, enabled = !state.hasDraft(), modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Bisherigen Lauf abschließen und exportieren") }
                    OutlinedButton(onClick = model::newRun, enabled = !state.running && !state.hasDraft(), modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Neuen Pilotlauf ausdrücklich starten") }
                }
            }
        }
        HorizontalDivider()
        when {
            document == null -> Button(onClick = model::newRun, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Pilotplan starten") }
            index >= 2 || (!state.running && step == null) || (state.finalized && !state.canAdvance()) -> Button(onClick = exportFile, enabled = !state.hasDraft(), modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Bericht exportieren") }
            step == null -> Button(onClick = model::startCurrent, enabled = state.running && !state.pending && !state.busy && !state.hasDraft(), modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Schritt ${index + 1} starten · Simulation") }
            pending -> Button(onClick = { model.simulateReply(false) }, enabled = !state.busy, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text(if (state.busy) "Fake-Antwort wird erzeugt …" else "Passende Endantwort simulieren") }
            editing -> Button(onClick = { model.observe(id!!, result, if (result == "not_observed") "" else actual) }, enabled = result == "not_observed" || actual.isNotBlank(), modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Beobachtung speichern") }
            else -> Button(onClick = model::nextStep, enabled = state.canAdvance(), modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text(if (index == 0) "Weiter zum nächsten Schritt" else "Weiter zum Abschluss") }
        }
    }
    if (skipConfirmation) AlertDialog(
        onDismissRequest = { skipConfirmation = false },
        title = { Text("Schritt ${index + 1} überspringen?") },
        text = { Text("Dieser Schritt wird als nicht geprüft gespeichert. Es wird keine Fake-Endantwort erzeugt und kein Erfolg behauptet.") },
        confirmButton = { TextButton(onClick = { skipConfirmation = false; model.skipCurrent() }) { Text("Als nicht geprüft überspringen") } },
        dismissButton = { TextButton(onClick = { skipConfirmation = false }) { Text("Zurück zum Schritt") } },
    )
}
