package de.larpescape.companion.ui.nfc

import android.content.pm.PackageManager
import android.nfc.NfcAdapter
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import de.larpescape.companion.platform.nfc.NfcCapabilityRun
import kotlinx.coroutines.delay

@Composable
fun NfcCapabilityCard(export: () -> Unit, exportNotice: String?) {
    val context = LocalContext.current
    val supported = remember { context.packageManager.hasSystemFeature(PackageManager.FEATURE_NFC_HOST_CARD_EMULATION) }
    val enabled = NfcAdapter.getDefaultAdapter(context)?.isEnabled == true
    var snapshot by remember { mutableStateOf(NfcCapabilityRun.snapshot(context)) }
    LaunchedEffect(Unit) {
        while (true) { snapshot = NfcCapabilityRun.snapshot(context); delay(250) }
    }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("NFC-Verbindungsprobe", style = MaterialTheme.typography.titleMedium)
            Text("Entwicklungstest: Das Handy antwortet nach deinem Start höchstens 30 Sekunden lang auf die Box. Es werden keine Spielleiterrechte oder Diagnoserechte vergeben.")
            when {
                !supported -> Text("Dieses Handy meldet keine NFC-Kartenemulation.")
                !enabled -> Text("NFC ist ausgeschaltet. Schalte es in den Android-Einstellungen ein.")
                else -> Text("NFC-Kartenemulation verfügbar und eingeschaltet.")
            }
            if (snapshot.status == "ARMED" || snapshot.status == "SELECTED") {
                OutlinedButton(onClick = { NfcCapabilityRun.stop(); snapshot = NfcCapabilityRun.snapshot(context) },
                    modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("NFC-Probe beenden") }
            } else {
                Button(onClick = { NfcCapabilityRun.start(context); snapshot = NfcCapabilityRun.snapshot(context) },
                    enabled = supported && enabled,
                    modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("NFC-Probe für 30 Sekunden starten") }
            }
            Text("Lokal: ${snapshot.status} · Geräteabgleich: ${snapshot.deviceCorrelation}")
            snapshot.runId?.let { Text("Laufkennung: $it", style = MaterialTheme.typography.bodySmall) }
            snapshot.challenge?.let { Text("Öffentliche Prüfzahl: $it", style = MaterialTheme.typography.bodySmall) }
            snapshot.events.takeLast(4).forEach { Text(it.kind, style = MaterialTheme.typography.bodySmall) }
            val report = NfcCapabilityRun.report(context)
            if (report != null) {
                Text("Letzter Bericht: ${report.localStatus} · ${report.events.size} Ereignisse · Geräteabgleich ${report.deviceCorrelation}")
                OutlinedButton(onClick = export, enabled = report.endedUtc != null,
                    modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("NFC-Beobachtungsbericht exportieren") }
            }
            exportNotice?.let { Text(it) }
            Text("Eine Antwort des Handys belegt noch keine Antwort der Box.", style = MaterialTheme.typography.bodySmall)
        }
    }
}
