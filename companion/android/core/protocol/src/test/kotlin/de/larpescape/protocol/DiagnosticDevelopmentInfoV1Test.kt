package de.larpescape.protocol

import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class DiagnosticDevelopmentInfoV1Test {
    private val frame = byteArrayOf('D'.code.toByte(), 'T'.code.toByte(), 1) +
        ByteArray(16) { (it + 1).toByte() } +
        byteArrayOf(0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01)

    @Test fun decodesDeviceAndBootWithoutGrantingAuthority() {
        assertEquals(DiagnosticDevelopmentInfoV1(
            "0102030405060708090A0B0C0D0E0F10", "0102030405060708"),
            DiagnosticDevelopmentInfoCodecV1.decode(frame))
    }

    @Test fun rejectsTruncationUnknownVersionAndEmptyIdentity() {
        for (invalid in listOf(frame.copyOf(22), frame.copyOf().also { it[2] = 2 },
                frame.copyOf().also { it.fill(0, 3, 19) },
                frame.copyOf().also { it.fill(0, 19, 27) }))
            assertThrows(IllegalArgumentException::class.java) {
                DiagnosticDevelopmentInfoCodecV1.decode(invalid)
            }
    }
}
