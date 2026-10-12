package de.larpescape.protocol

/** Development-image single LED test; software black is not physical off. */
data class DiagnosticDevelopmentLedStepRequestV1(
    val message: Long, val deviceId: String, val bootId: String,
    val runId: String, val grantId: String, val stepId: Long,
    val index: Int = 0, val red: Int = 16, val green: Int = 0, val blue: Int = 0,
    val brightness: Int = 16, val durationMs: Int = 250, val timeoutMs: Int = 1000,
)

enum class DiagnosticDevelopmentLedStepOutcomeV1 { BLACK_SUBMITTED, SOFTWARE_FAULT, REJECTED }
enum class DiagnosticDevelopmentLedStepRejectionV1 {
    NONE, HARDWARE_UNQUALIFIED, PERMISSION, EXPIRED, REPLAY, BUSY, INVALID, CAPACITY, LINK_CHANGED
}
data class DiagnosticDevelopmentLedStepReplyV1(
    val request: DiagnosticDevelopmentLedStepRequestV1,
    val startedDeviceMs: Long, val endedDeviceMs: Long,
    val outcome: DiagnosticDevelopmentLedStepOutcomeV1,
    val rejection: DiagnosticDevelopmentLedStepRejectionV1,
    val renderCalls: Int, val blackSubmitted: Boolean, val timerFired: Boolean,
) {
    val physicalOutputConfirmed: Boolean get() = false
}

object DiagnosticDevelopmentLedStepV1 {
    const val REQUEST_BYTES = 80
    const val REPLY_BYTES = 104
    private const val MAX_TIME = 9007199254740991L
    private val id = Regex("[0-9A-F]{32}")
    private val boot = Regex("[0-9A-F]{16}")
    private val uuid = Regex("[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}")
    private fun hex(value: String) = value.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private fun put(bytes: ByteArray, at: Int, value: ULong, count: Int) {
        repeat(count) { bytes[at + it] = (value shr (8 * it)).toByte() }
    }
    private fun read(bytes: ByteArray, at: Int, count: Int): ULong {
        var value = 0uL
        repeat(count) { value = value or ((bytes[at + it].toInt() and 255).toULong() shl (8 * it)) }
        return value
    }
    fun encode(request: DiagnosticDevelopmentLedStepRequestV1): ByteArray {
        require(request.message in 1..0xffff_ffffL && request.stepId in 1..0xffff_ffffL &&
            request.timeoutMs in 1000..10000 && id.matches(request.deviceId) &&
            request.deviceId.any { it != '0' } && boot.matches(request.bootId) &&
            request.bootId.any { it != '0' } && uuid.matches(request.runId) &&
            uuid.matches(request.grantId) && request.index in 0..22 &&
            request.red in 0..32 && request.green in 0..32 && request.blue in 0..32 &&
            request.red + request.green + request.blue > 0 &&
            request.brightness in 1..32 && request.durationMs in 100..250)
        return ByteArray(REQUEST_BYTES).apply {
            this[0] = 'D'.code.toByte(); this[1] = 'G'.code.toByte(); this[2] = 1; this[3] = 7
            put(this, 4, request.message.toULong(), 4)
            hex(request.deviceId).copyInto(this, 8)
            put(this, 24, request.bootId.toULong(16), 8)
            hex(request.runId.replace("-", "")).copyInto(this, 32)
            hex(request.grantId.replace("-", "")).copyInto(this, 48)
            put(this, 64, request.stepId.toULong(), 4)
            put(this, 68, request.timeoutMs.toULong(), 4)
            this[72] = 2; this[73] = request.index.toByte(); this[74] = request.red.toByte()
            this[75] = request.green.toByte(); this[76] = request.blue.toByte()
            this[77] = request.brightness.toByte()
            put(this, 78, request.durationMs.toULong(), 2)
        }
    }

    /** Full request echo, fixed result shape and no physical-confirmation bit. */
    fun correlate(bytes: ByteArray, original: DiagnosticDevelopmentLedStepRequestV1):
        DiagnosticDevelopmentLedStepReplyV1 {
        require(bytes.size == REPLY_BYTES &&
            bytes.copyOfRange(0, REQUEST_BYTES).contentEquals(encode(original))) {
            "Development LED-step correlation mismatch"
        }
        require(bytes[96].toInt() in 0..2 && bytes[97].toInt() in 0..8 &&
            bytes[98].toInt() in 0..2 && bytes[99].toInt() in 0..1 &&
            bytes[100].toInt() in 0..1 && bytes[101] == 0.toByte() &&
            bytes[102] == 0.toByte() && bytes[103] == 0.toByte())
        val outcome = DiagnosticDevelopmentLedStepOutcomeV1.entries[bytes[96].toInt()]
        val rejection = DiagnosticDevelopmentLedStepRejectionV1.entries[bytes[97].toInt()]
        val renders = bytes[98].toInt()
        val black = bytes[99] == 1.toByte()
        val fired = bytes[100] == 1.toByte()
        val started = read(bytes, 80, 8)
        val ended = read(bytes, 88, 8)
        require(started <= MAX_TIME.toULong() && ended <= MAX_TIME.toULong())
        if (outcome == DiagnosticDevelopmentLedStepOutcomeV1.REJECTED)
            require(started == 0uL && ended == 0uL && renders == 0 && !black && !fired &&
                rejection != DiagnosticDevelopmentLedStepRejectionV1.NONE)
        else {
            require(rejection == DiagnosticDevelopmentLedStepRejectionV1.NONE && started <= ended &&
                ended - started < original.timeoutMs.toULong())
            if (outcome == DiagnosticDevelopmentLedStepOutcomeV1.BLACK_SUBMITTED)
                require(renders == 2 && black && fired &&
                    ended - started >= original.durationMs.toULong())
            else require(renders < 2 || !black || !fired)
        }
        return DiagnosticDevelopmentLedStepReplyV1(original, started.toLong(), ended.toLong(),
            outcome, rejection, renders, black, fired)
    }
}
