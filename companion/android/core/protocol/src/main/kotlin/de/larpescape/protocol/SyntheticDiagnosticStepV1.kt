package de.larpescape.protocol

import com.google.gson.JsonObject

/** Explicit debug exchange. Metadata, device clock and every reply byte are fabricated.
 * Construct only from a journaled intent; no IO, authority issuer or production BLE route. */
class SyntheticDiagnosticStepV1(request: JsonObject, submittedAtMs: Long) {
    private val original = DiagnosticStepV1.fromJsonRequest(request, request["stepId"].asLong)
    private val origin = DiagnosticControlOriginV1(1, 1, 1, 1, 1, true)
    private val assembly = DiagnosticStepAssemblyV1(original, origin, submittedAtMs, submittedAtMs + original.timeoutMs)
    fun retire() = assembly.retire()

    /** The debug button produces bytes, exercises bounded reassembly and then bridges JSON. */
    fun respond(startedDeviceMs: Long, failed: Boolean, nowMs: Long): ByteArray {
        val body = body(startedDeviceMs, failed)
        for (offset in body.indices step 116) {
            val count = minOf(116, body.size - offset)
            val frame = ByteArray(12 + count).apply {
                this[0] = 'D'.code.toByte(); this[1] = 'G'.code.toByte(); this[2] = 1; this[3] = 0x84.toByte()
                DiagnosticStepV1.put(this, 4, original.message.toULong(), 4)
                DiagnosticStepV1.put(this, 8, body.size.toULong(), 2); DiagnosticStepV1.put(this, 10, offset.toULong(), 2)
                body.copyInto(this, 12, offset, offset + count)
            }
            val status = assembly.receive(frame, origin, origin, nowMs)
            check(status == DiagnosticStepAssemblyStatusV1.RECEIVING || status == DiagnosticStepAssemblyStatusV1.COMPLETE) { "Synthetische DG-Antwort verworfen: $status" }
        }
        return DiagnosticStepV1.toJson(checkNotNull(assembly.take(origin, nowMs)) { "Synthetische DG-Antwort nicht mehr gültig" })
    }
    private fun body(startedDeviceMs: Long, failed: Boolean): ByteArray {
        require(startedDeviceMs in 0..DiagnosticStepV1.MAX_TIME - 500)
        val led = original.operation == DiagnosticStepOperationV1.LED_SINGLE
        return ByteArray(if (led) 104 else 124).apply {
            DiagnosticStepV1.encode(original).copyInto(this)
            DiagnosticStepV1.put(this, 80, startedDeviceMs.toULong(), 8); DiagnosticStepV1.put(this, 88, (startedDeviceMs + 500).toULong(), 8)
            this[96] = if (failed) 1 else 0; this[97] = if (failed) 1 else 0; this[98] = 1; this[103] = 1
            if (led) { this[99] = if (failed) 0 else 1; this[100] = if (failed) 0 else 2 }
            else {
                this[102] = 2
                repeat(2) { i -> DiagnosticStepV1.put(this, 104 + 10 * i, (startedDeviceMs + 100 * (i + 1)).toULong(), 8); this[112 + 10 * i] = 1; this[113 + 10 * i] = if (i == 0) 3 else 0 }
            }
        }
    }
}
