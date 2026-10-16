package de.larpescape.companion.ui.nfc

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import de.larpescape.protocol.NfcTestState

@Composable
fun PhoneNfcTestScreen(state: NfcTestState, supported: Boolean, activate: () -> Unit, deactivate: () -> Unit) {
    Text("NFC-Baugruppe testen", style = MaterialTheme.typography.titleLarge)
    Text("Das Handy stellt einen vorübergehenden Testtag für die Box bereit. Für den gemeinsamen Durchlauf mit einem echten Tag anschließend „Echter Tag“ wählen.")
    Text(state.note)
    if (!supported) Text("Dieses Handy unterstützt die benötigte Kartenemulation nicht.")
    if (state.active) OutlinedButton(onClick = deactivate, modifier = Modifier.fillMaxWidth()) { Text("Testtag deaktivieren") }
    else Button(onClick = activate, enabled = supported, modifier = Modifier.fillMaxWidth()) { Text("Testtag vorbereiten und aktivieren") }
    Text("Nur solange diese Seite im Vordergrund bleibt, höchstens 3 Minuten. Aktivieren setzt die Testdaten und Messwerte zurück.", style = MaterialTheme.typography.bodySmall)
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Empfangene NFC-Kommandos", style = MaterialTheme.typography.titleMedium)
            Text("Lesen: ${state.reads} · Schreiben: ${state.updates} · Abgewiesen: ${state.rejected}")
            Text("Vollständig geprüfte Änderungen: ${state.commits}")
            Text(when {
                state.verified -> "Boxänderung empfangen: Flags 2. Alle Nachher-Daten wurden für die Rücklesung an den Reader ausgegeben. Bestätigung der Box separat prüfen."
                state.boxChangeReceived -> "Boxänderung empfangen: Flags 2. Vollständiges Rücklesen durch den Reader noch offen."
                else -> "Noch keine bestätigte Boxänderung empfangen."
            })
            Text(if (state.commits > 0) "Zuletzt empfangener und geprüfter Datensatz" else "Lokal vorbereiteter Datensatz · noch kein Schreibnachweis", style = MaterialTheme.typography.titleSmall)
            Text("Kennwert: ${state.data.uidHash} · Flags: ${state.data.flags}")
            Text("Zeitstempel: ${state.data.timestampBinary} · Besuch: ${state.data.visits.joinToString { "${it.nodeId}, ${it.result}, ${it.deltaMinutes}" }}")
        }
    }
}
