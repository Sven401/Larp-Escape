package de.larpescape.companion.platform.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.content.BroadcastReceiver
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import de.larpescape.companion.BuildConfig
import de.larpescape.companion.data.device.*
import de.larpescape.protocol.FrameAssembler
import de.larpescape.protocol.ConfiguredStatusDecoder
import de.larpescape.protocol.ConfiguredStatusTracker
import de.larpescape.protocol.StatusFreshness
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.util.UUID

// Lint cannot follow the shared permission predicate/guard through asynchronous
// callbacks. Entry points check both permissions; guarded catches revocation.
// Teardown catches SecurityException separately so it remains safe after revoke.
@SuppressLint("MissingPermission")
class AndroidBleRepository(private val context: Context) : DeviceRepository {
    private val mutable = MutableStateFlow(DeviceUiState())
    override val state = mutable.asStateFlow()
    private val handler = Handler(Looper.getMainLooper())
    private val adapter = context.getSystemService(BluetoothManager::class.java)?.adapter
    private val devices = mutableMapOf<String, BluetoothDevice>()
    private var scanner: BluetoothLeScanner? = null
    private var scanCallback: ScanCallback? = null
    private var gatt: BluetoothGatt? = null
    private var diagnostic: AndroidDiagnosticGattShellV1? = null
    private var generation = 0
    private var scanGeneration = 0
    private val frames = FrameAssembler()
    private val labService = UUID.fromString("86ddc5b0-3878-4ff7-9f55-17c62aeb1001")
    private val canonical = UUID.fromString("407e6a1d-1cc2-43fd-856b-4aff85a20001")
    private val markerId = UUID.fromString("86ddc5b0-3878-4ff7-9f55-17c62aeb1002")
    private val outputId = UUID.fromString("86ddc5b0-3878-4ff7-9f55-17c62aeb1004")
    private val cccd = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    private var receivedStatus = false
    // Observation policy: 30s status transfer budget + 1s source admission age.
    // Not a guaranteed RF cadence or an authorization lifetime.
    private val statuses = ConfiguredStatusTracker(31000)
    private var expiry: Runnable? = null
    private var receiverRegistered = false
    private val bluetoothState = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (!receiverRegistered) return
            if (intent?.action == BluetoothAdapter.ACTION_STATE_CHANGED) {
                when (intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)) {
                    BluetoothAdapter.STATE_OFF, BluetoothAdapter.STATE_TURNING_OFF -> {
                        clearSession()
                        mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.BLUETOOTH_OFF)
                        report("Bluetooth ausgeschaltet · erneut einschalten und suchen")
                    }
                    BluetoothAdapter.STATE_TURNING_ON -> {
                        clearSession(); mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.BLUETOOTH_OFF)
                        report("Bluetooth startet …")
                    }
                    BluetoothAdapter.STATE_ON -> {
                        if (permitted()) {
                            mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.DISCONNECTED)
                            report("Bluetooth eingeschaltet · bereit zum Suchen")
                        }
                        else permissionsChanged(false)
                    }
                }
            }
        }
    }
    private fun permitted() = listOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        .all { context.checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }
    private fun report(message: String) {
        mutable.value = mutable.value.copy(message = message)
        Log.i("LarpEscape", message)
    }
    private fun reportDiagnostic(shell: AndroidDiagnosticGattShellV1) {
        if (diagnostic === shell)
            mutable.value = mutable.value.copy(diagnosticAvailability = diagnosticAvailability(shell.link.phase, shell.link.failure, shell.blockStatus))
    }
    private fun guarded(action: () -> Unit) {
        if (!permitted()) { permissionsChanged(false); return }
        try { action() }
        catch (_: SecurityException) { permissionsChanged(false); report("Bluetoothberechtigung entzogen · Suche und Verbindung beendet") }
        catch (e: Exception) { clearSession(); report(e.message ?: "Bluetoothfehler") }
    }
    override fun permissionsChanged(granted: Boolean, denied: Boolean) {
        if (!granted || !permitted()) {
            val message = when {
                denied -> "Bluetoothzugriff abgelehnt · Suche und Verbindung bleiben ausgeschaltet"
                mutable.value.permissionMissing -> mutable.value.message
                else -> "Bluetoothzugriff fehlt · Suche und Verbindung sind ausgeschaltet"
            }
            close()
            mutable.value = mutable.value.copy(permissionMissing = true, message = message,
                diagnosticAvailability = DiagnosticAvailabilityV1.PERMISSION_MISSING)
        } else {
            if (!receiverRegistered) {
                // Bluetooth broadcasts can originate from the privileged Bluetooth UID.
                // Only subscribe to this protected framework action.
                context.registerReceiver(bluetoothState, IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED), Context.RECEIVER_EXPORTED)
                receiverRegistered = true
            }
            if (mutable.value.permissionMissing) {
                mutable.value = mutable.value.copy(permissionMissing = false,
                    diagnosticAvailability = DiagnosticAvailabilityV1.DISCONNECTED)
                report("Bluetoothzugriff erlaubt · bereit zum Suchen")
            }
            try {
                if (adapter?.isEnabled != true) {
                    clearSession(); mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.BLUETOOTH_OFF)
                    report("Bluetooth einschalten, danach erneut suchen")
                }
            } catch (_: SecurityException) { permissionsChanged(false) }
        }
    }
    override fun scan() = guarded {
        if (adapter?.isEnabled != true) {
            clearSession(); mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.BLUETOOTH_OFF)
            report("Bluetooth einschalten, danach erneut suchen"); return@guarded
        }
        stopScan(); disconnect(); devices.clear()
        mutable.value = DeviceUiState(message = "Suche läuft …", scanning = true,
            diagnosticAvailability = DiagnosticAvailabilityV1.SEARCHING)
        val token = ++scanGeneration
        scanner = adapter.bluetoothLeScanner ?: error("BLE-Scanner fehlt")
        scanCallback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) { handler.post {
                if (token != scanGeneration) return@post
                guarded {
                    val name = result.scanRecord?.deviceName ?: result.device.name ?: "Unbenannt"
                    val services = result.scanRecord?.serviceUuids.orEmpty().map { it.uuid }
                    val advertisesLab = labService in services
                    if (!BuildConfig.ALLOW_CONTRACT_LAB && advertisesLab && canonical !in services) return@guarded
                    if (!(BuildConfig.ALLOW_CONTRACT_LAB && advertisesLab) && canonical !in services && !name.contains("Larp", true) && !name.contains("ESP", true)) return@guarded
                    val id = result.device.address
                    if (devices.size >= 16 || devices.containsKey(id)) return@guarded
                    devices[id] = result.device
                    mutable.value = mutable.value.copy(targets = mutable.value.targets + DeviceTarget(id, name))
                    Log.i("LarpEscape", "SCAN_TARGET name=$name")
                }
            } }
            override fun onScanFailed(errorCode: Int) { handler.post {
                if (token == scanGeneration) {
                    stopScan(); mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.SEARCH_FAILED)
                    report("Suche fehlgeschlagen ($errorCode)")
                }
            } }
        }
        scanner!!.startScan(null, ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), scanCallback)
        handler.postDelayed({ if (token == scanGeneration) {
            stopScan(); mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.SEARCH_COMPLETE)
            report("Suche beendet: ${devices.size} Geräte")
        } }, 10000)
    }
    private fun stopScan() {
        ++scanGeneration
        try { if (permitted()) scanCallback?.let { scanner?.stopScan(it) } }
        catch (_: SecurityException) { report("Bluetoothberechtigung entzogen") }
        scanCallback = null
        mutable.value = mutable.value.copy(scanning = false)
    }
    override fun connect(id: String) = guarded {
        stopScan(); disconnect()
        check(permitted()) { "Bluetoothberechtigung fehlt" }
        val device = devices[id] ?: error("Gerät erneut suchen")
        val token = ++generation
        statuses.connect()
        mutable.value = mutable.value.copy(freshness = StatusFreshness.WAITING,
            diagnosticAvailability = DiagnosticAvailabilityV1.CONNECTING)
        report("Verbinde mit ${device.name ?: "ESP"} …")
        val callback = object : BluetoothGattCallback() {
            private fun dispatch(connection: BluetoothGatt, action: () -> Unit) { handler.post {
                if (token == generation && connection === gatt) guarded(action)
            } }
            override fun onConnectionStateChange(connection: BluetoothGatt, status: Int, newState: Int) = dispatch(connection) {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    disconnect()
                    mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.CONNECTION_FAILED)
                    report("GATT-Verbindungsfehler $status")
                    return@dispatch
                }
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    report("BLE verbunden; prüfe Dienste …")
                    check(connection.discoverServices()) { "Dienstsuche konnte nicht starten" }
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) { disconnect(); report("Verbindung getrennt") }
            }
            override fun onServicesDiscovered(connection: BluetoothGatt, status: Int) = dispatch(connection) {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    disconnect()
                    mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.DISCOVERY_FAILED)
                    report("Dienstsuche fehlgeschlagen ($status)")
                    return@dispatch
                }
                val lab = if (BuildConfig.ALLOW_CONTRACT_LAB) connection.getService(labService) else null
                if (lab == null && connection.getService(canonical) == null) {
                    disconnect()
                    mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.PRODUCT_SERVICE_MISSING)
                    report("Kein passender ESP-Produktdienst")
                    return@dispatch
                }
                if (lab == null) {
                    val shell = AndroidDiagnosticGattShellV1(connection, token)
                    diagnostic = shell
                    val started = shell.discover()
                    reportDiagnostic(shell)
                    if (!started) Log.i("LarpEscape", "DIAGNOSTIC_GATT_BLOCKED ${shell.blockStatus}")
                    else handler.postDelayed({
                        if (token == generation && connection === gatt && diagnostic === shell) {
                            shell.expireSetup()
                            reportDiagnostic(shell)
                            if (shell.link.failure == de.larpescape.protocol.DiagnosticGattFailureV1.SETUP_TIMEOUT)
                                Log.i("LarpEscape", "DIAGNOSTIC_GATT_BLOCKED ${shell.blockStatus}")
                        }
                    }, 10000)
                } else mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.LAB_ONLY)
                mutable.value = mutable.value.copy(connected = true, lab = lab != null)
                report("GATT_SERVICE_CONFIRMED ${if (lab != null) "v2-Vertragslabor" else "Produktservice"}")
                if (lab != null) {
                    val marker = lab.getCharacteristic(markerId) ?: error("Labmarker fehlt")
                    check(connection.readCharacteristic(marker)) { "Markerlesen nicht gestartet" }
                } else report("ESP verbunden. Produktanmeldung noch offen.")
            }
            override fun onCharacteristicRead(connection: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray, status: Int) {
                val copy = value.copyOf()
                dispatch(connection) {
                    if (characteristic.uuid != markerId) return@dispatch
                    check(status == BluetoothGatt.GATT_SUCCESS && copy.toString(Charsets.UTF_8) == "V2 LAB; fake owners; injected authority; RAM journal") { "Labmarker ungültig" }
                    val output = connection.getService(labService).getCharacteristic(outputId) ?: error("Statusausgabe fehlt")
                    check(connection.setCharacteristicNotification(output, true)) { "Notifications fehlgeschlagen" }
                    val descriptor = output.getDescriptor(cccd) ?: error("Statusabonnement fehlt")
                    check(connection.writeDescriptor(descriptor, byteArrayOf(1, 0)) == BluetoothStatusCodes.SUCCESS) { "Abonnement nicht gestartet" }
                }
            }
            override fun onDescriptorWrite(connection: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) = dispatch(connection) {
                if (descriptor.characteristic?.uuid == de.larpescape.protocol.DiagnosticGattFirmwareV1.route.controlReply ||
                    descriptor.characteristic?.uuid == de.larpescape.protocol.DiagnosticGattFirmwareV1.route.stepReply) {
                    diagnostic?.onDescriptorWrite(descriptor, status == BluetoothGatt.GATT_SUCCESS)
                    Log.i("LarpEscape", "DIAGNOSTIC_GATT ${diagnostic?.blockStatus}")
                    diagnostic?.let(::reportDiagnostic)
                    return@dispatch
                }
                if (descriptor.characteristic?.uuid != outputId) return@dispatch
                check(status == BluetoothGatt.GATT_SUCCESS) { "Abonnement fehlgeschlagen" }
                report("ESP verbunden; warte auf vollständigen Status …")
                handler.postDelayed({ if (token == generation && !receivedStatus) { disconnect(); report("Vollständiger Status fehlt nach 40 Sekunden") } }, 40000)
            }
            override fun onCharacteristicChanged(connection: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
                val copy = value.copyOf()
                dispatch(connection) {
                    diagnostic?.onNotification(characteristic, copy)
                    if (characteristic.uuid != outputId) return@dispatch
                    val complete = frames.accept(copy, SystemClock.elapsedRealtime()) ?: return@dispatch
                    if (complete.first != 4) return@dispatch
                    val status = ConfiguredStatusDecoder.decode(complete.second)
                    if (!statuses.accept(status, SystemClock.elapsedRealtime())) {
                        Log.i("LarpEscape", "V2_STATUS_IGNORED stale-sample")
                        return@dispatch
                    }
                    val summary = "Boot: ${status.boot}\nSample: ${status.sample}\nProfil: ${status.application.phase.wire}\nSpiel läuft: ${status.game.running}\nGeräte: ${status.devices.size}"
                    receivedStatus = true
                    mutable.value = mutable.value.copy(status = summary, configuredStatus = status, freshness = StatusFreshness.CURRENT)
                    expiry?.let { handler.removeCallbacks(it) }
                    expiry = Runnable {
                        if (token == generation && connection === gatt) {
                            val reading = statuses.read(SystemClock.elapsedRealtime())
                            mutable.value = mutable.value.copy(freshness = reading.freshness)
                            if (reading.freshness == StatusFreshness.STALE) report("Status überholt · keine neue Messung seit 31 Sekunden")
                        }
                    }.also { handler.postDelayed(it, 31000) }
                    Log.i("LarpEscape", "V2_STATUS_COMPLETE " + complete.second.toString(Charsets.UTF_8))
                    report("Verbunden · vollständiger ESP-Status empfangen")
                }
            }
            override fun onMtuChanged(connection: BluetoothGatt, mtu: Int, status: Int) = dispatch(connection) {
                if (diagnostic?.onMtuChanged(mtu, status == BluetoothGatt.GATT_SUCCESS) == false)
                    Log.i("LarpEscape", "DIAGNOSTIC_GATT_BLOCKED ${diagnostic?.blockStatus}")
                diagnostic?.let(::reportDiagnostic)
            }
            override fun onCharacteristicWrite(connection: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) = dispatch(connection) {
                diagnostic?.onCharacteristicWrite(characteristic, status == BluetoothGatt.GATT_SUCCESS)
            }
        }
        gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
        check(gatt != null) { "Verbindung konnte nicht starten" }
        handler.postDelayed({ if (token == generation && !mutable.value.connected) { disconnect(); report("Verbindung nach 20 Sekunden abgebrochen") } }, 20000)
    }
    override fun disconnect() {
        ++generation; diagnostic?.close(); diagnostic = null; frames.clear(); receivedStatus = false
        statuses.disconnect(); expiry?.let { handler.removeCallbacks(it) }; expiry = null
        val old = gatt; gatt = null
        try { if (permitted()) old?.disconnect() } catch (_: SecurityException) { }
        try { old?.close() } catch (_: SecurityException) { }
        mutable.value = mutable.value.copy(connected = false, status = null, configuredStatus = null,
            freshness = StatusFreshness.DISCONNECTED, lab = false, message = "Getrennt",
            diagnosticAvailability = DiagnosticAvailabilityV1.DISCONNECTED)
    }
    private fun clearSession() {
        stopScan(); disconnect(); handler.removeCallbacksAndMessages(null)
        devices.clear()
        mutable.value = mutable.value.copy(targets = emptyList())
    }
    override fun close() {
        val permissionMessage = mutable.value.message.takeIf { mutable.value.permissionMissing }
        clearSession()
        if (permissionMessage != null) mutable.value = mutable.value.copy(message = permissionMessage)
        if (receiverRegistered) { context.unregisterReceiver(bluetoothState); receiverRegistered = false }
    }
}
