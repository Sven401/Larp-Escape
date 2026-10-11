package de.larpescape.companion.platform.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothStatusCodes
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import de.larpescape.protocol.*

/** Bound to exactly one BluetoothGatt instance and repository generation. No role issuer is wired yet. */
@SuppressLint("MissingPermission")
internal class AndroidDiagnosticGattShellV1(
    private val gatt: BluetoothGatt,
    val generation: Int,
    private val originalAuthority: () -> DiagnosticControlOriginV1? = { null },
) {
    val link = DiagnosticGattLinkV1(generation)
    private val handler = Handler(Looper.getMainLooper())
    private val route = DiagnosticGattFirmwareV1.route
    private var controlWrite: BluetoothGattCharacteristic? = null
    private var stepWrite: BluetoothGattCharacteristic? = null
    private var controlReply: BluetoothGattCharacteristic? = null
    private var stepReply: BluetoothGattCharacteristic? = null
    private var pendingControl: PendingDiagnosticControlV1? = null
    private var controlRequest: DiagnosticControlRequestV1? = null
    private var controlSubmittedAtMs = 0L
    private var pendingControlOrigin: DiagnosticControlOriginV1? = null
    private var controlWriteConfirmed = false
    private var pendingStep: DiagnosticStepAssemblyV1? = null
    private var pendingStepOrigin: DiagnosticControlOriginV1? = null
    private var stepWriteConfirmed = false
    private var grant: DiagnosticControlReplyV1? = null
    private var grantOrigin: DiagnosticControlOriginV1? = null
    private var grantHostDeadlineMs = 0L

    val blockStatus: String get() {
        val origin = originalAuthority()
        return when {
            link.phase != DiagnosticGattPhaseV1.TRANSPORT_READY -> "LINK_${link.phase}_${link.failure}"
            origin == null -> "ROLE_ISSUER_MISSING"
            origin.localGattGeneration != generation.toLong() -> "ROLE_GENERATION_MISMATCH"
            pendingControl != null -> "CONTROL_${pendingControl?.state}"
            pendingStep != null -> "STEP_${pendingStep?.status}"
            else -> "NONE_TRANSPORT_ONLY"
        }
    }

    fun discover(): Boolean {
        val service = gatt.getService(route.service)
        val expected = setOf(route.controlRequest, route.controlReply, route.stepRequest, route.stepReply)
        if (service != null && expected.any { id -> service.characteristics.count { it.uuid == id } > 1 }) {
            link.failed(DiagnosticGattFailureV1.AMBIGUOUS_CHARACTERISTIC); return false
        }
        val characteristics = service?.characteristics.orEmpty().associateBy { it.uuid }
        val writes = characteristics.values.filter { it.properties and BluetoothGattCharacteristic.PROPERTY_WRITE != 0 }.map { it.uuid }.toSet()
        val notifies = characteristics.values.filter { it.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY != 0 }.map { it.uuid }.toSet()
        val cccds = characteristics.values.filter { it.getDescriptor(CCCD) != null }.map { it.uuid }.toSet()
        if (!link.discovered(service?.uuid, writes, notifies, cccds)) return false
        controlWrite = characteristics[route.controlRequest]
        stepWrite = characteristics[route.stepRequest]
        controlReply = characteristics[route.controlReply]
        stepReply = characteristics[route.stepReply]
        if (!gatt.requestMtu(DiagnosticGattFirmwareV1.REQUIRED_MTU)) {
            link.failed(DiagnosticGattFailureV1.MTU_REQUEST_FAILED); return false
        }
        return true
    }

    fun onMtuChanged(mtu: Int, success: Boolean): Boolean {
        if (!link.mtu(mtu, success)) return false
        return subscribe(controlReply, DiagnosticGattFailureV1.CONTROL_CCCD_FAILED)
    }

    private fun subscribe(characteristic: BluetoothGattCharacteristic?, failure: DiagnosticGattFailureV1): Boolean {
        val descriptor = characteristic?.getDescriptor(CCCD)
        if (characteristic == null || descriptor == null || !gatt.setCharacteristicNotification(characteristic, true) ||
            gatt.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) != BluetoothStatusCodes.SUCCESS) {
            link.failed(failure); return false
        }
        return true
    }

    fun onDescriptorWrite(descriptor: BluetoothGattDescriptor, success: Boolean): Boolean {
        if (descriptor.uuid != CCCD) return false
        val characteristic = descriptor.characteristic ?: return false
        if (characteristic.service?.uuid != route.service) return false
        val expected = when (link.phase) {
            DiagnosticGattPhaseV1.CONTROL_CCCD_PENDING -> route.controlReply
            DiagnosticGattPhaseV1.STEP_CCCD_PENDING -> route.stepReply
            else -> return false
        }
        if (characteristic.uuid != expected) return false
        if (!link.subscribed(expected, success)) return false
        if (link.phase == DiagnosticGattPhaseV1.STEP_CCCD_PENDING)
            return subscribe(stepReply, DiagnosticGattFailureV1.STEP_CCCD_FAILED)
        return link.preflight().isEmpty()
    }

    /** No call site currently supplies originalAuthority; consequently no live diagnostic write is possible. */
    fun submitControl(request: DiagnosticControlRequestV1, nowMs: Long = SystemClock.elapsedRealtime()): Boolean {
        val origin = originalAuthority() ?: return false
        if (!link.canReceive(origin.localGattGeneration.toInt()) || link.preflight().isNotEmpty() ||
            pendingControl != null || pendingStep != null || origin.localGattGeneration != generation.toLong()) return false
        if (request.command == DiagnosticControlCommandV1.GRANT && grant != null) return false
        if (request.command == DiagnosticControlCommandV1.CANCEL) {
            if (grant?.grantId != request.grantId || grant?.runId != request.runId || grantOrigin != origin) return false
            grant = null; grantOrigin = null; grantHostDeadlineMs = 0 // Retire locally before the write.
        }
        val payload = DiagnosticControlV1.encode(request)
        if (payload.size > link.effectiveAttValueBytes) return false
        val characteristic = controlWrite ?: return false
        val pending = PendingDiagnosticControlV1(request, origin, nowMs)
        pendingControl = pending; controlRequest = request; controlSubmittedAtMs = nowMs; pendingControlOrigin = origin; controlWriteConfirmed = false
        if (gatt.writeCharacteristic(characteristic, payload, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) != BluetoothStatusCodes.SUCCESS) {
            pending.cancel(); pendingControl = null; controlRequest = null; pendingControlOrigin = null; return false
        }
        handler.postDelayed({
            if (pendingControl === pending) {
                pending.tick(SystemClock.elapsedRealtime())
                if (pending.state != DiagnosticControlPendingStateV1.PENDING) {
                    grant = null; grantOrigin = null; grantHostDeadlineMs = 0
                }
            }
        }, DiagnosticControlV1.QUEUE_MS)
        return true
    }

    fun submitStep(request: DiagnosticStepRequestV1, nowMs: Long = SystemClock.elapsedRealtime()): Boolean {
        val origin = originalAuthority() ?: return false
        if (!link.canReceive(origin.localGattGeneration.toInt()) || link.preflight().isNotEmpty() ||
            pendingControl != null || pendingStep != null || grantOrigin != origin ||
            grant?.outcome != DiagnosticControlOutcomeV1.ACCEPTED || grant?.grantId != request.grantId ||
            grant?.runId != request.runId || grant?.deviceId != request.deviceId || grant?.bootId != request.bootId ||
            nowMs >= grantHostDeadlineMs || nowMs > Long.MAX_VALUE - request.timeoutMs) return false
        val payload = DiagnosticStepV1.encode(request)
        if (payload.size > link.effectiveAttValueBytes) return false
        val characteristic = stepWrite ?: return false
        val pending = DiagnosticStepAssemblyV1(request, origin, nowMs, nowMs + request.timeoutMs)
        pendingStep = pending; pendingStepOrigin = origin; stepWriteConfirmed = false
        if (gatt.writeCharacteristic(characteristic, payload, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) != BluetoothStatusCodes.SUCCESS) {
            pending.retire(); pendingStep = null; pendingStepOrigin = null; return false
        }
        handler.postDelayed({
            if (pendingStep === pending && !pending.tick(originalAuthority(), SystemClock.elapsedRealtime())) {
                grant = null; grantOrigin = null; grantHostDeadlineMs = 0
            }
        }, request.timeoutMs.toLong())
        return true
    }

    fun onCharacteristicWrite(characteristic: BluetoothGattCharacteristic, success: Boolean) {
        if (characteristic.service?.uuid != route.service) return
        when (characteristic.uuid) {
            route.controlRequest -> if (pendingControl != null) {
                if (success) controlWriteConfirmed = true else { pendingControl?.cancel(); pendingControl = null; controlRequest = null; pendingControlOrigin = null; grant = null; grantOrigin = null; grantHostDeadlineMs = 0 }
            }
            route.stepRequest -> if (pendingStep != null) {
                if (success) stepWriteConfirmed = true else { pendingStep?.retire(); pendingStep = null; pendingStepOrigin = null }
            }
        }
    }

    fun onNotification(characteristic: BluetoothGattCharacteristic, value: ByteArray, nowMs: Long = SystemClock.elapsedRealtime()) {
        if (!link.canReceive(generation) || characteristic.service?.uuid != route.service || value.size > link.effectiveAttValueBytes) return
        val current = originalAuthority()
        when (characteristic.uuid) {
            route.controlReply -> {
                val pending = pendingControl ?: return
                val origin = pendingControlOrigin ?: return
                if (!controlWriteConfirmed) return
                val reply = pending.receive(value.copyOf(), origin, current, nowMs, nowMs) ?: return
                val request = controlRequest
                pendingControl = null; controlRequest = null; pendingControlOrigin = null
                if (reply.command == DiagnosticControlCommandV1.GRANT && reply.outcome == DiagnosticControlOutcomeV1.ACCEPTED) {
                    if (request != null && controlSubmittedAtMs <= Long.MAX_VALUE - request.ttlMs) {
                        grant = reply; grantOrigin = origin; grantHostDeadlineMs = controlSubmittedAtMs + request.ttlMs
                    }
                }
                // Deliberately no UI callback: a correlated control reply is not physical proof.
            }
            route.stepReply -> {
                val pending = pendingStep ?: return
                val origin = pendingStepOrigin ?: return
                if (!stepWriteConfirmed) return
                if (pending.receive(value.copyOf(), origin, current, nowMs) == DiagnosticStepAssemblyStatusV1.COMPLETE) {
                    pending.take(current, nowMs)
                    pendingStep = null; pendingStepOrigin = null
                    // No journal or completion claim until a separate trusted diagnostic path is integrated.
                }
            }
        }
    }

    fun close() {
        handler.removeCallbacksAndMessages(null)
        pendingControl?.cancel(); pendingStep?.retire()
        pendingControl = null; controlRequest = null; pendingControlOrigin = null; pendingStep = null; pendingStepOrigin = null
        grant = null; grantOrigin = null; grantHostDeadlineMs = 0
        link.failed(DiagnosticGattFailureV1.DISCONNECTED)
    }

    fun expireSetup() {
        if (link.phase != DiagnosticGattPhaseV1.TRANSPORT_READY && link.phase != DiagnosticGattPhaseV1.BLOCKED)
            link.failed(DiagnosticGattFailureV1.SETUP_TIMEOUT)
    }

    companion object { private val CCCD = java.util.UUID.fromString("00002902-0000-1000-8000-00805f9b34fb") }
}
