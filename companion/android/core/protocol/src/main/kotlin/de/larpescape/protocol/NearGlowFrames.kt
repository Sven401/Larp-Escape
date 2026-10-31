package de.larpescape.protocol

/** Same LED-V2 bearer; PROX permits its specified 256-byte message budget. */
class NearGlowFrameAssembler {
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
        require(frame.size in 9..20 && frame[0] == 1.toByte() && frame[1] == 0x4c.toByte())
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
