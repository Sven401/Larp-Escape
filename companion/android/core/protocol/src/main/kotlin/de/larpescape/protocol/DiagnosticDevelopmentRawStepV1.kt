package de.larpescape.protocol

/** Exploratory I²C register observation; the physical button identity is unqualified. */
data class DiagnosticDevelopmentRawStepRequestV1(
    val message: Long, val deviceId: String, val bootId: String,
    val runId: String, val grantId: String, val stepId: Long,
    val timeoutMs: Int = 1000,
)

enum class DiagnosticDevelopmentRawStepOutcomeV1 { REGISTER_OBSERVED, I2C_FAILED, REJECTED }
enum class DiagnosticDevelopmentRawI2cStatusV1 { VALID, REGISTER_SELECT_FAILED, SHORT_READ, READ_FAILED }
enum class DiagnosticDevelopmentRawStepRejectionV1 {
    NONE, PERMISSION, EXPIRED, REPLAY, BUSY, INVALID, CAPACITY, LINK_CHANGED
}

data class DiagnosticDevelopmentRawStepReplyV1(
    val request: DiagnosticDevelopmentRawStepRequestV1,
    val startedDeviceMs: Long, val endedDeviceMs: Long,
    val outcome: DiagnosticDevelopmentRawStepOutcomeV1,
    val rejection: DiagnosticDevelopmentRawStepRejectionV1,
    val i2cStatus: DiagnosticDevelopmentRawI2cStatusV1,
    val rawByte: Int?, val rawHigh: Boolean?, val busCode: Int,
) {
    val physicalChannelQualified: Boolean get() = false
}

object DiagnosticDevelopmentRawStepV1 {
    const val REQUEST_BYTES = 80
    const val REPLY_BYTES = 104
    private const val MAX_TIME = 9007199254740991L
    private val id = Regex("[0-9A-F]{32}")
    private val boot = Regex("[0-9A-F]{16}")
    private val uuid = Regex("[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}")
    private fun hex(value: String): ByteArray = value.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private fun put(bytes: ByteArray, at: Int, value: ULong, count: Int) {
        repeat(count) { bytes[at + it] = (value shr (8 * it)).toByte() }
    }
    private fun read(bytes: ByteArray, at: Int, count: Int): ULong {
        var value = 0uL
        repeat(count) { value = value or ((bytes[at + it].toInt() and 255).toULong() shl (8 * it)) }
        return value
    }
    fun encode(request: DiagnosticDevelopmentRawStepRequestV1): ByteArray {
        require(request.message in 1..0xffff_ffffL && request.stepId in 1..0xffff_ffffL &&
            request.timeoutMs in 1000..10000 && id.matches(request.deviceId) &&
            request.deviceId.any { it != '0' } && boot.matches(request.bootId) &&
            request.bootId.any { it != '0' } && uuid.matches(request.runId) &&
            uuid.matches(request.grantId)) { "Invalid development raw-step context" }
        return ByteArray(REQUEST_BYTES).apply {
            this[0] = 'D'.code.toByte(); this[1] = 'G'.code.toByte(); this[2] = 1; this[3] = 6
            put(this, 4, request.message.toULong(), 4)
            hex(request.deviceId).copyInto(this, 8)
            put(this, 24, request.bootId.toULong(16), 8)
            hex(request.runId.replace("-", "")).copyInto(this, 32)
            hex(request.grantId.replace("-", "")).copyInto(this, 48)
            put(this, 64, request.stepId.toULong(), 4)
            put(this, 68, request.timeoutMs.toULong(), 4)
            this[72] = 1; this[73] = 0x20; this[74] = 0x13; this[75] = 6
        }
    }
    /** Requires the complete original 80 bytes, including all fixed I²C parameters. */
    fun correlate(bytes: ByteArray, original: DiagnosticDevelopmentRawStepRequestV1):
        DiagnosticDevelopmentRawStepReplyV1 {
        require(bytes.size == REPLY_BYTES && bytes.copyOfRange(0, REQUEST_BYTES)
            .contentEquals(encode(original))) { "Development raw-step correlation mismatch" }
        require(bytes[101] == 0.toByte() && bytes[103] == 0.toByte()) {
            "Physical qualification is unsupported"
        }
        val outcome = DiagnosticDevelopmentRawStepOutcomeV1.entries
            .getOrNull(bytes[96].toInt() and 255) ?: throw IllegalArgumentException("Unknown raw-step outcome")
        val status = DiagnosticDevelopmentRawI2cStatusV1.entries
            .getOrNull(bytes[97].toInt() and 255) ?: throw IllegalArgumentException("Unknown I2C status")
        val rejection = DiagnosticDevelopmentRawStepRejectionV1.entries
            .getOrNull(bytes[102].toInt() and 255) ?: throw IllegalArgumentException("Unknown rejection reason")
        val raw = bytes[98].toInt() and 255
        val high = bytes[99].toInt() and 255
        val busCode = bytes[100].toInt() and 255
        val started = read(bytes, 80, 8)
        val ended = read(bytes, 88, 8)
        require(started <= MAX_TIME.toULong() && ended <= MAX_TIME.toULong() && high <= 1)
        if (outcome == DiagnosticDevelopmentRawStepOutcomeV1.REJECTED) {
            require(started == 0uL && ended == 0uL &&
                rejection != DiagnosticDevelopmentRawStepRejectionV1.NONE &&
                status == DiagnosticDevelopmentRawI2cStatusV1.READ_FAILED &&
                raw == 0 && high == 0 && busCode == 0)
        } else {
            require(rejection == DiagnosticDevelopmentRawStepRejectionV1.NONE)
            require(started <= ended && ended - started < original.timeoutMs.toULong())
            if (outcome == DiagnosticDevelopmentRawStepOutcomeV1.REGISTER_OBSERVED)
                require(status == DiagnosticDevelopmentRawI2cStatusV1.VALID && busCode == 0 &&
                    (high == 1) == ((raw and 0x40) != 0))
            else require(status != DiagnosticDevelopmentRawI2cStatusV1.VALID && raw == 0 && high == 0)
        }
        return DiagnosticDevelopmentRawStepReplyV1(original, started.toLong(), ended.toLong(),
            outcome, rejection, status, raw.takeIf { outcome == DiagnosticDevelopmentRawStepOutcomeV1.REGISTER_OBSERVED },
            (high == 1).takeIf { outcome == DiagnosticDevelopmentRawStepOutcomeV1.REGISTER_OBSERVED },
            busCode)
    }
}
