package de.larpescape.protocol

import com.google.gson.stream.JsonReader
import com.google.gson.stream.JsonToken
import java.io.StringReader
import java.nio.ByteBuffer
import java.nio.charset.CodingErrorAction

/** Trusted USB setup file, never discovered or accepted from BLE. */
class WledAccess private constructor(private val device: ByteArray, private val secret: ByteArray, val name: String) {
    fun deviceId(): ByteArray = device.copyOf()
    internal fun secret(): ByteArray = secret.copyOf()
    companion object {
        const val MAX_BYTES = 2048
        fun parse(bytes: ByteArray): WledAccess {
            require(bytes.size in 1..MAX_BYTES) { "Box-Zugangsdatei ist zu groß oder leer." }
            val text = try {
                Charsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes)).toString()
            } catch (error: java.nio.charset.CharacterCodingException) {
                throw IllegalArgumentException("Ungültige UTF-8-Einrichtungsdatei.", error)
            }
            var version = false
            var device: ByteArray? = null
            var secret: ByteArray? = null
            var name: String? = null
            val fields = mutableSetOf<String>()
            JsonReader(StringReader(text)).use { reader ->
                reader.beginObject()
                while (reader.hasNext()) {
                    val field = reader.nextName()
                    require(fields.add(field)) { "Doppeltes Einrichtungsfeld." }
                    when (field) {
                        "schemaVersion" -> {
                            require(reader.peek() == JsonToken.NUMBER && reader.nextString() == "1") { "Unbekannte Einrichtungsversion." }
                            version = true
                        }
                        "deviceId", "secret", "name" -> {
                            require(reader.peek() == JsonToken.STRING) { "Ungültiges Einrichtungsfeld." }
                            val value = reader.nextString()
                            when (field) {
                                "deviceId" -> device = hex(value, 16)
                                "secret" -> secret = hex(value, 32)
                                else -> { require(value.length in 1..64 && value.none { it.isISOControl() }) { "Ungültiger Boxname." }; name = value }
                            }
                        }
                        else -> error("Unbekanntes Einrichtungsfeld.")
                    }
                }
                reader.endObject()
                require(reader.peek() == JsonToken.END_DOCUMENT) { "Zusätzliche Einrichtungsdaten." }
            }
            require(version && device != null && secret != null) { "Boxkennung oder Zugangsschlüssel fehlt." }
            return WledAccess(device!!, secret!!, name ?: "Box ${device!!.take(4).joinToString("") { "%02X".format(it) }}")
        }
        private fun hex(value: String, count: Int): ByteArray {
            require(value.length == count * 2 && value.all { it in '0'..'9' || it in 'a'..'f' || it in 'A'..'F' }) { "Ungültige Boxkennung oder Schlüssel." }
            return ByteArray(count) { value.substring(it * 2, it * 2 + 2).toInt(16).toByte() }.also {
                require(it.any { byte -> byte != 0.toByte() }) { "Leere Boxkennung oder Schlüssel." }
            }
        }
    }
}
