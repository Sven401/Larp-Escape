package de.larpescape.protocol

import com.google.gson.*
import com.google.gson.stream.JsonReader
import com.google.gson.stream.JsonToken
import java.io.StringReader
import java.nio.ByteBuffer
import java.nio.charset.CodingErrorAction

enum class ApplicationPhase(val wire: String) {
    EMPTY("empty"), PENDING_ACTIVATION("pending_activation"), APPLYING("applying"),
    SAVING("saving"), READY("ready"), FAULT("fault")
}
data class ApplicationStatus(
    val phase: ApplicationPhase, val configurationRevision: Long, val profileRevision: Long,
    val draftRevision: Long, val deviceGeneration: Long, val storedGeneration: Long,
    val storedProfileRevision: Long, val hasActive: Boolean, val draftReady: Boolean,
    val hardwareReady: Boolean, val storedMatchesActive: Boolean
)
data class ControlStatus(val lease: Long, val nextRequest: Long)
data class GameStatus(
    val running: Boolean, val paused: Boolean, val entryPending: Boolean, val state: Long?,
    val lastTransition: Long, val stateRevision: Long, val stateElapsedMilliseconds: Long,
    val role: Long?, val crystalCount: Long, val requiredCrystals: Long, val seenRoleCount: Long,
    val roundCount: Long, val audioPending: Boolean, val incorrectCrystal: Boolean,
    val keystoneError: Boolean, val rejectedActions: Long, val unavailableActions: Long,
    val lastTickOutcome: String
)
data class ComponentStatus(
    val id: Long, val enabled: Boolean, val available: Boolean, val cardReadable: Boolean,
    val matrixReadable: Boolean, val pollPending: Boolean, val lastOutcome: String
)
data class ConfiguredStatus(
    val boot: String, val sample: Long, val sampledMilliseconds: Long,
    val application: ApplicationStatus, val control: ControlStatus, val game: GameStatus,
    val devices: List<ComponentStatus>
)

/** Exact v2 schema plus unique component IDs. No Android JSON coercions or authority inference. */
object ConfiguredStatusDecoder {
    private const val U32 = 4294967295L
    fun decode(bytes: ByteArray): ConfiguredStatus {
        require(bytes.size in 1..6143) { "Statuslänge ungültig" }
        val text = Charsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
            .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes)).toString()
        val root = JsonReader(StringReader(text)).use { reader ->
            val value = read(reader, 0)
            require(reader.peek() == JsonToken.END_DOCUMENT) { "Zusätzliche JSONdaten" }
            value
        }.obj("schemaVersion capabilities boot sample sampledMilliseconds application control game devices")
        require(root.uint("schemaVersion") == 2L) { "Statusversion ungültig" }
        val capabilities = root.get("capabilities")
        require(capabilities.isJsonArray && capabilities.asJsonArray.map { it.string(64) } ==
            listOf("status.configured.read", "game.control.core")) { "Statusfähigkeiten ungültig" }
        val boot = root.get("boot").string(16)
        require(boot.matches(Regex("[0-9A-F]{16}")) && boot != "0000000000000000") { "Bootkennung ungültig" }
        val a = root.get("application").obj("phase configurationRevision profileRevision draftRevision deviceGeneration storedGeneration storedProfileRevision hasActive draftReady hardwareReady storedMatchesActive")
        val phase = ApplicationPhase.entries.singleOrNull { it.wire == a.get("phase").string(32) }
            ?: error("Profilphase ungültig")
        val application = ApplicationStatus(phase, a.uint("configurationRevision"), a.uint("profileRevision"),
            a.uint("draftRevision"), a.uint("deviceGeneration"), a.uint("storedGeneration"),
            a.uint("storedProfileRevision"), a.bool("hasActive"), a.bool("draftReady"),
            a.bool("hardwareReady"), a.bool("storedMatchesActive"))
        val c = root.get("control").obj("lease nextRequest")
        val g = root.get("game").obj("running paused entryPending state lastTransition stateRevision stateElapsedMilliseconds role crystalCount requiredCrystals seenRoleCount roundCount audioPending incorrectCrystal keystoneError rejectedActions unavailableActions lastTickOutcome")
        val game = GameStatus(g.bool("running"), g.bool("paused"), g.bool("entryPending"), g.nullableId("state"),
            g.uint("lastTransition", 65535), g.uint("stateRevision"), g.uint("stateElapsedMilliseconds"),
            g.nullableId("role"), g.uint("crystalCount", 8), g.uint("requiredCrystals", 8),
            g.uint("seenRoleCount", 16), g.uint("roundCount"), g.bool("audioPending"),
            g.bool("incorrectCrystal"), g.bool("keystoneError"), g.uint("rejectedActions"),
            g.uint("unavailableActions"), g.get("lastTickOutcome").string(32))
        val array = root.get("devices")
        require(array.isJsonArray && array.asJsonArray.size() <= 16) { "Gerätezahl ungültig" }
        val devices = array.asJsonArray.map {
            val d = it.obj("id enabled available cardReadable matrixReadable pollPending lastOutcome")
            ComponentStatus(d.uint("id", 65535, 1), d.bool("enabled"), d.bool("available"),
                d.bool("cardReadable"), d.bool("matrixReadable"), d.bool("pollPending"), d.get("lastOutcome").string(16))
        }
        require(devices.map { it.id }.distinct().size == devices.size) { "Doppelte Gerätekennung" }
        return ConfiguredStatus(boot, root.uint("sample", U32, 1), root.uint("sampledMilliseconds"),
            application, ControlStatus(c.uint("lease"), c.uint("nextRequest")), game,
            java.util.Collections.unmodifiableList(devices))
    }

    // Strict streaming syntax, bounded depth and duplicate-name rejection before building the tree.
    private fun read(r: JsonReader, depth: Int): JsonElement {
        require(depth <= 8) { "JSON zu tief" }
        return when (r.peek()) {
            JsonToken.BEGIN_OBJECT -> JsonObject().also { o ->
                r.beginObject()
                while (r.hasNext()) {
                    val name = r.nextName()
                    require(!o.has(name)) { "Doppeltes JSONfeld" }
                    o.add(name, read(r, depth + 1))
                }
                r.endObject()
            }
            JsonToken.BEGIN_ARRAY -> JsonArray().also { a ->
                r.beginArray(); while (r.hasNext()) a.add(read(r, depth + 1)); r.endArray()
            }
            JsonToken.STRING -> JsonPrimitive(r.nextString())
            JsonToken.NUMBER -> JsonPrimitive(r.nextString().toBigDecimal())
            JsonToken.BOOLEAN -> JsonPrimitive(r.nextBoolean())
            JsonToken.NULL -> { r.nextNull(); JsonNull.INSTANCE }
            else -> error("JSONwert ungültig")
        }
    }
    private fun JsonElement.obj(fields: String): JsonObject {
        require(isJsonObject && asJsonObject.keySet() == fields.split(' ').toSet()) { "Statusfelder ungültig" }
        return asJsonObject
    }
    private fun JsonElement.string(max: Int): String {
        require(isJsonPrimitive && asJsonPrimitive.isString) { "Text erwartet" }
        return asString.also { require(it.codePointCount(0, it.length) <= max) { "Text zu lang" } }
    }
    private fun JsonObject.bool(key: String): Boolean {
        val v = get(key)
        require(v.isJsonPrimitive && v.asJsonPrimitive.isBoolean) { "$key muss boolesch sein" }
        return v.asBoolean
    }
    private fun JsonObject.uint(key: String, max: Long = U32, min: Long = 0): Long {
        val v = get(key)
        require(v.isJsonPrimitive && v.asJsonPrimitive.isNumber) { "$key muss numerisch sein" }
        val value = v.asBigDecimal.longValueExact()
        require(value in min..max) { "$key außerhalb der Grenzen" }
        return value
    }
    private fun JsonObject.nullableId(key: String): Long? = if (get(key).isJsonNull) null else uint(key, 65535, 1)
}

enum class StatusFreshness { WAITING, CURRENT, STALE, DISCONNECTED }
data class StatusReading(val status: ConfiguredStatus?, val freshness: StatusFreshness)

/** Serialized connection owner; local monotonic receive times, never ESP uptime as a phone clock. */
class ConfiguredStatusTracker(private val freshnessMs: Long) {
    init { require(freshnessMs > 0) }
    private var connected = false
    private var current: ConfiguredStatus? = null
    private var receivedAt = 0L
    fun connect() { connected = true; current = null; receivedAt = 0 }
    fun disconnect() { connected = false; current = null; receivedAt = 0 }
    fun accept(next: ConfiguredStatus, nowMs: Long): Boolean {
        check(connected)
        require(nowMs >= 0)
        val old = current
        if (old != null && old.boot == next.boot && next.sample <= old.sample) return false
        current = next; receivedAt = nowMs
        return true
    }
    fun read(nowMs: Long): StatusReading = StatusReading(current, when {
        !connected -> StatusFreshness.DISCONNECTED
        current == null -> StatusFreshness.WAITING
        nowMs < receivedAt || nowMs - receivedAt >= freshnessMs -> StatusFreshness.STALE
        else -> StatusFreshness.CURRENT
    })
}
