package de.larpescape.gamemaster

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.Uri
import android.os.Bundle
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.lifecycleScope
import de.larpescape.gamemaster.station.*
import de.larpescape.protocol.LedAccess
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainActivity : ComponentActivity() {
    private val model by viewModels<StationViewModel>()
    private val permissions = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { model.environment() }
    private val enableBluetooth = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { model.environment() }
    private val importAccess = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) lifecycleScope.launch {
            try {
                val bytes = withContext(Dispatchers.IO) { contentResolver.openInputStream(uri)?.use { it.readNBytes(LedAccess.MAX_BYTES + 1) } ?: error("Datei fehlt") }
                model.importAccess(bytes)
            } catch (_: Exception) { model.importFailed() }
        }
    }
    private val bluetoothChanges = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) { if (intent?.action == BluetoothAdapter.ACTION_STATE_CHANGED) model.environment() }
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        title = getString(R.string.app_name)
        setContent {
            MaterialTheme(colorScheme = lightColorScheme()) {
                val state by model.controller.state.collectAsStateWithLifecycle()
                val catalog by model.catalog.collectAsStateWithLifecycle()
                val setup by model.setup.collectAsStateWithLifecycle()
                var setupVisible by rememberSaveable { mutableStateOf(false) }
                var editingId by rememberSaveable { mutableStateOf<String?>(null) }
                var help by rememberSaveable { mutableStateOf(false) }
                LaunchedEffect(setup.saved) { if (setup.saved) setupVisible = false }
                fun openSetup(station: Station?) {
                    editingId = station?.id; help = false; model.openSetup(); setupVisible = true
                }
                fun back() {
                    if (setupVisible && !setup.busy) { setupVisible = false; model.cancelSetup() }
                    else if (help) help = false
                    else if (!setupVisible) model.controller.stations()
                }
                BackHandler(setupVisible || help || state.station != null) { back() }
                Surface(Modifier.fillMaxSize()) {
                    Column(Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.safeDrawing)
                        .verticalScroll(rememberScrollState()).padding(24.dp),verticalArrangement = Arrangement.spacedBy(16.dp)) {
                        when {
                            setupVisible -> StationSetup(
                                catalog.stations.firstOrNull { it.id == editingId },setup,
                                import = { importAccess.launch(arrayOf("application/json","text/plain","application/octet-stream")) },
                                save = { event,name,address -> model.save(editingId,event,name,address) },cancel = ::back)
                            state.station == null -> {
                                Heading("Spielleitung",MaterialTheme.typography.labelLarge)
                                Heading("Stationen",MaterialTheme.typography.headlineLarge)
                                when {
                                    catalog.loading -> { CircularProgressIndicator(Modifier.size(28.dp)); Text("Stationen werden geladen …") }
                                    catalog.error != null -> { Text(catalog.error!!); Button(onClick = model::reload,modifier = Modifier.fillMaxWidth()) { Text("Erneut laden") } }
                                    catalog.stations.isEmpty() -> {
                                        Text("Auf diesem Handy sind noch keine Stationen vorbereitet.")
                                        Text("Bereite Veranstaltung, Station und Box-Zugang einmal vor. Danach genügt die Stationsauswahl.")
                                    }
                                    else -> {
                                        Text("Wähle die Station, die du vor Ort prüfen möchtest.")
                                        catalog.stations.groupBy { it.event }.forEach { (event,stations) ->
                                            Heading(event,MaterialTheme.typography.titleMedium)
                                            stations.forEach { station ->
                                                OutlinedCard(Modifier.fillMaxWidth().semantics(mergeDescendants = true) {}.clickable(role = Role.Button) { help = false; model.controller.select(station) }) {
                                                    Row(Modifier.padding(16.dp),verticalAlignment = androidx.compose.ui.Alignment.CenterVertically) {
                                                        Column(Modifier.weight(1f),verticalArrangement = Arrangement.spacedBy(4.dp)) {
                                                            Text(station.name,style = MaterialTheme.typography.titleLarge)
                                                            Text("Stationszustand ansehen",style = MaterialTheme.typography.bodyMedium)
                                                        }
                                                        TextButton(onClick = { openSetup(station) },modifier = Modifier.heightIn(min = 48.dp)) { Text("Vorbereitung") }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                if (!catalog.loading && catalog.error == null) OutlinedButton(onClick = { openSetup(null) },modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Station vorbereiten") }
                            }
                            else -> {
                                val station = state.station!!
                                TextButton(onClick = ::back,modifier = Modifier.heightIn(min = 48.dp)) { Text(if (help) "Zur Stationsbedienung" else "Stationen") }
                                Text(station.event,style = MaterialTheme.typography.labelLarge)
                                Heading(station.name,MaterialTheme.typography.headlineLarge)
                                if (help) {
                                    Heading("Einrichtung prüfen lassen",MaterialTheme.typography.titleLarge)
                                    Text("Wende dich an die Person, die die Veranstaltung eingerichtet hat.")
                                    Text(if (state.phase == StationPhase.ASSIGNMENT) "Prüfen, welche Box zu dieser Station gehört. Gerätezuordnung und Bluetoothadresse müssen zur vorbereiteten Box passen." else "Prüfen, ob auf diesem Handy die richtige Zugangsdatei für die zugeordnete Box hinterlegt ist.")
                                    Text("Nach der Korrektur zur Station zurückkehren und erneut prüfen. Wiederholen allein ändert keinen Zugang.")
                                    OutlinedButton(onClick = { openSetup(station) },modifier = Modifier.fillMaxWidth()) { Text("Lokale Vorbereitung bearbeiten") }
                                } else StationStatus(state,
                                    retry = model.controller::retry,pause = model.controller::pause,
                                    permission = { permissions.launch(arrayOf(Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT)) },
                                    settings = { startActivity(Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,Uri.parse("package:$packageName"))) },
                                    bluetooth = { runCatching { enableBluetooth.launch(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)) }.onFailure { model.environment() } },
                                    help = { help = true })
                            }
                        }
                    }
                }
            }
        }
    }
    override fun onStart() {
        super.onStart()
        registerReceiver(bluetoothChanges,IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED),Context.RECEIVER_EXPORTED)
        model.environment(); model.controller.foreground(true)
    }
    override fun onResume() { super.onResume(); model.environment() }
    override fun onStop() {
        model.controller.foreground(false)
        unregisterReceiver(bluetoothChanges)
        super.onStop()
    }
}

@Composable private fun Heading(text: String, style: androidx.compose.ui.text.TextStyle) {
    Text(text,style = style,modifier = Modifier.semantics { heading() })
}

@Composable private fun StationStatus(state: StationState,retry: () -> Unit,pause: () -> Unit,
    permission: () -> Unit,settings: () -> Unit,bluetooth: () -> Unit,help: () -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(12.dp),modifier = Modifier.fillMaxWidth().semantics { liveRegion = LiveRegionMode.Polite }) {
        val title = when (state.phase) {
            StationPhase.CONNECTING -> "Station wird verbunden …"
            StationPhase.CONFIRMED -> when { state.status!!.fault -> "Station meldet einen Fehler"; !state.status.ready -> "Station noch nicht bereit"; state.status.pending -> "Station ist beschäftigt"; else -> "Bereit" }
            StationPhase.PERMISSION -> "Gerätezugriff fehlt"
            StationPhase.BLUETOOTH_OFF -> "Bluetooth ist ausgeschaltet"
            StationPhase.UNSUPPORTED -> "Bluetooth wird nicht unterstützt"
            StationPhase.UNREACHABLE -> "Station nicht erreichbar"
            StationPhase.ACCESS -> "Stationszugang nicht bestätigt"
            StationPhase.ASSIGNMENT -> "Stationszuordnung unklar"
            StationPhase.INTERRUPTED -> "Verbindung unterbrochen"
            StationPhase.STALE -> "Status nicht aktuell"
            StationPhase.PAUSED -> "Verbindung pausiert"
            else -> "Verbindung wird erneut geprüft"
        }
        Heading(title,MaterialTheme.typography.titleLarge)
        if (state.phase == StationPhase.CONFIRMED) {
            val status = state.status!!
            Text(when (status.effect) { 0 -> "Licht aus"; 1 -> "Licht: Funkeln"; 2 -> "Licht: Lichtimpuls"; else -> "Licht: Ripple" },style = MaterialTheme.typography.headlineMedium)
            Text(when (status.mode) { 0 -> "Lichtbetrieb"; 1 -> "Spiel gestoppt"; 2 -> "Spiel läuft"; 3 -> "Spiel pausiert"; else -> "Spielzustand nicht bestätigt" })
        } else if (state.phase !in setOf(StationPhase.PERMISSION,StationPhase.BLUETOOTH_OFF,StationPhase.UNSUPPORTED)) Text("Aktueller Zustand unbekannt.")
        when (state.phase) {
            StationPhase.CONNECTING -> {
                CircularProgressIndicator(Modifier.size(28.dp))
                TextButton(onClick = pause,modifier = Modifier.heightIn(min = 48.dp)) { Text("Abbrechen") }
            }
            StationPhase.PERMISSION -> {
                Text("Zum Verbinden mit dieser Station wird Bluetoothzugriff benötigt.")
                Button(onClick = permission,modifier = Modifier.fillMaxWidth()) { Text("Zugriff erlauben") }
                TextButton(onClick = settings) { Text("App-Einstellungen öffnen") }
            }
            StationPhase.BLUETOOTH_OFF -> Button(onClick = bluetooth,modifier = Modifier.fillMaxWidth()) { Text("Bluetooth einschalten") }
            StationPhase.UNSUPPORTED -> Text("Prüfe diese Station mit einem Handy, das Bluetooth Low Energy unterstützt.")
            StationPhase.ACCESS,StationPhase.ASSIGNMENT -> {
                Text("Bitte die verantwortliche Person für die Veranstaltung, die lokale Einrichtung zu prüfen.")
                Button(onClick = help,modifier = Modifier.fillMaxWidth()) { Text("Hilfe zur Einrichtung") }
                TextButton(onClick = retry) { Text("Erneut prüfen") }
            }
            StationPhase.UNREACHABLE,StationPhase.INTERRUPTED,StationPhase.STALE -> {
                if (state.recovering) {
                    Text("Die Verbindung wird wiederhergestellt und der Zustand erneut geprüft.")
                    CircularProgressIndicator(Modifier.size(28.dp))
                    TextButton(onClick = pause) { Text("Abbrechen") }
                } else {
                    Text("Gehe näher an die Station und versuche es erneut.")
                    Button(onClick = retry,modifier = Modifier.fillMaxWidth()) { Text("Erneut versuchen") }
                }
            }
            StationPhase.PAUSED -> Button(onClick = retry,modifier = Modifier.fillMaxWidth()) { Text("Erneut verbinden") }
            StationPhase.CONFIRMED -> TextButton(onClick = pause,modifier = Modifier.heightIn(min = 48.dp)) { Text("Verbindung pausieren") }
            else -> Unit
        }
    }
}

@Composable private fun StationSetup(station: Station?,state: SetupState,import: () -> Unit,
    save: (String,String,String) -> Unit,cancel: () -> Unit) {
    var event by rememberSaveable(station?.id) { mutableStateOf(station?.event ?: "") }
    var name by rememberSaveable(station?.id) { mutableStateOf(station?.name ?: "") }
    var address by rememberSaveable(station?.id) { mutableStateOf(station?.address ?: "") }
    Heading("Lokale Vorbereitung",MaterialTheme.typography.headlineMedium)
    Text("Die verantwortliche Person ordnet eine konkrete Box zu. Diese Angaben werden nur in Larp GM gespeichert.")
    OutlinedTextField(event,{ event = it.take(80) },label = { Text("Veranstaltung") },enabled = !state.busy,singleLine = true,modifier = Modifier.fillMaxWidth())
    OutlinedTextField(name,{ name = it.take(64) },label = { Text("Station") },enabled = !state.busy,singleLine = true,modifier = Modifier.fillMaxWidth())
    OutlinedTextField(address,{ address = it.take(17) },label = { Text("Bluetoothadresse der Box") },supportingText = { Text("Aus der geprüften Boxeinrichtung, zum Beispiel AA:BB:CC:DD:EE:FF. Die App prüft die Boxidentität zusätzlich beim Verbinden.") },enabled = !state.busy,singleLine = true,modifier = Modifier.fillMaxWidth())
    OutlinedButton(onClick = import,enabled = !state.busy,modifier = Modifier.fillMaxWidth()) { Text(if (state.imported) "Andere Zugangsdatei auswählen" else "Box-Zugangsdatei auswählen") }
    if (state.imported) Text("Zugangsdatei ausgewählt.") else if (station != null) Text("Der bisherige Stationszugang bleibt hinterlegt, solange keine neue Datei gewählt wird.")
    Text("Verwende die vorhandene, vertrauenswürdig eingerichtete Box-Zugangsdatei. Der Import vergibt keine neuen Spielleitungsrechte.")
    state.error?.let { Text(it,color = MaterialTheme.colorScheme.error,modifier = Modifier.semantics { liveRegion = LiveRegionMode.Polite }) }
    Button(onClick = { save(event,name,address) },enabled = !state.busy && event.isNotBlank() && name.isNotBlank() && address.isNotBlank() && (station != null || state.imported),modifier = Modifier.fillMaxWidth()) { Text(if (state.busy) "Wird gespeichert …" else "Station speichern") }
    TextButton(onClick = cancel,enabled = !state.busy,modifier = Modifier.heightIn(min = 48.dp)) { Text("Abbrechen") }
}
