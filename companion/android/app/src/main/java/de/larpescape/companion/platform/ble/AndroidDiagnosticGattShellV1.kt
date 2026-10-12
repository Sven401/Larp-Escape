package de.larpescape.companion.platform.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothStatusCodes
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import de.larpescape.companion.data.diagnostic_observation.DiagnosticConnectionEventV1
import de.larpescape.protocol.*
import java.util.UUID

/** Bound to exactly one BluetoothGatt instance and repository generation. No role issuer is wired yet. */
@SuppressLint("MissingPermission")
internal class AndroidDiagnosticGattShellV1(
    private val gatt: BluetoothGatt,
    val generation: Int,
    private val originalAuthority: () -> DiagnosticControlOriginV1? = { null },
    targetAddress: String,
    evidenceSink: (DiagnosticConnectionEventV1) -> Boolean,
    private val statusChanged: () -> Unit = {},
) {
    val link = DiagnosticGattLinkV1(generation)
    private val handler = Handler(Looper.getMainLooper())
    private val route = DiagnosticGattFirmwareV1.route
    private val evidence = DiagnosticGattEvidenceBoundaryV1(generation, targetAddress, evidenceSink)
    private var controlWrite: BluetoothGattCharacteristic? = null
    private var stepWrite: BluetoothGattCharacteristic? = null
    private var controlReply: BluetoothGattCharacteristic? = null
    private var stepReply: BluetoothGattCharacteristic? = null
    private var developmentInfoCharacteristic: BluetoothGattCharacteristic? = null
    private var developmentWrite: BluetoothGattCharacteristic? = null
    private var developmentReply: BluetoothGattCharacteristic? = null
    private var developmentSubscriptionAck = false
    private var developmentProbeSeen = false
    private var developmentTransportFailed = false
    private var developmentRequest: DiagnosticControlRequestV1? = null
    private var developmentRequestAtMs = 0L
    private var developmentWriteConfirmed = false
    private var developmentEarlyReply: ByteArray? = null
    private var developmentCapability: DiagnosticControlReplyV1? = null
    private var developmentGrant: DiagnosticControlReplyV1? = null
    private var developmentGrantDeadlineMs = 0L
    private var developmentInfoReadState = DevelopmentInfoReadState.ABSENT
    var developmentInfo: DiagnosticDevelopmentInfoV1? = null
        private set
    private var pendingControl: PendingDiagnosticControlV1? = null
    private var controlRequest: DiagnosticControlRequestV1? = null
    private var controlSubmittedAtMs = 0L
    private var pendingControlOrigin: DiagnosticControlOriginV1? = null
    private var controlWriteConfirmed = false
    private var pendingStep: DiagnosticStepAssemblyV1? = null
    private var pendingStepRequest: DiagnosticStepRequestV1? = null
    private var pendingStepOrigin: DiagnosticControlOriginV1? = null
    private var stepWriteConfirmed = false
    private var grant: DiagnosticControlReplyV1? = null
    private var grantOrigin: DiagnosticControlOriginV1? = null
    private var grantHostDeadlineMs = 0L

    val blockStatus: String get() {
        val origin = originalAuthority()
        return when {
            link.phase == DiagnosticGattPhaseV1.CCCD_WRITES_ACKNOWLEDGED -> "SUBSCRIPTION_UNPROVEN"
            link.phase != DiagnosticGattPhaseV1.TRANSPORT_READY -> "LINK_${link.phase}_${link.failure}"
            developmentInfoReadState == DevelopmentInfoReadState.DISCOVERED ||
                developmentInfoReadState == DevelopmentInfoReadState.PENDING -> "DEVELOPMENT_INFO_PENDING"
            developmentInfoReadState == DevelopmentInfoReadState.FAILED -> "DEVELOPMENT_INFO_FAILED"
            developmentInfo != null -> when {
                developmentWrite == null || developmentReply == null -> "DEVELOPMENT_CONTROL_MISSING"
                developmentTransportFailed -> "DEVELOPMENT_CONTROL_FAILED"
                !developmentSubscriptionAck -> "DEVELOPMENT_SUBSCRIPTION_PENDING"
                !developmentProbeSeen -> "DEVELOPMENT_PROBE_PENDING"
                developmentRequest?.command == DiagnosticControlCommandV1.GRANT -> "DEVELOPMENT_GRANT_PENDING"
                developmentRequest != null -> "DEVELOPMENT_CAPABILITY_PENDING"
                developmentGrant?.outcome == DiagnosticControlOutcomeV1.ACCEPTED &&
                    SystemClock.elapsedRealtime() < developmentGrantDeadlineMs -> "DEVELOPMENT_GRANT_CONFIRMED"
                developmentGrant?.outcome == DiagnosticControlOutcomeV1.ACCEPTED -> "DEVELOPMENT_GRANT_EXPIRED"
                developmentGrant != null -> "DEVELOPMENT_GRANT_REJECTED"
                developmentCapability?.outcome == DiagnosticControlOutcomeV1.ACCEPTED -> "DEVELOPMENT_CONTROL_CONFIRMED"
                developmentCapability != null -> "DEVELOPMENT_CONTROL_REJECTED"
                else -> "DEVELOPMENT_CONTROL_READY"
            }
            origin == null -> "ROLE_ISSUER_MISSING"
            origin.localGattGeneration != generation.toLong() -> "ROLE_GENERATION_MISMATCH"
            pendingControl != null -> "CONTROL_${pendingControl?.state}"
            pendingStep != null -> "STEP_${pendingStep?.status}"
            else -> "NONE_TRANSPORT_ONLY"
        }
    }

    fun discover(): Boolean {
        val service = gatt.getService(route.service)
        val expected = setOf(route.controlRequest, route.controlReply, route.stepRequest, route.stepReply,
            DiagnosticDevelopmentInfoCodecV1.characteristic,
            DiagnosticDevelopmentControlCodecV1.write, DiagnosticDevelopmentControlCodecV1.reply)
        if (service != null && expected.any { id -> service.characteristics.count { it.uuid == id } > 1 }) {
            link.failed(DiagnosticGattFailureV1.AMBIGUOUS_CHARACTERISTIC)
            evidence.transportError(generation, "setup", null, link.failure.name)
            return false
        }
        val characteristics = service?.characteristics.orEmpty().associateBy { it.uuid }
        val writes = characteristics.values.filter { it.properties and BluetoothGattCharacteristic.PROPERTY_WRITE != 0 }.map { it.uuid }.toSet()
        val notifies = characteristics.values.filter { it.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY != 0 }.map { it.uuid }.toSet()
        val cccds = characteristics.values.filter { it.getDescriptor(CCCD) != null }.map { it.uuid }.toSet()
        if (!link.discovered(service?.uuid, writes, notifies, cccds)) {
            evidence.transportError(generation, "setup", null, link.failure.name)
            return false
        }
        controlWrite = characteristics[route.controlRequest]
        stepWrite = characteristics[route.stepRequest]
        controlReply = characteristics[route.controlReply]
        stepReply = characteristics[route.stepReply]
        developmentInfoCharacteristic = characteristics[DiagnosticDevelopmentInfoCodecV1.characteristic]
        developmentWrite = characteristics[DiagnosticDevelopmentControlCodecV1.write]
            ?.takeIf { it.properties and BluetoothGattCharacteristic.PROPERTY_WRITE != 0 }
        developmentReply = characteristics[DiagnosticDevelopmentControlCodecV1.reply]
            ?.takeIf { it.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY != 0 &&
                it.getDescriptor(CCCD) != null }
        developmentInfoReadState = when {
            developmentInfoCharacteristic == null -> DevelopmentInfoReadState.ABSENT
            developmentInfoCharacteristic!!.properties and BluetoothGattCharacteristic.PROPERTY_READ == 0 -> DevelopmentInfoReadState.FAILED
            else -> DevelopmentInfoReadState.DISCOVERED
        }
        if (!gatt.requestMtu(DiagnosticGattFirmwareV1.REQUIRED_MTU)) {
            link.failed(DiagnosticGattFailureV1.MTU_REQUEST_FAILED)
            evidence.transportError(generation, "setup", null, link.failure.name)
            return false
        }
        return true
    }

    fun onMtuChanged(mtu: Int, success: Boolean): Boolean {
        if (!link.mtu(mtu, success)) {
            evidence.transportError(generation, "setup", null, link.failure.name)
            return false
        }
        return subscribe(controlReply, DiagnosticGattFailureV1.CONTROL_CCCD_FAILED)
    }

    /** Starts one read only after the existing notification transport is ready. */
    fun readDevelopmentInfoIfReady() {
        if (link.phase != DiagnosticGattPhaseV1.TRANSPORT_READY ||
            developmentInfoReadState != DevelopmentInfoReadState.DISCOVERED) return
        val characteristic = developmentInfoCharacteristic ?: return
        developmentInfoReadState = DevelopmentInfoReadState.PENDING
        if (!gatt.readCharacteristic(characteristic))
            developmentInfoReadState = DevelopmentInfoReadState.FAILED
    }

    fun onDevelopmentInfoRead(characteristic: BluetoothGattCharacteristic,
                              value: ByteArray, success: Boolean): Boolean {
        if (characteristic !== developmentInfoCharacteristic ||
            characteristic.service?.uuid != route.service) return false
        if (developmentInfoReadState != DevelopmentInfoReadState.PENDING) return true
        val decoded = if (success) runCatching { DiagnosticDevelopmentInfoCodecV1.decode(value) }.getOrNull()
            else null
        if (decoded != null && evidence.developmentInfo(generation, characteristic.uuid, decoded)) {
            developmentInfo = decoded
            developmentInfoReadState = DevelopmentInfoReadState.CONFIRMED
            if (developmentWrite == null || !subscribeDevelopment())
                developmentTransportFailed = true
        } else developmentInfoReadState = DevelopmentInfoReadState.FAILED
        return true
    }

    private fun subscribe(characteristic: BluetoothGattCharacteristic?, failure: DiagnosticGattFailureV1): Boolean {
        val descriptor = characteristic?.getDescriptor(CCCD)
        if (characteristic == null || descriptor == null || !gatt.setCharacteristicNotification(characteristic, true) ||
            gatt.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) != BluetoothStatusCodes.SUCCESS) {
            link.failed(failure)
            evidence.transportError(generation, "setup", null, link.failure.name)
            return false
        }
        return true
    }

    private fun subscribeDevelopment(): Boolean {
        val answer = developmentReply ?: return false
        val descriptor = answer.getDescriptor(CCCD) ?: return false
        return gatt.setCharacteristicNotification(answer, true) &&
            gatt.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) ==
                BluetoothStatusCodes.SUCCESS
    }

    fun onDescriptorWrite(descriptor: BluetoothGattDescriptor, success: Boolean): Boolean {
        if (descriptor.uuid != CCCD) return false
        val characteristic = descriptor.characteristic ?: return false
        if (characteristic.service?.uuid != route.service) return false
        if (characteristic === developmentReply) {
            if (developmentInfo == null || developmentSubscriptionAck) return false
            if (!success || !evidence.cccdWriteAck(generation, characteristic.uuid)) {
                developmentTransportFailed = true
                evidence.transportError(generation, "development", null, "CCCD_WRITE_FAILED")
                return false
            }
            developmentSubscriptionAck = true
            startDevelopmentCapabilityIfReady()
            return true
        }
        val expected = when (link.phase) {
            DiagnosticGattPhaseV1.CONTROL_CCCD_PENDING -> route.controlReply
            DiagnosticGattPhaseV1.STEP_CCCD_PENDING -> route.stepReply
            else -> return false
        }
        if (characteristic.uuid != expected) return false
        if (!link.descriptorWriteAcknowledged(expected, success)) {
            evidence.transportError(generation, "setup", null, link.failure.name)
            return false
        }
        if (!evidence.cccdWriteAck(generation, expected)) {
            link.failed(DiagnosticGattFailureV1.PROBE_EVIDENCE_FAILED)
            return false
        }
        if (link.phase == DiagnosticGattPhaseV1.STEP_CCCD_PENDING)
            return subscribe(stepReply, DiagnosticGattFailureV1.STEP_CCCD_FAILED)
        if (link.phase == DiagnosticGattPhaseV1.TRANSPORT_READY && !evidence.transportProbeComplete(generation)) {
            link.failed(DiagnosticGattFailureV1.PROBE_EVIDENCE_FAILED)
            return false
        }
        return link.phase == DiagnosticGattPhaseV1.CCCD_WRITES_ACKNOWLEDGED || link.phase == DiagnosticGattPhaseV1.TRANSPORT_READY
    }

    private fun startDevelopmentCapabilityIfReady() {
        val info = developmentInfo ?: return
        if (developmentWrite == null) return
        if (link.phase != DiagnosticGattPhaseV1.TRANSPORT_READY ||
            developmentTransportFailed || !developmentSubscriptionAck ||
            !developmentProbeSeen || developmentRequest != null ||
            developmentCapability != null) return
        val request = DiagnosticControlRequestV1(DiagnosticControlCommandV1.CAPABILITY,
            1, info.deviceId, info.bootId)
        startDevelopmentRequest(request)
    }

    /** A user action only asks the development image for a short control grant. */
    fun requestDevelopmentGrant(): Boolean {
        val info = developmentInfo ?: return false
        if (link.phase != DiagnosticGattPhaseV1.TRANSPORT_READY || developmentTransportFailed ||
            !developmentSubscriptionAck || !developmentProbeSeen || developmentRequest != null ||
            developmentCapability?.outcome != DiagnosticControlOutcomeV1.ACCEPTED ||
            developmentGrant != null) return false
        val request = DiagnosticControlRequestV1(DiagnosticControlCommandV1.GRANT,
            2, info.deviceId, info.bootId, runId = UUID.randomUUID().toString(),
            ttlMs = 30000, explicitConsent = true)
        return startDevelopmentRequest(request)
    }

    private fun startDevelopmentRequest(request: DiagnosticControlRequestV1): Boolean {
        val endpoint = developmentWrite ?: return false
        val payload = DiagnosticControlV1.encode(request)
        if (payload.size > link.effectiveAttValueBytes) {
            developmentTransportFailed = true
            evidence.transportError(generation, "development", request.message, "MTU_TOO_SMALL")
            return false
        }
        val started = SystemClock.elapsedRealtime()
        developmentRequest = request
        developmentRequestAtMs = started
        developmentWriteConfirmed = false
        if (gatt.writeCharacteristic(endpoint, payload, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) !=
            BluetoothStatusCodes.SUCCESS) {
            developmentRequest = null
            developmentTransportFailed = true
            evidence.transportError(generation, "development", request.message, "WRITE_START_FAILED")
            return false
        }
        handler.postDelayed({
            if (developmentRequest === request) {
                developmentRequest = null
                developmentEarlyReply = null
                developmentTransportFailed = true
                evidence.transportError(generation, "development", request.message, "REPLY_TIMEOUT")
                statusChanged()
            }
        }, DiagnosticControlV1.QUEUE_MS)
        return true
    }

    private fun receiveDevelopmentReply(bytes: ByteArray, nowMs: Long) {
        val request = developmentRequest ?: return
        val info = developmentInfo ?: return
        if (!developmentWriteConfirmed) {
            developmentEarlyReply = bytes.copyOf()
            return
        }
        if (nowMs < developmentRequestAtMs ||
            nowMs - developmentRequestAtMs >= DiagnosticControlV1.QUEUE_MS) return
        val answer = runCatching { DiagnosticDevelopmentControlCodecV1.correlate(bytes, request) }
            .getOrNull() ?: return
        if (!evidence.developmentControlFinal(generation,
                DiagnosticDevelopmentControlCodecV1.reply, info, request, answer)) {
            developmentTransportFailed = true
            return
        }
        if (request.command == DiagnosticControlCommandV1.CAPABILITY)
            developmentCapability = answer
        else if (request.command == DiagnosticControlCommandV1.GRANT) {
            developmentGrant = answer
            if (answer.outcome == DiagnosticControlOutcomeV1.ACCEPTED) {
                developmentGrantDeadlineMs = developmentRequestAtMs + request.ttlMs
                handler.postDelayed({ if (developmentGrant === answer) statusChanged() },
                    (developmentGrantDeadlineMs - SystemClock.elapsedRealtime()).coerceAtLeast(0))
            }
        }
        developmentRequest = null
        developmentEarlyReply = null
    }

    /** No call site currently supplies originalAuthority; consequently no live diagnostic write is possible. */
    fun submitControl(request: DiagnosticControlRequestV1, nowMs: Long = SystemClock.elapsedRealtime()): Boolean {
        val cancelling = request.command == DiagnosticControlCommandV1.CANCEL
        val matchingGrant = grant?.grantId == request.grantId && grant?.runId == request.runId
        val matchingOrigin = grantOrigin
        if (cancelling) {
            // A local cancellation must not wait for an active step or a GATT write callback.
            grant = null; grantOrigin = null; grantHostDeadlineMs = 0
            pendingStepRequest?.let { evidence.transportError(generation, "step", it.message, "LOCAL_CANCELLED") }
            pendingStep?.retire(); pendingStep = null; pendingStepOrigin = null
            pendingStepRequest = null
            pendingControl?.cancel()
        }
        val origin = originalAuthority() ?: return false
        if (!link.canReceive(origin.localGattGeneration.toInt()) || link.preflight().isNotEmpty() ||
            pendingControl != null || pendingStep != null || origin.localGattGeneration != generation.toLong()) return false
        if (request.command == DiagnosticControlCommandV1.GRANT && grant != null) return false
        if (cancelling && (!matchingGrant || matchingOrigin != origin)) return false
        val payload = DiagnosticControlV1.encode(request)
        if (payload.size > link.effectiveAttValueBytes) return false
        val characteristic = controlWrite ?: return false
        val pending = PendingDiagnosticControlV1(request, origin, nowMs)
        pendingControl = pending; controlRequest = request; controlSubmittedAtMs = nowMs; pendingControlOrigin = origin; controlWriteConfirmed = false
        if (gatt.writeCharacteristic(characteristic, payload, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) != BluetoothStatusCodes.SUCCESS) {
            pending.cancel(); pendingControl = null; controlRequest = null; pendingControlOrigin = null
            evidence.transportError(generation, "control", request.message, "WRITE_START_FAILED")
            return false
        }
        handler.postDelayed({
            if (pendingControl === pending) {
                pending.tick(SystemClock.elapsedRealtime())
                if (pending.state != DiagnosticControlPendingStateV1.PENDING) {
                    grant = null; grantOrigin = null; grantHostDeadlineMs = 0
                    evidence.transportError(generation, "control", request.message, pending.state.name)
                    pendingControl = null; controlRequest = null; pendingControlOrigin = null
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
        pendingStep = pending; pendingStepRequest = request; pendingStepOrigin = origin; stepWriteConfirmed = false
        if (gatt.writeCharacteristic(characteristic, payload, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) != BluetoothStatusCodes.SUCCESS) {
            pending.retire(); pendingStep = null; pendingStepRequest = null; pendingStepOrigin = null
            evidence.transportError(generation, "step", request.message, "WRITE_START_FAILED")
            return false
        }
        handler.postDelayed({
            if (pendingStep === pending && !pending.tick(originalAuthority(), SystemClock.elapsedRealtime())) {
                grant = null; grantOrigin = null; grantHostDeadlineMs = 0
                evidence.transportError(generation, "step", request.message, pending.status.name)
                pendingStep = null; pendingStepRequest = null; pendingStepOrigin = null
            }
        }, request.timeoutMs.toLong())
        return true
    }

    fun onCharacteristicWrite(characteristic: BluetoothGattCharacteristic, success: Boolean) {
        if (characteristic.service?.uuid != route.service) return
        if (characteristic === developmentWrite) {
            val request = developmentRequest ?: return
            if (success) {
                developmentWriteConfirmed = true
                if (!evidence.writeAck(generation, characteristic.uuid, request.message))
                    developmentTransportFailed = true
                developmentEarlyReply?.let { receiveDevelopmentReply(it, SystemClock.elapsedRealtime()) }
            } else {
                evidence.transportError(generation, "development", request.message, "ATT_WRITE_FAILED")
                developmentRequest = null
                developmentEarlyReply = null
                developmentTransportFailed = true
            }
            return
        }
        when (characteristic.uuid) {
            route.controlRequest -> if (pendingControl != null) {
                if (success) {
                    controlWriteConfirmed = true
                    controlRequest?.let { evidence.writeAck(generation, characteristic.uuid, it.message) }
                } else {
                    controlRequest?.let { evidence.transportError(generation, "control", it.message, "ATT_WRITE_FAILED") }
                    pendingControl?.cancel(); pendingControl = null; controlRequest = null; pendingControlOrigin = null; grant = null; grantOrigin = null; grantHostDeadlineMs = 0
                }
            }
            route.stepRequest -> if (pendingStep != null) {
                if (success) {
                    stepWriteConfirmed = true
                    pendingStepRequest?.let { evidence.writeAck(generation, characteristic.uuid, it.message) }
                } else {
                    pendingStepRequest?.let { evidence.transportError(generation, "step", it.message, "ATT_WRITE_FAILED") }
                    pendingStep?.retire(); pendingStep = null; pendingStepOrigin = null; pendingStepRequest = null
                }
            }
        }
    }

    fun onNotification(characteristic: BluetoothGattCharacteristic, value: ByteArray, nowMs: Long = SystemClock.elapsedRealtime()) {
        if (characteristic.service?.uuid != route.service) return
        if (characteristic === developmentReply) {
            if (value.contentEquals(DiagnosticDevelopmentControlCodecV1.probe)) {
                if (developmentInfo != null && !developmentProbeSeen &&
                    evidence.transportProbe(generation, characteristic.uuid)) {
                    developmentProbeSeen = true
                    startDevelopmentCapabilityIfReady()
                }
            } else if (value.size == DiagnosticControlV1.REPLY_BYTES &&
                developmentSubscriptionAck && developmentProbeSeen &&
                link.phase == DiagnosticGattPhaseV1.TRANSPORT_READY)
                receiveDevelopmentReply(value.copyOf(), nowMs)
            return
        }
        if (link.canObserveProbe(generation, characteristic.uuid, value)) {
            if (!evidence.transportProbe(generation, characteristic.uuid)) {
                link.failed(DiagnosticGattFailureV1.PROBE_EVIDENCE_FAILED)
                return
            }
            link.observeProbe(generation, characteristic.uuid, value)
            if (link.phase == DiagnosticGattPhaseV1.TRANSPORT_READY && !evidence.transportProbeComplete(generation))
                link.failed(DiagnosticGattFailureV1.PROBE_EVIDENCE_FAILED)
            return
        }
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
                if (request == null || !evidence.controlFinal(generation, characteristic.uuid, origin, current, request, reply)) return
                if (reply.command == DiagnosticControlCommandV1.GRANT && reply.outcome == DiagnosticControlOutcomeV1.ACCEPTED) {
                    if (controlSubmittedAtMs <= Long.MAX_VALUE - request.ttlMs) {
                        grant = reply; grantOrigin = origin; grantHostDeadlineMs = controlSubmittedAtMs + request.ttlMs
                    }
                }
                // Deliberately no UI callback: a correlated control reply is not physical proof.
            }
            route.stepReply -> {
                val pending = pendingStep ?: return
                val origin = pendingStepOrigin ?: return
                if (!stepWriteConfirmed) return
                val status = pending.receive(value.copyOf(), origin, current, nowMs)
                if (status == DiagnosticStepAssemblyStatusV1.COMPLETE) {
                    val reply = pending.take(current, nowMs)
                    val request = pendingStepRequest
                    pendingStep = null; pendingStepOrigin = null; pendingStepRequest = null
                    if (reply == null || request == null ||
                        !evidence.stepFinal(generation, characteristic.uuid, origin, current, request, reply)) {
                        grant = null; grantOrigin = null; grantHostDeadlineMs = 0
                    }
                } else if (status in setOf(DiagnosticStepAssemblyStatusV1.REJECTED, DiagnosticStepAssemblyStatusV1.EXPIRED)) {
                    pendingStepRequest?.let { evidence.transportError(generation, "step", it.message, status.name) }
                    pendingStep = null; pendingStepRequest = null; pendingStepOrigin = null
                }
            }
        }
    }

    fun close() {
        handler.removeCallbacksAndMessages(null)
        controlRequest?.let { evidence.transportError(generation, "control", it.message, "DISCONNECTED") }
        pendingStepRequest?.let { evidence.transportError(generation, "step", it.message, "DISCONNECTED") }
        evidence.transportError(generation, "link", null, "DISCONNECTED")
        pendingControl?.cancel(); pendingStep?.retire()
        pendingControl = null; controlRequest = null; pendingControlOrigin = null; pendingStep = null; pendingStepOrigin = null; pendingStepRequest = null
        grant = null; grantOrigin = null; grantHostDeadlineMs = 0
        developmentInfo = null; developmentInfoReadState = DevelopmentInfoReadState.ABSENT
        developmentRequest?.let { evidence.transportError(generation, "development", it.message, "DISCONNECTED") }
        developmentRequest = null; developmentEarlyReply = null; developmentCapability = null
        developmentGrant = null; developmentGrantDeadlineMs = 0L
        developmentSubscriptionAck = false; developmentProbeSeen = false
        developmentTransportFailed = true
        link.failed(DiagnosticGattFailureV1.DISCONNECTED)
    }

    fun expireSetup() {
        if (link.phase != DiagnosticGattPhaseV1.CCCD_WRITES_ACKNOWLEDGED &&
            link.phase != DiagnosticGattPhaseV1.TRANSPORT_READY && link.phase != DiagnosticGattPhaseV1.BLOCKED) {
            val pendingPhase = link.phase
            link.failed(DiagnosticGattFailureV1.SETUP_TIMEOUT)
            evidence.transportError(generation, "setup", null, link.failure.name, pendingPhase)
        }
    }

    private enum class DevelopmentInfoReadState { ABSENT, DISCOVERED, PENDING, CONFIRMED, FAILED }
    companion object { private val CCCD = java.util.UUID.fromString("00002902-0000-1000-8000-00805f9b34fb") }
}
