package de.larpescape.protocol

/** Client transaction tracking only. Construct exclusively inside an already authorized original
 * peer connection. Neither a generation number nor this class authenticates a bearer or grants a role.
 * Use one instance for the whole connection: control request numbers never restart per transfer.
 */
class ProfileExchange(private val generation: Long, private val clock: () -> Long) {
    enum class Phase { IDLE, SENDING, SOURCE_SENT, FINISHING, DRAFT_READY, APPLYING, QUEUED, SAVED, ABORTING, ABORTED, REJECTED, UNKNOWN, CLOSED }
    class Write internal constructor(val id: Long, bytes: ByteArray) {
        private val value = bytes.copyOf()
        fun bytes() = value.copyOf()
    }
    var phase = Phase.IDLE; private set
    var response: ProfileResponse? = null; private set
    private var source: ProfileSourceValues? = null
    private var index = 0
    private var transfer = 0
    private var expectedDraft = 0L
    private var expectedConfiguration = 0L
    private var acceptedDraft = 0L
    private var started = 0L
    private var waitingSince = 0L
    private var nextWrite = 1L
    private var nextRequest = 1L
    private var commandRequest = 0L
    private var operation: ProfileOperation? = null
    private var pending: Write? = null
    private val usedTransfers = mutableSetOf<Int>()

    init { require(generation > 0) }

    fun begin(source: ByteArray, transfer: Int, draftRevision: Long, configurationRevision: Long, valueBytes: Int = 20) {
        check(phase in setOf(Phase.IDLE, Phase.SAVED, Phase.ABORTED, Phase.REJECTED))
        // Transfer IDs cannot distinguish a delayed old response if reused on this connection.
        require(transfer !in usedTransfers)
        val values = ProfileSourceValues(source, transfer, draftRevision, configurationRevision, valueBytes)
        require(configurationRevision < 0xffff_ffffL) { "Konfigurationszaehler erschoepft." }
        this.source = values; this.transfer = transfer; index = 0
        expectedDraft = draftRevision; expectedConfiguration = configurationRevision; acceptedDraft = 0L
        usedTransfers.add(transfer); response = null; operation = null; started = clock(); phase = Phase.SENDING
    }

    /** Polls a single immutable pending write. Transport must never automatically resend a value. */
    fun nextValue(): Write? {
        tick()
        pending?.let { return it }
        if (phase != Phase.SENDING) return null
        val values = checkNotNull(source)
        return makeWrite(values.value(index))
    }

    private fun makeWrite(bytes: ByteArray): Write {
        check(nextWrite < Long.MAX_VALUE)
        return Write(nextWrite++, bytes).also { pending = it; waitingSince = clock() }
    }

    /** ATT success means a copied value, never imported/active/saved. Duplicate/stale callbacks ignored. */
    fun copied(connection: Long, writeId: Long): Boolean {
        tick()
        if (connection != generation || pending?.id != writeId) return false
        pending = null
        if (phase == Phase.SENDING) {
            index++
            if (index == source?.size) { source = null; phase = Phase.SOURCE_SENT }
        }
        return true
    }

    fun finish(): Write {
        tick(); check(phase == Phase.SOURCE_SENT && pending == null)
        return control(ProfileOperation.FINISH, Phase.FINISHING)
    }

    /** Always an explicit caller action after the firmware has accepted the draft. */
    fun apply(): Write {
        tick(); check(phase == Phase.DRAFT_READY && pending == null)
        return control(ProfileOperation.APPLY, Phase.APPLYING)
    }

    fun abort(): Write {
        tick(); check(phase in setOf(Phase.SOURCE_SENT, Phase.DRAFT_READY) && pending == null)
        return control(ProfileOperation.ABORT, Phase.ABORTING)
    }

    private fun control(op: ProfileOperation, target: Phase): Write {
        check(nextRequest <= 0xffff_ffffL) { "Auftragszaehler erschoepft; neu verbinden." }
        commandRequest = nextRequest++; operation = op; phase = target
        return makeWrite(ProfileTransferV1.control(transfer, commandRequest, op))
    }

    /** Called only for bytes received on the original verified bearer. No bytes here prove authority. */
    fun receive(connection: Long, bytes: ByteArray): Boolean {
        tick()
        if (connection != generation || phase !in setOf(Phase.FINISHING, Phase.APPLYING, Phase.QUEUED, Phase.ABORTING)) return false
        val value = try { ProfileTransferV1.response(bytes) } catch (error: IllegalArgumentException) { unknown(); throw error }
        if (value.transfer != transfer || value.request != commandRequest || value.operation != operation) return false
        val next = when (value.code) {
            ProfileResponseCode.DRAFT_READY -> {
                if (value.draftRevision != expectedDraft + 1 || value.configurationRevision != expectedConfiguration) return invalid()
                acceptedDraft = value.draftRevision
                Phase.DRAFT_READY
            }
            ProfileResponseCode.APPLY_QUEUED -> {
                if (value.configurationRevision != expectedConfiguration || value.draftRevision != acceptedDraft) return invalid()
                Phase.QUEUED
            }
            ProfileResponseCode.SAVED -> {
                if (value.configurationRevision != expectedConfiguration + 1 || value.draftRevision != acceptedDraft + 1) return invalid()
                Phase.SAVED
            }
            ProfileResponseCode.ABORTED -> Phase.ABORTED
            ProfileResponseCode.UNAUTHORIZED, ProfileResponseCode.REQUEST_CONFLICT, ProfileResponseCode.REQUEST_EXPIRED,
            ProfileResponseCode.OUT_OF_ORDER, ProfileResponseCode.EXPIRED -> Phase.UNKNOWN
            else -> if (operation == ProfileOperation.APPLY) Phase.UNKNOWN else Phase.REJECTED
        }
        // Late duplicate queued replies cannot refresh the original operation deadline.
        pending = null; response = value; phase = next
        return true
    }

    private fun invalid(): Boolean { unknown(); return false }
    private fun unknown() { pending = null; source = null; response = null; phase = Phase.UNKNOWN }

    fun tick() {
        val now = clock()
        if (phase in setOf(Phase.SENDING, Phase.SOURCE_SENT, Phase.FINISHING) && (now < started || now - started >= 120000)) unknown()
        if ((pending != null || phase in setOf(Phase.FINISHING, Phase.APPLYING, Phase.QUEUED, Phase.ABORTING)) &&
            (now < waitingSince || now - waitingSince >= 10000)) unknown()
    }

    fun disconnected(connection: Long) {
        if (connection != generation) return
        val uncertain = phase in setOf(Phase.SENDING, Phase.SOURCE_SENT, Phase.FINISHING, Phase.DRAFT_READY, Phase.APPLYING, Phase.QUEUED, Phase.ABORTING, Phase.UNKNOWN)
        pending = null; source = null; response = null; phase = if (uncertain) Phase.UNKNOWN else Phase.CLOSED
    }
}
