package de.larpescape.protocol

import com.google.gson.*
import org.junit.Assert.*
import org.junit.Test

class DiagnosticV1Test {
    private val start = "2026-10-10T12:00:00.000Z"
    private val end = "2026-10-10T12:00:02.000Z"
    private fun fixture(name: String = "completed") = checkNotNull(javaClass.getResourceAsStream("/diagnostic-v1/$name.json")).use { it.readBytes() }
    private fun report() = DiagnosticReportCodecV1.import(fixture()).snapshot()
    private fun template(): DiagnosticReportV1 = DiagnosticReportCodecV1.fromDocument(report().apply { add("steps", JsonArray()) })
    private fun reply(index: Int = 0): JsonObject = report().getAsJsonArray("steps")[index].asJsonObject.getAsJsonObject("deviceReply").deepCopy()
    private fun encoded(value: JsonObject) = DiagnosticJsonV1.canonical(value)
    private fun rejects(block: () -> Unit) { try { block(); fail("Expected rejection") } catch (_: IllegalArgumentException) { } }
    private fun repo(journal: DiagnosticJournalV1 = MemoryDiagnosticJournalV1(), send: (JsonObject) -> Unit = {}) = FakeDiagnosticRepositoryV1.start(template(), journal, SyntheticDiagnosticTransportV1(send), 7)

    @Test fun eightGoldenDocumentsRoundTripByteForByte() {
        listOf("completed", "failed", "not-tested", "timeout", "cancelled", "disconnected", "app_restarted", "device_restarted").forEach {
            val bytes = fixture(it); val parsed = DiagnosticReportCodecV1.import(bytes)
            assertArrayEquals(bytes, DiagnosticReportCodecV1.export(parsed))
            assertEquals(parsed.verdicts, DiagnosticReportCodecV1.import(DiagnosticReportCodecV1.export(parsed)).verdicts)
            assertEquals("synthetic", parsed.evidence)
        }
    }
    @Test fun canonicalUnicodeEscapesMatchPython() {
        val value = JsonObject().apply { addProperty("z", "ä😀\u007f\n"); addProperty("a", "<>&") }
        assertEquals("{\"a\":\"<>&\",\"z\":\"\\u00e4\\ud83d\\ude00\\u007f\\n\"}", encoded(value).toString(Charsets.US_ASCII))
        assertEquals(value, DiagnosticJsonV1.parse(encoded(value)))
    }
    @Test fun rejectsDuplicateKeysFloatExponentInvalidUtf8SurrogatesAndDepth() {
        listOf("{\"x\":1,\"x\":2}", "{\"x\":1.0}", "{\"x\":1e0}", "{\"x\":\"\\ud800\"}", "[".repeat(34) + "0" + "]".repeat(34), "{}{}", "{\"x\":NaN}").forEach { rejects { DiagnosticJsonV1.parse(it.toByteArray()) } }
        rejects { DiagnosticJsonV1.parse(byteArrayOf(0xc3.toByte(), 0x28)) }
        rejects { DiagnosticJsonV1.parse(ByteArray(DiagnosticJsonV1.MAX_BYTES + 1)) }
    }
    @Test fun checksumCannotLegitimizeForgedVerdictsOrSchema() {
        listOf<(JsonObject) -> Unit>(
            { it.addProperty("schemaVersion", 2) }, { it.addProperty("extra", true) },
            { it.getAsJsonArray("steps")[0].asJsonObject.addProperty("verdict", "failed") },
            { it.getAsJsonArray("steps")[0].asJsonObject.getAsJsonObject("request").addProperty("operation", "write_nfc") },
            { it.getAsJsonArray("steps")[0].asJsonObject.getAsJsonObject("deviceReply").addProperty("stepId", 9) },
            { it.getAsJsonObject("grant").addProperty("expiresDeviceMs", 9999999) }
        ).forEach { change -> val doc = report().apply(change); val envelope = JsonObject().apply { add("report", doc); addProperty("sha256", DiagnosticJsonV1.sha256(encoded(doc))) }; rejects { DiagnosticReportCodecV1.import(encoded(envelope)) } }
        val bytes = fixture().copyOf().apply { this[size - 5] = '0'.code.toByte() }
        rejects { DiagnosticReportCodecV1.import(bytes) }
    }
    @Test fun pendingIntentIsCommittedBeforeSendAndWriteSuccessRemainsUnclear() {
        val journal = MemoryDiagnosticJournalV1()
        val repository = repo(journal) { request ->
            val persisted = DiagnosticJsonV1.parse(journal.load()!!).asJsonObject.getAsJsonObject("report").getAsJsonArray("steps")[0].asJsonObject
            assertEquals(request, persisted.getAsJsonObject("request")); assertEquals("pending", persisted.text("state"))
        }
        repository.begin("wake_capture", start, 0)
        assertEquals("unclear", repository.snapshot().getAsJsonArray("steps")[0].asJsonObject.text("verdict"))
    }
    @Test fun persistenceFailurePreventsSendingAndStateChange() {
        var failWrite = false; var sends = 0
        val store = MemoryDiagnosticJournalV1()
        val journal = object : DiagnosticJournalV1 { override fun load() = store.load(); override fun replace(bytes: ByteArray) { if (failWrite) throw IllegalStateException("disk full"); store.replace(bytes) } }
        val repository = repo(journal) { sends++ }; failWrite = true
        try { repository.begin("led_single", start, 0); fail() } catch (_: IllegalStateException) { }
        assertEquals(0, sends); assertEquals(0, repository.snapshot().getAsJsonArray("steps").size())
    }
    @Test fun bothOperationsNeedSeparateObservationBeforePassing() {
        val repository = repo()
        listOf("wake_capture", "led_single").forEachIndexed { i, op ->
            repository.begin(op, start, i * 3000L)
            assertTrue(repository.receive(encoded(reply(i)), 7, end, i * 3000L + 500))
            assertEquals("unclear", repository.snapshot().getAsJsonArray("steps")[i].asJsonObject.text("verdict"))
            repository.observe(i + 1L, "matches", "expected signal", "synthetic observation", end)
        }
        val parsed = DiagnosticReportCodecV1.import(DiagnosticReportCodecV1.export(repository.finish(end)))
        assertEquals(listOf("passed", "passed"), parsed.verdicts)
    }
    @Test fun bindingCorrelationOperationAndDeviceTimeAreRequired() {
        val repository = repo(); repository.begin("wake_capture", start, 0)
        assertFalse(repository.receive(encoded(reply()), 8, end, 500))
        (diagnosticCorrelation + listOf("startedDeviceMs", "endedDeviceMs")).forEach { key ->
            val wrong = reply().apply { when (key) {
                "stepId" -> addProperty(key, 99); "operation" -> { addProperty(key, "led_single"); add("telemetry", reply(1)["telemetry"].deepCopy()) }
                "startedDeviceMs" -> addProperty(key, 500); "endedDeviceMs" -> addProperty(key, 31000)
                "deviceId" -> addProperty(key, "F".repeat(32)); "bootId" -> addProperty(key, "F".repeat(16))
                else -> addProperty(key, "ffffffff-ffff-4fff-8fff-ffffffffffff")
            } }
            assertFalse(repository.receive(encoded(wrong), 7, end, 500))
        }
        assertTrue(repository.receive(encoded(reply()), 7, end, 500))
        assertFalse(repository.receive(encoded(reply()), 7, end, 500))
    }
    @Test fun interruptionsStayUnclearAndLateAnswersNeverRepairThem() {
        diagnosticInterruptions.forEach { reason ->
            val repository = repo(); repository.begin("led_single", start, 0)
            repository.interrupt(reason, end)
            assertFalse(repository.receive(encoded(reply(1)), 7, end, 500))
            assertEquals("unclear", repository.finish(end).verdicts.single())
            rejects { repository.begin("wake_capture", end, 5000) }
        }
    }
    @Test fun timeoutIncludesQueuedTimeAndNoSecondPendingStep() {
        val repository = repo(); repository.begin("wake_capture", start, 100)
        rejects { repository.begin("led_single", start, 100) }
        assertFalse(repository.receive(encoded(reply()), 7, end, 2100))
        assertEquals("timeout", repository.snapshot().getAsJsonArray("steps")[0].asJsonObject.text("state"))
    }
    @Test fun recoveryRetiresIntentWithoutAnyResend() {
        val journal = MemoryDiagnosticJournalV1(); var sends = 0
        repo(journal) { sends++ }.begin("led_single", start, 0)
        val recovered = FakeDiagnosticRepositoryV1.recover(journal, end)
        assertEquals(1, sends); assertEquals("app_restarted", recovered.snapshot().getAsJsonArray("steps")[0].asJsonObject.text("state"))
        assertEquals("unclear", recovered.finish(end).verdicts.single())
        rejects { recovered.begin("wake_capture", end, 5000) }
    }
    @Test fun finalizedReportStaysImmutableAcrossRecoveryAndRepeatedExport() {
        val journal = MemoryDiagnosticJournalV1()
        val original = repo(journal)
        original.skip("wake_capture", start)
        val exported = DiagnosticReportCodecV1.export(original.finish(end))
        val saved = journal.load()!!
        val recovered = FakeDiagnosticRepositoryV1.recover(journal, "2026-10-10T12:00:03.000Z")
        rejects { recovered.observe(1, "matches", "expected", "changed", "2026-10-10T12:00:03.000Z") }
        assertArrayEquals(exported, DiagnosticReportCodecV1.export(recovered.finish("2026-10-10T12:00:04.000Z")))
        assertArrayEquals(saved, journal.load())
        assertEquals("not_tested", DiagnosticReportCodecV1.import(exported).verdicts.single())
    }
    @Test fun legacyRawJournalWithoutPendingIsFrozenWithoutInventingAnObservation() {
        val journal = MemoryDiagnosticJournalV1()
        val original = repo(journal)
        original.begin("wake_capture", start, 0)
        assertTrue(original.receive(encoded(reply()), 7, end, 500))
        journal.replace(encoded(original.snapshot())) // v1 raw journal, possibly already exported
        val recovered = FakeDiagnosticRepositoryV1.recover(journal, "2026-10-10T12:00:03.000Z")
        assertTrue(recovered.isLegacyLocked); assertTrue(recovered.isFinalized)
        rejects { recovered.observe(1, "matches", "expected", "invented", "2026-10-10T12:00:03.000Z") }
        assertFalse(recovered.receive(encoded(reply()), 7, "2026-10-10T12:00:03.000Z", 600))
        val report = DiagnosticReportCodecV1.export(recovered.finish("2026-10-10T12:00:04.000Z"))
        assertEquals(listOf("unclear"), DiagnosticReportCodecV1.import(report).verdicts)
        val reopened = FakeDiagnosticRepositoryV1.recover(journal, "2026-10-10T12:00:05.000Z")
        assertTrue(reopened.isLegacyLocked)
        assertArrayEquals(report, DiagnosticReportCodecV1.export(reopened.finish("2026-10-10T12:00:06.000Z")))
    }
    @Test fun explicitUnfinalizedMarkerAllowsObservationAfterRestartWithoutReplay() {
        val journal = MemoryDiagnosticJournalV1()
        val original = repo(journal)
        original.begin("wake_capture", start, 0)
        assertTrue(original.receive(encoded(reply()), 7, end, 500))
        val recovered = FakeDiagnosticRepositoryV1.recover(journal, "2026-10-10T12:00:03.000Z")
        assertFalse(recovered.isLegacyLocked); assertFalse(recovered.isFinalized)
        recovered.observe(1, "matches", "expected", "saved after restart", "2026-10-10T12:00:03.000Z")
        assertEquals(listOf("passed"), DiagnosticReportCodecV1.import(DiagnosticReportCodecV1.export(recovered.finish("2026-10-10T12:00:03.000Z"))).verdicts)
    }
    @Test fun legacyRawPendingIntentCanOnlyBeRetiredAsUnknown() {
        val journal = MemoryDiagnosticJournalV1()
        val original = repo(journal)
        original.begin("led_single", start, 0)
        journal.replace(encoded(original.snapshot()))
        val recovered = FakeDiagnosticRepositoryV1.recover(journal, end)
        assertFalse(recovered.isLegacyLocked)
        assertEquals("app_restarted", recovered.snapshot().getAsJsonArray("steps")[0].asJsonObject.text("state"))
        assertEquals(listOf("unclear"), DiagnosticReportCodecV1.import(DiagnosticReportCodecV1.export(recovered.finish(end))).verdicts)
    }
    @Test fun failedTelemetryAndUserMismatchCannotPass() {
        listOf<(JsonObject) -> Unit>(
            { it.getAsJsonObject("telemetry").addProperty("overflow", true) },
            { it.getAsJsonObject("telemetry").getAsJsonArray("edges")[0].asJsonObject.addProperty("rawHigh", false) },
            { it.getAsJsonObject("telemetry").getAsJsonArray("edges").add(it.getAsJsonObject("telemetry").getAsJsonArray("edges")[0].deepCopy()) }
        ).forEach { change -> val repository = repo(); repository.begin("wake_capture", start, 0); repository.receive(encoded(reply().apply(change)), 7, end, 500); repository.observe(1, "matches", "signal", "signal", end); assertEquals("failed", repository.finish(end).verdicts.single()) }
        val repository = repo(); repository.begin("led_single", start, 0)
        repository.receive(encoded(reply(1).apply { addProperty("stepId", 1) }), 7, end, 500)
        repository.observe(1, "mismatch", "red LED", "no light", end)
        assertEquals("failed", repository.finish(end).verdicts.single())
    }
    @Test fun skippedStepNeverSendsAndExportRejectsPending() {
        var sends = 0; val repository = repo { sends++ }
        repository.skip("led_single", start)
        assertEquals(0, sends); assertEquals("not_tested", repository.finish(end).verdicts.single())
        val pending = repo(); pending.begin("wake_capture", start, 0)
        rejects { pending.finish(end) }
    }
    @Test fun rawReplyBudgetIsBoundedAndEndsRun() {
        val repository = repo(); repository.begin("wake_capture", start, 0)
        repeat(16) { assertFalse(repository.receive(encoded(reply()), 9, end, 500)) }
        try { repository.receive(encoded(reply()), 9, end, 500); fail() } catch (_: IllegalStateException) { }
        assertEquals(16, repository.snapshot().getAsJsonArray("steps")[0].asJsonObject.getAsJsonArray("ignoredReplies").size())
        assertEquals("unclear", repository.finish(end).verdicts.single())
    }
    @Test fun snapshotsAndJournalOwnTheirBytes() {
        val repository = repo(); val request = repository.begin("wake_capture", start, 0)
        request.addProperty("stepId", 999); repository.snapshot().getAsJsonArray("steps").remove(0)
        assertTrue(repository.receive(encoded(reply()), 7, end, 500))
        val journal = MemoryDiagnosticJournalV1(); val input = byteArrayOf(1); journal.replace(input); input[0] = 9; journal.load()!![0] = 8
        assertArrayEquals(byteArrayOf(1), journal.load())
    }
}
