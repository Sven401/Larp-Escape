package de.larpescape.protocol

import com.google.gson.stream.JsonReader
import com.google.gson.stream.JsonToken
import java.io.StringReader
import java.nio.ByteBuffer
import java.nio.charset.CodingErrorAction

/** Explicit USB-exported app credential. An ESP-NOW pair file is never accepted. */
class NearGlowAccess private constructor(private val id: ByteArray, private val key: ByteArray, val stationMac: String) {
    fun deviceId() = id.copyOf()
    internal fun secret() = key.copyOf()
    companion object {
        const val MAX_BYTES = 2048
        fun parse(bytes: ByteArray): NearGlowAccess {
            require(bytes.size in 1..MAX_BYTES) { "Ungültige Stationszugangsdatei." }
            val text = try {
                Charsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes)).toString()
            } catch (_: Exception) { throw IllegalArgumentException("Ungültige Stationszugangsdatei.") }
            val fields = mutableSetOf<String>()
            var id: ByteArray? = null; var key: ByteArray? = null; var mac: String? = null
            JsonReader(StringReader(text)).use { reader ->
                reader.beginObject()
                while (reader.hasNext()) {
                    val field = reader.nextName()
                    require(fields.add(field)) { "Doppeltes Einrichtungsfeld." }
                    if (field == "schemaVersion") {
                        require(reader.peek() == JsonToken.NUMBER && reader.nextString() == "1")
                    } else {
                        require(reader.peek() == JsonToken.STRING)
                        val value = reader.nextString()
                        when (field) {
                            "protocol" -> require(value == "LARP-PROX-BLE/1")
                            "deviceId" -> id = hex(value, 16)
                            "secret" -> key = hex(value, 32)
                            "stationMac" -> {
                                require(espNowPeer(value) != null && value.length == 17)
                                mac = value.uppercase()
                            }
                            "name" -> require(value.length in 1..64 && value.none { it.isISOControl() })
                            else -> throw IllegalArgumentException("Unbekanntes Einrichtungsfeld.")
                        }
                    }
                }
                reader.endObject()
                require(reader.peek() == JsonToken.END_DOCUMENT)
            }
            require(fields == setOf("schemaVersion", "protocol", "deviceId", "secret", "name", "stationMac"))
            return NearGlowAccess(requireNotNull(id), requireNotNull(key), requireNotNull(mac))
        }
        private fun hex(text: String, length: Int): ByteArray {
            require(text.length == length * 2 && text.all { it in '0'..'9' || it in 'a'..'f' || it in 'A'..'F' })
            return ByteArray(length) { text.substring(it * 2, it * 2 + 2).toInt(16).toByte() }.also {
                require(it.any { byte -> byte != 0.toByte() })
            }
        }
    }
}
