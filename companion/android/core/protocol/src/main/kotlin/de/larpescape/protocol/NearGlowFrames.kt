package de.larpescape.protocol

/** Unchanged eight-byte LED-V2 header, bounded by the negotiated ATT budget. */
object NearGlowFrames {
    fun encode(message: ByteArray, transfer: Int, maxFrameBytes: Int = 20): List<ByteArray> {
        require(message.size in 1..256 && transfer in 1..65535 && maxFrameBytes in 9..244)
        val payload = maxFrameBytes - 8
        return (message.indices step payload).map { offset ->
            byteArrayOf(1, 0x4c, transfer.toByte(), (transfer shr 8).toByte(), message.size.toByte(),
                (message.size shr 8).toByte(), offset.toByte(), (offset shr 8).toByte()) +
                message.copyOfRange(offset, minOf(offset + payload, message.size))
        }
    }
}

/** PROX keeps a 256-byte message bound; legacy callers retain their 20-byte frame default. */
class NearGlowFrameAssembler(private val maxFrameBytes: Int = 20) {
    init { require(maxFrameBytes in 9..244) }
    private var transfer = 0
    private var completed = 0
    private var started = 0L
    private var expected = 0
    private var total = 0
    private val bytes = ByteArray(256)
    private var previous: ByteArray? = null
    var completedStartedAt: Long? = null
        private set
    fun clear() { transfer = 0; completed = 0; expected = 0; previous = null; completedStartedAt = null }
    fun expired(now: Long) = transfer != 0 && (now < started || now - started >= 2000)
    fun accept(frame: ByteArray, now: Long): ByteArray? {
        require(frame.size in 9..maxFrameBytes && frame[0] == 1.toByte() && frame[1] == 0x4c.toByte())
        fun u16(at: Int) = (frame[at].toInt() and 255) or ((frame[at + 1].toInt() and 255) shl 8)
        val id = u16(2); val size = u16(4); val offset = u16(6)
        require(id > 0 && size in 1..256 && offset < size && offset + frame.size - 8 <= size && !expired(now))
        if (frame.contentEquals(previous)) return null
        if (transfer == 0) {
            require(id > completed && offset == 0)
            transfer = id; total = size; started = now; expected = 0
        }
        require(id == transfer && total == size && offset == expected)
        frame.copyInto(bytes, expected, 8); expected += frame.size - 8; previous = frame.copyOf()
        if (expected != total) return null
        val result = bytes.copyOf(total); completedStartedAt = started; completed = transfer; transfer = 0; previous = null
        return result
    }
}
