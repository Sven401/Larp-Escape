package de.larpescape.protocol

import java.nio.ByteBuffer
import java.nio.ByteOrder

/** Host-only fixture fields. No value in this type is an authorization result. */
class AppMutualDevTranscriptV1(
    val chip: ByteArray, val deviceId: ByteArray, val bootId: Long,
    val sourceRegistration: Long, val sdkGeneration: Long, val gattInterface: Int,
    val peerAddress: ByteArray, val connectionToken: Long, val connectionHandle: Int,
    val phoneInstallId: ByteArray, val boxKeyId: ByteArray, val boxPublic: ByteArray,
    val phoneKeyId: ByteArray, val phonePublic: ByteArray,
    val boxNonce: ByteArray, val phoneNonce: ByteArray, val attempt: Long,
    val boxEnrollmentGeneration: Long, val phoneEnrollmentGeneration: Long,
    val boxRevocationGeneration: Long, val phoneRevocationGeneration: Long,
)

/** Exact counterpart to AppMutualDevTranscriptV1.cpp, not APP_MUTUAL_V1. */
object AppMutualDevTranscriptCodecV1 {
    const val FRAME_SIZE = 289
    private val HEADER = byteArrayOf(0x41, 0x4d, 0x44, 0x31, 1, 0xff.toByte(), 0x42, 0x50)
    private fun populated(bytes: ByteArray, length: Int) = bytes.size == length && bytes.any { it != 0.toByte() }
    private fun publicShape(bytes: ByteArray) = bytes.size == 65 && bytes[0] == 4.toByte() &&
        bytes.drop(1).any { it != 0.toByte() }
    private fun u32(value: Long) = value in 0..0xffff_ffffL

    private fun valid(v: AppMutualDevTranscriptV1): Boolean =
        populated(v.chip, 6) && populated(v.deviceId, 16) && v.bootId != 0L &&
        u32(v.sourceRegistration) && v.sourceRegistration != 0L &&
        u32(v.sdkGeneration) && v.sdkGeneration != 0L &&
        v.gattInterface in 0..254 && populated(v.peerAddress, 6) &&
        u32(v.connectionToken) && v.connectionToken != 0L &&
        v.connectionHandle in 1..65535 && populated(v.phoneInstallId, 16) &&
        populated(v.boxKeyId, 16) && populated(v.phoneKeyId, 16) &&
        !v.boxKeyId.contentEquals(v.phoneKeyId) &&
        publicShape(v.boxPublic) && publicShape(v.phonePublic) &&
        populated(v.boxNonce, 16) && populated(v.phoneNonce, 16) &&
        !v.boxNonce.contentEquals(v.phoneNonce) && u32(v.attempt) && v.attempt != 0L &&
        u32(v.boxEnrollmentGeneration) && v.boxEnrollmentGeneration != 0L &&
        u32(v.phoneEnrollmentGeneration) && v.phoneEnrollmentGeneration != 0L &&
        u32(v.boxRevocationGeneration) && u32(v.phoneRevocationGeneration)

    fun encode(v: AppMutualDevTranscriptV1): ByteArray {
        require(valid(v)) { "Invalid development transcript fixture" }
        val out = ByteBuffer.allocate(FRAME_SIZE).order(ByteOrder.BIG_ENDIAN)
        out.put(HEADER); out.put(v.chip); out.put(v.deviceId); out.putLong(v.bootId)
        out.putInt(v.sourceRegistration.toInt()); out.putInt(v.sdkGeneration.toInt())
        out.put(v.gattInterface.toByte()); out.put(v.peerAddress)
        out.putInt(v.connectionToken.toInt()); out.putShort(v.connectionHandle.toShort())
        out.put(v.phoneInstallId); out.put(v.boxKeyId); out.put(v.boxPublic)
        out.put(v.phoneKeyId); out.put(v.phonePublic); out.put(v.boxNonce); out.put(v.phoneNonce)
        out.putInt(v.attempt.toInt()); out.putInt(v.boxEnrollmentGeneration.toInt())
        out.putInt(v.phoneEnrollmentGeneration.toInt()); out.putInt(v.boxRevocationGeneration.toInt())
        out.putInt(v.phoneRevocationGeneration.toInt())
        check(out.position() == FRAME_SIZE)
        return out.array()
    }

    fun decode(bytes: ByteArray): AppMutualDevTranscriptV1 {
        require(bytes.size == FRAME_SIZE && bytes.copyOfRange(0, 8).contentEquals(HEADER)) {
            "Invalid development transcript header or length"
        }
        val input = ByteBuffer.wrap(bytes).order(ByteOrder.BIG_ENDIAN)
        input.position(8)
        fun array(size: Int) = ByteArray(size).also(input::get)
        fun unsigned32() = input.int.toLong() and 0xffff_ffffL
        val v = AppMutualDevTranscriptV1(
            array(6), array(16), input.long, unsigned32(), unsigned32(),
            input.get().toInt() and 255, array(6), unsigned32(), input.short.toInt() and 65535,
            array(16), array(16), array(65), array(16), array(65), array(16), array(16),
            unsigned32(), unsigned32(), unsigned32(), unsigned32(), unsigned32())
        require(input.position() == FRAME_SIZE && valid(v)) { "Invalid development transcript fields" }
        return v
    }
}

enum class AppMutualDevFakeStateV1 { Empty, AwaitFrame, TranscriptCheckedClosed, Closed }

/** One-shot byte fixture comparison; success remains closed, without cryptographic proof. */
class AppMutualDevClosedFakeV1 {
    var state = AppMutualDevFakeStateV1.Empty
        private set
    private var expected: ByteArray? = null

    fun begin(local: AppMutualDevTranscriptV1): Boolean {
        if (state != AppMutualDevFakeStateV1.Empty) return false
        expected = runCatching { AppMutualDevTranscriptCodecV1.encode(local) }.getOrNull()
        state = if (expected == null) AppMutualDevFakeStateV1.Closed else AppMutualDevFakeStateV1.AwaitFrame
        return state == AppMutualDevFakeStateV1.AwaitFrame
    }
    fun check(received: ByteArray): Boolean {
        if (state != AppMutualDevFakeStateV1.AwaitFrame) return false
        val syntax = runCatching { AppMutualDevTranscriptCodecV1.decode(received) }.isSuccess
        if (!syntax || !received.contentEquals(expected)) {
            state = AppMutualDevFakeStateV1.Closed
            return false
        }
        state = AppMutualDevFakeStateV1.TranscriptCheckedClosed
        return true
    }
    fun close() { if (state == AppMutualDevFakeStateV1.AwaitFrame) state = AppMutualDevFakeStateV1.Closed }
}
