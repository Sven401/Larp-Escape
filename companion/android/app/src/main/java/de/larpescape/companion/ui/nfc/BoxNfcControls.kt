package de.larpescape.companion.ui.nfc

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import de.larpescape.companion.data.device.LedUiState
import de.larpescape.protocol.NfcTestFixture
import de.larpescape.protocol.BoxNfcResult

@Composable
fun BoxNfcControls(state: LedUiState, command: (Boolean) -> Unit) {
    val nfc = state.nfc
    // Confirmation belongs to this exact read result; a new result/link invalidates it.
    var confirm by remember(nfc.result, state.authenticated) { mutableStateOf(false) }
    val enabled = state.authenticated && state.current && nfc.supported && !nfc.busy && !state.busy && state.active == null
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("NFC an der Box", style = MaterialTheme.typography.titleLarge)
            Text("1. Tag an der Box lesen. 2. Am Handy Testdaten mit Flags 1 schreiben. 3. Wieder an der Box lesen und Flags 2 schreiben. 4. Am Handy frisch zurücklesen.")
            Text(nfc.message)
            if (state.authenticated && !nfc.supported) Text("Diese Box-Firmware bietet noch keinen NFC-Test über die App.")
            nfc.result?.let { result ->
                if (result.uid.isNotEmpty()) Text("Box-Tagkennung: ${result.uidText}")
                result.data?.let { Text("Box gelesen: Kennwert ${it.uidHash} · Flags ${it.flags} · Zeitwert ${it.timestampBinary} · Besuch 1, 1, 0 · CRC gültig") }
            }
            Button(onClick = { command(false) }, enabled = enabled) { Text("Tag an der Box lesen") }
            Button(onClick = { confirm = true }, enabled = enabled && nfc.canChange) { Text("Mit Box Flags 2 schreiben …") }
            if (nfc.busy) Text("Tag ruhig an der Box lassen. Der Auftrag wird nicht automatisch wiederholt.")
            if (state.active != null) Text("Vor dem NFC-Test LEDs stoppen.")
        }
    }
    if (confirm && enabled && nfc.canChange) AlertDialog(
        onDismissRequest = { confirm = false }, title = { Text("Diesen Testtag mit der Box schreiben?") },
        text = { Text("Tag ${nfc.result!!.uidText}: Flags 1 werden einmalig durch Flags 2 ersetzt. Die Box prüft Tagkennung und Inhalt erneut und liest das Ergebnis zurück.") },
        confirmButton = { TextButton(onClick = { confirm = false; command(true) }) { Text("Einmal schreiben") } },
        dismissButton = { TextButton(onClick = { confirm = false }) { Text("Abbrechen") } })
}

fun boxRoundtripConfirmed(result: BoxNfcResult, baseline: Long, tag: NfcUiState): Boolean =
    result.outcome == 8 && result.data == NfcTestFixture.changed && result.uid.size == 7 &&
        tag.listening && !tag.busy && !tag.armed && tag.readOnlySelectionId > baseline &&
        tag.tagId.orEmpty().equals(result.uidText, ignoreCase = true) && tag.data == NfcTestFixture.changed

@Composable
fun BoxNfcComparison(box: LedUiState, tag: NfcUiState) {
    val result = box.nfc.result ?: return
    val baseline = remember(result) { tag.selectionId }
    if (result.data != NfcTestFixture.changed || result.outcome != 8 || tag.data == null) return
    Text(if (boxRoundtripConfirmed(result, baseline, tag))
        "Handy liest dieselbe Tagkennung und Flags 2 · vollständige Testpayload stimmt mit der Boxänderung überein."
    else "Letzte Handylesung bestätigt den Box-Rundlauf noch nicht. Nach der Boxänderung denselben Tag am Handy erneut lesen.")
}
