package de.larpescape.protocol

/** Deliberately narrow, disposable fixture for the NFC assembly test. */
object NfcTestFixture {
    val initial get() = NfcTagData(20261015, 1, 0, listOf(NfcNodeVisit(1, 1, 0)))
    val changed get() = initial.copy(flags = 2)
    private val mime = NfcTagDataCodec.MIME.toByteArray(Charsets.US_ASCII)
    fun message(data: NfcTagData): ByteArray {
        val payload = NfcTagDataCodec.encode(data)
        require(payload.size < 256)
        return byteArrayOf(0xd2.toByte(), mime.size.toByte(), payload.size.toByte()) + mime + payload
    }
    fun readMessage(bytes: ByteArray): NfcTagData {
        require(bytes.size >= 3 && bytes[0].toInt() and 255 == 0xd2)
        val typeLength = bytes[1].toInt() and 255
        val payloadLength = bytes[2].toInt() and 255
        require(typeLength == mime.size && bytes.size == 3 + typeLength + payloadLength)
        require(bytes.copyOfRange(3, 3 + typeLength).contentEquals(mime))
        return NfcTagDataCodec.decode(bytes.copyOfRange(3 + typeLength, bytes.size))
    }
}

data class NfcTestState(
    val active: Boolean = false,
    val reads: Int = 0,
    val updates: Int = 0,
    val rejected: Int = 0,
    val commits: Int = 0,
    val readBackComplete: Boolean = false,
    val data: NfcTagData = NfcTestFixture.initial,
    val note: String = "Testtag vorbereiten und bewusst aktivieren",
) {
    val boxChangeReceived get() = commits > 0 && data == NfcTestFixture.changed
    val verified get() = boxChangeReceived && readBackComplete
}

/**
 * NFC Forum Type 4 test tag: one short MIME record, no persistent arming or arbitrary storage.
 * The caller ends the session on foreground loss. All methods own their byte arrays.
 * NLEN=0 begins a bounded transaction; only a complete, validated fixture commits.
 */
class NfcTestSession(private val now: () -> Long) {
    companion object {
        const val CAPACITY = 512
        const val SESSION_MILLIS = 180_000L
        private val AID = byteArrayOf(0xd2.toByte(), 0x76, 0, 0, 0x85.toByte(), 1, 1)
        // CCLEN 15, mapping 2.0, MLe 256, MLc 255, NDEF file E104, 514 bytes incl. NLEN.
        private val CC = byteArrayOf(0, 15, 0x20, 1, 0, 0, 0xff.toByte(), 4, 6, 0xe1.toByte(), 4, 2, 2, 0, 0)
    }
    private var value = NfcTestState()
    private var deadline = 0L
    private var appSelected = false
    private var file = 0
    private var committed = NfcTestFixture.message(NfcTestFixture.initial)
    private var staged: ByteArray? = null
    private var coverage = BooleanArray(CAPACITY)
    private var readCoverage = BooleanArray(committed.size)

    @Synchronized fun state(): NfcTestState { expire(); return value }
    @Synchronized fun start() {
        committed = NfcTestFixture.message(NfcTestFixture.initial)
        resetLink()
        readCoverage = BooleanArray(committed.size)
        deadline = now() + SESSION_MILLIS
        value = NfcTestState(active = true, note = "Testtag aktiv · 3 Minuten · Handy auf den Boxreader legen")
    }
    @Synchronized fun stop(reason: String = "Testtag deaktiviert") {
        resetLink()
        value = value.copy(active = false, note = reason)
    }
    @Synchronized fun linkLost() {
        val incomplete = staged != null
        resetLink()
        if (value.active) value = value.copy(note = if (incomplete)
            "Funkkontakt beendet · unvollständige Änderung verworfen" else "Funkkontakt beendet · Messwerte bleiben sichtbar")
    }
    private fun resetLink() { appSelected = false; file = 0; staged = null; coverage.fill(false) }
    private fun expire() {
        if (value.active && now() >= deadline) stop("Testzeit abgelaufen · erneut bewusst aktivieren")
    }
    private fun status(a: Int, b: Int) = byteArrayOf(a.toByte(), b.toByte())
    private fun ok(bytes: ByteArray = byteArrayOf()) = bytes + status(0x90, 0)
    private fun reject(a: Int, b: Int): ByteArray {
        // A malformed update may never be followed by a successful partial commit.
        staged = null; coverage.fill(false)
        value = value.copy(rejected = value.rejected + 1, note = "Kommando abgewiesen · bestätigte Daten unverändert")
        return status(a, b)
    }
    @Synchronized fun process(command: ByteArray): ByteArray {
        expire()
        if (!value.active) return status(0x69, 0x85)
        if (command.size > 260) return reject(0x67, 0)
        val apdu = command.copyOf()
        if (apdu.size < 4) return reject(0x67, 0)
        fun byte(i: Int) = apdu[i].toInt() and 255
        if (byte(0) != 0) return reject(0x6e, 0)
        return when (byte(1)) {
            0xa4 -> {
                if (apdu.size < 5) return reject(0x67, 0)
                val length = byte(4)
                if (length == 0 || (apdu.size != 5 + length && apdu.size != 6 + length)) return reject(0x67, 0)
                val data = apdu.copyOfRange(5, 5 + length)
                when {
                    byte(2) == 4 && byte(3) == 0 && data.contentEquals(AID) -> {
                        resetLink(); appSelected = true; ok()
                    }
                    byte(2) == 0 && byte(3) in listOf(0, 12) && length == 2 && appSelected -> {
                        val id = ((data[0].toInt() and 255) shl 8) or (data[1].toInt() and 255)
                        if (id != 0xe103 && id != 0xe104) reject(0x6a, 0x82)
                        else { file = id; ok() }
                    }
                    else -> reject(0x6a, 0x82)
                }
            }
            0xb0 -> {
                if (!appSelected || file == 0) return reject(0x69, 0x86)
                if (apdu.size != 5 || byte(2) and 0x80 != 0) return reject(0x67, 0)
                val offset = (byte(2) shl 8) or byte(3)
                val length = byte(4).let { if (it == 0) 256 else it }
                val bytes = if (file == 0xe103) CC else {
                    val current = if (staged == null) committed else byteArrayOf()
                    byteArrayOf((current.size shr 8).toByte(), current.size.toByte()) + current
                }
                if (offset > bytes.size || length > bytes.size - offset) return reject(0x6b, 0)
                if (file == 0xe104) {
                    if (value.commits > 0 && staged == null) {
                        for (i in maxOf(2, offset) until offset + length) readCoverage[i - 2] = true
                    }
                    value = value.copy(reads = value.reads + 1,
                        readBackComplete = value.commits > 0 && readCoverage.all { it },
                        note = "Lesekommando vom Reader empfangen")
                }
                ok(bytes.copyOfRange(offset, offset + length))
            }
            0xd6 -> {
                if (!appSelected || file != 0xe104) return reject(0x69, 0x86)
                if (apdu.size < 5 || byte(2) and 0x80 != 0) return reject(0x67, 0)
                val length = byte(4)
                if (length == 0 || apdu.size != 5 + length) return reject(0x67, 0)
                val offset = (byte(2) shl 8) or byte(3)
                if (offset > CAPACITY + 2 || length > CAPACITY + 2 - offset) return reject(0x6b, 0)
                if (offset == 0 && length == 2) {
                    val nlen = (byte(5) shl 8) or byte(6)
                    if (nlen == 0) {
                        staged = ByteArray(CAPACITY); coverage.fill(false)
                    } else {
                        val pending = staged ?: return reject(0x69, 0x85)
                        if (nlen > CAPACITY || (0 until nlen).any { !coverage[it] } ||
                            (nlen until CAPACITY).any { coverage[it] }) return reject(0x69, 0x85)
                        val candidate = pending.copyOf(nlen)
                        val data = try { NfcTestFixture.readMessage(candidate) } catch (_: IllegalArgumentException) { return reject(0x6a, 0x80) }
                        if (data != NfcTestFixture.changed || !candidate.contentEquals(NfcTestFixture.message(data))) return reject(0x6a, 0x80)
                        committed = candidate; staged = null; coverage.fill(false)
                        readCoverage = BooleanArray(committed.size)
                        value = value.copy(commits = value.commits + 1, data = data, readBackComplete = false)
                    }
                } else {
                    if (offset < 2) return reject(0x6b, 0)
                    val pending = staged ?: return reject(0x69, 0x85)
                    apdu.copyInto(pending, offset - 2, 5, apdu.size)
                    for (i in offset - 2 until offset - 2 + length) coverage[i] = true
                }
                value = value.copy(updates = value.updates + 1, note = if (staged == null)
                    "Boxänderung empfangen und geprüft · Rücklesen durch Reader steht aus" else "Schreibkommando empfangen · Änderung noch unvollständig")
                ok()
            }
            else -> reject(0x6d, 0)
        }
    }
}
