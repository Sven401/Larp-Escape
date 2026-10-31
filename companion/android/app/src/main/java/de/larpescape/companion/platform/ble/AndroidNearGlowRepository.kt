package de.larpescape.companion.platform.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.content.pm.PackageManager
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.os.SystemClock
import android.util.Log
import de.larpescape.companion.BuildConfig
import de.larpescape.companion.data.device.*
import de.larpescape.companion.platform.access.NearGlowAccessStore
import de.larpescape.protocol.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.security.MessageDigest
import java.security.SecureRandom
import java.util.UUID

/** Two independent foreground-only GATT owners. No ARM, STOP or RF command exists here. */
@SuppressLint("MissingPermission")
class AndroidNearGlowRepository(private val context: Context) {
    private val mutable = MutableStateFlow(NearGlowUiState())
    val state = mutable.asStateFlow()
    private val handler = Handler(Looper.getMainLooper())
    private val adapter = context.getSystemService(BluetoothManager::class.java)?.adapter
    private val store = NearGlowAccessStore(context)
    private val accesses = arrayOfNulls<NearGlowAccess>(2)
    private val sessions = arrayOfNulls<Session>(2)
    private val awaiting = mutableSetOf<Int>()
    private var scanner: BluetoothLeScanner? = null
    private var scanCallback: ScanCallback? = null
    private var scanGeneration = 0
    private var foreground = false
    private val desired = arrayOfNulls<Pair<Long, NearGlowCurve>>(2)
    private fun now() = SystemClock.elapsedRealtime()
    private fun update(slot: Int, action: (NearGlowStationUi) -> NearGlowStationUi) {
        mutable.value = mutable.value.copy(stations = mutable.value.stations.mapIndexed { i, old -> if (i == slot) action(old) else old }, now = now())
    }
    private fun permitted() = listOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        .all { context.checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }
    init {
        for (slot in 0..1) try {
            store.read(slot)?.let { bytes ->
                try { accesses[slot] = NearGlowAccess.parse(bytes) } finally { bytes.fill(0) }
            }
            accesses[slot]?.let { update(slot) { old -> old.copy(mac = it.stationMac, message = "Zugang gespeichert · bereit zum Verbinden") } }
        } catch (_: Exception) { update(slot) { it.copy(message = "Stationszugang nicht lesbar · erneut importieren") } }
        if (accesses[0] != null && accesses[1] != null && sameStation(accesses[0]!!, accesses[1]!!)) {
            accesses[1] = null
            update(1) { it.copy(mac = null, message = "Doppelte Station · anderen Zugang importieren") }
        }
    }
    private fun sameStation(a: NearGlowAccess, b: NearGlowAccess) = a.stationMac == b.stationMac || a.deviceId().contentEquals(b.deviceId())
    fun importAccess(slot: Int, bytes: ByteArray) {
        if (slot !in 0..1 || sessions[slot] != null || slot in awaiting) return
        try {
            val access = NearGlowAccess.parse(bytes)
            val other = accesses[1 - slot]
            require(other == null || !sameStation(access, other))
            store.save(slot, bytes)
            accesses[slot] = access
            desired[slot] = null
            update(slot) { NearGlowStationUi(it.label, mac = access.stationMac, message = "Zugang gespeichert · bereit zum Verbinden") }
        } catch (_: Exception) { update(slot) { it.copy(message = "Zugangsimport fehlgeschlagen · eigene Stationsdatei prüfen") } }
    }
    fun edit(point: Int, rssi: String, percent: String) {
        if (point !in 0..3 || rssi.length > 5 || percent.length > 4) return
        val points = mutable.value.draft.points.toMutableList().apply { this[point] = NearGlowPointInput(rssi, percent) }
        mutable.value = mutable.value.copy(draft = NearGlowDraft(points), stations = mutable.value.stations.map {
            if (it.change == NearGlowChange.SENT) it else it.copy(change = NearGlowChange.DRAFT, changeMessage = "Entwurf · noch nicht übernommen")
        })
    }
    fun foregroundStarted(granted: Boolean) {
        foreground = true; permissionsChanged(granted); handler.removeCallbacks(tick); handler.post(tick)
    }
    fun foregroundEnded() {
        foreground = false; handler.removeCallbacks(tick); stopScan()
        for (slot in 0..1) disconnect(slot)
    }
    fun permissionsChanged(granted: Boolean) {
        if (!granted || !permitted()) {
            stopScan(); for (slot in 0..1) disconnect(slot)
            mutable.value = mutable.value.copy(permissionMissing = true)
        } else mutable.value = mutable.value.copy(permissionMissing = false)
    }
    private val tick = object : Runnable {
        override fun run() {
            if (!foreground) return
            mutable.value = mutable.value.copy(now = now())
            if (!permitted()) permissionsChanged(false)
            sessions.toList().filterNotNull().forEach { it.watch() }
            handler.postDelayed(this, 250)
        }
    }
    fun connect(slot: Int) {
        if (slot !in 0..1 || !foreground || accesses[slot] == null || sessions[slot] != null) return
        if (!permitted()) { permissionsChanged(false); return }
        try {
            if (adapter?.isEnabled != true) { update(slot) { it.copy(message = "Bluetooth einschalten und erneut verbinden") }; return }
            awaiting.add(slot)
            update(slot) { it.copy(busy = true, message = "Station wird gesucht …") }
            if (scanCallback != null) return
            val token = ++scanGeneration
            val callback = object : ScanCallback() {
                override fun onScanResult(callbackType: Int, result: ScanResult) {
                    val hint = result.scanRecord?.getManufacturerSpecificData(0xFFFF)?.copyOf() ?: return
                    handler.post {
                        if (token != scanGeneration || !foreground || !permitted()) return@post
                        for (index in awaiting.toList()) {
                            if (hint.contentEquals(accesses[index]?.deviceId())) {
                                awaiting.remove(index)
                                try {
                                    val session = Session(index, accesses[index]!!)
                                    sessions[index] = session
                                    session.open(result.device)
                                } catch (_: Exception) { fail(index, "Handyverbindung fehlgeschlagen · erneut verbinden") }
                            }
                        }
                        if (awaiting.isEmpty()) stopScan()
                    }
                }
                override fun onScanFailed(errorCode: Int) { handler.post {
                    if (token == scanGeneration) scanFailed("Suche fehlgeschlagen · erneut versuchen")
                } }
            }
            scanCallback = callback; scanner = adapter.bluetoothLeScanner
            check(scanner != null)
            scanner!!.startScan(listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(SERVICE)).build()),
                ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), callback)
            mutable.value = mutable.value.copy(scanning = true)
            handler.postDelayed({ if (token == scanGeneration) scanFailed("Station nicht gefunden · eingeschaltetes Gerät und Zugang prüfen") }, 12000)
        } catch (_: Exception) { scanFailed("Bluetoothzugriff fehlgeschlagen · Erlaubnis und Bluetooth prüfen") }
    }
    private fun scanFailed(message: String) {
        awaiting.toList().forEach { slot -> update(slot) { it.copy(busy = false, message = message) } }; stopScan()
    }
    private fun stopScan() {
        ++scanGeneration
        try { scanCallback?.let { scanner?.stopScan(it) } } catch (_: Exception) { }
        scanCallback = null; scanner = null; awaiting.clear()
        mutable.value = mutable.value.copy(scanning = false)
    }
    fun disconnect(slot: Int) {
        if (slot !in 0..1) return
        awaiting.remove(slot)
        if (awaiting.isEmpty() && scanCallback != null) stopScan()
        sessions[slot]?.close(); sessions[slot] = null
        update(slot) { it.disconnected() }
    }
    private fun fail(slot: Int, message: String) {
        disconnect(slot); update(slot) { it.copy(message = message) }
    }
    fun applyBoth() {
        val snapshot = mutable.value
        if (!snapshot.canApply) return
        val curve = snapshot.draft.curve ?: return
        for (slot in 0..1) {
            val session = sessions[slot]
            if (session == null) { update(slot) { it.copy(change = NearGlowChange.REJECTED, changeMessage = "Nicht gesendet · Handyverbindung fehlt") }; continue }
            try { session.apply(curve) }
            catch (_: Exception) { fail(slot, "Übernahme nicht bestätigt · neu verbinden und Gerätestand lesen") }
        }
    }

    private inner class Session(val slot: Int, val access: NearGlowAccess) {
        private val protocol = NearGlowProtocol(access, ByteArray(16).also { SecureRandom().nextBytes(it) })
        private var frames = NearGlowFrameAssembler()
        private val setup = NearGlowConnectionSetup()
        private val outgoing = ArrayDeque<ByteArray>()
        private var writing = false
        private var subscribed = false
        private var transfer = 0
        private var alive = true
        private var gatt: BluetoothGatt? = null
        private var input: BluetoothGattCharacteristic? = null
        private var authenticated = false
        private var pending = false
        private var pendingSet = false
        private var pendingToken = 0
        private var lastTelemetry: ProxTelemetry? = null
        private var lastCurve: ProxCurve? = null
        private var telemetryAt = 0L
        private var openedAt = 0L
        private var receivedFragments = 0
        private fun debugMilestone(event: NearGlowHandshakeMilestone, fragments: Int) {
            try {
                if (BuildConfig.DEBUG) Log.d(NearGlowCallbackDiagnostics.TAG,
                    NearGlowCallbackDiagnostics.milestone(slot, event, openedAt, now(), fragments))
            } catch (_: Exception) { /* Diagnostics never change transport state. */ }
        }
        private fun current() = alive && sessions[slot] === this
        private val callbackDispatcher = NearGlowCallbackDispatcher(
            post = { action -> handler.post { action() } },
            isCurrent = ::current,
            hasPermission = ::permitted,
            failed = { stage, error ->
                try {
                    if (BuildConfig.DEBUG) Log.d(NearGlowCallbackDiagnostics.TAG,
                        NearGlowCallbackDiagnostics.line(slot, stage, error))
                } catch (_: Exception) { /* Diagnostics must not replace session cleanup. */ }
                fail(slot, when (error) {
                    is NearGlowPriorityRequestRejected -> "Schnelle Handyverbindung nicht verfügbar · erneut verbinden"
                    is NearGlowMtuRequestRejected -> "Größere Bluetooth-Datenpakete nicht verfügbar · erneut verbinden"
                    is NearGlowMtuInsufficient -> "Bluetooth-Datenpakete sind zu klein · passende Stationsfirmware prüfen"
                    else -> "Verbindung oder Datenprüfung fehlgeschlagen · erneut verbinden"
                })
            },
        )
        private fun dispatchCallback(stage: NearGlowCallbackStage, action: () -> Unit) = callbackDispatcher.dispatch(stage, action)
        private val callback = object : BluetoothGattCallback() {
            override fun onConnectionStateChange(connection: BluetoothGatt, status: Int, newState: Int) { this@Session.dispatchCallback(NearGlowCallbackStage.CONNECTION) {
                check(connection === gatt && status == BluetoothGatt.GATT_SUCCESS && newState == BluetoothProfile.STATE_CONNECTED)
                update(slot) { it.copy(connected = true, message = "Handy verbunden · Station wird geprüft …") }
                setup.begin(
                    requestHighPriority = {
                        connection.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH).also {
                            if (it) debugMilestone(NearGlowHandshakeMilestone.HIGH_PRIORITY_REQUESTED, 0)
                        }
                    },
                    requestMtu = { connection.requestMtu(247) },
                )
                debugMilestone(NearGlowHandshakeMilestone.MTU_REQUESTED, 0)
            } }
            override fun onMtuChanged(connection: BluetoothGatt, mtu: Int, status: Int) { this@Session.dispatchCallback(NearGlowCallbackStage.MTU) {
                check(connection === gatt)
                setup.mtuChanged(status == BluetoothGatt.GATT_SUCCESS, mtu) {
                    frames = NearGlowFrameAssembler(setup.maxFrameBytes)
                    try {
                        if (BuildConfig.DEBUG) Log.d(NearGlowCallbackDiagnostics.TAG,
                            NearGlowCallbackDiagnostics.mtu(slot, mtu, setup.maxFrameBytes))
                    } catch (_: Exception) { /* Diagnostics never change MTU admission. */ }
                    connection.discoverServices()
                }
            } }
            override fun onServicesDiscovered(connection: BluetoothGatt, status: Int) { this@Session.dispatchCallback(NearGlowCallbackStage.SERVICES) {
                check(connection === gatt && status == BluetoothGatt.GATT_SUCCESS && setup.negotiatedMtu != null)
                val service = connection.getService(SERVICE) ?: error("service")
                input = service.getCharacteristic(INPUT) ?: error("input")
                val output = service.getCharacteristic(OUTPUT) ?: error("output")
                check(input!!.properties and BluetoothGattCharacteristic.PROPERTY_WRITE != 0)
                check(output.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY != 0)
                check(connection.setCharacteristicNotification(output, true))
                check(connection.writeDescriptor(output.getDescriptor(CCCD) ?: error("CCCD"),
                    BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) == BluetoothStatusCodes.SUCCESS)
            } }
            override fun onDescriptorWrite(connection: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) { this@Session.dispatchCallback(NearGlowCallbackStage.DESCRIPTOR) {
                check(connection === gatt && descriptor.uuid == CCCD && status == BluetoothGatt.GATT_SUCCESS)
                debugMilestone(NearGlowHandshakeMilestone.CCCD_ACK, 0)
                subscribed = true; writeNext()
            } }
            override fun onCharacteristicWrite(connection: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) { this@Session.dispatchCallback(NearGlowCallbackStage.WRITE) {
                val connectionMatches = connection === gatt
                val inputMatches = characteristic.uuid == INPUT
                val accepted = connectionMatches && inputMatches && status == BluetoothGatt.GATT_SUCCESS && writing
                if (!accepted) try {
                    if (BuildConfig.DEBUG) Log.d(NearGlowCallbackDiagnostics.TAG,
                        NearGlowCallbackDiagnostics.writeRejected(slot, status, writing, outgoing.size, connectionMatches, inputMatches))
                } catch (_: Exception) { /* Diagnostics never change ACK admission. */ }
                check(accepted)
                outgoing.removeFirst(); writing = false; writeNext()
                update(slot) { it.copy(busy = pending || outgoing.isNotEmpty()) }
            } }
            override fun onCharacteristicChanged(connection: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
                val bytes = value.copyOf()
                val fragmentAt = now()
                this@Session.dispatchCallback(NearGlowCallbackStage.NOTIFY) {
                    check(connection === gatt && characteristic.uuid == OUTPUT)
                    ++receivedFragments
                    frames.accept(bytes, fragmentAt)?.let { received(it, checkNotNull(frames.completedStartedAt)) }
                }
            }
        }
        fun open(device: BluetoothDevice) {
            openedAt = now()
            update(slot) { it.copy(busy = true, message = "Handyverbindung wird aufgebaut …") }
            gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
            check(gatt != null)
            handler.postDelayed({ if (current() && !authenticated) fail(slot, "Anmeldung nicht bestätigt · erneut verbinden") }, 15000)
        }
        private fun send(bytes: ByteArray) {
            check(transfer < 65535 && outgoing.isEmpty())
            update(slot) { it.copy(busy = true) }
            outgoing.addAll(NearGlowFrames.encode(bytes, ++transfer, setup.maxFrameBytes)); writeNext()
        }
        private fun writeNext() {
            if (!subscribed || writing || outgoing.isEmpty()) return
            writing = true
            check(gatt!!.writeCharacteristic(input!!, outgoing.first(), BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS)
        }
        private fun received(bytes: ByteArray, firstFragmentAt: Long) {
            check(bytes.size >= 4)
            when (bytes[1].toInt() and 255) {
                0x81 -> {
                    val proof = protocol.proof(bytes)
                    debugMilestone(NearGlowHandshakeMilestone.CHALLENGE_COMPLETE, receivedFragments)
                    send(proof)
                    debugMilestone(NearGlowHandshakeMilestone.PROOF_QUEUED, outgoing.size)
                }
                0x82 -> {
                    protocol.auth(bytes); authenticated = true
                    debugMilestone(NearGlowHandshakeMilestone.AUTH_OK, receivedFragments)
                    update(slot) { it.copy(authenticated = true, message = "Station geprüft · warte auf Livewerte …") }
                }
                0x84 -> {
                    val t = protocol.telemetry(bytes)
                    val peer = accesses[1 - slot]?.stationMac
                    require(peer != null && t.peer == peer)
                    lastTelemetry = t; telemetryAt = firstFragmentAt
                    val matches = lastCurve?.let { it.durable && it.revision == t.revision && MessageDigest.isEqual(it.digest, t.digest) && t.flag(10) } == true
                    update(slot) { it.copy(peer = t.peer, configMatchesTelemetry = matches,
                        message = "Handy verbunden · Station geprüft",
                        reading = NearGlowReading(telemetryAt, t.flag(4), t.sourceAge, t.filteredRssi,
                            t.level, t.confirmedRgb, t.flag(1), t.fault,
                            outputKnown = t.flag(7), outputPending = t.flag(8), configFault = t.configFault,
                            configurationRevision = t.revision.takeIf { t.flag(10) }, outputRevision = t.confirmedRevision.takeIf { revision -> revision > 0 })) }
                    if (!matches && !pending && outgoing.isEmpty() && now() - telemetryAt in 0..1000) requestCurve()
                }
                0x83 -> {
                    check(pending)
                    val result = protocol.result(bytes)
                    val wasSet = pendingSet
                    val trustedCurve = result.curve.durable && result.code != 5
                    pending = false; ++pendingToken; lastCurve = result.curve.takeIf { trustedCurve }
                    val curve = NearGlowCurve(result.curve.points.map { NearGlowPoint(it.rssi, it.percent) })
                    val target = desired[slot]
                    val confirmed = result.code == 0 && result.curve.durable &&
                        (wasSet || (target != null && result.curve.revision == target.first && curve == target.second && lastTelemetry?.configFault == 0))
                    val t = lastTelemetry
                    val matches = t != null && trustedCurve && t.flag(10) && result.curve.revision == t.revision && MessageDigest.isEqual(result.curve.digest, t.digest)
                    update(slot) { old ->
                        val outcome = nearGlowChangeOutcome(mutable.value.draft.curve, curve, confirmed, wasSet,
                            old.change, result.curve.revision, result.code, result.curve.durable)
                        old.copy(
                        busy = outgoing.isNotEmpty(), revision = result.curve.revision.takeIf { trustedCurve },
                        confirmed = curve.takeIf { trustedCurve }, configMatchesTelemetry = matches,
                        change = outcome.first, changeMessage = outcome.second) }
                }
                else -> error("type")
            }
        }
        private fun freshTelemetry(): ProxTelemetry {
            check(authenticated && now() - telemetryAt in 0..1000)
            return checkNotNull(lastTelemetry)
        }
        private fun begin(set: Boolean, message: ByteArray) {
            check(!pending && outgoing.isEmpty())
            pending = true; pendingSet = set
            val token = ++pendingToken
            send(message)
            handler.postDelayed({ if (current() && pending && pendingToken == token) fail(slot,
                "Gerätebestätigung ausgeblieben · Ausgang unklar, erneut verbinden") }, 5000)
        }
        private fun requestCurve() { begin(false, protocol.get(freshTelemetry().millis)) }
        fun apply(curve: NearGlowCurve) {
            val t = freshTelemetry()
            check(t.configFault == 0)
            val current = checkNotNull(lastCurve)
            check(current.durable && t.flag(10) && current.revision == t.revision && MessageDigest.isEqual(current.digest, t.digest))
            val points = curve.points.map { ProxPoint(it.rssi, it.percent) }
            val message = protocol.set(t.millis, current.revision, points)
            desired[slot] = Pair(current.revision + 1, curve)
            update(slot) { it.copy(change = NearGlowChange.SENT, changeMessage = "Gesendet · dauerhafte Bestätigung ausstehend") }
            begin(true, message)
        }
        fun close() {
            alive = false; ++pendingToken; protocol.close(); frames.clear(); outgoing.clear()
            try { gatt?.disconnect() } catch (_: Exception) { }
            try { gatt?.close() } catch (_: Exception) { }
            gatt = null
        }
        fun watch() { if (current() && frames.expired(now())) fail(slot, "Liveübertragung unvollständig · erneut verbinden") }
    }
    companion object {
        val SERVICE: UUID = UUID.fromString("6c790101-2f50-4d5a-a7f4-35a478c4b101")
        private val INPUT = UUID.fromString("6c790102-2f50-4d5a-a7f4-35a478c4b101")
        private val OUTPUT = UUID.fromString("6c790103-2f50-4d5a-a7f4-35a478c4b101")
        private val CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }
}
