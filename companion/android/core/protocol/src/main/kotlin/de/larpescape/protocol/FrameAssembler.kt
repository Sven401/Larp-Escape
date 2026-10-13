package de.larpescape.protocol

/** Firmware v1 envelope / v2 JSON. Call from one serialized session only. */
class FrameAssembler {
    private data class Partial(val id: Int, val bytes: ByteArray, val started: Long, var offset: Int = 0)
    private val pending = mutableMapOf<Int, Partial>()
    fun clear() = pending.clear()
    fun accept(frame: ByteArray, nowMs: Long): Pair<Int, ByteArray>? {
        require(frame.size in 9..128 && frame[0].toInt() == 1) { "Frameversion oder Länge ungültig" }
        val kind = frame[1].toInt()
        require(kind == 2 || kind == 4) { "Unbekannter Frametyp" }
        fun u16(at: Int) = (frame[at].toInt() and 255) or ((frame[at + 1].toInt() and 255) shl 8)
        val id = u16(2); val total = u16(4); val offset = u16(6)
        require(id > 0 && total in 1..(if (kind == 4) 6143 else 511) && offset + frame.size - 8 <= total) { "Framegrenzen ungültig" }
        var partial = pending[kind]
        if (partial != null && nowMs - partial.started >= if (kind == 4) 30000 else 2000) {
            pending.remove(kind)
            error("Absolute Framefrist überschritten")
        }
        if (partial == null) {
            // Subscription can begin in the middle of a frozen status sample.
            if (kind == 4 && offset != 0) return null
            require(offset == 0) { "Nachrichtenanfang fehlt" }
            partial = Partial(id, ByteArray(total), nowMs)
            pending[kind] = partial
        }
        require(partial.id == id && partial.bytes.size == total && partial.offset == offset) { "Fragmentreihenfolge ungültig" }
        frame.copyInto(partial.bytes, offset, 8)
        partial.offset += frame.size - 8
        if (partial.offset != total) return null
        pending.remove(kind)
        return kind to partial.bytes
    }
}
