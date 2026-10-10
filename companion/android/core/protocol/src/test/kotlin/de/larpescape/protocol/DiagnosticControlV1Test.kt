package de.larpescape.protocol

import com.google.gson.JsonObject
import com.google.gson.JsonParser
import org.junit.Assert.*
import org.junit.Test

class DiagnosticControlV1Test {
    private fun hex(value: String) = value.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private val vectors = checkNotNull(javaClass.getResourceAsStream("/diagnostic-control-v1.json")).use {
        JsonParser.parseString(it.readBytes().toString(Charsets.US_ASCII)).asJsonObject.getAsJsonArray("vectors").map { v -> v.asJsonObject }
    }
    private fun vector(name: String) = vectors.single { it["name"].asString == name }
    private fun request(v: JsonObject) = DiagnosticControlRequestV1(
        DiagnosticControlCommandV1.valueOf(v["command"].asString), v["message"].asLong,
        v["deviceId"].asString, v["bootId"].asString, v["runId"].takeUnless { it.isJsonNull }?.asString,
        v["grantId"].takeUnless { it.isJsonNull }?.asString, v["ttlMs"].asInt, v["explicitConsent"].asBoolean)
    private fun original() = request(vector("firmware-grant"))
    private fun answer() = hex(vector("firmware-grant")["replyHex"].asString)
    private val origin = DiagnosticControlOriginV1(7, 42, 9, 43, 1, true)
    private fun pending(request: DiagnosticControlRequestV1 = original()) = PendingDiagnosticControlV1(request, origin, 100)
    private fun rejects(action: () -> Unit) { try { action(); fail("Expected invalid DG data") } catch (_: IllegalArgumentException) { } }

    @Test fun fourteenIndependentVectorsMatchEveryByteAndField() {
        assertEquals(14, vectors.size)
        vectors.forEach { v ->
            val expected = request(v)
            val bytes = hex(v["requestHex"].asString)
            assertArrayEquals(bytes, DiagnosticControlV1.encode(expected))
            assertEquals(expected, DiagnosticControlV1.request(bytes))
            val reply = DiagnosticControlV1.correlate(hex(v["replyHex"].asString), expected)
            assertEquals(DiagnosticControlOutcomeV1.valueOf(v["outcome"].asString), reply.outcome)
            assertEquals(v["issuedDeviceMs"].asLong, reply.issuedDeviceMs)
            assertEquals(v["expiresDeviceMs"].asLong, reply.expiresDeviceMs)
            assertEquals(v["replyGrantId"].takeUnless { it.isJsonNull }?.asString, reply.grantId)
            assertTrue(reply.synthetic); assertTrue(reply.controlOnly); assertEquals(60000, reply.maxGrantMs)
        }
    }
    @Test fun unsignedBootAndU32MessageDoNotBecomeNegativeOrTruncated() {
        val extreme = request(vector("unsigned-context"))
        assertEquals(4294967295L, extreme.message); assertEquals("FFFFFFFFFFFFFFFF", extreme.bootId)
        assertEquals(extreme, DiagnosticControlV1.request(DiagnosticControlV1.encode(extreme)))
        val last = DiagnosticControlV1.correlate(hex(vector("last-exact-device-time")["replyHex"].asString), request(vector("last-exact-device-time")))
        assertEquals(9007199254740991L, last.expiresDeviceMs)
    }
    @Test fun lengthsBudgetsAndNoFragmentsAreStrict() {
        for (size in 0..129) {
            if (size != 69) rejects { DiagnosticControlV1.request(DiagnosticControlV1.encode(original()).copyOf(size)) }
            if (size != 88) rejects { DiagnosticControlV1.reply(answer().copyOf(size)) }
        }
        rejects { DiagnosticControlV1.reply(ByteArray(1024 * 1024)) }
        rejects { DiagnosticControlV1.request(ByteArray(1024 * 1024)) }
        rejects { DiagnosticControlV1.requireValueCapacity(87) }
        DiagnosticControlV1.requireValueCapacity(88); DiagnosticControlV1.requireValueCapacity(512)
        rejects { DiagnosticControlV1.reply(answer().copyOfRange(0, 44)) }
        rejects { DiagnosticControlV1.reply(answer() + answer()) }
    }
    @Test fun malformedRequestsAreRejectedWithoutMutatingCallerBytes() {
        val grant = DiagnosticControlV1.encode(original())
        listOf(0 to 0, 1 to 0, 2 to 2, 3 to 0, 3 to 0x82, 38 to 0, 40 to 0, 48 to 1, 68 to 0, 68 to 2).forEach { (offset, value) ->
            val bad = grant.copyOf().apply { this[offset] = value.toByte() }; val saved = bad.copyOf()
            rejects { DiagnosticControlV1.request(bad) }; assertArrayEquals(saved, bad)
        }
        listOf(4..7, 8..23, 24..31, 64..67).forEach { range -> val bad = grant.copyOf().apply { range.forEach { this[it] = 0 } }; rejects { DiagnosticControlV1.request(bad) } }
        val capability = DiagnosticControlV1.encode(request(vector("capability")))
        listOf(32, 48, 64, 68).forEach { at -> rejects { DiagnosticControlV1.request(capability.copyOf().apply { this[at] = 1 }) } }
        val cancel = DiagnosticControlV1.encode(request(vector("cancel")))
        listOf(64, 68).forEach { at -> rejects { DiagnosticControlV1.request(cancel.copyOf().apply { this[at] = 1 }) } }
        rejects { DiagnosticControlV1.request(grant.copyOf().apply { this[64] = 0x61; this[65] = 0xea.toByte() }) }
    }
    @Test fun invalidRequestObjectsCannotBeEncoded() {
        val valid = original()
        listOf(valid.copy(message = 0), valid.copy(message = 4294967296L), valid.copy(deviceId = "0".repeat(32)),
            valid.copy(bootId = "0".repeat(16)), valid.copy(deviceId = "f".repeat(32)),
            valid.copy(runId = "00000000-0000-5000-8000-000000000002"), valid.copy(grantId = "00000000-0000-4000-8000-00000000000b"),
            valid.copy(ttlMs = 0), valid.copy(ttlMs = 60001), valid.copy(explicitConsent = false)).forEach { bad -> rejects { DiagnosticControlV1.encode(bad) } }
    }
    @Test fun malformedRepliesUnsupportedFlagsAndImpossibleTimesFailClosed() {
        listOf(0 to 0, 1 to 0, 2 to 2, 3 to 2, 3 to 0x84, 38 to 0, 40 to 0, 54 to 0,
            56 to 0, 64 to 8, 65 to 1, 65 to 2, 65 to 7, 66 to 2, 67 to 1, 84 to 0).forEach { (at, byte) ->
            rejects { DiagnosticControlV1.reply(answer().apply { this[at] = byte.toByte() }) }
        }
        listOf(4..7, 8..23, 24..31, 48..63, 76..83).forEach { range -> rejects { DiagnosticControlV1.reply(answer().apply { range.forEach { this[it] = 0 } }) } }
        rejects { DiagnosticControlV1.reply(answer().apply { this[75] = 0x80.toByte() }) }
        rejects { DiagnosticControlV1.reply(answer().apply { this[83] = 0x80.toByte() }) }
        rejects { DiagnosticControlV1.reply(answer().apply { this[68] = 0xff.toByte(); this[69] = 0xff.toByte() }) }
        val capability = hex(vector("capability")["replyHex"].asString)
        listOf(32, 48, 68, 76).forEach { at -> rejects { DiagnosticControlV1.reply(capability.copyOf().apply { this[at] = 1 }) } }
        val denied = hex(vector("grant-permission_denied")["replyHex"].asString)
        listOf(48, 68, 76).forEach { at -> rejects { DiagnosticControlV1.reply(denied.copyOf().apply { this[at] = 1 }) } }
    }
    @Test fun everyRequestCorrelationFieldAndGrantTtlIsRequired() {
        val valid = original()
        listOf(valid.copy(message = 2), valid.copy(deviceId = "F".repeat(32)), valid.copy(bootId = "FFFFFFFFFFFFFFFF"),
            valid.copy(runId = "ffffffff-ffff-4fff-8fff-ffffffffffff"), valid.copy(ttlMs = 9999)).forEach { changed -> rejects { DiagnosticControlV1.correlate(answer(), changed) } }
        rejects { DiagnosticControlV1.correlate(hex(vector("capability")["replyHex"].asString), valid) }
        val cancel = request(vector("cancel")); val differentGrant = cancel.copy(grantId = "ffffffff-ffff-4fff-8fff-ffffffffffff")
        rejects { DiagnosticControlV1.correlate(hex(vector("cancel")["replyHex"].asString), differentGrant) }
    }
    @Test fun originalFullMetadataRequiredAtCaptureAndAtDelivery() {
        val replacements = listOf(origin.copy(localGattGeneration = 8), origin.copy(connectionToken = 43), origin.copy(connectionHandle = 10),
            origin.copy(authorityEpoch = 44), origin.copy(authorityPrincipal = 2), origin.copy(gameMaster = false))
        replacements.forEach { replacement ->
            val captured = pending()
            assertNull(captured.receive(answer(), replacement, origin, 110, 110))
            assertEquals(DiagnosticControlPendingStateV1.PENDING, captured.state)
            assertNotNull(captured.receive(answer(), origin, origin, 110, 110))
            val current = pending()
            assertNull(current.receive(answer(), origin, replacement, 110, 110))
            assertEquals(DiagnosticControlPendingStateV1.INVALIDATED, current.state)
            assertNull(current.receive(answer(), origin, origin, 110, 110))
        }
        val disconnected = pending(); disconnected.invalidate(null)
        assertNull(disconnected.receive(answer(), origin, origin, 110, 110))
    }
    @Test fun deadlineIncludesQueueTimeAndRejectsFutureOrEarlyReceipt() {
        val last = pending(); assertNotNull(last.receive(answer(), origin, origin, 2099, 2099))
        val expired = pending(); assertNull(expired.receive(answer(), origin, origin, 110, 2100)); assertEquals(DiagnosticControlPendingStateV1.TIMEOUT, expired.state)
        val notBorn = pending(); assertNull(notBorn.receive(answer(), origin, origin, 99, 110))
        val future = pending(); assertNull(future.receive(answer(), origin, origin, 111, 110))
        val movedBack = pending(); movedBack.tick(99); assertEquals(DiagnosticControlPendingStateV1.TIMEOUT, movedBack.state)
    }
    @Test fun duplicateLateInvalidAndLocallyCancelledRepliesCannotReactivateRequest() {
        val once = pending(); assertNotNull(once.receive(answer(), origin, origin, 110, 110))
        assertNull(once.receive(answer(), origin, origin, 110, 110))
        val invalid = pending(); assertNull(invalid.receive(answer().copyOf(87), origin, origin, 110, 110))
        assertEquals(DiagnosticControlPendingStateV1.PENDING, invalid.state)
        assertNotNull(invalid.receive(answer(), origin, origin, 110, 110))
        val cancelled = pending(); cancelled.cancel(); assertNull(cancelled.receive(answer(), origin, origin, 110, 110))
        assertEquals(DiagnosticControlPendingStateV1.CANCELLED, cancelled.state)
    }
    @Test fun acceptedCancelIsOnlyControlAndHasNoPhysicalOffResult() {
        val cancel = request(vector("cancel"))
        val result = pending(cancel).receive(hex(vector("cancel")["replyHex"].asString), origin, origin, 110, 110)!!
        assertEquals(DiagnosticControlOutcomeV1.ACCEPTED, result.outcome)
        assertTrue(result.synthetic); assertTrue(result.controlOnly)
        assertEquals(0L, result.issuedDeviceMs); assertEquals(0L, result.expiresDeviceMs)
        assertEquals(cancel.grantId, result.grantId)
    }
}
