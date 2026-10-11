package de.larpescape.protocol

/** Development-only ISO-DEP byte exchange. An echo is never authentication. */
object NfcCapabilityV1 {
    val SELECT = hex("00A4040008F04C4152504E464300")
    val UNAVAILABLE = hex("6A82")
    val NOT_ARMED = hex("6985")
    val MALFORMED = hex("6A80")
    private val CHALLENGE_PREFIX = hex("8010000010")
    private val RESPONSE_PREFIX = hex("4E4301")
    private val SUCCESS = hex("9000")

    fun challengeCommand(challenge: ByteArray): ByteArray {
        require(challenge.size == 16)
        return CHALLENGE_PREFIX + challenge
    }

    fun response(payload: ByteArray): ByteArray {
        require(payload.size == 16)
        return RESPONSE_PREFIX + payload + SUCCESS
    }

    fun isChallenge(command: ByteArray) = command.size == 21 &&
        command.copyOfRange(0, 5).contentEquals(CHALLENGE_PREFIX)

    fun bytesToHex(bytes: ByteArray) = bytes.joinToString("") { "%02X".format(it.toInt() and 0xff) }
    fun hex(value: String): ByteArray {
        require(value.length % 2 == 0)
        return ByteArray(value.length / 2) { value.substring(it * 2, it * 2 + 2).toInt(16).toByte() }
    }
}

data class NfcCapabilityEventV1(val kind: String, val elapsedMs: Long)
data class NfcCapabilitySnapshotV1(
    val runId: String?, val startedElapsedMs: Long?, val challenge: String?,
    val status: String, val events: List<NfcCapabilityEventV1>,
    val deviceCorrelation: String = "UNVERIFIED"
)

/** Volatile single-run gate. No persisted arm state, key, or privileged output. */
class NfcCapabilityGateV1(private val clockMs: () -> Long) {
    private var runId: ByteArray? = null
    private var started: Long? = null
    private var selected = false
    private var challenge: ByteArray? = null
    private var status = "NOT_TESTED"
    private val events = mutableListOf<NfcCapabilityEventV1>()
    private fun record(kind: String) {
        if (events.size < 12) events.add(NfcCapabilityEventV1(kind, clockMs()))
    }
    @Synchronized fun arm(id: ByteArray): Boolean {
        require(id.size == 16)
        if (active()) return false
        runId = id.copyOf(); started = clockMs(); selected = false; challenge = null
        events.clear(); status = "ARMED"; record("ARMED")
        return true
    }
    @Synchronized fun stop() {
        if (status == "ARMED" || status == "SELECTED") {
            status = "STOPPED"; record("STOPPED")
        }
        selected = false
    }
    @Synchronized fun deactivate() {
        if (active()) status = "DEACTIVATED"
        if (started != null && events.lastOrNull()?.kind != "DEACTIVATED") record("DEACTIVATED")
        selected = false
    }
    @Synchronized fun snapshot(): NfcCapabilitySnapshotV1 {
        active()
        return NfcCapabilitySnapshotV1(runId?.let(NfcCapabilityV1::bytesToHex), started,
            challenge?.let(NfcCapabilityV1::bytesToHex), status, events.toList())
    }
    private fun active(): Boolean {
        val since = started ?: return false
        if (status != "ARMED" && status != "SELECTED") return false
        if (clockMs() - since >= 30_000L || clockMs() < since) {
            status = "TIMEOUT"; selected = false; record("TIMEOUT"); return false
        }
        return true
    }
    @Synchronized fun process(command: ByteArray): ByteArray {
        val available = active()
        if (command.contentEquals(NfcCapabilityV1.SELECT)) {
            if (!available || selected) return NfcCapabilityV1.NOT_ARMED.copyOf()
            selected = true; status = "SELECTED"; record("SELECT")
            return NfcCapabilityV1.response(requireNotNull(runId))
        }
        if (command.size >= 2 && command[0] == 0x00.toByte() && command[1] == 0xA4.toByte())
            return (if (command.size == NfcCapabilityV1.SELECT.size) NfcCapabilityV1.UNAVAILABLE
                else NfcCapabilityV1.MALFORMED).copyOf()
        if (!NfcCapabilityV1.isChallenge(command)) return NfcCapabilityV1.MALFORMED.copyOf()
        if (!available || !selected) return NfcCapabilityV1.NOT_ARMED.copyOf()
        challenge = command.copyOfRange(5, 21)
        status = "LOCAL_ECHO_ONLY"; selected = false; record("CHALLENGE_ECHO")
        return NfcCapabilityV1.response(requireNotNull(challenge))
    }
}
