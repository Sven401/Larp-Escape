package de.larpescape.protocol

import com.google.gson.*
import org.junit.Assert.*
import org.junit.Test
import java.io.File

class ConfiguredStatusTest {
    private fun fixture(name: String = "status-ready.json") =
        File(System.getProperty("firmwareFixtures"), name).readText()
    private fun decode(text: String) = ConfiguredStatusDecoder.decode(text.toByteArray())
    private fun ready() = decode(fixture())
    private fun rejects(text: String) {
        assertThrows(Exception::class.java) { decode(text) }
    }
    @Test fun firmwareGoldenStatusHasAllTypedValues() {
        val r = ready(); val running = decode(fixture("status-running.json"))
        assertEquals("12345678ABCDEF01", r.boot)
        assertEquals(ApplicationPhase.READY, r.application.phase)
        assertEquals(3L, r.application.profileRevision)
        assertTrue(r.application.hardwareReady)
        assertNull(r.game.state); assertFalse(r.game.running)
        assertEquals(7, r.devices.size)
        assertTrue(r.devices.all { it.enabled && it.available && it.cardReadable && it.matrixReadable })
        assertEquals(2L, running.sample); assertEquals(1L, running.game.state)
        assertTrue(running.game.running); assertTrue(running.game.entryPending)
        assertEquals(2L, running.control.nextRequest)
    }
    @Test fun everySchemaFieldIsRequiredAndUnknownFieldsAreRejected() {
        fun visit(root: JsonObject, path: List<String>) {
            val objectAt = path.fold(root) { o, key -> o.getAsJsonObject(key) }
            for (key in objectAt.keySet().toList()) {
                val changed = root.deepCopy()
                path.fold(changed) { o, p -> o.getAsJsonObject(p) }.remove(key)
                rejects(changed.toString())
                if (objectAt[key].isJsonObject) visit(root, path + key)
            }
            val extra = root.deepCopy()
            path.fold(extra) { o, p -> o.getAsJsonObject(p) }.addProperty("unknown", 1)
            rejects(extra.toString())
        }
        val root = JsonParser.parseString(fixture()).asJsonObject
        visit(root, emptyList())
        val device = root.getAsJsonArray("devices")[0].asJsonObject
        for (key in device.keySet().toList()) {
            val changed = root.deepCopy()
            changed.getAsJsonArray("devices")[0].asJsonObject.remove(key)
            rejects(changed.toString())
        }
        device.addProperty("unknown", true); rejects(root.toString())
    }
    @Test fun rejectsWrongTypesAndNumericBoundsForEveryField() {
        fun visit(value: JsonElement, change: (JsonElement) -> Unit) {
            when {
                value.isJsonObject -> for ((key, child) in value.asJsonObject.entrySet()) {
                    visit(child) { replacement ->
                        val copy = value.deepCopy().asJsonObject; copy.add(key, replacement); change(copy)
                    }
                }
                value.isJsonArray -> value.asJsonArray.forEachIndexed { index, child ->
                    visit(child) { replacement ->
                        val copy = value.deepCopy().asJsonArray; copy.set(index, replacement); change(copy)
                    }
                }
                value.isJsonNull -> { change(JsonPrimitive(0)); change(JsonPrimitive("1")) }
                value.asJsonPrimitive.isBoolean -> change(JsonPrimitive("true"))
                value.asJsonPrimitive.isNumber -> {
                    change(JsonPrimitive("1")); change(JsonPrimitive(-1)); change(JsonPrimitive(4294967296L))
                    change(JsonPrimitive(1.5)); change(JsonNull.INSTANCE)
                }
                value.asJsonPrimitive.isString -> change(JsonPrimitive(false))
            }
        }
        visit(JsonParser.parseString(fixture())) { rejects(it.toString()) }
    }
    @Test fun validatesPhaseBootCapabilitiesDevicesAndTextLimits() {
        val root = JsonParser.parseString(fixture()).asJsonObject
        fun changed(edit: (JsonObject) -> Unit) = root.deepCopy().also(edit).toString()
        for (phase in ApplicationPhase.entries) {
            assertEquals(phase, decode(changed { it.getAsJsonObject("application").addProperty("phase", phase.wire) }).application.phase)
        }
        rejects(changed { it.getAsJsonObject("application").addProperty("phase", "paused") })
        for (boot in listOf("bad", "0000000000000000", "12345678abcdef01")) rejects(changed { it.addProperty("boot", boot) })
        rejects(changed { it.addProperty("sample", 0) })
        rejects(changed { it.getAsJsonArray("capabilities").remove(1) })
        rejects(changed { it.getAsJsonArray("devices").add(it.getAsJsonArray("devices")[0].deepCopy()) })
        rejects(changed { it.getAsJsonObject("game").addProperty("lastTickOutcome", "x".repeat(33)) })
        rejects(changed { it.getAsJsonArray("devices")[0].asJsonObject.addProperty("lastOutcome", "x".repeat(17)) })
        rejects(changed { it.getAsJsonObject("game").addProperty("crystalCount", 9) })
        rejects(changed { it.getAsJsonObject("game").addProperty("seenRoleCount", 17) })
        rejects(changed { it.getAsJsonObject("game").addProperty("state", 65536) })
        val sixteen = changed { o -> o.add("devices", JsonArray().also { a ->
            for (id in 1..16) a.add(root.getAsJsonArray("devices")[0].deepCopy().asJsonObject.also { it.addProperty("id", id) })
        }) }
        assertEquals(16, decode(sixteen).devices.size)
        rejects(JsonParser.parseString(sixteen).asJsonObject.also { o ->
            o.getAsJsonArray("devices").add(root.getAsJsonArray("devices")[0].deepCopy().asJsonObject.also { it.addProperty("id", 17) })
        }.toString())
        assertEquals(4294967295L, decode(changed { it.addProperty("sample", 4294967295L) }).sample)
        assertEquals(1L, decode(fixture().replace("\"sample\":1", "\"sample\":1.0")).sample)
    }
    @Test fun rejectsMalformedSyntaxDuplicateNamesAndInvalidUtf8() {
        rejects(fixture() + "{}")
        rejects(fixture().replace("\"sample\":1", "\"sample\":1,\"sample\":2"))
        rejects(fixture().replace("\"sample\":1", "sample:1"))
        rejects(fixture().replace("\"sample\":1", "\"sample\":NaN"))
        rejects("/*comment*/" + fixture())
        assertThrows(Exception::class.java) { ConfiguredStatusDecoder.decode(byteArrayOf(0xc3.toByte(), 0x28)) }
        assertThrows(Exception::class.java) { ConfiguredStatusDecoder.decode(ByteArray(6144)) }
    }
    @Test fun sequenceAndExpiryCannotBeRefreshedByDuplicatesOrOldSamples() {
        val tracker = ConfiguredStatusTracker(31000)
        assertEquals(StatusFreshness.DISCONNECTED, tracker.read(0).freshness)
        tracker.connect(); assertEquals(StatusFreshness.WAITING, tracker.read(0).freshness)
        assertTrue(tracker.accept(ready(), 100))
        assertEquals(StatusFreshness.CURRENT, tracker.read(31099).freshness)
        assertFalse(tracker.accept(ready(), 31099))
        assertEquals(StatusFreshness.STALE, tracker.read(31100).freshness)
        val running = decode(fixture("status-running.json"))
        assertTrue(tracker.accept(running, 31200))
        assertFalse(tracker.accept(ready(), 31300))
        assertEquals(2L, tracker.read(31300).status!!.sample)
        assertTrue(tracker.accept(ready().copy(boot = "12345678ABCDEF02"), 31400))
        assertEquals(1L, tracker.read(31400).status!!.sample)
        assertEquals(StatusFreshness.CURRENT, tracker.read(31400).freshness)
        tracker.disconnect(); assertNull(tracker.read(40000).status)
        tracker.connect(); assertTrue(tracker.accept(ready(), 50000))
    }
    @Test fun u32SampleDoesNotWrapWithinSameBootAndUptimeDoesNotDriveFreshness() {
        val tracker = ConfiguredStatusTracker(31000); tracker.connect()
        assertTrue(tracker.accept(ready().copy(sample = 4294967295L, sampledMilliseconds = 4294967295L), 0))
        assertFalse(tracker.accept(ready().copy(sampledMilliseconds = 0), 1))
        assertEquals(StatusFreshness.STALE, tracker.read(31000).freshness)
        tracker.disconnect(); tracker.connect()
        assertTrue(tracker.accept(ready().copy(sampledMilliseconds = 4294967295L), 40000))
        assertTrue(tracker.accept(ready().copy(sample = 2, sampledMilliseconds = 0), 40100))
    }
}
