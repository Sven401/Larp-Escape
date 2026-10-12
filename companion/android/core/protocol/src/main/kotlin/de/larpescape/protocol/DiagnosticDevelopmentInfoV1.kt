package de.larpescape.protocol

import java.util.UUID

/** Public image identity read from the development-only diagnostic characteristic. */
data class DiagnosticDevelopmentInfoV1(val deviceId: String, val bootId: String)

object DiagnosticDevelopmentInfoCodecV1 {
    val characteristic: UUID = UUID.fromString("407e6a1d-1cc2-43fd-856b-4aff85a20015")
    const val BYTES = 27

    fun decode(value: ByteArray): DiagnosticDevelopmentInfoV1 {
        require(value.size == BYTES && value[0] == 'D'.code.toByte() &&
            value[1] == 'T'.code.toByte() && value[2] == 1.toByte()) {
            "Unknown development image identity"
        }
        val device = value.copyOfRange(3, 19)
        val boot = value.copyOfRange(19, 27)
        require(device.any { it != 0.toByte() } && boot.any { it != 0.toByte() }) {
            "Missing development image identity"
        }
        val deviceId = device.joinToString("") { "%02X".format(it.toInt() and 255) }
        val bootId = boot.reversedArray().joinToString("") { "%02X".format(it.toInt() and 255) }
        return DiagnosticDevelopmentInfoV1(deviceId, bootId)
    }
}

/** Separate control endpoint of the development image; never a product GM role. */
object DiagnosticDevelopmentControlCodecV1 {
    val write: UUID = UUID.fromString("407e6a1d-1cc2-43fd-856b-4aff85a20016")
    val reply: UUID = UUID.fromString("407e6a1d-1cc2-43fd-856b-4aff85a20017")
    val probe: ByteArray = byteArrayOf('D'.code.toByte(), 'G'.code.toByte(), 1, 0x50, 3)

    fun correlate(bytes: ByteArray, request: DiagnosticControlRequestV1): DiagnosticControlReplyV1 =
        DiagnosticControlV1.correlate(bytes, request, expectedOwner = 4)
}
