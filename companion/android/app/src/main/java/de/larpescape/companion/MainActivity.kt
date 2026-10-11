package de.larpescape.companion

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import de.larpescape.companion.platform.ble.AndroidBleRepository
import de.larpescape.companion.ui.device.DeviceViewModel
import de.larpescape.companion.data.device.BleScanModeV1
import de.larpescape.companion.data.device.DeviceTargetKindV1
import de.larpescape.protocol.StatusFreshness
import de.larpescape.companion.platform.nfc.*
import de.larpescape.companion.ui.nfc.*
import de.larpescape.companion.data.scenario.LocalScenarioDraftRepository
import de.larpescape.companion.ui.scenario.*
import de.larpescape.protocol.ScenarioDraftCodec
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import de.larpescape.companion.data.diagnostic.AndroidDiagnosticRunStore
import de.larpescape.companion.ui.diagnostic.*

class MainActivity : ComponentActivity() {
    private var nfcCapabilityExportNotice by mutableStateOf<String?>(null)
    private var nfcCapabilityExportBytes: ByteArray? = null
    private val exportNfcCapability = registerForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        val bytes = nfcCapabilityExportBytes.also { nfcCapabilityExportBytes = null }
        if (uri != null && bytes != null) lifecycleScope.launch {
            try {
                withContext(Dispatchers.IO) {
                    contentResolver.openOutputStream(uri, "wt")?.use { it.write(bytes) }
                        ?: error("Datei konnte nicht geschrieben werden")
                }
                nfcCapabilityExportNotice = "Lokaler NFC-Beobachtungsbericht exportiert. Boxfunktion und Rechte bleiben ungeprüft."
            } catch (error: Exception) { nfcCapabilityExportNotice = "NFC-Export fehlgeschlagen: ${error.message}" }
        } else nfcCapabilityExportNotice = if (uri == null) "NFC-Export abgebrochen; Bericht bleibt in der App."
            else "NFC-Bericht nicht lesbar; kein Export erstellt."
    }
    private var pairingExportBytes: ByteArray? = null
    private var pairingExportNotice by mutableStateOf<String?>(null)
    private var connectionExportNotice by mutableStateOf<String?>(null)
    private val exportConnection = registerForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        // Read after the document picker returns: the link may have closed while it was open.
        val snapshot = if (uri != null) runCatching { model.connectionEvidenceExportBytes() } else null
        val bytes = snapshot?.getOrNull()
        when {
            uri == null -> connectionExportNotice = "Dateiauswahl abgebrochen. Der gespeicherte Transportbeleg bleibt in der App."
            snapshot?.isFailure == true -> connectionExportNotice = "Gespeicherter Transportbeleg ist nicht lesbar; kein Export erstellt."
            bytes == null -> connectionExportNotice = "Exportvorbereitung nicht mehr verfügbar. Bitte erneut exportieren."
            else -> lifecycleScope.launch {
                try {
                    withContext(Dispatchers.IO) {
                        contentResolver.openOutputStream(uri, "wt")?.use { it.write(bytes) }
                            ?: error("Datei konnte nicht geschrieben werden")
                    }
                    connectionExportNotice = "BLE-Transportbeleg exportiert. Autorität und physisches Ergebnis bleiben offen."
                } catch (error: Exception) { connectionExportNotice = "Export fehlgeschlagen: ${error.message}" }
            }
        }
    }
    private val exportPairing = registerForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        val bytes = pairingExportBytes.also { pairingExportBytes = null } ?: model.pendingPairingReportBytes()
        if (uri != null && bytes != null) lifecycleScope.launch {
            try {
                withContext(Dispatchers.IO) { contentResolver.openOutputStream(uri, "wt")?.use { it.write(bytes) }
                    ?: error("Datei konnte nicht geschrieben werden") }
                model.pairingExportCompleted()
                pairingExportNotice = "Android-BLE-Beobachtungsbericht exportiert. Firmware-Nachweise bleiben offen."
            } catch (error: Exception) { pairingExportNotice = "Export fehlgeschlagen: ${error.message}" }
        } else pairingExportNotice = "Dateiauswahl abgebrochen; Bericht bleibt in der App gespeichert."
    }
    private var nfcScreen by mutableStateOf(false)
    private var scenarioScreen by mutableStateOf(false)
    private var diagnosticScreen by mutableStateOf(false)
    private val diagnosticModel by viewModels<DiagnosticViewModel> {
        object : ViewModelProvider.Factory {
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                @Suppress("UNCHECKED_CAST")
                return DiagnosticViewModel(AndroidDiagnosticRunStore(applicationContext)) as T
            }
        }
    }
    private val exportDiagnostic = registerForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        val bytes = diagnosticModel.takeExport()
        if (uri != null && bytes != null) lifecycleScope.launch {
            try {
                withContext(Dispatchers.IO) { contentResolver.openOutputStream(uri, "wt")?.use { it.write(bytes) } ?: error("Datei konnte nicht geschrieben werden") }
                diagnosticModel.notice("Simulationsbericht exportiert.")
            } catch (error: Exception) { diagnosticModel.notice("Export fehlgeschlagen: ${error.message}") }
        } else if (uri == null) diagnosticModel.notice("Dateiauswahl abgebrochen. Der abgeschlossene Lauf bleibt gespeichert.")
    }
    private fun selectDiagnostic(selected: Boolean) {
        if (!BuildConfig.ALLOW_CONTRACT_LAB || selected == diagnosticScreen) return
        if (!selected) diagnosticModel.backgroundEnded()
        diagnosticScreen = selected
        if (selected) { nfcReader.stop(); nfcScreen = false; scenarioScreen = false; model.foregroundEnded(); showPermissionExplanation = false }
        else if (!nfcScreen && !scenarioScreen) model.foregroundStarted(permissionsGranted())
    }
    private val scenarioModel by viewModels<ScenarioViewModel> {
        object : ViewModelProvider.Factory {
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                @Suppress("UNCHECKED_CAST")
                return ScenarioViewModel(LocalScenarioDraftRepository(applicationContext)) as T
            }
        }
    }
    private val importScenario = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) lifecycleScope.launch {
            try {
                val bytes = withContext(Dispatchers.IO) { contentResolver.openInputStream(uri)?.use {
                    it.readNBytes(ScenarioDraftCodec.MAX_BYTES + 1)
                } ?: error("Datei konnte nicht geöffnet werden") }
                scenarioModel.import(bytes)
            } catch (error: Exception) { scenarioModel.notice("Import fehlgeschlagen: ${error.message}") }
        }
    }
    private val exportScenario = registerForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        val bytes = scenarioModel.takeExport()
        if (uri != null && bytes != null) lifecycleScope.launch {
            try {
                withContext(Dispatchers.IO) { contentResolver.openOutputStream(uri, "wt")?.use { it.write(bytes) }
                    ?: error("Datei konnte nicht geschrieben werden") }
                scenarioModel.notice("Entwurf exportiert.")
            } catch (error: Exception) { scenarioModel.notice("Export fehlgeschlagen: ${error.message}") }
        }
    }
    private fun selectScenario(selected: Boolean) {
        if (selected && diagnosticScreen) selectDiagnostic(false)
        if (selected == scenarioScreen) return
        scenarioScreen = selected
        if (selected) { nfcReader.stop(); nfcScreen = false; model.foregroundEnded() }
        else if (!nfcScreen) model.foregroundStarted(permissionsGranted())
    }
    private val nfcModel by viewModels<NfcViewModel> {
        object : ViewModelProvider.Factory {
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                @Suppress("UNCHECKED_CAST")
                return NfcViewModel(AndroidNfcEnvelope(), AndroidNfcWriteJournal(applicationContext),
                    event = { if (BuildConfig.DEBUG) android.util.Log.i("LarpEscape", it) }) as T
            }
        }
    }
    private val nfcReader by lazy { AndroidNfcReader(this, nfcModel) }
    private fun selectNfc(selected: Boolean) {
        if (selected && diagnosticScreen) selectDiagnostic(false)
        if (selected == nfcScreen) return
        nfcScreen = selected
        showPermissionExplanation = false
        if (selected) { model.foregroundEnded(); nfcReader.start() }
        else { nfcReader.stop(); if (!scenarioScreen) model.foregroundStarted(permissionsGranted()) }
    }
    private val model by viewModels<DeviceViewModel> {
        object : ViewModelProvider.Factory {
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                @Suppress("UNCHECKED_CAST")
                return DeviceViewModel(AndroidBleRepository(applicationContext)) as T
            }
        }
    }
    private val permissions = arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
    private var showPermissionExplanation by mutableStateOf(false)
    private var permissionScanMode = BleScanModeV1.PRODUCT
    private fun permissionsGranted() = permissions.all { checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }
    private val request = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        // Recheck both OS permissions; an empty or partial result map is not a grant.
        model.permissionResult(permissionsGranted())
    }
    private fun requestBluetooth(mode: BleScanModeV1 = permissionScanMode) {
        model.permissionRequestStarted(mode)
        request.launch(permissions)
    }
    private fun startScan(mode: BleScanModeV1) {
        permissionScanMode = mode
        val granted = permissionsGranted()
        model.permissionsChanged(granted)
        when {
            granted -> model.scan(mode)
            permissions.any { shouldShowRequestPermissionRationale(it) } -> showPermissionExplanation = true
            else -> requestBluetooth()
        }
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        nfcScreen = savedInstanceState?.getBoolean("nfcScreen") ?: false
        scenarioScreen = savedInstanceState?.getBoolean("scenarioScreen") ?: false
        diagnosticScreen = BuildConfig.ALLOW_CONTRACT_LAB && (BuildConfig.APPLICATION_ID.endsWith(".diagnosticpreview") || savedInstanceState?.getBoolean("diagnosticScreen") == true)
        setContent {
            MaterialTheme(colorScheme = lightColorScheme()) {
                val state by model.state.collectAsStateWithLifecycle()
                if (showPermissionExplanation) {
                    AlertDialog(
                        onDismissRequest = { showPermissionExplanation = false },
                        title = { Text("Geräte in der Nähe") },
                        text = { Text("Zum Suchen und Verbinden mit dem ESP benötigt die App Bluetoothzugriff. Ohne diese Erlaubnis bleiben Suche und Verbindung ausgeschaltet.") },
                        confirmButton = { TextButton(onClick = { showPermissionExplanation = false; requestBluetooth() }) { Text("Erlaubnis anfragen") } },
                        dismissButton = { TextButton(onClick = { showPermissionExplanation = false }) { Text("Abbrechen") } }
                    )
                }
                Surface(Modifier.fillMaxSize()) {
                    if (BuildConfig.APPLICATION_ID.endsWith(".diagnosticpreview") && diagnosticScreen) {
                        val diagnosticState by diagnosticModel.state.collectAsStateWithLifecycle()
                        Column(Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.safeDrawing).imePadding().padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                FilterChip(selected = false, onClick = { selectDiagnostic(false) }, label = { Text("ESP · BLE") }, modifier = Modifier.heightIn(min = 48.dp))
                                FilterChip(selected = true, onClick = {}, label = { Text("Simulation") }, modifier = Modifier.heightIn(min = 48.dp))
                            }
                            Box(Modifier.weight(1f)) {
                                DiagnosticScreen(diagnosticState, diagnosticModel, exportFile = { if (diagnosticModel.beginExport()) exportDiagnostic.launch("diagnose-simulation-v1.json") })
                            }
                        }
                    } else {
                    Column(Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.safeDrawing).imePadding().verticalScroll(rememberScrollState()).padding(24.dp), verticalArrangement = Arrangement.spacedBy(16.dp)) {
                        Text("Larp Escape", style = MaterialTheme.typography.headlineLarge)
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            FilterChip(selected = !nfcScreen && !scenarioScreen && !diagnosticScreen, onClick = { selectDiagnostic(false); selectScenario(false); selectNfc(false) }, label = { Text("ESP") }, modifier = Modifier.heightIn(min = 48.dp))
                            if (!BuildConfig.APPLICATION_ID.endsWith(".diagnosticpreview")) {
                                FilterChip(selected = nfcScreen && !scenarioScreen, onClick = { selectScenario(false); selectNfc(true) }, label = { Text("NFC-Tags") })
                                FilterChip(selected = scenarioScreen, onClick = { selectScenario(true) }, label = { Text("Szenario") })
                            }
                        }
                        if (BuildConfig.ALLOW_CONTRACT_LAB) {
                            FilterChip(selected = diagnosticScreen, onClick = { selectDiagnostic(true) }, label = { Text("Diagnose · Simulation") }, modifier = Modifier.heightIn(min = 48.dp))
                        }
                        if (diagnosticScreen) {
                            val diagnosticState by diagnosticModel.state.collectAsStateWithLifecycle()
                            DiagnosticScreen(diagnosticState, diagnosticModel, exportFile = { if (diagnosticModel.beginExport()) exportDiagnostic.launch("diagnose-simulation-v1.json") })
                        } else if (scenarioScreen) {
                            val scenarioState by scenarioModel.state.collectAsStateWithLifecycle()
                            ScenarioScreen(scenarioState, scenarioModel,
                                importFile = { importScenario.launch(arrayOf("application/json", "text/plain")) },
                                exportFile = { if (scenarioModel.beginExport()) exportScenario.launch("szenario-entwurf.json") })
                        } else if (nfcScreen) {
                            val nfcState by nfcModel.state.collectAsStateWithLifecycle()
                            NfcScreen(nfcState, nfcModel::prepareWrite, nfcModel::cancelWrite)
                        } else {
                        Text("Gerät verbinden", style = MaterialTheme.typography.titleLarge)
                        Text(state.message)
                        if (BuildConfig.ALLOW_CONTRACT_LAB) NfcCapabilityCard(
                            export = {
                                nfcCapabilityExportBytes = runCatching { NfcCapabilityRun.exportBytes(this@MainActivity) }.getOrNull()
                                if (nfcCapabilityExportBytes != null) exportNfcCapability.launch("nfc-beobachtung-v1.json")
                                else nfcCapabilityExportNotice = "NFC-Bericht nicht lesbar; kein Export erstellt."
                            },
                            exportNotice = nfcCapabilityExportNotice)
                        if (state.permissionMissing) Text("Suche und Verbindung benötigen die Erlaubnis „Geräte in der Nähe“.")
                        Card(Modifier.fillMaxWidth()) {
                            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                                Text("Reale BLE-Diagnoseverfügbarkeit", style = MaterialTheme.typography.titleMedium)
                                Text(state.diagnosticAvailability.detail)
                                Text("Diese Anzeige bestätigt keine Gerätefreigabe oder Hardwarefunktion.", style = MaterialTheme.typography.bodySmall)
                            }
                        }
                        Card(Modifier.fillMaxWidth()) {
                            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                                Text("BLE-Kopplung beobachten", style = MaterialTheme.typography.titleMedium)
                                Text("Bericht vor der Suche starten, dann den ESP verbinden. Android erfasst Verbindung, Produktdienst und lokalen Bond-Status mit Zeit und Herkunft.")
                                Text("Ein OS-Bond ist kein Firmware-Nachweis für MITM, Rolle oder Diagnosefreigabe.", style = MaterialTheme.typography.bodySmall)
                                Button(onClick = { model.startPairingReport(); pairingExportNotice = null }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) {
                                    Text("Neuen Kopplungsbericht starten")
                                }
                                state.pairingReport?.let { report ->
                                    val assessment = report.androidAssessment()
                                    Text("Bericht ${report.reportId.take(8)} · ${report.events.size} Beobachtungen · Beginn ${report.startedUtc}")
                                    Text(assessment.label, style = MaterialTheme.typography.titleSmall)
                                    if (assessment.observedOsBondSequences.isNotEmpty()) {
                                        Text("Android meldete OS_BONDED in diesem Bericht (Belege: ${assessment.observedOsBondSequences.joinToString()}). " +
                                            if (assessment.latestObservedOsBondState == "OS_BONDED")
                                                "Dies ist ein lokaler Bond-Status, keine aktuelle GATT-Verbindung."
                                            else "Späterer Android-Bond-Status: ${assessment.latestObservedOsBondState ?: "unbekannt"}.",
                                            style = MaterialTheme.typography.bodySmall)
                                    }
                                    if (assessment.previousBondFailureSequences.isNotEmpty() && assessment.code != "ANDROID_PAIRING_OBSERVED")
                                        Text("Früherer Android-Bond-Versuch scheiterte; Belege im Bericht: ${assessment.previousBondFailureSequences.joinToString()}.",
                                            style = MaterialTheme.typography.bodySmall)
                                    Text("Firmware-Belege offen: Verbindungsgeneration, Bond-Schlüssel, Verschlüsselung/MITM und Rolle.")
                                    report.events.takeLast(5).forEach { event ->
                                        Text("${event.recordedUtc} · ${event.kind}: ${event.value}", style = MaterialTheme.typography.bodySmall)
                                    }
                                    if (state.connected) OutlinedButton(onClick = model::requestBond,
                                        modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Android-Kopplung anfragen") }
                                    OutlinedButton(onClick = {
                                        pairingExportBytes = model.pairingReportBytes()
                                        if (pairingExportBytes != null) exportPairing.launch("ble-kopplungsbeobachtung-v1.json")
                                    }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Beobachtungsbericht exportieren") }
                                }
                                pairingExportNotice?.let { Text(it) }
                            }
                        }
                        Card(Modifier.fillMaxWidth()) {
                            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                                Text("Gespeicherter BLE-Transportbeleg", style = MaterialTheme.typography.titleMedium)
                                Text("Der letzte GATT-Verbindungsverlauf kann auch nach einem App-Neustart exportiert werden. Er enthält keine bestätigte Rolle, Diagnosefreigabe oder physische Geräteprüfung.")
                                OutlinedButton(onClick = {
                                    connectionExportNotice = null
                                    try {
                                        if (model.connectionEvidenceExportBytes() != null) exportConnection.launch("ble-transportverlauf-v1.json")
                                        else connectionExportNotice = "Noch kein gespeicherter GATT-Verbindungsverlauf vorhanden."
                                    } catch (_: Exception) {
                                        connectionExportNotice = "Gespeicherter Transportbeleg ist nicht lesbar; kein Export erstellt."
                                    }
                                }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) {
                                    Text("BLE-Transportbeleg exportieren")
                                }
                                connectionExportNotice?.let { Text(it) }
                            }
                        }
                        Button(onClick = { startScan(BleScanModeV1.PRODUCT) }, enabled = !state.scanning,
                            modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) {
                            Text(if (state.scanning && state.scanMode == BleScanModeV1.PRODUCT) "Produktsuche läuft …" else "ESP suchen und Diagnose prüfen")
                        }
                        OutlinedButton(onClick = { startScan(BleScanModeV1.UNVERIFIED_DIAGNOSTIC) },
                            enabled = !state.scanning, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) {
                            Text(if (state.scanning && state.scanMode == BleScanModeV1.UNVERIFIED_DIAGNOSTIC)
                                "Diagnosesuche läuft …" else "Alle BLE-Kandidaten anzeigen (Diagnose)")
                        }
                        if (state.scanMode == BleScanModeV1.UNVERIFIED_DIAGNOSTIC)
                            Text("Diagnosesuche: Auch fremde Geräte erscheinen. Name und Adresse beweisen nicht, dass es die Box ist. Erst der Produktdienst wird nach dem Verbinden geprüft.",
                                style = MaterialTheme.typography.bodySmall)
                        state.targets.forEach { target ->
                            OutlinedButton(onClick = { model.connect(target.id) }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) {
                                Column {
                                    Text("${target.name} · ${when (target.kind) {
                                        DeviceTargetKindV1.PRODUCT_ADVERTISED -> "Produktdienst beworben, unbestätigt"
                                        DeviceTargetKindV1.LAB_ADVERTISED -> "Vertragslabor beworben"
                                        DeviceTargetKindV1.UNVERIFIED -> "Unbestätigter Kandidat"
                                    }}")
                                    Text("BLE-Adresse: ${target.id}", style = MaterialTheme.typography.bodySmall)
                                    Text("Antippen: verbinden und Dienst prüfen", style = MaterialTheme.typography.bodySmall)
                                }
                            }
                        }
                        if (state.connected) {
                            Button(onClick = model::disconnect, modifier = Modifier.fillMaxWidth()) { Text("Verbindung trennen") }
                            if (state.lab) Text("Vertragslabor: Testgeräte und eingespeiste Rechte. Keine Produktanmeldung.")
                            state.status?.let { Card(Modifier.fillMaxWidth()) {
                                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                                    Text(if (state.freshness == StatusFreshness.STALE) "Überholter Status · letzter Empfang" else "Zuletzt empfangener Status",
                                        style = MaterialTheme.typography.titleMedium)
                                    Text(it)
                                }
                            } }
                        }
                        Text("Version 0.1 · lesender Verbindungsprüfstand", style = MaterialTheme.typography.bodySmall)
                        }
                    }
                    }
                }
            }
        }
    }
    override fun onStart() { super.onStart(); if (!nfcScreen && !scenarioScreen && !diagnosticScreen) model.foregroundStarted(permissionsGranted()) }
    override fun onResume() { super.onResume(); if (nfcScreen && !scenarioScreen && !diagnosticScreen) nfcReader.start() else if (!scenarioScreen && !diagnosticScreen) model.permissionsChanged(permissionsGranted()) }
    override fun onPause() { nfcReader.stop(); super.onPause() }
    override fun onSaveInstanceState(outState: Bundle) { outState.putBoolean("diagnosticScreen", diagnosticScreen); outState.putBoolean("nfcScreen", nfcScreen); outState.putBoolean("scenarioScreen", scenarioScreen); super.onSaveInstanceState(outState) }
    override fun onStop() {
        if (!isChangingConfigurations) { showPermissionExplanation = false; model.foregroundEnded(); if (diagnosticScreen) diagnosticModel.backgroundEnded() }
        super.onStop()
    }
}
