package de.larpescape.protocol

enum class DiagnosticControlCommandV1(val wire: Int) { CAPABILITY(1), GRANT(2), CANCEL(3) }
enum class DiagnosticControlOutcomeV1 { ACCEPTED, PERMISSION_DENIED, INVALID, BUSY, CONSUMED, EXPIRED, OWNER_ERROR, CAPACITY }

/** Public IDs select a context; they are not server or role proof. */
data class DiagnosticControlRequestV1(
    val command: DiagnosticControlCommandV1,
    val message: Long,
    val deviceId: String,
    val bootId: String,
    val runId: String? = null,
    val grantId: String? = null,
    val ttlMs: Int = 0,
    val explicitConsent: Boolean = false,
)

/** Detached control result. ACCEPTED never implies physical output or app authority. */
class DiagnosticControlReplyV1 internal constructor(
    val command: DiagnosticControlCommandV1,
    val message: Long,
    val deviceId: String,
    val bootId: String,
    val runId: String?,
    val grantId: String?,
    val outcome: DiagnosticControlOutcomeV1,
    val issuedDeviceMs: Long,
    val expiresDeviceMs: Long,
    val owner: Int,
) {
    val synthetic: Boolean get() = owner == 3
    val controlOnly: Boolean get() = true
    val maxGrantMs: Int get() = 60000
}

/** Pinned to firmware 3209275. Exact frames only: no implicit fragmentation or GameCommand reuse. */
object DiagnosticControlV1 {
    const val REQUEST_BYTES = 69
    const val REPLY_BYTES = 88
    const val FRAME_BUDGET = 128
    const val QUEUE_MS = 2000L
    private const val MAX_TIME = 9007199254740991L
    private val uuid = Regex("[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}")
    private fun device(id: String) = Regex("[0-9A-F]{32}").matches(id) && id.any { it != '0' }
    private fun boot(id: String) = Regex("[0-9A-F]{16}").matches(id) && id.any { it != '0' }
    private fun validUuid(id: String?) = id != null && uuid.matches(id)
    private fun validate(request: DiagnosticControlRequestV1) {
        require(request.message in 1..0xffff_ffffL && device(request.deviceId) && boot(request.bootId)) { "Invalid DG request context" }
        when (request.command) {
            DiagnosticControlCommandV1.CAPABILITY -> require(request.runId == null && request.grantId == null && request.ttlMs == 0 && !request.explicitConsent)
            DiagnosticControlCommandV1.GRANT -> require(validUuid(request.runId) && request.grantId == null && request.ttlMs in 1..60000 && request.explicitConsent)
            DiagnosticControlCommandV1.CANCEL -> require(validUuid(request.runId) && validUuid(request.grantId) && request.ttlMs == 0 && !request.explicitConsent)
        }
    }
    /** Check the effective ATT value capacity, not the negotiated MTU including protocol overhead. */
    fun requireValueCapacity(valueBytes: Int) { require(valueBytes >= REPLY_BYTES) { "DG control requires at least 88 ATT value bytes" } }
    private fun hex(id: String): ByteArray = id.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private fun hex(bytes: ByteArray, offset: Int, count: Int): String = (offset until offset + count).joinToString("") { "%02X".format(bytes[it]) }
    private fun put(bytes: ByteArray, offset: Int, value: ULong, count: Int) { repeat(count) { bytes[offset + it] = (value shr (8 * it)).toByte() } }
    private fun read(bytes: ByteArray, offset: Int, count: Int): ULong {
        var value = 0uL
        repeat(count) { value = value or ((bytes[offset + it].toInt() and 255).toULong() shl (8 * it)) }
        return value
    }
    private fun uuid(bytes: ByteArray, offset: Int): String? {
        val text = hex(bytes, offset, 16).lowercase()
        if (text.all { it == '0' }) return null
        return "${text.substring(0, 8)}-${text.substring(8, 12)}-${text.substring(12, 16)}-${text.substring(16, 20)}-${text.substring(20)}".also { require(validUuid(it)) { "Invalid DG UUID" } }
    }
    private fun command(byte: Int) = DiagnosticControlCommandV1.entries.singleOrNull { it.wire == byte } ?: throw IllegalArgumentException("Unknown DG command")
    private fun header(bytes: ByteArray, expected: Int) {
        require(bytes.size <= FRAME_BUDGET && bytes.size == expected) { "DG frame length invalid" }
        require(bytes[0] == 'D'.code.toByte() && bytes[1] == 'G'.code.toByte() && bytes[2] == 1.toByte()) { "DG header invalid" }
    }
    fun encode(request: DiagnosticControlRequestV1): ByteArray {
        validate(request)
        return ByteArray(REQUEST_BYTES).apply {
            this[0] = 'D'.code.toByte(); this[1] = 'G'.code.toByte(); this[2] = 1; this[3] = request.command.wire.toByte()
            put(this, 4, request.message.toULong(), 4); hex(request.deviceId).copyInto(this, 8)
            put(this, 24, request.bootId.toULong(16), 8)
            request.runId?.let { hex(it.replace("-", "")).copyInto(this, 32) }
            request.grantId?.let { hex(it.replace("-", "")).copyInto(this, 48) }
            put(this, 64, request.ttlMs.toULong(), 4); this[68] = if (request.explicitConsent) 1 else 0
        }
    }
    fun request(bytes: ByteArray): DiagnosticControlRequestV1 {
        header(bytes, REQUEST_BYTES)
        require(bytes[68].toInt() in 0..1)
        val ttl = read(bytes, 64, 4)
        require(ttl <= 60000uL)
        return DiagnosticControlRequestV1(command(bytes[3].toInt() and 255), read(bytes, 4, 4).toLong(), hex(bytes, 8, 16),
            read(bytes, 24, 8).toString(16).uppercase().padStart(16, '0'), uuid(bytes, 32), uuid(bytes, 48), ttl.toInt(), bytes[68] == 1.toByte()).also(::validate)
    }
    fun reply(bytes: ByteArray, expectedOwner: Int = 3): DiagnosticControlReplyV1 {
        header(bytes, REPLY_BYTES)
        val kind = bytes[3].toInt() and 255
        require(kind in 0x81..0x83)
        val cmd = command(kind and 0x7f)
        val message = read(bytes, 4, 4).toLong()
        val deviceId = hex(bytes, 8, 16); val bootId = read(bytes, 24, 8).toString(16).uppercase().padStart(16, '0')
        require(message != 0L && device(deviceId) && boot(bootId))
        val runId = uuid(bytes, 32); val grantId = uuid(bytes, 48)
        val outcome = DiagnosticControlOutcomeV1.entries.getOrNull(bytes[64].toInt() and 255) ?: throw IllegalArgumentException("Unknown DG outcome")
        require(expectedOwner in setOf(3, 4) && bytes[65] == expectedOwner.toByte() &&
            bytes[66] == 1.toByte() && bytes[67] == 0.toByte()) { "Unsupported DG capability flags/schema" }
        require(read(bytes, 84, 4) == 60000uL)
        val issued = read(bytes, 68, 8); val expires = read(bytes, 76, 8)
        require(issued <= MAX_TIME.toULong() && expires <= MAX_TIME.toULong())
        when (cmd) {
            DiagnosticControlCommandV1.CAPABILITY -> require(runId == null && grantId == null && issued == 0uL && expires == 0uL)
            DiagnosticControlCommandV1.GRANT -> {
                require(runId != null)
                if (outcome == DiagnosticControlOutcomeV1.ACCEPTED) require(grantId != null && expires > issued && expires - issued <= 60000uL)
                else require(grantId == null && issued == 0uL && expires == 0uL)
            }
            DiagnosticControlCommandV1.CANCEL -> require(runId != null && grantId != null && issued == 0uL && expires == 0uL)
        }
        return DiagnosticControlReplyV1(cmd, message, deviceId, bootId, runId, grantId,
            outcome, issued.toLong(), expires.toLong(), expectedOwner)
    }
    fun correlate(bytes: ByteArray, original: DiagnosticControlRequestV1,
                  expectedOwner: Int = 3): DiagnosticControlReplyV1 {
        validate(original)
        val reply = reply(bytes, expectedOwner)
        require(reply.command == original.command && reply.message == original.message && reply.deviceId == original.deviceId &&
            reply.bootId == original.bootId && reply.runId == original.runId) { "DG response correlation mismatch" }
        when (original.command) {
            DiagnosticControlCommandV1.GRANT -> if (reply.outcome == DiagnosticControlOutcomeV1.ACCEPTED) require(reply.expiresDeviceMs - reply.issuedDeviceMs == original.ttlMs.toLong())
            DiagnosticControlCommandV1.CANCEL -> require(reply.grantId == original.grantId)
            DiagnosticControlCommandV1.CAPABILITY -> Unit
        }
        return reply
    }
}

/** Host copies of the FULL original metadata. No live trusted Android issuer exists yet.
 * Values are not wire fields and constructing them does not authenticate anyone. */
data class DiagnosticControlOriginV1(
    val localGattGeneration: Long,
    val connectionToken: Long,
    val connectionHandle: Int,
    val authorityEpoch: Long,
    val authorityPrincipal: Long,
    val gameMaster: Boolean,
)
enum class DiagnosticControlPendingStateV1 { PENDING, ANSWERED, TIMEOUT, CANCELLED, INVALIDATED }

/** One original request and a fixed pre-send deadline. No resend, reconnect or grant activation. */
class PendingDiagnosticControlV1(
    private val request: DiagnosticControlRequestV1,
    private val original: DiagnosticControlOriginV1,
    private val submittedAtMs: Long,
) {
    var state = DiagnosticControlPendingStateV1.PENDING
        private set
    init {
        DiagnosticControlV1.encode(request)
        require(submittedAtMs in 0..Long.MAX_VALUE - DiagnosticControlV1.QUEUE_MS)
        require(original.localGattGeneration > 0 && original.connectionToken in 1..0xffff_ffffL && original.connectionHandle in 0..65535 &&
            original.authorityEpoch in 1..0xffff_ffffL && original.authorityPrincipal in 1..0xffff_ffffL && original.gameMaster)
    }
    fun tick(nowMs: Long) {
        if (state == DiagnosticControlPendingStateV1.PENDING && (nowMs < submittedAtMs || nowMs - submittedAtMs >= DiagnosticControlV1.QUEUE_MS)) state = DiagnosticControlPendingStateV1.TIMEOUT
    }
    fun cancel() { if (state == DiagnosticControlPendingStateV1.PENDING) state = DiagnosticControlPendingStateV1.CANCELLED }
    fun invalidate(current: DiagnosticControlOriginV1?) {
        if (state == DiagnosticControlPendingStateV1.PENDING && current != original) state = DiagnosticControlPendingStateV1.INVALIDATED
    }
    fun receive(bytes: ByteArray, captured: DiagnosticControlOriginV1, current: DiagnosticControlOriginV1?, receivedAtMs: Long, nowMs: Long): DiagnosticControlReplyV1? {
        tick(nowMs)
        invalidate(current)
        if (state != DiagnosticControlPendingStateV1.PENDING || captured != original || receivedAtMs < submittedAtMs || receivedAtMs > nowMs || nowMs - receivedAtMs >= DiagnosticControlV1.QUEUE_MS) return null
        val reply = try { DiagnosticControlV1.correlate(bytes, request) } catch (_: IllegalArgumentException) { return null }
        state = DiagnosticControlPendingStateV1.ANSWERED
        return reply
    }
}
