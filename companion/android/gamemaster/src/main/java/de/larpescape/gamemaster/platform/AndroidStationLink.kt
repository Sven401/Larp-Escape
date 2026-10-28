package de.larpescape.gamemaster.platform

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.content.pm.PackageManager
import android.os.Handler
import android.os.ParcelUuid
import android.os.SystemClock
import de.larpescape.gamemaster.station.*
import de.larpescape.protocol.LedAccess
import de.larpescape.protocol.LedFrameAssembler
import de.larpescape.protocol.LedFrames
import java.util.ArrayDeque
import java.util.UUID

/** Main-thread owner of one exact-address discovery and one mutually authenticated read-only link. */
@SuppressLint("MissingPermission")
class AndroidStationLink(private val context: Context, private val handler: Handler) : StationLink {
    private val adapter get() = context.getSystemService(BluetoothManager::class.java)?.adapter
    private var generation = 0L
    private var scanner: BluetoothLeScanner? = null
    private var scan: ScanCallback? = null
    private var connection: BluetoothGatt? = null
    private var listener: StationLink.Listener? = null
    private var proof: StationProof? = null
    private var assembler = LedFrameAssembler()
    private var input: BluetoothGattCharacteristic? = null
    private var subscribed = false
    private var writing = false
    private var transfer = 0
    private val frames = ArrayDeque<ByteArray>()
    private val timers = mutableSetOf<Runnable>()
    private var connectDeadline: Runnable? = null
    private var fragmentDeadline: Runnable? = null
    private fun permitted() = listOf(Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT).all {
        context.checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED
    }
    private fun after(milliseconds: Long, action: () -> Unit): Runnable {
        val token = generation
        lateinit var task: Runnable
        task = Runnable { timers.remove(task); if (token == generation) action() }
        timers.add(task); handler.postDelayed(task,milliseconds); return task
    }
    private fun cancel(task: Runnable?) { task?.let { handler.removeCallbacks(it); timers.remove(it) } }
    private fun fail(reason: LinkFailure) { val target = listener; close(); target?.failed(reason) }
    private inline fun guarded(action: () -> Unit) {
        if (!permitted()) { fail(LinkFailure.PERMISSION); return }
        try {
            if (adapter?.isEnabled != true) { fail(LinkFailure.BLUETOOTH_OFF); return }
            action()
        } catch (_: SecurityException) { fail(LinkFailure.PERMISSION) }
        catch (_: AssignmentMismatch) { fail(LinkFailure.ASSIGNMENT) }
        catch (_: Exception) { fail(LinkFailure.ACCESS) }
    }
    override fun connect(station: Station, access: LedAccess, listener: StationLink.Listener) {
        close(); this.listener = listener
        val token = generation
        guarded {
            proof = StationProof(access)
            scanner = adapter!!.bluetoothLeScanner ?: error("BLE-Scanner fehlt")
            scan = object : ScanCallback() {
                override fun onScanResult(callbackType: Int, result: ScanResult) { handler.post {
                    if (token != generation || scan !== this) return@post
                    guarded {
                        // The address is a prepared locator, never the identity proof. HMAC verifies the full device ID.
                        if (!result.device.address.equals(station.address,true)) return@guarded
                        stopScan()
                        connectDevice(result.device,token)
                    }
                } }
                override fun onScanFailed(errorCode: Int) { handler.post { if (token == generation) fail(LinkFailure.UNREACHABLE) } }
            }
            val filter = ScanFilter.Builder().setDeviceAddress(station.address).setServiceUuid(ParcelUuid(SERVICE)).build()
            scanner!!.startScan(listOf(filter),ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),scan)
            after(10000) { if (scan != null) fail(LinkFailure.UNREACHABLE) }
        }
    }
    private fun stopScan() {
        val callback = scan; scan = null
        runCatching { if (callback != null) scanner?.stopScan(callback) }; scanner = null
    }
    private fun connectDevice(device: BluetoothDevice, token: Long) {
        val callback = object : BluetoothGattCallback() {
            private fun dispatch(gatt: BluetoothGatt, action: () -> Unit) { handler.post {
                if (token == generation && connection === gatt) guarded(action)
            } }
            override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) = dispatch(gatt) {
                if (status != BluetoothGatt.GATT_SUCCESS || newState == BluetoothProfile.STATE_DISCONNECTED) fail(LinkFailure.INTERRUPTED)
                else if (newState == BluetoothProfile.STATE_CONNECTED && !gatt.discoverServices()) fail(LinkFailure.UNREACHABLE)
            }
            override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) = dispatch(gatt) {
                if (status != BluetoothGatt.GATT_SUCCESS) { fail(LinkFailure.UNREACHABLE); return@dispatch }
                val service = gatt.getService(SERVICE)
                val command = service?.getCharacteristic(COMMAND)
                val output = service?.getCharacteristic(OUTPUT)
                if (command == null || output == null || command.properties and BluetoothGattCharacteristic.PROPERTY_WRITE == 0 || output.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY == 0) {
                    fail(LinkFailure.ASSIGNMENT); return@dispatch
                }
                input = command
                val descriptor = output.getDescriptor(CCCD) ?: run { fail(LinkFailure.ASSIGNMENT); return@dispatch }
                check(gatt.setCharacteristicNotification(output,true))
                check(gatt.writeDescriptor(descriptor,BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) == BluetoothStatusCodes.SUCCESS)
                after(10000) { if (!subscribed || connectDeadline != null) fail(LinkFailure.ACCESS) }
            }
            override fun onDescriptorWrite(gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) = dispatch(gatt) {
                check(!subscribed && descriptor.uuid == CCCD && descriptor.characteristic.uuid == OUTPUT && status == BluetoothGatt.GATT_SUCCESS)
                subscribed = true; send(proof!!.hello())
            }
            override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) = dispatch(gatt) {
                check(characteristic.uuid == COMMAND && writing && status == BluetoothGatt.GATT_SUCCESS && frames.isNotEmpty())
                frames.removeFirst(); writing = false; writeNext()
            }
            override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
                if (value.size !in 9..20) { dispatch(gatt) { fail(LinkFailure.ACCESS) }; return }
                val copy = value.copyOf()
                dispatch(gatt) {
                    check(subscribed && characteristic.uuid == OUTPUT)
                    val message = assembler.accept(copy,SystemClock.elapsedRealtime())
                    if (message == null) {
                        if (fragmentDeadline == null) fragmentDeadline = after(2000) {
                            fragmentDeadline = null
                            if (assembler.expired(SystemClock.elapsedRealtime())) fail(LinkFailure.ACCESS)
                        }
                        return@dispatch
                    }
                    cancel(fragmentDeadline); fragmentDeadline = null
                    val reading = proof!!.receive(message)
                    reading.reply?.let(::send)
                    reading.status?.let {
                        cancel(connectDeadline); connectDeadline = null
                        listener?.confirmed(it)
                    }
                }
            }
        }
        connection = device.connectGatt(context,false,callback,BluetoothDevice.TRANSPORT_LE)
        if (connection == null) { fail(LinkFailure.UNREACHABLE); return }
        connectDeadline = after(20000) { fail(LinkFailure.UNREACHABLE) }
    }
    private fun send(message: ByteArray) {
        check(message.size >= 2 && message[0] == 2.toByte() && message[1].toInt() in 1..2)
        check(transfer < 65535 && frames.size <= 4)
        frames.addAll(LedFrames.encode(message,++transfer)); writeNext()
    }
    private fun writeNext() {
        if (writing || frames.isEmpty()) return
        writing = true
        check(connection!!.writeCharacteristic(input!!,frames.first(),BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS)
    }
    override fun close() {
        generation++; timers.forEach { handler.removeCallbacks(it) }; timers.clear(); connectDeadline = null; fragmentDeadline = null
        stopScan(); val old = connection; connection = null
        runCatching { old?.disconnect() }; runCatching { old?.close() }
        proof?.close(); proof = null; listener = null; input = null; frames.clear(); writing = false; subscribed = false; transfer = 0; assembler = LedFrameAssembler()
    }
    companion object {
        val SERVICE: UUID = UUID.fromString("6c790001-2f50-4d5a-a7f4-35a478c4b101")
        val COMMAND: UUID = UUID.fromString("6c790002-2f50-4d5a-a7f4-35a478c4b101")
        val OUTPUT: UUID = UUID.fromString("6c790003-2f50-4d5a-a7f4-35a478c4b101")
        val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }
}
