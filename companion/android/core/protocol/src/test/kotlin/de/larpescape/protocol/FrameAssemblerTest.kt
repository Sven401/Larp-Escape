package de.larpescape.protocol

import org.junit.Assert.*
import org.junit.Test

class FrameAssemblerTest {
    @Test fun reassemblesExistingFirmwareGoldenFixtures() {
        val fixtures = java.io.File(System.getProperty("firmwareFixtures"))
        for (name in listOf("status-ready.json", "status-running.json", "reply-executed.json", "reply-conflict.json")) {
            val bytes = java.io.File(fixtures, name).readBytes()
            val kind = if (name.startsWith("status")) 4 else 2
            val assembler = FrameAssembler()
            var complete: Pair<Int, ByteArray>? = null
            for (offset in bytes.indices step 12) {
                val header = byteArrayOf(1, kind.toByte(), 1, 0, bytes.size.toByte(), (bytes.size shr 8).toByte(), offset.toByte(), (offset shr 8).toByte())
                complete = assembler.accept(header + bytes.copyOfRange(offset, minOf(offset + 12, bytes.size)), offset.toLong())
            }
            assertEquals(kind, complete!!.first)
            assertArrayEquals(name, bytes, complete.second)
        }
    }
    private fun frame(offset: Int, data: ByteArray, total: Int = 4, kind: Int = 4, id: Int = 1) =
        byteArrayOf(1, kind.toByte(), id.toByte(), 0, total.toByte(), (total shr 8).toByte(), offset.toByte(), 0) + data
    @Test fun assemblesCopiedFragments() {
        val assembler = FrameAssembler()
        val first = frame(0, byteArrayOf(10, 20))
        assertNull(assembler.accept(first, 0))
        first[8] = 99
        assertArrayEquals(byteArrayOf(10, 20, 30, 40), assembler.accept(frame(2, byteArrayOf(30, 40)), 100)!!.second)
    }
    @Test fun subscriptionMayStartMidStatus() { assertNull(FrameAssembler().accept(frame(2, byteArrayOf(30, 40)), 0)) }
    @Test fun rejectsOffsetConflict() {
        val assembler = FrameAssembler(); assembler.accept(frame(0, byteArrayOf(1)), 0)
        assertThrows(IllegalArgumentException::class.java) { assembler.accept(frame(2, byteArrayOf(2)), 1) }
    }
    @Test fun enforcesAbsoluteDeadline() {
        val assembler = FrameAssembler(); assembler.accept(frame(0, byteArrayOf(1)), 0)
        assertThrows(IllegalStateException::class.java) { assembler.accept(frame(1, byteArrayOf(2)), 30000) }
    }
    @Test fun rejectsOversizeAndWrongType() {
        assertThrows(IllegalArgumentException::class.java) { FrameAssembler().accept(frame(0, byteArrayOf(1), 6144), 0) }
        assertThrows(IllegalArgumentException::class.java) { FrameAssembler().accept(frame(0, byteArrayOf(1), kind = 1), 0) }
    }
    @Test fun reconnectClearsPartialMessage() {
        val assembler = FrameAssembler(); assembler.accept(frame(0, byteArrayOf(1)), 0); assembler.clear()
        assertArrayEquals(byteArrayOf(2, 3, 4, 5), assembler.accept(frame(0, byteArrayOf(2, 3, 4, 5), id = 2), 1)!!.second)
    }
}
