package de.larpescape.protocol

import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.CRC32

data class NfcNodeVisit(val nodeId: Int, val result: Int, val deltaMinutes: Int)
data class NfcTagData(
    val uidHash: Long = 0, val flags: Int = 0,
    // Preserve DateTime.ToBinary() exactly, including Kind bits; never substitute a Java epoch.
    val timestampBinary: Long = 0, val visits: List<NfcNodeVisit> = emptyList()
)

/** GM-Companion TagData v1 payload, not the complete NDEF message. */
object NfcTagDataCodec {
    const val MIME = "application/vnd.yourapp.tagdata"
    const val MAX_BYTES = 65535
    const val MAX_VISITS = (MAX_BYTES - 21) / 4
    fun encode(data: NfcTagData): ByteArray {
        require(data.uidHash in 0..4294967295L && data.flags in 0..65535) { "Kennwert oder Flags außerhalb des Bereichs" }
        require(data.visits.size <= MAX_VISITS) { "Zu viele Stationsbesuche" }
        val b = ByteBuffer.allocate(21 + data.visits.size * 4).order(ByteOrder.LITTLE_ENDIAN)
        b.put(1); b.putInt(data.uidHash.toInt()); b.putShort(data.flags.toShort())
        b.putLong(data.timestampBinary); b.putShort(data.visits.size.toShort())
        data.visits.forEach {
            require(it.nodeId in 0..255 && it.result in 0..255 && it.deltaMinutes in 0..65535) { "Stationsbesuch außerhalb des Bereichs" }
            b.put(it.nodeId.toByte()); b.put(it.result.toByte()); b.putShort(it.deltaMinutes.toShort())
        }
        val crc = CRC32().apply { update(b.array(), 0, b.position()) }.value
        b.putInt(crc.toInt())
        return b.array()
    }
    fun decode(bytes: ByteArray): NfcTagData {
        require(bytes.size in 21..MAX_BYTES) { "Tagdatenlänge ungültig" }
        val b = ByteBuffer.wrap(bytes.copyOf()).order(ByteOrder.LITTLE_ENDIAN)
        require(b.get().toInt() == 1) { "Unbekannte Tagdatenversion" }
        val hash = b.int.toLong() and 0xffffffffL
        val flags = b.short.toInt() and 65535
        val timestamp = b.long
        val count = b.short.toInt() and 65535
        require(bytes.size == 21 + 4 * count) { "Tagdaten unvollständig oder mit Zusatzbytes" }
        val crc = CRC32().apply { update(b.array(), 0, bytes.size - 4) }.value
        val expected = ByteBuffer.wrap(b.array(), bytes.size - 4, 4).order(ByteOrder.LITTLE_ENDIAN).int.toLong() and 0xffffffffL
        require(crc == expected) { "Prüfsumme der Tagdaten fehlerhaft" }
        val visits = List(count) { NfcNodeVisit(b.get().toInt() and 255, b.get().toInt() and 255, b.short.toInt() and 65535) }
        return NfcTagData(hash, flags, timestamp, java.util.Collections.unmodifiableList(visits))
    }
}

sealed interface NfcReadback {
    data class Bytes(val message: ByteArray?) : NfcReadback
    data object RequiresRediscovery : NfcReadback
}

/** Uses NDEF reads and explicitly reports when verification needs a new discovery. */
interface NfcMessagePort : AutoCloseable {
    fun connect()
    val writable: Boolean
    val capacity: Int
    fun read(): ByteArray?
    fun write(message: ByteArray)
    fun readAfterWrite(): NfcReadback = NfcReadback.Bytes(read())
}
enum class NfcWriteResult { VERIFIED, VERIFICATION_REQUIRED, CHANGED, READ_ONLY, TOO_SMALL, READ_FAILED, UNCERTAIN, CANCELLED }

object VerifiedNfcWrite {
    fun execute(port: NfcMessagePort, expected: ByteArray?, replacement: ByteArray,
                current: () -> Boolean = { true }): NfcWriteResult {
        // The caller owns immutable request bytes; do not expose them to a bearer implementation.
        val before = expected?.copyOf(); val next = replacement.copyOf()
        var attempted = false
        try {
            if (!current()) return NfcWriteResult.CANCELLED
            port.connect()
            val actual = port.read()
            if (if (before == null) actual != null else actual == null || !actual.contentEquals(before)) return NfcWriteResult.CHANGED
            if (!port.writable) return NfcWriteResult.READ_ONLY
            if (next.size > port.capacity) return NfcWriteResult.TOO_SMALL
            if (!current()) return NfcWriteResult.CANCELLED
            attempted = true
            port.write(next.copyOf())
            if (!current()) return NfcWriteResult.UNCERTAIN
            return when (val verified = port.readAfterWrite()) {
                is NfcReadback.Bytes -> if (current() && verified.message?.contentEquals(next) == true)
                    NfcWriteResult.VERIFIED else NfcWriteResult.UNCERTAIN
                NfcReadback.RequiresRediscovery -> if (current()) NfcWriteResult.VERIFICATION_REQUIRED else NfcWriteResult.UNCERTAIN
            }
        } catch (_: Exception) {
            return if (attempted) NfcWriteResult.UNCERTAIN else if (!current()) NfcWriteResult.CANCELLED else NfcWriteResult.READ_FAILED
        } finally {
            try { port.close() } catch (_: Exception) { /* A verified RF read is already complete. */ }
        }
    }
}
