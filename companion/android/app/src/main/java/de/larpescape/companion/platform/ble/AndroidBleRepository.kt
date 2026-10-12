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
import de.larpescape.companion.data.diagnostic_observation.DiagnosticConnectionEvidenceJournalV1
import de.larpescape.protocol.FrameAssembler
import de.larpescape.protocol.ConfiguredStatusDecoder
import de.larpescape.protocol.ConfiguredStatusTracker
import de.larpescape.protocol.StatusFreshness
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.util.UUID
import java.time.Instant
import java.io.File

// Lint cannot follow the shared permission predicate/guard through asynchronous
// callbacks. Entry points check both permissions; guarded catches revocation.
// Teardown catches SecurityException separately so it remains safe after revoke.
@SuppressLint("MissingPermission")
class AndroidBleRepository(private val context: Context) : DeviceRepository {
    private val mutable = MutableStateFlow(DeviceUiState())
    override val state = mutable.asStateFlow()
    private val pairingJournal = PairingObservationJournalV1(context.filesDir)
    private val diagnosticEvidence = DiagnosticConnectionEvidenceJournalV1(File(context.filesDir, "diagnostic-connection-v1"))
    init {
        runCatching { diagnosticEvidence.loadRecovered() }
            .onFailure { Log.w("LarpEscape", "Stored diagnostic connection evidence could not be read", it) }
        runCatching { pairingJournal.loadRecovered(Instant.now().toString()) }
            .onSuccess { if (it != null) mutable.value = mutable.value.copy(pairingReport = it,
                message = "Gespeicherter Kopplungsbericht geladen · Verbindung erneut prüfen") }
            .onFailure { mutable.value = mutable.value.copy(message = "Gespeicherter Kopplungsbericht nicht lesbar: ${it.message}") }
    }
    private val handler = Handler(Looper.getMainLooper())
    private val adapter = context.getSystemService(BluetoothManager::class.java)?.adapter
    private val devices = mutableMapOf<String, BluetoothDevice>()
    private var scanner: BluetoothLeScanner? = null
    private var scanCallback: ScanCallback? = null
    private var gatt: BluetoothGatt? = null
    private var diagnostic: AndroidDiagnosticGattShellV1? = null
    private var generation = 0
    private var selectedAddress: String? = null
    private var pendingBondAddress: String? = null
    private var pendingBondGeneration: Int? = null
    private var scanGeneration = 0
    private var scanEvidence: BleScanEvidenceV1? = null
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
            if (intent?.action == BluetoothDevice.ACTION_PAIRING_REQUEST && permitted()) {
                val device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java)
                if (device != null && (device.address == selectedAddress && gatt != null || device.address == pendingBondAddress)) {
                    val variant = intent.getIntExtra(BluetoothDevice.EXTRA_PAIRING_VARIANT, BluetoothDevice.ERROR)
                    // Never read EXTRA_PAIRING_KEY. A broadcast is not proof that a dialog was displayed.
                    observePairing("ANDROID_PAIRING_REQUEST_BROADCAST", "pairing_request_variant",
                        pairingVariantLabel(variant), pendingBondGeneration ?: generation, device.address)
                }
                return
            }
            if (intent?.action == BluetoothDevice.ACTION_BOND_STATE_CHANGED && permitted()) {
                val device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java)
                if (device != null && (device.address == selectedAddress && gatt != null || device.address == pendingBondAddress)) {
                    val bond = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, BluetoothDevice.ERROR)
                    val previous = intent.getIntExtra(BluetoothDevice.EXTRA_PREVIOUS_BOND_STATE, BluetoothDevice.ERROR)
                    observePairing("ANDROID_BOND_STATE_BROADCAST", "bond_transition",
                        "previous=${bondLabel(previous)};current=${bondLabel(bond)}",
                        pendingBondGeneration ?: generation, device.address)
                    observePairing("ANDROID_BOND_STATE_BROADCAST", "bond_state", bondLabel(bond),
                        pendingBondGeneration ?: generation, device.address)
                    if (bond == BluetoothDevice.BOND_BONDED || bond == BluetoothDevice.BOND_NONE) {
                        pendingBondAddress = null; pendingBondGeneration = null
                    }
                }
                return
            }
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
    private fun observePairing(source: String, kind: String, value: String,
                               observedGeneration: Int? = generation.takeIf { selectedAddress != null },
                               observedAddress: String? = selectedAddress) {
        val current = mutable.value.pairingReport ?: return
        val next = current.observe(Instant.now().toString(), source, observedGeneration, observedAddress, kind, value)
        if (next === current) return
        mutable.value = mutable.value.copy(pairingReport = next)
        runCatching { pairingJournal.save(next) }
            .onFailure { report("Kopplungsbericht konnte nicht gespeichert werden: ${it.message}") }
    }
    private fun bondLabel(state: Int) = when (state) {
        BluetoothDevice.BOND_BONDED -> "OS_BONDED"
        BluetoothDevice.BOND_BONDING -> "OS_BONDING"
        BluetoothDevice.BOND_NONE -> "OS_NOT_BONDED"
        else -> "OS_UNKNOWN_$state"
    }
    private fun pairingVariantLabel(variant: Int) = when (variant) {
        BluetoothDevice.PAIRING_VARIANT_PIN -> "PIN_REQUEST"
        BluetoothDevice.PAIRING_VARIANT_PASSKEY_CONFIRMATION -> "PASSKEY_CONFIRMATION"
        // Other platform variants are not exposed as SDK constants at compileSdk 35.
        else -> "OS_VARIANT_$variant"
    }
    private fun observeBond(device: BluetoothDevice) {
        try { if (permitted()) observePairing("ANDROID_BLUETOOTH_DEVICE_GET_BOND_STATE", "bond_state", bondLabel(device.bondState)) }
        catch (_: SecurityException) { observePairing("ANDROID_BLUETOOTH_DEVICE_GET_BOND_STATE", "bond_state", "PERMISSION_REVOKED") }
    }
    override fun startPairingReport() {
        pendingBondAddress = null; pendingBondGeneration = null
        val initial = PairingObservationReportV1.start()
        runCatching { pairingJournal.clearPendingExport(); pairingJournal.save(initial) }
            .onFailure { report("Neuer Kopplungsbericht konnte nicht gespeichert werden: ${it.message}"); return }
        mutable.value = mutable.value.copy(pairingReport = initial)
        observePairing("ANDROID_APP_USER_ACTION", "report_started", "user_requested")
        if (gatt != null) {
            observePairing("ANDROID_APP_STATE", "current_connection", "already_open_before_report_start")
            selectedAddress?.let { devices[it]?.let(::observeBond) }
        }
    }
    override fun pairingReportBytes(): ByteArray? {
        val report = mutable.value.pairingReport ?: return null
        return runCatching { pairingJournal.prepareExport(report, Instant.now().toString()) }
            .onFailure { report("Kopplungsbericht konnte nicht für Export gesichert werden: ${it.message}") }.getOrNull()
    }
    override fun pendingPairingReportBytes(): ByteArray? = runCatching { pairingJournal.pendingExport() }
        .onFailure { report("Gesicherter Export nicht lesbar: ${it.message}") }.getOrNull()
    override fun pairingExportCompleted() {
        runCatching { pairingJournal.clearPendingExport() }
            .onFailure { report("Export abgeschlossen; Exportkopie konnte nicht gelöscht werden: ${it.message}") }
    }
    override fun connectionEvidenceExportBytes(): ByteArray? = diagnosticEvidence.exportHistoryBytes()
    override fun requestBond() = guarded {
        val address = selectedAddress ?: error("Zuerst einen ESP verbinden")
        check(gatt != null && mutable.value.connected) { "Aktive Verbindung fehlt" }
        val device = devices[address] ?: error("Zielgerät nicht mehr bekannt")
        val bondState = device.bondState
        observePairing("ANDROID_BLUETOOTH_DEVICE_GET_BOND_STATE", "bond_state", bondLabel(bondState))
        if (bondRequestDecisionV1(bondLabel(bondState)) == BondRequestDecisionV1.ALREADY_BONDED) {
            report("Android meldet dieses Gerät bereits als gekoppelt; kein neuer Bond angefragt")
            return@guarded
        }
        if (bondRequestDecisionV1(bondLabel(bondState)) == BondRequestDecisionV1.IN_PROGRESS) {
            report("Android-Kopplung läuft bereits; auf OS-Ergebnis warten")
            return@guarded
        }
        val started = device.createBond()
        if (started) { pendingBondAddress = address; pendingBondGeneration = generation }
        observePairing("ANDROID_BLUETOOTH_DEVICE_CREATE_BOND", "bond_request", if (started) "OS_REQUEST_ACCEPTED" else "OS_REQUEST_REJECTED")
        report(if (started) "Android-Kopplung angefragt; auf OS-Ergebnis warten" else "Android konnte Kopplung nicht starten")
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
                // Only subscribe to these protected framework actions.
                context.registerReceiver(bluetoothState, IntentFilter().apply {
                    addAction(BluetoothAdapter.ACTION_STATE_CHANGED)
                    addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED)
                    addAction(BluetoothDevice.ACTION_PAIRING_REQUEST)
                }, Context.RECEIVER_EXPORTED)
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
    override fun scan(mode: BleScanModeV1) = guarded {
        if (adapter?.isEnabled != true) {
            clearSession(); mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.BLUETOOTH_OFF)
            report("Bluetooth einschalten, danach erneut suchen"); return@guarded
        }
        stopScan(); disconnect(); devices.clear()
        mutable.value = DeviceUiState(message = "Suche läuft …", scanning = true, scanMode = mode,
            pairingReport = mutable.value.pairingReport,
            diagnosticAvailability = DiagnosticAvailabilityV1.SEARCHING)
        observePairing("ANDROID_BLUETOOTH_LE_SCANNER", "scan_started", "mode=${mode.reportValue};requested")
        val token = ++scanGeneration
        scanner = adapter.bluetoothLeScanner ?: error("BLE-Scanner fehlt")
        scanEvidence = BleScanEvidenceV1()
        scanCallback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) { handler.post {
                if (token != scanGeneration) return@post
                scanEvidence?.callback()
                guarded {
                    val name = result.scanRecord?.deviceName ?: result.device.name ?: "Unbenannt"
                    val services = result.scanRecord?.serviceUuids.orEmpty().map { it.uuid }
                    val advertisesLab = labService in services
                    val advertisesProduct = canonical in services
                    val id = result.device.address
                    if (scanEvidence?.productCandidate(id, advertisesProduct) == true)
                        observePairing("ANDROID_BLUETOOTH_LE_SCAN_RECORD", "product_candidate",
                            "canonical_service_advertised", null, id)
                    val kind = classifyBleTargetV1(mode, name, advertisesProduct, advertisesLab,
                        BuildConfig.ALLOW_CONTRACT_LAB)
                    if (kind == null) { scanEvidence?.filtered(); return@guarded }
                    val existing = mutable.value.targets.indexOfFirst { it.id == id }
                    if (existing >= 0) {
                        if (kind == DeviceTargetKindV1.PRODUCT_ADVERTISED &&
                            mutable.value.targets[existing].kind != kind)
                            mutable.value = mutable.value.copy(targets = mutable.value.targets.toMutableList().also {
                                it[existing] = DeviceTarget(id, name, kind)
                            })
                        return@guarded
                    }
                    val limit = if (mode == BleScanModeV1.PRODUCT) 16 else 32
                    if (devices.size >= limit) {
                        if (kind != DeviceTargetKindV1.PRODUCT_ADVERTISED) return@guarded
                        val displaced = mutable.value.targets.firstOrNull { it.kind == DeviceTargetKindV1.UNVERIFIED }
                            ?: return@guarded
                        devices.remove(displaced.id)
                        mutable.value = mutable.value.copy(targets = mutable.value.targets - displaced)
                    }
                    devices[id] = result.device
                    mutable.value = mutable.value.copy(targets = mutable.value.targets + DeviceTarget(id, name, kind))
                }
            } }
            override fun onScanFailed(errorCode: Int) { handler.post {
                if (token == scanGeneration) {
                    observePairing("ANDROID_BLUETOOTH_LE_SCAN_CALLBACK", "scan_failed", errorCode.toString())
                    stopScan("failed"); mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.SEARCH_FAILED)
                    report("Suche fehlgeschlagen ($errorCode)")
                }
            } }
        }
        scanner!!.startScan(null, ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), scanCallback)
        handler.postDelayed({ if (token == scanGeneration) {
            stopScan("complete"); mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.SEARCH_COMPLETE)
            report("Suche beendet: ${devices.size} Geräte")
        } }, 10000)
    }
    private fun stopScan(reason: String = "stopped") {
        scanEvidence?.let { observePairing("ANDROID_BLUETOOTH_LE_SCANNER", "scan_end",
            "mode=${mutable.value.scanMode.reportValue};${it.summary(reason)}", null, null) }
        scanEvidence = null
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
        val selectedKind = mutable.value.targets.firstOrNull { it.id == id }?.kind
            ?: error("Ausgewähltes Gerät erneut suchen")
        val token = ++generation
        pendingBondAddress = null; pendingBondGeneration = null
        selectedAddress = id
        observePairing("ANDROID_APP_USER_ACTION", "candidate_selected",
            "${selectedKind.name.lowercase()};user_selected")
        observePairing("ANDROID_APP_USER_ACTION", "connection_attempt", "connectGatt_requested")
        observeBond(device)
        statuses.connect()
        mutable.value = mutable.value.copy(freshness = StatusFreshness.WAITING,
            diagnosticAvailability = DiagnosticAvailabilityV1.CONNECTING)
        report("Verbinde mit ${device.name ?: "ESP"} …")
        val callback = object : BluetoothGattCallback() {
            private fun dispatch(connection: BluetoothGatt, action: () -> Unit) { handler.post {
                if (token == generation && connection === gatt) guarded(action)
            } }
            override fun onConnectionStateChange(connection: BluetoothGatt, status: Int, newState: Int) = dispatch(connection) {
                observePairing("ANDROID_BLUETOOTH_GATT_CALLBACK", "connection_state", "status=$status;state=$newState")
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    disconnect()
                    mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.CONNECTION_FAILED)
                    report("GATT-Verbindungsfehler $status")
                    return@dispatch
                }
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    observeBond(device)
                    report("BLE verbunden; prüfe Dienste …")
                    check(connection.discoverServices()) { "Dienstsuche konnte nicht starten" }
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) { disconnect(); report("Verbindung getrennt") }
            }
            override fun onServicesDiscovered(connection: BluetoothGatt, status: Int) = dispatch(connection) {
                observePairing("ANDROID_BLUETOOTH_GATT_CALLBACK", "service_discovery", "status=$status")
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    disconnect()
                    mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.DISCOVERY_FAILED)
                    report("Dienstsuche fehlgeschlagen ($status)")
                    return@dispatch
                }
                val canonicalPresent = connection.getService(canonical) != null
                val labPresent = BuildConfig.ALLOW_CONTRACT_LAB && connection.getService(labService) != null
                val lab = if (selectedKind == DeviceTargetKindV1.LAB_ADVERTISED && labPresent)
                    connection.getService(labService) else null
                observePairing("ANDROID_BLUETOOTH_GATT_SERVICE_TABLE", "product_service_present", canonicalPresent.toString())
                observePairing("ANDROID_BLUETOOTH_GATT_SERVICE_TABLE", "lab_service_present", labPresent.toString())
                observeBond(device)
                if (!acceptsGattServiceV1(selectedKind, canonicalPresent, labPresent)) {
                    disconnect()
                    mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.PRODUCT_SERVICE_MISSING)
                    report("Kein passender ESP-Produktdienst; Kandidat bleibt unbestätigt")
                    return@dispatch
                }
                if (lab == null) {
                    val evidenceReady = runCatching { diagnosticEvidence.begin(token, id) }
                        .onFailure { Log.e("LarpEscape", "Diagnostic evidence storage failed", it) }.isSuccess
                    if (evidenceReady) {
                        val shell = AndroidDiagnosticGattShellV1(connection, token, targetAddress = id,
                            evidenceSink = { event -> runCatching { diagnosticEvidence.append(event) }
                                .onFailure { Log.e("LarpEscape", "Diagnostic evidence write failed", it) }.getOrDefault(false) })
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
                    } else {
                        mutable.value = mutable.value.copy(diagnosticAvailability = DiagnosticAvailabilityV1.OTHER_BLOCK)
                        report("Diagnoseereignisse nicht speicherbar · Diagnosezugriff gesperrt")
                    }
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
                    if (diagnostic?.onDevelopmentInfoRead(characteristic, copy,
                            status == BluetoothGatt.GATT_SUCCESS) == true) {
                        diagnostic?.let(::reportDiagnostic)
                        return@dispatch
                    }
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
                    diagnostic?.readDevelopmentInfoIfReady()
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
                    diagnostic?.readDevelopmentInfoIfReady()
                    diagnostic?.let(::reportDiagnostic)
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
        if (gatt != null) observePairing("ANDROID_APP_OR_GATT", "connection_closed", "local_session_ended")
        ++generation; diagnostic?.close(); diagnostic = null; frames.clear(); receivedStatus = false
        statuses.disconnect(); expiry?.let { handler.removeCallbacks(it) }; expiry = null
        val old = gatt; gatt = null
        selectedAddress = null
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
