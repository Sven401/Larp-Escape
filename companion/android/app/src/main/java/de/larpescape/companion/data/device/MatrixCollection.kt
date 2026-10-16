package de.larpescape.companion.data.device

import java.io.StringReader
import java.io.StringWriter
import java.util.Properties
import java.util.UUID

enum class MatrixRunPhase { IDLE, RECORDING, PAUSED, REVIEW, CONFIRMED }
data class MatrixHit(val count: Int, val firstAt: Long, val lastAt: Long)

/** Saved observations are independent of the short-lived live display. No physical position is inferred. */
data class MatrixCollection(
    val device: String = "",
    val run: String = "",
    val phase: MatrixRunPhase = MatrixRunPhase.IDLE,
    val startedAt: Long = 0,
    val endedAt: Long = 0,
    val hits: Map<Int, MatrixHit> = emptyMap(),
    val disabled: Set<Int> = emptySet(),
    val interruptions: Int = 0,
    val rejectedSamples: Int = 0,
    val notice: String = "Bereit für einen neuen Sammeldurchgang",
    val problems: Set<Int> = emptySet(),
) {
    val unseen: Set<Int> get() = (0..80).toSet() - hits.keys
    val collecting: Boolean get() = phase == MatrixRunPhase.RECORDING
    val coordinates: String get() = hits.keys.sorted().joinToString(", ", transform = ::matrixCoordinate)
}

interface MatrixCollectionStore {
    fun load(device: String): MatrixCollection?
    /** Must durably save before returning; previous runs must remain available. */
    fun save(value: MatrixCollection)
}

/** Main-thread owner. Receives every accepted authenticated status, never conflated UI snapshots. */
class MatrixCollectionRecorder(private val store: MatrixCollectionStore) {
    var state = MatrixCollection()
        private set
    private var previous: Set<Int>? = null
    private var storageFailed = false

    fun selectDevice(device: String) {
        if (device == state.device) return
        require(device.matches(Regex("[0-9a-f]{32}")))
        pause()
        try {
            state = store.load(device) ?: MatrixCollection(device = device)
            storageFailed = false
            if (state.collecting) {
                save(state.copy(phase = MatrixRunPhase.PAUSED, interruptions = state.interruptions + 1,
                    notice = "App neu geöffnet · Treffer erhalten. In Ruhe fortsetzen."))
            }
        } catch (_: Exception) {
            state = MatrixCollection(device = device, notice = "Gespeicherter Lauf nicht lesbar · keinen neuen Lauf starten")
            storageFailed = true
        }
        previous = null
    }

    private fun save(next: MatrixCollection): Boolean {
        return try {
            store.save(next)
            state = next
            storageFailed = false
            true
        } catch (_: Exception) {
            state = state.copy(phase = if (state.collecting) MatrixRunPhase.PAUSED else state.phase,
                notice = "Speichern fehlgeschlagen · Sammlung angehalten. Letzte gesicherte Treffer erhalten.")
            storageFailed = true
            previous = null
            false
        }
    }

    fun start(live: MatrixUiState, now: Long): Boolean {
        if (storageFailed || state.device.isEmpty() || state.phase !in setOf(MatrixRunPhase.IDLE, MatrixRunPhase.REVIEW, MatrixRunPhase.CONFIRMED) || !atRest(live)) return false
        val next = MatrixCollection(device = state.device, run = UUID.randomUUID().toString(),
            phase = MatrixRunPhase.RECORDING, startedAt = now, disabled = state.disabled, problems = state.problems,
            notice = "Sammlung läuft · jeden Kontakt mindestens 1 Sekunde halten und wieder lösen")
        if (!save(next)) return false
        previous = emptySet()
        return true
    }

    fun resume(live: MatrixUiState): Boolean {
        if (state.phase != MatrixRunPhase.PAUSED || !atRest(live)) return false
        if (!save(state.copy(phase = MatrixRunPhase.RECORDING, notice = "Sammlung läuft · Treffer bleiben erhalten"))) return false
        previous = emptySet()
        return true
    }

    fun observe(live: MatrixUiState, now: Long) {
        if (!state.collecting) return
        if (!live.current || !live.supported || live.pressed == null) { pause(); return }
        val contacts = live.pressed
        if (live.ambiguous || contacts.size > 1 || contacts.any { it !in 0..80 }) {
            // Do not turn ghosting/multiple contacts into installed positions. Require a clean release.
            if (previous != null) save(state.copy(rejectedSamples = state.rejectedSamples + 1,
                notice = "Mehrfachkontakt übersprungen · lösen und einzeln wiederholen"))
            previous = null
            return
        }
        if (contacts.isEmpty()) { previous = emptySet(); return }
        if (previous?.isEmpty() == true) {
            val index = contacts.single()
            val hit = state.hits[index]
            if (!save(state.copy(hits = state.hits + (index to MatrixHit((hit?.count ?: 0) + 1,
                    hit?.firstAt ?: now, now)), notice = "Treffer gespeichert · ${matrixCoordinate(index)}"))) return
        }
        // A direct change of key without an observed release must not create a second hit.
        previous = contacts.toSet()
    }

    fun pause() {
        previous = null
        if (state.collecting) save(state.copy(phase = MatrixRunPhase.PAUSED,
            interruptions = state.interruptions + 1,
            notice = "Sammlung unterbrochen · Treffer erhalten. Verbindung prüfen, dann in Ruhe fortsetzen."))
    }

    fun finish(now: Long) {
        if (state.phase !in setOf(MatrixRunPhase.RECORDING, MatrixRunPhase.PAUSED) || storageFailed) return
        save(state.copy(phase = MatrixRunPhase.REVIEW, endedAt = now,
            notice = "Lauf gesichert · unbeobachtete Koordinaten sind zunächst offen"))
        previous = null
    }

    fun confirmUnpopulated(problems: Set<Int> = emptySet()) {
        if (state.phase != MatrixRunPhase.REVIEW || state.hits.isEmpty()) return
        require(state.unseen.containsAll(problems))
        save(state.copy(phase = MatrixRunPhase.CONFIRMED, disabled = state.unseen - problems, problems = problems.toSet(),
            notice = "Bestückung gespeichert · ${state.unseen.size - problems.size} unbestückt deaktiviert · ${problems.size} Problemstellen offen"))
    }

    fun reopen() {
        if (state.phase !in setOf(MatrixRunPhase.REVIEW, MatrixRunPhase.CONFIRMED)) return
        save(state.copy(phase = MatrixRunPhase.PAUSED, endedAt = 0,
            notice = "Nachprüfung vorbereitet · in Ruhe fortsetzen; bisherige Treffer bleiben erhalten"))
        previous = null
    }

    companion object {
        fun atRest(live: MatrixUiState) = live.supported && live.current && !live.ambiguous && live.pressed?.isEmpty() == true
    }
}

/** Bounded, versioned, editable evidence format without access credentials. Also used by the export. */
object MatrixCollectionCodec {
    fun encode(value: MatrixCollection): String {
        val p = Properties()
        p["schema"] = "1"; p["device"] = value.device; p["run"] = value.run
        p["phase"] = value.phase.name; p["startedAt"] = value.startedAt.toString(); p["endedAt"] = value.endedAt.toString()
        p["interruptions"] = value.interruptions.toString(); p["rejectedSamples"] = value.rejectedSamples.toString()
        p["notice"] = value.notice; p["disabled"] = value.disabled.sorted().joinToString(",")
        p["problems"] = value.problems.sorted().joinToString(",")
        value.hits.forEach { (index, hit) -> p["hit.$index"] = "${hit.count},${hit.firstAt},${hit.lastAt}" }
        return StringWriter().also { p.store(it, "Larp Escape matrix collection; indices 0..80; times UTC epoch milliseconds") }.toString()
    }

    fun decode(text: String): MatrixCollection {
        require(text.length <= 32768)
        val p = Properties().apply { load(StringReader(text)) }
        require(p.getProperty("schema") == "1")
        fun number(key: String) = p.getProperty(key).toLong().also { require(it >= 0) }
        fun counter(key: String) = number(key).also { require(it <= Int.MAX_VALUE) }.toInt()
        val device = p.getProperty("device").also { require(it.matches(Regex("[0-9a-f]{32}"))) }
        val run = p.getProperty("run").also { require(UUID.fromString(it).toString() == it) }
        val disabled = p.getProperty("disabled").takeIf { it.isNotEmpty() }?.split(",")?.map { it.toInt().also { n -> require(n in 0..80) } }?.toSet().orEmpty()
        val hits = p.stringPropertyNames().filter { it.startsWith("hit.") }.associate { key ->
            val index = key.removePrefix("hit.").toInt().also { require(it in 0..80) }
            val values = p.getProperty(key).split(",").map(String::toLong)
            require(values.size == 3 && values[0] in 1..Int.MAX_VALUE.toLong() && values[1] >= 0 && values[2] >= values[1])
            index to MatrixHit(values[0].toInt(), values[1], values[2])
        }
        val problems = p.getProperty("problems", "").takeIf { it.isNotEmpty() }?.split(",")
            ?.map { it.toInt().also { n -> require(n in 0..80 && n !in disabled) } }?.toSet().orEmpty()
        return MatrixCollection(device, run, MatrixRunPhase.valueOf(p.getProperty("phase")), number("startedAt"),
            number("endedAt"), hits, disabled, counter("interruptions"), counter("rejectedSamples"), p.getProperty("notice"), problems)
    }

    fun report(value: MatrixCollection): String = buildString {
        appendLine("Tastenmatrix · Sammeldurchgang ${value.run}")
        appendLine("Status: ${value.phase}; Beginn/Ende: ${value.startedAt}/${value.endedAt} (UTC Epoch ms)")
        appendLine("Unterbrechungen: ${value.interruptions}; übersprungene Mehrfachkontakte: ${value.rejectedSamples}")
        appendLine("Erkannt: ${value.coordinates}")
        appendLine("Unbeobachtet: ${value.unseen.sorted().joinToString(", ", transform = ::matrixCoordinate)}")
        appendLine("Deaktiviert (Bestückungsliste): ${value.disabled.sorted().joinToString(", ", transform = ::matrixCoordinate)}")
        appendLine("Vorhanden, aber gestört/offen: ${value.problems.sorted().joinToString(", ", transform = ::matrixCoordinate)}")
        appendLine("Während der Erfassung werden auch deaktivierte Koordinaten geprüft. Keine räumliche Zuordnung abgeleitet.")
        appendLine("Koordinate;Betätigungen;Erster Treffer UTC ms;Letzter Treffer UTC ms;Im Lauf erkannt;Deaktiviert;Problemstelle")
        (0..80).forEach { index ->
            val hit = value.hits[index]
            appendLine("${matrixCoordinate(index)};${hit?.count ?: 0};${hit?.firstAt ?: ""};${hit?.lastAt ?: ""};${hit != null};${index in value.disabled};${index in value.problems}")
        }
    }
}

/** Comma-separated user-confirmed electrical coordinates; invalid text must never mean an empty exception list. */
fun matrixProblemIndices(text: String): Set<Int>? {
    if (text.isBlank()) return emptySet()
    val coordinates = text.split(',').map { it.trim().uppercase() }
    if (coordinates.any { !it.matches(Regex("[A-I][1-9]")) }) return null
    return coordinates.map { (it[1] - '1') * 9 + (it[0] - 'A') }.toSet()
}
