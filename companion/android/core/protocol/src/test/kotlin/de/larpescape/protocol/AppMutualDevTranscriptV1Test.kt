package de.larpescape.protocol

import org.junit.Assert.*
import org.junit.Test
import java.io.File

class AppMutualDevTranscriptV1Test {
    private fun fill(size: Int, seed: Int) = ByteArray(size) { (seed + it).toByte() }
    private fun fixture() = AppMutualDevTranscriptV1(
        chip = fill(6, 1), deviceId = fill(16, 11), bootId = 0x0102030405060708L,
        sourceRegistration = 0x11223344, sdkGeneration = 0x55667788, gattInterface = 9,
        peerAddress = fill(6, 31), connectionToken = 0x91a2b3c4, connectionHandle = 0x1234,
        phoneInstallId = fill(16, 41), boxKeyId = fill(16, 61),
        boxPublic = fill(65, 101).also { it[0] = 4 },
        phoneKeyId = fill(16, 81), phonePublic = fill(65, 151).also { it[0] = 4 },
        boxNonce = fill(16, 201), phoneNonce = fill(16, 221), attempt = 0x01020304,
        boxEnrollmentGeneration = 3, phoneEnrollmentGeneration = 5,
        boxRevocationGeneration = 7, phoneRevocationGeneration = 9)

    private fun golden(): ByteArray {
        var directory = File(System.getProperty("user.dir")).absoluteFile
        repeat(7) {
            val candidate = File(directory, "docs/companion/fixtures/app-mutual-dev-transcript-v1.hex")
            if (candidate.isFile) {
                val hex = candidate.readText().trim()
                assertEquals(AppMutualDevTranscriptCodecV1.FRAME_SIZE * 2, hex.length)
                return ByteArray(hex.length / 2) { hex.substring(it * 2, it * 2 + 2).toInt(16).toByte() }
            }
            directory = directory.parentFile ?: return@repeat
        }
        error("Shared APP_MUTUAL development golden fixture missing")
    }

    @Test fun cppGoldenFrameAndAllByteComparison() {
        val fixture = fixture()
        val frame = AppMutualDevTranscriptCodecV1.encode(fixture)
        assertEquals(289, frame.size)
        assertArrayEquals(golden(), frame)
        assertArrayEquals(frame, AppMutualDevTranscriptCodecV1.encode(AppMutualDevTranscriptCodecV1.decode(frame)))
        frame.indices.forEach { index ->
            val fake = AppMutualDevClosedFakeV1()
            assertTrue(fake.begin(fixture))
            val changed = frame.copyOf().also { it[index] = (it[index].toInt() xor 1).toByte() }
            assertFalse("byte $index", fake.check(changed))
            assertEquals(AppMutualDevFakeStateV1.Closed, fake.state)
            assertFalse(fake.check(frame))
        }
        val matched = AppMutualDevClosedFakeV1()
        assertTrue(matched.begin(fixture))
        assertTrue(matched.check(frame))
        assertEquals(AppMutualDevFakeStateV1.TranscriptCheckedClosed, matched.state)
        assertFalse(matched.check(frame))
        assertFalse(matched.begin(fixture))
    }

    @Test fun boundariesAndMalformedFieldsClose() {
        val frame = golden()
        assertThrows(IllegalArgumentException::class.java) { AppMutualDevTranscriptCodecV1.decode(frame.copyOf(288)) }
        assertThrows(IllegalArgumentException::class.java) { AppMutualDevTranscriptCodecV1.decode(frame + byteArrayOf(0)) }
        for (offset in 0..7) {
            val changed = frame.copyOf().also { it[offset] = (it[offset].toInt() xor 1).toByte() }
            assertThrows(IllegalArgumentException::class.java) { AppMutualDevTranscriptCodecV1.decode(changed) }
        }
        for (offset in listOf(53, 269, 273, 277)) {
            val changed = frame.copyOf().also { it.fill(0, offset, offset + 4) }
            assertThrows(IllegalArgumentException::class.java) { AppMutualDevTranscriptCodecV1.decode(changed) }
        }
        for (offset in listOf(91, 172)) {
            val changed = frame.copyOf().also { it[offset] = 0 }
            assertThrows(IllegalArgumentException::class.java) { AppMutualDevTranscriptCodecV1.decode(changed) }
        }
        val staleAttempt = AppMutualDevClosedFakeV1()
        val next = fixture().also { it.boxNonce[0] = (it.boxNonce[0] + 1).toByte() }
        assertTrue(staleAttempt.begin(next))
        assertFalse(staleAttempt.check(frame))
        assertEquals(AppMutualDevFakeStateV1.Closed, staleAttempt.state)
        val closed = AppMutualDevClosedFakeV1()
        assertTrue(closed.begin(fixture()))
        closed.close()
        assertFalse(closed.check(frame))
    }
}
