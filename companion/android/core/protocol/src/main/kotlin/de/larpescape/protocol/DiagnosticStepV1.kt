package de.larpescape.protocol

import com.google.gson.*
import java.util.Collections

enum class DiagnosticStepOperationV1 { WAKE_CAPTURE, LED_SINGLE }
enum class DiagnosticStepOutcomeV1 { COMPLETED, FAILED, CANCELLED, REJECTED }
enum class DiagnosticStepReasonV1 { NONE, OWNER_ERROR, UNSUPPORTED, PERMISSION_DENIED, EXPIRED, BUSY, CANCELLED, OVERFLOW }

/** Public selectors, never authority. Immutable parameters include the entire original wire request. */
data class DiagnosticStepRequestV1(
    val message: Long, val deviceId: String, val bootId: String, val runId: String, val grantId: String,
    val stepId: Long, val timeoutMs: Int, val operation: DiagnosticStepOperationV1,
    val channel: Int = if (operation == DiagnosticStepOperationV1.WAKE_CAPTURE) 1 else 0,
    val index: Int = 0, val red: Int = if (operation == DiagnosticStepOperationV1.LED_SINGLE) 16 else 0,
    val green: Int = 0, val blue: Int = 0, val holdMs: Int = if (operation == DiagnosticStepOperationV1.LED_SINGLE) 500 else 0,
)
data class DiagnosticStepEdgeV1(val deviceMs: Long, val channel: Int, val rawHigh: Boolean, val active: Boolean)

/** Detached synthetic wire evidence; no user observation, voltage measurement or device permission. */
class DiagnosticStepReplyV1 internal constructor(
    val request: DiagnosticStepRequestV1, val startedDeviceMs: Long, val endedDeviceMs: Long,
    val outcome: DiagnosticStepOutcomeV1, val reason: DiagnosticStepReasonV1,
    val activeHigh: Boolean, val restoredOff: Boolean, val renderCalls: Int, val overflow: Boolean,
    edges: List<DiagnosticStepEdgeV1>,
) {
    val edges: List<DiagnosticStepEdgeV1> = Collections.unmodifiableList(ArrayList(edges))
    val synthetic: Boolean get() = true
    val supplyMillivolts: Int? get() = null
}

/** Fixed pilot contract b5a8c430, wire layout from 04c145f. No BLE IO or implicit retry. */
object DiagnosticStepV1 {
    const val REQUEST_BYTES = 80
    const val REPLY_HEADER_BYTES = 104
    const val REPLY_BYTES = 744
    const val FRAME_HEADER_BYTES = 12
    const val FRAME_BYTES = 128
    const val MAX_TIME = 9007199254740991L
    private val uuid = Regex("[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}")
    private fun identifier(id: String, digits: Int) = Regex("[0-9A-F]{$digits}").matches(id) && id.any { it != '0' }
    internal fun read(bytes: ByteArray, offset: Int, count: Int): ULong {
        var value = 0uL
        repeat(count) { value = value or ((bytes[offset + it].toInt() and 255).toULong() shl (8 * it)) }
        return value
    }
    internal fun put(bytes: ByteArray, offset: Int, value: ULong, count: Int) { repeat(count) { bytes[offset + it] = (value shr (8 * it)).toByte() } }
    private fun hex(bytes: ByteArray, offset: Int, count: Int) = (offset until offset + count).joinToString("") { "%02X".format(bytes[it]) }
    private fun hex(id: String) = id.replace("-", "").chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private fun uuid(bytes: ByteArray, offset: Int): String {
        val id = hex(bytes, offset, 16).lowercase()
        return "${id.substring(0, 8)}-${id.substring(8, 12)}-${id.substring(12, 16)}-${id.substring(16, 20)}-${id.substring(20)}".also { require(uuid.matches(it)) }
    }
    private fun validate(r: DiagnosticStepRequestV1) {
        require(r.message in 1..0xffff_ffffL && r.stepId in 1..0xffff_ffffL && r.timeoutMs in 1000..10000)
        require(identifier(r.deviceId, 32) && identifier(r.bootId, 16) && uuid.matches(r.runId) && uuid.matches(r.grantId))
        when (r.operation) {
            DiagnosticStepOperationV1.WAKE_CAPTURE -> require(r.channel == 1 && r.index == 0 && r.red == 0 && r.green == 0 && r.blue == 0 && r.holdMs == 0)
            DiagnosticStepOperationV1.LED_SINGLE -> require(r.channel == 0 && r.index == 0 && r.red == 16 && r.green == 0 && r.blue == 0 && r.holdMs == 500)
        }
    }
    fun encode(request: DiagnosticStepRequestV1): ByteArray {
        validate(request)
        return ByteArray(REQUEST_BYTES).apply {
            this[0] = 'D'.code.toByte(); this[1] = 'G'.code.toByte(); this[2] = 1; this[3] = 4
            put(this, 4, request.message.toULong(), 4); hex(request.deviceId).copyInto(this, 8)
            put(this, 24, request.bootId.toULong(16), 8); hex(request.runId).copyInto(this, 32); hex(request.grantId).copyInto(this, 48)
            put(this, 64, request.stepId.toULong(), 4); put(this, 68, request.timeoutMs.toULong(), 4)
            this[72] = request.operation.ordinal.toByte(); this[73] = request.channel.toByte(); this[74] = request.index.toByte()
            this[75] = request.red.toByte(); this[76] = request.green.toByte(); this[77] = request.blue.toByte(); put(this, 78, request.holdMs.toULong(), 2)
        }
    }
    fun request(bytes: ByteArray): DiagnosticStepRequestV1 {
        require(bytes.size == REQUEST_BYTES)
        require(bytes[0] == 'D'.code.toByte() && bytes[1] == 'G'.code.toByte() && bytes[2] == 1.toByte() && bytes[3] == 4.toByte())
        val op = DiagnosticStepOperationV1.entries.getOrNull(bytes[72].toInt() and 255) ?: throw IllegalArgumentException("Unknown DG step operation")
        val timeout = read(bytes, 68, 4); require(timeout in 1000uL..10000uL)
        return DiagnosticStepRequestV1(read(bytes, 4, 4).toLong(), hex(bytes, 8, 16), read(bytes, 24, 8).toString(16).uppercase().padStart(16, '0'),
            uuid(bytes, 32), uuid(bytes, 48), read(bytes, 64, 4).toLong(), timeout.toInt(), op, bytes[73].toInt() and 255, bytes[74].toInt() and 255,
            bytes[75].toInt() and 255, bytes[76].toInt() and 255, bytes[77].toInt() and 255, read(bytes, 78, 2).toInt()).also(::validate)
    }
    fun reply(bytes: ByteArray): DiagnosticStepReplyV1 {
        require(bytes.size in REPLY_HEADER_BYTES..REPLY_BYTES)
        val count = bytes[102].toInt() and 255
        require(count <= 64 && bytes.size == REPLY_HEADER_BYTES + count * 10)
        require(listOf(98, 99, 101).all { bytes[it].toInt() in 0..1 } && bytes[103] == 1.toByte())
        val req = request(bytes.copyOfRange(0, REQUEST_BYTES))
        val started = read(bytes, 80, 8); val ended = read(bytes, 88, 8)
        require(started <= ended && ended <= MAX_TIME.toULong() && ended - started < req.timeoutMs.toULong())
        val outcome = DiagnosticStepOutcomeV1.entries.getOrNull(bytes[96].toInt() and 255) ?: throw IllegalArgumentException("Unknown step outcome")
        val reason = DiagnosticStepReasonV1.entries.getOrNull(bytes[97].toInt() and 255) ?: throw IllegalArgumentException("Unknown step reason")
        require((outcome == DiagnosticStepOutcomeV1.COMPLETED) == (reason == DiagnosticStepReasonV1.NONE))
        val render = bytes[100].toInt() and 255; require(render <= 2)
        val high = bytes[98] == 1.toByte(); val off = bytes[99] == 1.toByte(); val overflow = bytes[101] == 1.toByte()
        if (req.operation == DiagnosticStepOperationV1.LED_SINGLE) {
            require(count == 0 && !overflow)
            if (outcome == DiagnosticStepOutcomeV1.COMPLETED) require(render == 2 && off && ended - started >= req.holdMs.toULong())
        } else require(render == 0 && !off)
        var previous = started
        val edges = ArrayList<DiagnosticStepEdgeV1>(count)
        repeat(count) { i ->
            val at = REPLY_HEADER_BYTES + 10 * i; val time = read(bytes, at, 8)
            val channel = bytes[at + 8].toInt() and 255; val flags = bytes[at + 9].toInt() and 255
            require(time >= previous && time <= ended && channel in 1..4 && flags <= 3)
            val raw = flags and 1 != 0; val active = flags and 2 != 0; require(active == (raw == high))
            edges.add(DiagnosticStepEdgeV1(time.toLong(), channel, raw, active)); previous = time
        }
        return DiagnosticStepReplyV1(req, started.toLong(), ended.toLong(), outcome, reason, high, off, render, overflow, edges)
    }
    fun correlate(bytes: ByteArray, original: DiagnosticStepRequestV1): DiagnosticStepReplyV1 {
        validate(original)
        return reply(bytes).also { require(it.request == original) { "DG step original request mismatch" } }
    }
    fun fromJsonRequest(request: JsonObject, message: Long): DiagnosticStepRequestV1 {
        DiagnosticSchemaV1.validate(request, "request")
        val parameters = request.getAsJsonObject("parameters")
        val op = when (request.text("operation")) { "wake_capture" -> DiagnosticStepOperationV1.WAKE_CAPTURE; "led_single" -> DiagnosticStepOperationV1.LED_SINGLE; else -> error("Unsupported DG step") }
        val r = DiagnosticStepRequestV1(message, request.text("deviceId"), request.text("bootId"), request.text("runId"), request.text("grantId"), request.number("stepId"), request.number("timeoutMs").toInt(), op)
        if (op == DiagnosticStepOperationV1.WAKE_CAPTURE) require(parameters.number("channel") == r.channel.toLong())
        else require(parameters.number("index") == 0L && parameters.number("holdMs") == 500L && parameters.getAsJsonArray("rgb").map { it.asInt } == listOf(16, 0, 0))
        validate(r); return r
    }
    /** JSON v1 and b5a8c430 agree on duration. Preserve bytes and reject, never repair evidence. */
    fun toJson(reply: DiagnosticStepReplyV1): ByteArray {
        val r = reply.request
        require(reply.endedDeviceMs - reply.startedDeviceMs < r.timeoutMs) { "Wire duration equals timeout; JSON report requires less" }
        if (r.operation == DiagnosticStepOperationV1.LED_SINGLE && reply.outcome == DiagnosticStepOutcomeV1.COMPLETED)
            require(reply.endedDeviceMs - reply.startedDeviceMs >= r.holdMs) { "Completed wire LED shorter than requested hold; JSON report rejects it" }
        val json = JsonObject().apply {
            addProperty("schemaVersion", 1); addProperty("kind", "diagnostic_reply")
            addProperty("deviceId", r.deviceId); addProperty("bootId", r.bootId); addProperty("runId", r.runId); addProperty("grantId", r.grantId); addProperty("stepId", r.stepId)
            addProperty("operation", r.operation.name.lowercase()); addProperty("outcome", reply.outcome.name.lowercase()); addProperty("reason", reply.reason.name.lowercase())
            addProperty("startedDeviceMs", reply.startedDeviceMs); addProperty("endedDeviceMs", reply.endedDeviceMs)
            add("telemetry", JsonObject().apply {
                if (r.operation == DiagnosticStepOperationV1.LED_SINGLE) {
                    addProperty("renderCalls", reply.renderCalls); addProperty("restoredOff", reply.restoredOff); add("supplyMillivolts", JsonNull.INSTANCE)
                } else {
                    addProperty("overflow", reply.overflow); addProperty("activeLevel", if (reply.activeHigh) "high" else "low")
                    add("edges", JsonArray().apply { reply.edges.forEach { e -> add(JsonObject().apply { addProperty("deviceMs", e.deviceMs); addProperty("channel", e.channel); addProperty("rawHigh", e.rawHigh); addProperty("active", e.active) }) } })
                }
            })
        }
        DiagnosticSchemaV1.validate(json, "reply"); return DiagnosticJsonV1.canonical(json)
    }
}

enum class DiagnosticStepAssemblyStatusV1 { RECEIVING, DUPLICATE, COMPLETE, REJECTED, EXPIRED, CLOSED }

/** One bounded, serialized receiver. A future adapter persists pending before sending and retires on restart. */
class DiagnosticStepAssemblyV1(
    private val original: DiagnosticStepRequestV1, private val origin: DiagnosticControlOriginV1,
    nowMs: Long, private val absoluteDeadlineMs: Long,
) {
    private val bytes = ByteArray(DiagnosticStepV1.REPLY_BYTES)
    private var lastNow = nowMs
    private var total = 0
    private var received = 0
    var status = DiagnosticStepAssemblyStatusV1.RECEIVING
        private set
    init {
        DiagnosticStepV1.encode(original)
        require(nowMs >= 0 && nowMs < absoluteDeadlineMs && absoluteDeadlineMs <= DiagnosticStepV1.MAX_TIME && absoluteDeadlineMs - nowMs <= original.timeoutMs)
        require(origin.localGattGeneration > 0 && origin.connectionToken in 1..0xffff_ffffL && origin.connectionHandle in 0..65535 && origin.authorityEpoch in 1..0xffff_ffffL && origin.authorityPrincipal in 1..0xffff_ffffL && origin.gameMaster)
    }
    private fun close(state: DiagnosticStepAssemblyStatusV1): DiagnosticStepAssemblyStatusV1 {
        bytes.fill(0); total = 0; received = 0; status = state; return state
    }
    fun retire() { close(DiagnosticStepAssemblyStatusV1.CLOSED) }
    fun tick(current: DiagnosticControlOriginV1?, nowMs: Long): Boolean {
        if (status != DiagnosticStepAssemblyStatusV1.RECEIVING && status != DiagnosticStepAssemblyStatusV1.COMPLETE) return false
        if (nowMs < lastNow || nowMs >= absoluteDeadlineMs || current != origin) { close(DiagnosticStepAssemblyStatusV1.EXPIRED); return false }
        lastNow = nowMs; return true
    }
    fun receive(frame: ByteArray, captured: DiagnosticControlOriginV1, current: DiagnosticControlOriginV1?, nowMs: Long): DiagnosticStepAssemblyStatusV1 {
        if (status != DiagnosticStepAssemblyStatusV1.RECEIVING) return DiagnosticStepAssemblyStatusV1.CLOSED
        if (!tick(current, nowMs)) return status
        if (captured != origin || frame.size !in 13..DiagnosticStepV1.FRAME_BYTES) return close(DiagnosticStepAssemblyStatusV1.REJECTED)
        if (frame[0] != 'D'.code.toByte() || frame[1] != 'G'.code.toByte() || frame[2] != 1.toByte() || frame[3] != 0x84.toByte() || DiagnosticStepV1.read(frame, 4, 4).toLong() != original.message) return close(DiagnosticStepAssemblyStatusV1.REJECTED)
        val incomingTotal = DiagnosticStepV1.read(frame, 8, 2).toInt(); val offset = DiagnosticStepV1.read(frame, 10, 2).toInt(); val count = frame.size - 12
        if (incomingTotal !in 104..744 || (incomingTotal - 104) % 10 != 0 || offset >= incomingTotal || count > incomingTotal - offset || (total != 0 && total != incomingTotal) || offset > received) return close(DiagnosticStepAssemblyStatusV1.REJECTED)
        if (offset < received) {
            if (count <= received - offset && (0 until count).all { bytes[offset + it] == frame[12 + it] }) return DiagnosticStepAssemblyStatusV1.DUPLICATE
            return close(DiagnosticStepAssemblyStatusV1.REJECTED)
        }
        total = incomingTotal; frame.copyInto(bytes, offset, 12); received += count
        if (received < total) return status
        try { DiagnosticStepV1.correlate(bytes.copyOf(total), original) } catch (_: IllegalArgumentException) { return close(DiagnosticStepAssemblyStatusV1.REJECTED) }
        status = DiagnosticStepAssemblyStatusV1.COMPLETE; return status
    }
    fun take(current: DiagnosticControlOriginV1?, nowMs: Long): DiagnosticStepReplyV1? {
        if (!tick(current, nowMs) || status != DiagnosticStepAssemblyStatusV1.COMPLETE) return null
        val result = DiagnosticStepV1.correlate(bytes.copyOf(total), original); retire(); return result
    }
}
