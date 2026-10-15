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
import de.larpescape.companion.ui.device.LedControls
import de.larpescape.companion.ui.device.WakeControls
import de.larpescape.protocol.LedAccess
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

class MainActivity : ComponentActivity() {
    private var nfcScreen by mutableStateOf(false)
    private var scenarioScreen by mutableStateOf(false)
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
    private val importBoxAccess = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) lifecycleScope.launch {
            val bytes = try {
                withContext(Dispatchers.IO) { contentResolver.openInputStream(uri)?.use { it.readNBytes(LedAccess.MAX_BYTES + 1) } ?: ByteArray(0) }
            } catch (_: Exception) { ByteArray(0) }
            model.importLedAccess(bytes)
            bytes.fill(0)
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
    private fun permissionsGranted() = permissions.all { checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }
    private val request = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        // Recheck both OS permissions; an empty or partial result map is not a grant.
        model.permissionResult(permissionsGranted())
    }
    private fun requestBluetooth() {
        model.permissionRequestStarted()
        request.launch(permissions)
    }
    private fun startScan() {
        val granted = permissionsGranted()
        model.permissionsChanged(granted)
        when {
            granted -> model.scan()
            permissions.any { shouldShowRequestPermissionRationale(it) } -> showPermissionExplanation = true
            else -> requestBluetooth()
        }
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        nfcScreen = savedInstanceState?.getBoolean("nfcScreen") ?: false
        scenarioScreen = savedInstanceState?.getBoolean("scenarioScreen") ?: false
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
                    Column(Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.safeDrawing).verticalScroll(rememberScrollState()).padding(24.dp), verticalArrangement = Arrangement.spacedBy(16.dp)) {
                        Text("Larp Escape", style = MaterialTheme.typography.headlineLarge)
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            FilterChip(selected = !nfcScreen && !scenarioScreen, onClick = { selectScenario(false); selectNfc(false) }, label = { Text("ESP") })
                            FilterChip(selected = nfcScreen && !scenarioScreen, onClick = { selectScenario(false); selectNfc(true) }, label = { Text("NFC-Tags") })
                            FilterChip(selected = scenarioScreen, onClick = { selectScenario(true) }, label = { Text("Szenario") })
                        }
                        if (scenarioScreen) {
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
                        if (state.permissionMissing) Text("Suche und Verbindung benötigen die Erlaubnis „Geräte in der Nähe“.")
                        Button(onClick = ::startScan, enabled = !state.scanning, modifier = Modifier.fillMaxWidth()) { Text(if (state.scanning) "Suche läuft …" else "ESP suchen") }
                        state.targets.forEach { target ->
                            OutlinedButton(onClick = { model.connect(target.id) }, modifier = Modifier.fillMaxWidth()) { Text("${target.name} verbinden") }
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
                        WakeControls(state.leds.wake)
                        LedControls(state.leds, model::selectLedEffect, model::startLedEffect, model::stopLeds,
                            importAccess = { importBoxAccess.launch(arrayOf("application/json", "text/plain")) })
                        Text("LED-Bedienung · Geräteantwort und sichtbare Wirkung getrennt", style = MaterialTheme.typography.bodySmall)
                        }
                    }
                }
            }
        }
    }
    override fun onStart() { super.onStart(); if (!nfcScreen && !scenarioScreen) model.foregroundStarted(permissionsGranted()) }
    override fun onResume() { super.onResume(); if (nfcScreen && !scenarioScreen) nfcReader.start() else if (!scenarioScreen) model.permissionsChanged(permissionsGranted()) }
    override fun onPause() { nfcReader.stop(); super.onPause() }
    override fun onSaveInstanceState(outState: Bundle) { outState.putBoolean("nfcScreen", nfcScreen); outState.putBoolean("scenarioScreen", scenarioScreen); super.onSaveInstanceState(outState) }
    override fun onStop() {
        if (!isChangingConfigurations) { showPermissionExplanation = false; model.foregroundEnded() }
        super.onStop()
    }
}
