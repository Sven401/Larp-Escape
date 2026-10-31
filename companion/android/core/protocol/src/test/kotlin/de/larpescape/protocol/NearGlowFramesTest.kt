package de.larpescape.protocol

import org.junit.Assert.*
import org.junit.Test

class NearGlowFramesTest {
    @Test fun legacyDefaultFramesKeepExactlyTheSameHeaderAndTwelveByteChunks() {
        val message = ByteArray(25) { it.toByte() }
        val encoded = LedFrames.encode(message, 0x1234)
        assertEquals(3, encoded.size)
        assertArrayEquals(byteArrayOf(1, 0x4c, 0x34, 0x12, 25, 0, 0, 0) + message.copyOfRange(0, 12), encoded[0])
        assertArrayEquals(byteArrayOf(1, 0x4c, 0x34, 0x12, 25, 0, 12, 0) + message.copyOfRange(12, 24), encoded[1])
        assertArrayEquals(byteArrayOf(1, 0x4c, 0x34, 0x12, 25, 0, 24, 0) + message.copyOfRange(24, 25), encoded[2])
        assertThrows(IllegalArgumentException::class.java) { LedFrames.encode(ByteArray(129), 1) }
        val larger = LedFrames.encode(message, 1, 153).single()
        assertThrows(IllegalArgumentException::class.java) { LedFrameAssembler().accept(larger, 0) }
        assertArrayEquals(message, LedFrameAssembler(153).accept(larger, 0))
    }

    @Test fun allSupportedBudgetsRoundTripWithoutChangingTheMessageLimit() {
        for (budget in listOf(9, 20, 153, 244)) for (size in listOf(1, 52, 74, 128, 145, 256)) {
            val message = ByteArray(size) { (it xor 0x5a).toByte() }
            val frames = NearGlowFrames.encode(message, 65535, budget)
            assertEquals((size + budget - 9) / (budget - 8), frames.size)
            assertTrue(frames.all { it.size in 9..budget })
            val assembler = NearGlowFrameAssembler(budget)
            var completed: ByteArray? = null
            frames.forEachIndexed { index, frame -> completed = assembler.accept(frame, 10L + index) }
            assertArrayEquals(message, completed); assertEquals(10L, assembler.completedStartedAt)
        }
        assertEquals(1, NearGlowFrames.encode(ByteArray(145), 1, 153).size)
        assertEquals(listOf(244, 28), NearGlowFrames.encode(ByteArray(256), 1, 244).map { it.size })
        assertEquals(13, NearGlowFrames.encode(ByteArray(145), 1).size)
    }

    private fun frame(message: ByteArray, offset: Int, length: Int): ByteArray =
        byteArrayOf(1, 0x4c, 1, 0, message.size.toByte(), (message.size shr 8).toByte(), offset.toByte(), (offset shr 8).toByte()) +
            message.copyOfRange(offset, offset + length)

    @Test fun mixedSmallerFragmentsRemainLegalButTamperedOffsetsAndOversizedFramesFail() {
        val message = ByteArray(256) { it.toByte() }
        val assembler = NearGlowFrameAssembler(244)
        assertNull(assembler.accept(frame(message, 0, 12), 0))
        assertNull(assembler.accept(frame(message, 12, 145), 10))
        assertArrayEquals(message, assembler.accept(frame(message, 157, 99), 20))
        val reordered = NearGlowFrameAssembler(244)
        reordered.accept(frame(message, 0, 12), 0)
        assertThrows(IllegalArgumentException::class.java) { reordered.accept(frame(message, 13, 145), 1) }
        assertThrows(IllegalArgumentException::class.java) { NearGlowFrameAssembler().accept(frame(message, 0, 145), 0) }
        assertThrows(IllegalArgumentException::class.java) { NearGlowFrameAssembler(244).accept(frame(message, 0, 237), 0) }
        assertThrows(IllegalArgumentException::class.java) { NearGlowFrameAssembler(244).accept(frame(message, 0, 12).also { it[4] = 1; it[5] = 1 }, 0) }
    }

    @Test fun zeroPayloadOversizedMessagesInvalidTransferAndBudgetsAreRejected() {
        for (budget in listOf(0, 8, 245, 517)) {
            assertThrows(IllegalArgumentException::class.java) { NearGlowFrames.encode(ByteArray(1), 1, budget) }
            assertThrows(IllegalArgumentException::class.java) { NearGlowFrameAssembler(budget) }
        }
        assertThrows(IllegalArgumentException::class.java) { NearGlowFrames.encode(ByteArray(257), 1, 244) }
        assertThrows(IllegalArgumentException::class.java) { NearGlowFrames.encode(ByteArray(0), 1, 244) }
        assertThrows(IllegalArgumentException::class.java) { NearGlowFrames.encode(ByteArray(1), 0, 244) }
    }
}
