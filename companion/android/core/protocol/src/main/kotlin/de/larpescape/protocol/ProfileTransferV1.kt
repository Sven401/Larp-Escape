package de.larpescape.protocol

import java.util.zip.CRC32

enum class ProfileOperation(val wire: Int) { FINISH(1), ABORT(2), APPLY(3) }
enum class ProfileResponseCode { DRAFT_READY, APPLY_QUEUED, SAVED, ABORTED, RECEIVE_REJECTED, IMPORT_REJECTED, APPLY_REJECTED, UNAUTHORIZED, REQUEST_CONFLICT, REQUEST_EXPIRED, OUT_OF_ORDER, EXPIRED, APPLY_FAILED }
data class ProfileResponse(
    val transfer: Int, val request: Long, val operation: ProfileOperation,
    val code: ProfileResponseCode, val detail: Int, val draftRevision: Long, val configurationRevision: Long,
)

/** Exact existing firmware profile v1 values. This codec grants no authority and validates no game schema. */
object ProfileTransferV1 {
    const val MAX_SOURCE_BYTES = 131072
    private fun u16(b: ByteArray, at: Int) = (b[at].toInt() and 255) or ((b[at + 1].toInt() and 255) shl 8)
    private fun u32(b: ByteArray, at: Int) = u16(b, at).toLong() or (u16(b, at + 2).toLong() shl 16)
    private fun put(b: ByteArray, at: Int, v: Long, count: Int) { repeat(count) { b[at + it] = (v ushr (8 * it)).toByte() } }
    private fun transfer(value: Int) = require(value in 1..65535) { "Ungueltige Profiltransferkennung." }
    private fun uint(value: Long, minimum: Long = 0) = require(value in minimum..0xffff_ffffL)

    fun metadata(source: ByteArray, transfer: Int, expectedDraft: Long, expectedConfiguration: Long): ByteArray {
        transfer(transfer)
        require(source.size in 1..MAX_SOURCE_BYTES) { "Profil muss 1 bis 131072 Bytes enthalten." }
        require(expectedDraft in 0..0xffff_fffeL); uint(expectedConfiguration, 1)
        val crc = CRC32().also { it.update(source) }.value
        return ByteArray(20).also {
            it[0] = 1; it[1] = 5; put(it, 2, transfer.toLong(), 2); put(it, 4, source.size.toLong(), 4)
            put(it, 8, crc, 4); put(it, 12, expectedDraft, 4); put(it, 16, expectedConfiguration, 4)
        }
    }

    fun data(transfer: Int, offset: Int, payload: ByteArray): ByteArray {
        transfer(transfer)
        require(offset >= 0 && payload.size in 1..120 && offset.toLong() + payload.size <= MAX_SOURCE_BYTES)
        return ByteArray(8 + payload.size).also {
            it[0] = 1; it[1] = 6; put(it, 2, transfer.toLong(), 2); put(it, 4, offset.toLong(), 4)
            payload.copyInto(it, 8)
        }
    }

    fun control(transfer: Int, request: Long, operation: ProfileOperation): ByteArray {
        transfer(transfer); uint(request, 1)
        return ByteArray(12).also {
            it[0] = 1; it[1] = 7; put(it, 2, transfer.toLong(), 2); it[4] = operation.wire.toByte(); put(it, 8, request, 4)
        }
    }

    fun response(bytes: ByteArray): ProfileResponse {
        require(bytes.size == 20 && bytes[0] == 1.toByte() && bytes[1] == 8.toByte()) { "Ungueltiges Profilantwortformat." }
        val id = u16(bytes, 2); transfer(id)
        val request = u32(bytes, 4); uint(request, 1)
        val operation = ProfileOperation.entries.firstOrNull { it.wire == (bytes[8].toInt() and 255) }
            ?: throw IllegalArgumentException("Profiloperation unbekannt.")
        val code = bytes[9].toInt() and 255
        val detail = u16(bytes, 10); val draft = u32(bytes, 12); val configuration = u32(bytes, 16)
        require(code in 1..13 && detail in 0..11)
        require(code > 4 || detail == 0)
        require(code > 3 || (draft > 0 && configuration > 0))
        require(code != 1 || operation == ProfileOperation.FINISH)
        require(code !in setOf(2, 3, 13) || operation == ProfileOperation.APPLY)
        require(code != 4 || operation == ProfileOperation.ABORT)
        return ProfileResponse(id, request, operation, ProfileResponseCode.entries[code - 1], detail, draft, configuration)
    }
}

/** One immutable bounded source; the caller must serialize values on the original authorized connection.
 * Each returned value is owned. No retry of data frames: the firmware rejects duplicated offsets.
 */
class ProfileSourceValues(source: ByteArray, private val transfer: Int, expectedDraft: Long, expectedConfiguration: Long, valueBytes: Int = 20) {
    init {
        // Reject external bounds before making a second document allocation or calculating CRC.
        require(source.size in 1..ProfileTransferV1.MAX_SOURCE_BYTES)
        require(transfer in 1..65535 && expectedDraft in 0..0xffff_fffeL && expectedConfiguration in 1..0xffff_ffffL)
        require(valueBytes in 20..128)
    }
    private val source = source.copyOf()
    private val metadata = ProfileTransferV1.metadata(this.source, transfer, expectedDraft, expectedConfiguration)
    private val payloadBytes = valueBytes - 8
    val size: Int get() = 1 + (source.size + payloadBytes - 1) / payloadBytes
    fun value(index: Int): ByteArray {
        require(index in 0 until size)
        if (index == 0) return metadata.copyOf()
        val offset = (index - 1) * payloadBytes
        return ProfileTransferV1.data(transfer, offset, source.copyOfRange(offset, minOf(offset + payloadBytes, source.size)))
    }
}
