package de.larpescape.protocol

import com.google.gson.JsonObject
import com.google.gson.JsonParser
import org.junit.Assert.*
import org.junit.Test

class DiagnosticStepV1Test {
    private fun hex(value: String) = value.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private val vectors = checkNotNull(javaClass.getResourceAsStream("/diagnostic-step-v1.json")).use {
        JsonParser.parseString(it.readBytes().toString(Charsets.US_ASCII)).asJsonObject.getAsJsonArray("vectors").map { v -> v.asJsonObject }
    }
    private fun vector(name: String) = vectors.single { it["name"].asString == name }
    private fun request(v: JsonObject) = DiagnosticStepRequestV1(v["message"].asLong, v["deviceId"].asString, v["bootId"].asString,
        v["runId"].asString, v["grantId"].asString, v["stepId"].asLong, v["timeoutMs"].asInt, DiagnosticStepOperationV1.valueOf(v["operation"].asString))
    private fun body(v: JsonObject = vector("firmware-wake-max64")) = hex(v["replyHex"].asString)
    private fun frames(v: JsonObject = vector("firmware-wake-max64"), capacity: Int = 128) = v.getAsJsonObject("frames").getAsJsonArray(capacity.toString()).map { hex(it.asString) }
    private val origin = DiagnosticControlOriginV1(7, 42, 9, 43, 1, true)
    private fun assembly(v: JsonObject = vector("firmware-wake-max64"), now: Long = 100, deadline: Long = 1100) = DiagnosticStepAssemblyV1(request(v), origin, now, deadline)
    private fun rejects(action: () -> Unit) { try { action(); fail("Expected invalid DG data") } catch (_: IllegalArgumentException) { } }
    private fun receive(a: DiagnosticStepAssemblyV1, bytes: ByteArray, now: Long = 201) = a.receive(bytes, origin, origin, now)
    private fun complete(a: DiagnosticStepAssemblyV1, frames: List<ByteArray>, now: Long = 201) { frames.forEachIndexed { i, bytes -> assertEquals(if (i == frames.lastIndex) DiagnosticStepAssemblyStatusV1.COMPLETE else DiagnosticStepAssemblyStatusV1.RECEIVING, receive(a, bytes, now)) } }
    private fun segment(source: ByteArray, offset: Int, count: Int, total: Int = source.size): ByteArray {
        // A separate test-side header constructor, independent of parser helpers.
        return byteArrayOf(68, 71, 1, 0x84.toByte(), 17, 0, 0, 0, total.toByte(), (total shr 8).toByte(), offset.toByte(), (offset shr 8).toByte()) + source.copyOfRange(offset, offset + count)
    }
    private fun replacements() = listOf(origin.copy(localGattGeneration = 8), origin.copy(connectionToken = 43), origin.copy(connectionHandle = 10), origin.copy(authorityEpoch = 44), origin.copy(authorityPrincipal = 2), origin.copy(gameMaster = false))

    @Test fun tenIndependentGoldenBodiesAndEverySegmentCapacityMatch() {
        assertEquals(10, vectors.size)
        vectors.forEach { v ->
            val req = request(v)
            assertArrayEquals(hex(v["requestHex"].asString), DiagnosticStepV1.encode(req)); assertEquals(req, DiagnosticStepV1.request(hex(v["requestHex"].asString)))
            if (!v["wireAccept"].asBoolean) {
                rejects { DiagnosticStepV1.reply(body(v)) }
                listOf(13, 20, 127, 128).forEach { capacity ->
                    val a = assembly(v); val parts = frames(v, capacity)
                    parts.dropLast(1).forEach { assertEquals(DiagnosticStepAssemblyStatusV1.RECEIVING, receive(a, it)) }
                    assertEquals(DiagnosticStepAssemblyStatusV1.REJECTED, receive(a, parts.last())); assertNull(a.take(origin, 202))
                }
                return@forEach
            }
            val reply = DiagnosticStepV1.correlate(body(v), req)
            assertEquals(v["startedDeviceMs"].asLong, reply.startedDeviceMs); assertEquals(v["endedDeviceMs"].asLong, reply.endedDeviceMs)
            assertEquals(v["outcome"].asInt, reply.outcome.ordinal); assertEquals(v["reason"].asInt, reply.reason.ordinal)
            assertEquals(v["edgeCount"].asInt, reply.edges.size); assertEquals(v["activeHigh"].asBoolean, reply.activeHigh)
            assertEquals(v["overflow"].asBoolean, reply.overflow); assertEquals(v["restoredOff"].asBoolean, reply.restoredOff); assertEquals(v["renderCalls"].asInt, reply.renderCalls)
            assertTrue(reply.synthetic); assertNull(reply.supplyMillivolts)
            listOf(13, 20, 127, 128).forEach { capacity ->
                val parts = frames(v, capacity); assertTrue(parts.all { it.size <= capacity })
                assertArrayEquals(body(v), parts.flatMap { it.drop(12) }.toByteArray())
                val a = assembly(v); complete(a, parts)
                assertEquals(req, a.take(origin, 202)!!.request); assertNull(a.take(origin, 202))
            }
        }
        assertEquals(7, frames().size); assertEquals(13, frames(vector("firmware-led"), 20).size)
        val wake = DiagnosticStepV1.reply(body())
        wake.edges.forEachIndexed { i, edge -> assertEquals(110L + i, edge.deviceMs); assertEquals(1 + i % 4, edge.channel); assertEquals(i % 2 == 0, edge.rawHigh); assertEquals(edge.rawHigh, edge.active) }
    }
    @Test fun requestLengthsUnknownOperationsUnusedFieldsAndInvalidSelectorsFail() {
        val valid = hex(vector("firmware-wake-max64")["requestHex"].asString)
        for (length in 0..129) if (length != 80) rejects { DiagnosticStepV1.request(valid.copyOf(length)) }
        rejects { DiagnosticStepV1.request(ByteArray(1024 * 1024)) }
        listOf(0 to 0, 1 to 0, 2 to 2, 3 to 0x84, 38 to 0, 40 to 0, 54 to 0, 56 to 0, 72 to 2, 73 to 2).forEach { (at, byte) -> rejects { DiagnosticStepV1.request(valid.copyOf().apply { this[at] = byte.toByte() }) } }
        listOf(4..7, 8..23, 24..31, 64..67, 68..71).forEach { range -> rejects { DiagnosticStepV1.request(valid.copyOf().apply { range.forEach { this[it] = 0 } }) } }
        for (at in 74..79) rejects { DiagnosticStepV1.request(valid.copyOf().apply { this[at] = 1 }) }
        val led = hex(vector("firmware-led")["requestHex"].asString)
        listOf(73 to 1, 74 to 1, 75 to 17, 76 to 1, 77 to 1, 78 to 0, 79 to 0).forEach { (at, byte) -> rejects { DiagnosticStepV1.request(led.copyOf().apply { this[at] = byte.toByte() }) } }
        val req = request(vector("firmware-wake-max64"))
        listOf(req.copy(message = 0), req.copy(message = 4294967296L), req.copy(stepId = 0), req.copy(timeoutMs = 999), req.copy(timeoutMs = 10001),
            req.copy(bootId = "0".repeat(16)), req.copy(deviceId = "0".repeat(32)), req.copy(runId = "00000000-0000-5000-8000-000000000001"), req.copy(holdMs = 500)).forEach { rejects { DiagnosticStepV1.encode(it) } }
    }
    @Test fun responseLengthCountsTimesReasonsPolarityAndFlagsAreStrict() {
        val valid = body(); val saved = valid.copyOf()
        for (length in 0..755) if (length != 744) rejects { DiagnosticStepV1.reply(valid.copyOf(length)) }
        rejects { DiagnosticStepV1.reply(ByteArray(1024 * 1024)) }
        listOf(96 to 4, 97 to 8, 97 to 1, 98 to 2, 99 to 1, 100 to 1, 101 to 2, 102 to 65, 103 to 0, 103 to 3, 112 to 0, 113 to 4, 113 to 1).forEach { (at, byte) -> rejects { DiagnosticStepV1.reply(valid.copyOf().apply { this[at] = byte.toByte() }) } }
        rejects { DiagnosticStepV1.reply(valid.copyOf().apply { this[87] = 0x80.toByte() }) }
        rejects { DiagnosticStepV1.reply(valid.copyOf().apply { this[95] = 0x80.toByte() }) }
        rejects { DiagnosticStepV1.reply(valid.copyOf().apply { this[104] = 99 }) }
        rejects { DiagnosticStepV1.reply(valid.copyOf().apply { this[114] = 100 }) }
        rejects { DiagnosticStepV1.reply(valid.copyOf().apply { this[88] = 99 }) }
        assertArrayEquals(saved, valid)
        val led = body(vector("firmware-led"))
        listOf(99 to 0, 100 to 1, 100 to 3, 101 to 1).forEach { (at, byte) -> rejects { DiagnosticStepV1.reply(led.copyOf().apply { this[at] = byte.toByte() }) } }
        rejects { DiagnosticStepV1.reply((led + ByteArray(10)).apply { this[102] = 1 }) }
        val failed = body(vector("led-failed")); rejects { DiagnosticStepV1.reply(failed.apply { this[97] = 0 }) }
    }
    @Test fun u32AndU64SelectorsAndExactTimeLimitAreNotTruncated() {
        val v = vector("last-exact-time"); val req = request(v); val reply = DiagnosticStepV1.correlate(body(v), req)
        assertEquals(4294967295L, req.message); assertEquals(4294967295L, req.stepId); assertEquals("FFFFFFFFFFFFFFFF", req.bootId)
        assertEquals(9007199254740991L, reply.endedDeviceMs)
        rejects { DiagnosticStepV1.reply(body(v).apply { this[88] = 0; this[89] = 0; this[90] = 0; this[91] = 0; this[92] = 0; this[93] = 0; this[94] = 0x20 }) }
        val a = assembly(now = 9007199254739991L, deadline = 9007199254740991L)
        complete(a, frames(), 9007199254740990L); assertNull(a.take(origin, 9007199254740991L))
        rejects { assembly(now = 100, deadline = 9007199254740992L) }
        rejects { assembly(now = -1) }; rejects { assembly(now = 100, deadline = 100) }; rejects { assembly(now = 100, deadline = 1101) }
    }
    @Test fun allOriginalRequestFieldsIncludingOperationParametersMustMatch() {
        val original = request(vector("firmware-wake-max64"))
        val changed = listOf(original.copy(message = 18), original.copy(deviceId = "F".repeat(32)), original.copy(bootId = "FFFFFFFFFFFFFFFF"),
            original.copy(runId = "ffffffff-ffff-4fff-8fff-ffffffffffff"), original.copy(grantId = "ffffffff-ffff-4fff-8fff-ffffffffffff"), original.copy(stepId = 2), original.copy(timeoutMs = 1001),
            original.copy(operation = DiagnosticStepOperationV1.LED_SINGLE, channel = 0, red = 16, holdMs = 500))
        changed.forEach { req -> rejects { DiagnosticStepV1.correlate(body(), req) } }
        val wrongBody = body().apply { this[64] = 2 }
        val a = assembly(); val parts = (0 until wrongBody.size step 116).map { segment(wrongBody, it, minOf(116, wrongBody.size - it)) }
        parts.dropLast(1).forEach { assertEquals(DiagnosticStepAssemblyStatusV1.RECEIVING, receive(a, it)) }
        assertEquals(DiagnosticStepAssemblyStatusV1.REJECTED, receive(a, parts.last())); assertNull(a.take(origin, 202))
    }
    @Test fun identicalContainedDuplicatesAreHarmlessAndDoNotRefreshDeadline() {
        val a = assembly(); val first = frames().first()
        assertEquals(DiagnosticStepAssemblyStatusV1.RECEIVING, receive(a, first))
        assertEquals(DiagnosticStepAssemblyStatusV1.DUPLICATE, receive(a, first, 1099))
        assertEquals(DiagnosticStepAssemblyStatusV1.DUPLICATE, receive(a, segment(body(), 10, 5), 1099))
        assertEquals(DiagnosticStepAssemblyStatusV1.EXPIRED, receive(a, first, 1100)); assertNull(a.take(origin, 1100))
    }
    @Test fun alteredDuplicateAndPartialOverlapTerminateReception() {
        val changed = assembly(); receive(changed, frames().first())
        assertEquals(DiagnosticStepAssemblyStatusV1.REJECTED, receive(changed, frames().first().apply { this[12] = 0 }))
        val overlap = assembly(); receive(overlap, frames().first())
        assertEquals(DiagnosticStepAssemblyStatusV1.REJECTED, receive(overlap, segment(body(), 110, 12)))
        assertNull(changed.take(origin, 202)); assertNull(overlap.take(origin, 202))
    }
    @Test fun gapsWrongHeadersOversizedTotalsAndChangingTotalsCloseWithoutRetry() {
        val first = frames().first()
        val malformed = listOf(first.copyOf(12), first.copyOf(129), ByteArray(1024 * 1024),
            first.copyOf().apply { this[0] = 0 }, first.copyOf().apply { this[1] = 0 }, first.copyOf().apply { this[2] = 2 }, first.copyOf().apply { this[3] = 0x82.toByte() },
            first.copyOf().apply { this[4] = 18 }, first.copyOf().apply { this[8] = 0xff.toByte(); this[9] = 0xff.toByte() },
            first.copyOf().apply { this[8] = 105; this[9] = 0 }, first.copyOf().apply { this[8] = 103; this[9] = 0 }, first.copyOf().apply { this[10] = 1 },
            first.copyOf().apply { this[10] = this[8]; this[11] = this[9] }, frames()[1])
        malformed.forEach { bad -> val a = assembly(); assertEquals(DiagnosticStepAssemblyStatusV1.REJECTED, receive(a, bad)); assertNull(a.take(origin, 202)); assertEquals(DiagnosticStepAssemblyStatusV1.CLOSED, receive(a, first)) }
        val changed = assembly(); receive(changed, first)
        assertEquals(DiagnosticStepAssemblyStatusV1.REJECTED, receive(changed, frames()[1].apply { this[8] = (this[8] - 10).toByte() }))
        val beyond = assembly(vector("firmware-led")); assertEquals(DiagnosticStepAssemblyStatusV1.REJECTED, receive(beyond, segment(body(vector("firmware-led")), 100, 4) + byteArrayOf(0)))
    }
    @Test fun fullCapturedAndCurrentMetadataAreRequiredForEachSegmentAndTake() {
        replacements().forEach { other ->
            val captured = assembly(); assertEquals(DiagnosticStepAssemblyStatusV1.REJECTED, captured.receive(frames().first(), other, origin, 201))
            val current = assembly(); receive(current, frames().first()); assertEquals(DiagnosticStepAssemblyStatusV1.EXPIRED, current.receive(frames()[1], origin, other, 202))
            val done = assembly(); complete(done, frames()); assertNull(done.take(other, 202)); assertNull(done.take(origin, 202))
        }
        val lost = assembly(); assertEquals(DiagnosticStepAssemblyStatusV1.EXPIRED, lost.receive(frames().first(), origin, null, 201))
        rejects { DiagnosticStepAssemblyV1(request(vector("firmware-led")), origin.copy(gameMaster = false), 100, 1100) }
    }
    @Test fun absoluteQueueDeadlineBackwardsTimeAndDisconnectAreTerminal() {
        val queued = assembly(now = 900, deadline = 1100); receive(queued, frames().first(), 901)
        assertEquals(DiagnosticStepAssemblyStatusV1.EXPIRED, receive(queued, frames()[1], 1100))
        val backwards = assembly(); receive(backwards, frames().first(), 201)
        assertEquals(DiagnosticStepAssemblyStatusV1.EXPIRED, receive(backwards, frames()[1], 200))
        val early = assembly(); assertEquals(DiagnosticStepAssemblyStatusV1.EXPIRED, receive(early, frames().first(), 99))
        val disconnected = assembly(); receive(disconnected, frames().first()); disconnected.retire()
        assertEquals(DiagnosticStepAssemblyStatusV1.CLOSED, receive(disconnected, frames()[1])); assertNull(disconnected.take(origin, 202))
        val cancelled = assembly(); complete(cancelled, frames()); cancelled.retire(); assertNull(cancelled.take(origin, 202))
    }
    @Test fun completeStillRequiresTimelyTakeAndNeverReplacesTerminalResult() {
        val onTime = assembly(); complete(onTime, frames()); assertNotNull(onTime.take(origin, 1099))
        assertEquals(DiagnosticStepAssemblyStatusV1.CLOSED, receive(onTime, frames()[0], 1099)); assertNull(onTime.take(origin, 1099))
        val late = assembly(); complete(late, frames()); assertNull(late.take(origin, 1100)); assertEquals(DiagnosticStepAssemblyStatusV1.EXPIRED, late.status)
        val backwards = assembly(); complete(backwards, frames()); assertNull(backwards.take(origin, 200))
        val incomplete = assembly(); receive(incomplete, frames().first()); assertNull(incomplete.take(origin, 202)); assertTrue(incomplete.tick(origin, 1099)); assertFalse(incomplete.tick(origin, 1100))
    }
    @Test fun correctedWireTimeSemanticsAndReportBridgeAgreeWithoutRepairingBytes() {
        vectors.forEach { v ->
            if (v["jsonAccept"].asBoolean) {
                val reply = DiagnosticStepV1.reply(body(v))
                val json = DiagnosticJsonV1.parse(DiagnosticStepV1.toJson(reply)).asJsonObject
                assertEquals(reply.endedDeviceMs, json["endedDeviceMs"].asLong)
                if (reply.request.operation == DiagnosticStepOperationV1.LED_SINGLE) assertTrue(json.getAsJsonObject("telemetry")["supplyMillivolts"].isJsonNull)
                else assertEquals(reply.overflow, json.getAsJsonObject("telemetry")["overflow"].asBoolean)
            } else {
                rejects { DiagnosticStepV1.reply(body(v)) }
                // Defensive bridge check: a test-only internal object must not bypass duration validation.
                val r = DiagnosticStepReplyV1(request(v), v["startedDeviceMs"].asLong, v["endedDeviceMs"].asLong, DiagnosticStepOutcomeV1.COMPLETED, DiagnosticStepReasonV1.NONE,
                    v["activeHigh"].asBoolean, v["restoredOff"].asBoolean, v["renderCalls"].asInt, false, emptyList())
                rejects { DiagnosticStepV1.toJson(r) }
            }
        }
        assertEquals(1100L, vector("wire-duration-equals-timeout")["endedDeviceMs"].asLong)
        assertEquals(599L, vector("wire-led-short-hold")["endedDeviceMs"].asLong)
        val before = body(vector("wire-duration-equals-timeout")).apply { this[88] = 0x4b; this[89] = 4 }
        assertEquals(999L, DiagnosticStepV1.reply(before).endedDeviceMs - 100)
        rejects { DiagnosticStepV1.reply(before.copyOf().apply { this[88] = 0x4d }) }
        val failed = body(vector("led-failed")).apply { this[88] = 100; this[89] = 0 }
        assertEquals(0L, DiagnosticStepV1.reply(failed).endedDeviceMs - 100)
    }
    @Test fun edgeListCannotBeChangedAfterDecode() {
        val reply = DiagnosticStepV1.reply(body())
        try { (reply.edges as MutableList).clear(); fail("Edges must be immutable") } catch (_: UnsupportedOperationException) { }
        assertEquals(64, reply.edges.size)
    }
    @Test fun overflowAndMaximumEdgesSurviveChecksummedReportRoundTripWithoutPassedVerdict() {
        val utc = "2026-10-10T12:00:00.000Z"
        val repo = FakeDiagnosticRepositoryV1.start(SyntheticDiagnosticV1.newRun("step-test", utc), MemoryDiagnosticJournalV1(), SyntheticDiagnosticTransportV1 { }, 1)
        val jsonRequest = repo.begin("wake_capture", utc, 100, 1000)
        val original = DiagnosticStepV1.fromJsonRequest(jsonRequest, 17)
        val response = body(vector("wake-overflow")).apply { DiagnosticStepV1.encode(original).copyInto(this) }
        assertTrue(repo.receive(DiagnosticStepV1.toJson(DiagnosticStepV1.correlate(response, original)), 1, utc, 201))
        repo.observe(1, "matches", "Taste", "synthetische Beobachtung", utc)
        val report = repo.finish(utc); val bytes = DiagnosticReportCodecV1.export(report)
        val imported = DiagnosticReportCodecV1.import(bytes)
        assertEquals("synthetic", imported.evidence); assertEquals(listOf("failed"), imported.verdicts)
        assertArrayEquals(bytes, DiagnosticReportCodecV1.export(imported))
        val telemetry = imported.snapshot().getAsJsonArray("steps")[0].asJsonObject.getAsJsonObject("deviceReply").getAsJsonObject("telemetry")
        assertEquals(64, telemetry.getAsJsonArray("edges").size()); assertTrue(telemetry["overflow"].asBoolean)
    }
    @Test fun syntheticExchangeUsesJournaledIntentAndMatchesFormerJsonWithoutGrantingAnything() {
        val utc = "2026-10-10T12:00:00.000Z"
        listOf("wake_capture", "led_single").forEach { op -> listOf(false, true).forEach { failed ->
            val journal = MemoryDiagnosticJournalV1(); var exchange: SyntheticDiagnosticStepV1? = null
            val repo = FakeDiagnosticRepositoryV1.start(SyntheticDiagnosticV1.newRun("step-test", utc), journal, SyntheticDiagnosticTransportV1 {
                val saved = DiagnosticJsonV1.parse(journal.load()!!).asJsonObject.getAsJsonObject("report").getAsJsonArray("steps").last().asJsonObject
                assertEquals("pending", saved["state"].asString); assertEquals(it, saved.getAsJsonObject("request"))
                exchange = SyntheticDiagnosticStepV1(it, 100)
            }, 1)
            val req = repo.begin(op, utc, 100, 1000)
            val bytes = exchange!!.respond(100, failed, 600)
            assertArrayEquals(SyntheticDiagnosticV1.reply(req, 100, failed), bytes)
            assertTrue(repo.receive(bytes, 1, utc, 600)); assertEquals(if (failed) "failed" else "unclear", repo.snapshot().getAsJsonArray("steps").last().asJsonObject["verdict"].asString)
            try { exchange!!.respond(100, failed, 601); fail("Synthetic result must be consumed once") } catch (_: IllegalStateException) { }
        } }
    }
    @Test fun syntheticExchangeCannotResumeAfterRetirementOrDeadline() {
        val utc = "2026-10-10T12:00:00.000Z"
        val repo = FakeDiagnosticRepositoryV1.start(SyntheticDiagnosticV1.newRun("step-test", utc), MemoryDiagnosticJournalV1(), SyntheticDiagnosticTransportV1 { }, 1)
        val req = repo.begin("led_single", utc, 100, 1000)
        val retired = SyntheticDiagnosticStepV1(req, 100); retired.retire()
        try { retired.respond(100, false, 600); fail("Retired exchange cannot receive") } catch (_: IllegalStateException) { }
        val expired = SyntheticDiagnosticStepV1(req, 100)
        try { expired.respond(100, false, 1100); fail("Expired exchange cannot receive") } catch (_: IllegalStateException) { }
        assertEquals("pending", repo.snapshot().getAsJsonArray("steps").last().asJsonObject["state"].asString)
    }
}
