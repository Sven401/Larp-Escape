package de.larpescape.gamemaster.data

import de.larpescape.gamemaster.station.Station
import de.larpescape.protocol.LedAccess
import java.io.*

internal data class StationEntry(val station: Station, val bytes: ByteArray)

/** Strict bounded plaintext codec. StationVault encrypts before writing any bytes to disk. */
internal object StationCatalogCodec {
    fun decode(plain: ByteArray): List<StationEntry> {
        require(plain.size in 8..131043)
        return DataInputStream(ByteArrayInputStream(plain)).use { input ->
            require(input.readInt() == 1)
            val count = input.readInt(); require(count in 0..32)
            val entries = mutableListOf<StationEntry>()
            try {
                repeat(count) {
                    val id = input.readUTF(); val event = input.readUTF(); val name = input.readUTF(); val address = input.readUTF()
                    val size = input.readInt(); require(size in 1..LedAccess.MAX_BYTES)
                    val bytes = ByteArray(size); input.readFully(bytes)
                    val access = LedAccess.parse(bytes)
                    entries.add(StationEntry(Station(id,event,name,address,access.deviceId().joinToString("") { "%02x".format(it) }),bytes))
                }
                require(input.read() == -1); Station.validateCatalog(entries.map { it.station }); entries
            } catch (error: Exception) { entries.forEach { it.bytes.fill(0) }; throw error }
        }
    }
    fun encode(entries: List<StationEntry>): ByteArray {
        Station.validateCatalog(entries.map { it.station })
        val buffer = object : ByteArrayOutputStream() { fun wipe() { buf.fill(0) } }
        try {
            DataOutputStream(buffer).use { output ->
                output.writeInt(1); output.writeInt(entries.size)
                entries.forEach { entry ->
                    val access = LedAccess.parse(entry.bytes)
                    require(access.deviceId().joinToString("") { "%02x".format(it) } == entry.station.deviceId)
                    output.writeUTF(entry.station.id); output.writeUTF(entry.station.event); output.writeUTF(entry.station.name); output.writeUTF(entry.station.address)
                    output.writeInt(entry.bytes.size); output.write(entry.bytes)
                }
            }
            return buffer.toByteArray().also { require(it.size <= 131043) }
        } finally { buffer.wipe() }
    }
}
