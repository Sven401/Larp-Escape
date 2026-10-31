package de.larpescape.companion.platform.ble

import de.larpescape.protocol.NearGlowFrameAssembler
import org.junit.Assert.*
import org.junit.Test
import java.io.File
import java.nio.file.Files

class NearGlowCallbackDispatcherTest {
    private class Queue {
        val pending = ArrayDeque<() -> Unit>()
        fun post(action: () -> Unit) { pending.addLast(action) }
        fun drain() { while (pending.isNotEmpty()) pending.removeFirst().invoke() }
    }

    @Test fun sdkDisconnectFailedAcksAndMalformedNotifyStayInsideTheProductionBoundary() {
        val failures: List<() -> Unit> = listOf(
            { check(false) { "SDK disconnected" } },
            { check(false) { "SDK status failure" } },
            { check(false) { "Service discovery failed" } },
            { check(false) { "Descriptor ACK failed" } },
            { check(false) { "Write ACK failed or unexpected" } },
            { NearGlowFrameAssembler().accept(byteArrayOf(1), 0); Unit },
            { throw SecurityException("Permission revoked during SDK call") },
        )
        for (failure in failures) {
            val queue = Queue(); var active = true; var errors = 0; var succeeded = false
            var reported: Exception? = null
            var reportedStage: NearGlowCallbackStage? = null
            val dispatcher = NearGlowCallbackDispatcher(queue::post, { active }, { true }, { stage, error ->
                reportedStage = stage; reported = error; errors++; active = false
            })
            dispatcher.dispatch(NearGlowCallbackStage.NOTIFY) { failure(); succeeded = true }
            assertEquals(0, errors)
            queue.drain()
            assertEquals(1, errors); assertFalse(succeeded); assertFalse(active)
            assertNotNull(reported); assertEquals(NearGlowCallbackStage.NOTIFY, reportedStage)
        }
    }

    @Test fun permissionAndSessionIdentityAreCheckedAfterQueueingAndOldCallbacksCannotCloseReplacement() {
        val queue = Queue(); val old = Any(); val replacement = Any()
        var current: Any? = old; var permitted = true; var errors = 0; var handled = 0
        val dispatcher = NearGlowCallbackDispatcher(queue::post, { current === old }, { permitted }, { _, _ -> errors++; current = null })
        dispatcher.dispatch(NearGlowCallbackStage.CONNECTION) { handled++ }
        current = replacement
        queue.drain()
        assertSame(replacement, current); assertEquals(0, handled); assertEquals(0, errors)
        current = old
        dispatcher.dispatch(NearGlowCallbackStage.WRITE) { handled++ }
        current = null
        queue.drain()
        assertEquals(0, handled); assertEquals(0, errors)
        current = old
        dispatcher.dispatch(NearGlowCallbackStage.DESCRIPTOR) { handled++ }
        permitted = false
        queue.drain()
        assertEquals(0, handled); assertEquals(1, errors); assertNull(current)
    }

    @Test fun failureInBLeavesAAliveAndLateBCallbacksDoNotRunOrSendAnything() {
        val queue = Queue(); val active = booleanArrayOf(true, true)
        val errors = intArrayOf(0, 0); val operations = mutableListOf<String>()
        fun dispatcher(slot: Int) = NearGlowCallbackDispatcher(queue::post, { active[slot] }, { true }, { _, _ ->
            errors[slot]++; active[slot] = false
        })
        val a = dispatcher(0); val b = dispatcher(1)
        b.dispatch(NearGlowCallbackStage.CONNECTION) { check(false) }
        b.dispatch(NearGlowCallbackStage.WRITE) { operations.add("SET") }
        a.dispatch(NearGlowCallbackStage.NOTIFY) { operations.add("A status") }
        queue.drain()
        assertTrue(active[0]); assertFalse(active[1]); assertArrayEquals(intArrayOf(0, 1), errors)
        assertEquals(listOf("A status"), operations)
    }

    @Test fun debugDiagnosticsIncludeOnlyStageSlotTypeAndBoundedCodeFrames() {
        val privateText = "PRIVATE_FRAME_KEY_AND_MESSAGE"
        val error = IllegalStateException(privateText, IllegalArgumentException(privateText))
        error.addSuppressed(SecurityException(privateText))
        error.stackTrace = Array(20) { index -> StackTraceElement("FixedClass", "decode", "FixedFile.kt", index) }
        val line = NearGlowCallbackDiagnostics.line(1, NearGlowCallbackStage.NOTIFY, error)
        assertTrue(line.startsWith("slot=1 stage=NOTIFY exception=java.lang.IllegalStateException"))
        assertTrue(line.contains("FixedClass.decode:11")); assertFalse(line.contains("FixedClass.decode:12"))
        assertFalse(line.contains(privateText)); assertFalse(line.contains("IllegalArgumentException"))
        assertFalse(line.contains("SecurityException")); assertFalse(line.contains("FixedFile.kt"))
    }

    @Test fun handshakeMilestonesContainOnlyEnumsCountsAndMonotonicElapsedTime() {
        for (event in NearGlowHandshakeMilestone.entries) {
            assertEquals("slot=1 milestone=${event.name} elapsedMs=750 fragments=8",
                NearGlowCallbackDiagnostics.milestone(1, event, 1000, 1750, 8))
        }
        assertThrows(IllegalArgumentException::class.java) {
            NearGlowCallbackDiagnostics.milestone(1, NearGlowHandshakeMilestone.AUTH_OK, 1000, 999, 8)
        }
        assertThrows(IllegalArgumentException::class.java) {
            NearGlowCallbackDiagnostics.milestone(0, NearGlowHandshakeMilestone.PROOF_QUEUED, 1000, 1000, -1)
        }
    }

    @Test fun rejectedWriteMetadataSeparatesSdkStatusQueueAndIdentityWithoutPayload() {
        assertEquals("slot=1 event=WRITE_REJECTED sdkStatus=133 writing=true queued=2 connectionMatches=true inputMatches=true",
            NearGlowCallbackDiagnostics.writeRejected(1, 133, true, 2, true, true))
        assertEquals("slot=0 event=WRITE_REJECTED sdkStatus=0 writing=false queued=0 connectionMatches=false inputMatches=false",
            NearGlowCallbackDiagnostics.writeRejected(0, 0, false, 0, false, false))
    }

    @Test fun everyCompiledAndroidGattCallbackReallyInvokesTheExplicitSessionDispatcher() {
        // Guard-only tests miss kotlin.run shadowing: inspect the actual emitted app callbacks.
        val binaryName = "de.larpescape.companion.platform.ble.AndroidNearGlowRepository\$Session\$callback\$1"
        val resourceName = binaryName.replace('.', '/') + ".class"
        val resource = requireNotNull(javaClass.classLoader!!.getResource(resourceName))
        // AGP may expose app classes from a jar, not a file URL. Copy those exact
        // compiled bytes so javap sees the same production class on either host.
        val root = Files.createTempDirectory("nearglow-callback-binding-").toFile()
        try {
            val classFile = File(root, resourceName)
            check(requireNotNull(classFile.parentFile).mkdirs())
            resource.openStream().use { source -> classFile.outputStream().use { source.copyTo(it) } }
            val executable = File(requireNotNull(System.getProperty("java.home")), "bin/" +
                if (System.getProperty("os.name").orEmpty().startsWith("Windows")) "javap.exe" else "javap")
            check(executable.isFile) { "JDK javap is required for callback binding regression" }
            val process = ProcessBuilder(executable.absolutePath, "-p", "-c", "-classpath", root.absolutePath, binaryName)
                .redirectErrorStream(true).start()
            val bytecode = process.inputStream.bufferedReader().use { it.readText() }
            assertEquals(bytecode, 0, process.waitFor())
            val methods = Regex("(?m)^  (?:public|private|protected|static).*\\([^\\n]*\\);\\r?$")
                .findAll(bytecode).toList()
        for (callback in listOf("onConnectionStateChange", "onMtuChanged", "onServicesDiscovered", "onDescriptorWrite",
                "onCharacteristicWrite", "onCharacteristicChanged")) {
                val index = methods.indexOfFirst { it.value.contains(" $callback(") }
                assertTrue("Actual compiled callback missing: $callback", index >= 0)
                val end = methods.getOrNull(index + 1)?.range?.first ?: bytecode.length
                val body = bytecode.substring(methods[index].range.first, end)
                assertTrue("$callback bypasses the production exception/session guard:\n$body",
                    body.contains("access\$dispatchCallback:"))
                assertFalse("$callback still inlines unchecked failure", body.contains("java/lang/IllegalStateException"))
            }
        } finally {
            check(root.deleteRecursively())
        }
    }
}
