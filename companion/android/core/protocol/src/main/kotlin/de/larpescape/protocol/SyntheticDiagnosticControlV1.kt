package de.larpescape.protocol

/** Byte transport used only by the diagnostic preview. A write or reply is never device authority. */
fun interface SyntheticDiagnosticControlTransportV1 {
    fun exchange(request: ByteArray): ByteArray?
}

/** A deterministic, explicitly fabricated control peer for the isolated preview. */
object SyntheticDiagnosticControlPeerV1 {
    fun reply(request: ByteArray, syntheticGrantId: String): ByteArray {
        val parsed = DiagnosticControlV1.request(request)
        return ByteArray(DiagnosticControlV1.REPLY_BYTES).apply {
            request.copyInto(this)
            this[3] = (this[3].toInt() or 0x80).toByte()
            this[64] = 0; this[65] = 3; this[66] = 1
            this[68] = 0
            if (parsed.command == DiagnosticControlCommandV1.GRANT) {
                syntheticGrantId.replace("-", "").chunked(2).forEachIndexed { i, hex -> this[48 + i] = hex.toInt(16).toByte() }
                DiagnosticStepV1.put(this, 76, parsed.ttlMs.toULong(), 8)
            }
            this[84] = 0x60; this[85] = 0xea.toByte()
        }.also { DiagnosticControlV1.correlate(it, parsed) }
    }
}

/** Preview-only GRANT/CANCEL exchange; never survives a process or authorizes BLE. */
class SyntheticDiagnosticControlSessionV1(
    private val deviceId: String, private val bootId: String, private val runId: String,
    private val syntheticGrantId: String, private val transport: SyntheticDiagnosticControlTransportV1,
) {
    private val origin = DiagnosticControlOriginV1(1, 1, 1, 1, 1, true)
    private var active = false
    val synthetic: Boolean get() = true
    val hasCorrelatedGrant: Boolean get() = active

    fun start(nowMs: Long): Boolean {
        check(!active)
        val request = DiagnosticControlRequestV1(DiagnosticControlCommandV1.GRANT, 1, deviceId, bootId, runId, ttlMs = 60000, explicitConsent = true)
        val pending = PendingDiagnosticControlV1(request, origin, nowMs)
        val answer = transport.exchange(DiagnosticControlV1.encode(request)) ?: return false
        val result = pending.receive(answer, origin, origin, nowMs, nowMs)
        active = result?.outcome == DiagnosticControlOutcomeV1.ACCEPTED && result.grantId == syntheticGrantId
        return active
    }

    /** Local retirement is immediate, including when CANCEL has no answer. */
    fun end(nowMs: Long): Boolean {
        if (!active) return false
        active = false
        val request = DiagnosticControlRequestV1(DiagnosticControlCommandV1.CANCEL, 2, deviceId, bootId, runId, syntheticGrantId)
        val pending = PendingDiagnosticControlV1(request, origin, nowMs)
        val answer = transport.exchange(DiagnosticControlV1.encode(request)) ?: return false
        return pending.receive(answer, origin, origin, nowMs, nowMs)?.outcome == DiagnosticControlOutcomeV1.ACCEPTED
    }
}
