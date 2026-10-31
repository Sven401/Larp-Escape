package de.larpescape.companion.data.device

data class NearGlowPoint(val rssi: Int, val percent: Int)

data class NearGlowCurve(val points: List<NearGlowPoint>) {
    init {
        require(points.size == 4)
        require(points.all { it.rssi in -127..-1 && it.percent in 0..100 })
        require(points.zipWithNext().all { (a, b) -> a.rssi < b.rssi && a.percent <= b.percent })
    }
    companion object {
        val DEFAULT = NearGlowCurve(listOf(NearGlowPoint(-90, 0), NearGlowPoint(-75, 30),
            NearGlowPoint(-60, 60), NearGlowPoint(-40, 100)))
    }
}

data class NearGlowPointInput(val rssi: String, val percent: String)
data class NearGlowDraft(val points: List<NearGlowPointInput> = NearGlowCurve.DEFAULT.points.map {
    NearGlowPointInput(it.rssi.toString(), it.percent.toString())
}) {
    val curve: NearGlowCurve? get() = try {
        NearGlowCurve(points.map { NearGlowPoint(it.rssi.toInt(), it.percent.toInt()) })
    } catch (_: IllegalArgumentException) { null }
    val error: String? get() = when {
        points.size != 4 -> "Vier Stützpunkte sind erforderlich."
        points.any { it.rssi.toIntOrNull()?.let { value -> value in -127..-1 } != true } -> "Signalstärke muss eine ganze Zahl zwischen −127 und −1 dBm sein."
        points.any { it.percent.toIntOrNull()?.let { value -> value in 0..100 } != true } -> "Helligkeit muss zwischen 0 und 100 Prozent liegen."
        points.zipWithNext().any { (a, b) -> a.rssi.toInt() >= b.rssi.toInt() } -> "Signalstärken müssen von schwach nach stark steigen."
        points.zipWithNext().any { (a, b) -> a.percent.toInt() > b.percent.toInt() } -> "Helligkeit darf mit stärkerem Signal nicht sinken."
        else -> null
    }
}

enum class NearGlowChange { DRAFT, SENT, CONFIRMED, UNCLEAR, REJECTED }

fun nearGlowChangeOutcome(draft: NearGlowCurve?, reported: NearGlowCurve, accepted: Boolean,
    setResult: Boolean, previous: NearGlowChange, revision: Long, code: Int, durable: Boolean = true): Pair<NearGlowChange, String> = when {
    !durable || code == 5 -> NearGlowChange.UNCLEAR to "Ausgang unklar · Gerätestand erneut lesen"
    code == 9 -> NearGlowChange.REJECTED to "Gespeichert, Anwendung fehlgeschlagen · Gerätestatus prüfen"
    accepted && draft == reported -> NearGlowChange.CONFIRMED to "Dauerhaft bestätigt · Revision $revision"
    accepted -> NearGlowChange.DRAFT to "Vorherige Kennlinie bestätigt · neuer Entwurf noch nicht übernommen"
    setResult -> NearGlowChange.REJECTED to "Nicht übernommen · Geräteantwort $code; aktuellen Stand prüfen"
    previous == NearGlowChange.UNCLEAR -> NearGlowChange.UNCLEAR to "Gerätestand gelesen · vorheriger Ausgang nicht zugeordnet"
    else -> NearGlowChange.DRAFT to "Gespeicherten Gerätestand gelesen · Revision $revision"
}

/** Handset age and the station's partner-source age are independent. */
data class NearGlowReading(
    val receivedAt: Long,
    val peerFresh: Boolean,
    val sourceAgeMs: Long?,
    val rssi: Int?,
    val level: Int,
    val rgb: List<Int>,
    val armed: Boolean,
    val fault: Int,
    val outputKnown: Boolean = true,
    val outputPending: Boolean = false,
    val configFault: Int = 0,
    val configurationRevision: Long? = null,
    val outputRevision: Long? = null,
) {
    fun handsetFresh(now: Long) = now >= receivedAt && now - receivedAt < 2500
    fun sourceFresh(now: Long) = handsetFresh(now) && peerFresh && sourceAgeMs != null &&
        sourceAgeMs + (now - receivedAt) < 3000
}

data class NearGlowStationUi(
    val label: String,
    val mac: String? = null,
    val peer: String? = null,
    val connected: Boolean = false,
    val authenticated: Boolean = false,
    val message: String = "Stationszugang importieren",
    val reading: NearGlowReading? = null,
    val revision: Long? = null,
    val confirmed: NearGlowCurve? = null,
    val configMatchesTelemetry: Boolean = false,
    val busy: Boolean = false,
    val change: NearGlowChange = NearGlowChange.DRAFT,
    val changeMessage: String = "Entwurf · noch nicht gesendet",
) {
    fun disconnected() = copy(connected = false, authenticated = false, reading = null,
        revision = null, confirmed = null, configMatchesTelemetry = false, busy = false,
        message = "Handy getrennt · Station arbeitet autonom weiter",
        change = if (change == NearGlowChange.SENT) NearGlowChange.UNCLEAR else change,
        changeMessage = if (change == NearGlowChange.SENT) "Ausgang unklar · Gerätestand erneut lesen" else changeMessage)
}

data class NearGlowUiState(
    val stations: List<NearGlowStationUi> = listOf(NearGlowStationUi("Station A"), NearGlowStationUi("Station B")),
    val draft: NearGlowDraft = NearGlowDraft(),
    val now: Long = 0,
    val scanning: Boolean = false,
    val permissionMissing: Boolean = false,
    val message: String = "Zwei autonome Stationen beobachten",
) {
    val canApply: Boolean get() = draft.curve != null && stations.all {
        it.authenticated && it.revision != null && it.revision in 1 until 0xFFFFFFFFL && it.confirmed != null && it.configMatchesTelemetry && !it.busy &&
            it.reading?.let { reading -> now - reading.receivedAt in 0..1000 && reading.configFault == 0 } == true && it.change != NearGlowChange.SENT
    }
}
