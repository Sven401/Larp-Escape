package de.larpescape.gamemaster

import de.larpescape.gamemaster.setup.SetupKeyFileWorker
import de.larpescape.protocol.SetupKeyFileV1
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.security.Key
import java.security.KeyFactory
import java.security.KeyFactorySpi
import java.security.PrivateKey
import java.security.Provider
import java.security.PublicKey
import java.security.Security
import java.security.interfaces.ECPrivateKey
import java.security.spec.KeySpec
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicReference
import javax.crypto.SecretKey
import javax.crypto.SecretKeyFactory
import javax.crypto.SecretKeyFactorySpi
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.awaitCancellation
import kotlinx.coroutines.async
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import org.junit.Assert.*
import org.junit.Test

class SetupKeyFileWorkerTest {
    private val worker = SetupKeyFileWorker()
    private val box = ByteArray(16) { 7 }
    private val password = "synthetic-worker-passphrase-123".toCharArray()
    private fun file() = SetupKeyFileV1.create(box, password).encodedFile()

    private class ReadFile(bytes: ByteArray) : ByteArrayInputStream(bytes) {
        var closed = false
        override fun close() { closed = true; super.close() }
    }

    @Test fun writeClosesThenFreshReadImportsOnlyWithinOwnedScope() = runBlocking {
        var outputClosed = false
        val output = object : ByteArrayOutputStream() {
            override fun close() { outputClosed = true; super.close() }
        }
        val metadata = worker.createAndWrite(box, password) { output }
        assertTrue(outputClosed)
        val input = ReadFile(output.toByteArray())
        var imported: SetupKeyFileV1.Opened? = null
        val reopened = worker.useOpened(box, password, { input }) {
            assertTrue(input.closed)
            assertFalse(it.isClosed())
            imported = it
            it.metadata
        }
        assertTrue(requireNotNull(imported).isClosed())
        assertArrayEquals(metadata.keyId(), reopened.keyId())
        assertArrayEquals(metadata.boxId(), reopened.boxId())
        assertArrayEquals("synthetic-worker-passphrase-123".toCharArray(), password)
    }

    @Test fun callbackFailureClosesImportedOwner() = runBlocking {
        val input = ReadFile(file())
        var imported: SetupKeyFileV1.Opened? = null
        try {
            worker.useOpened(box, password, { input }) {
                imported = it
                throw IOException("synthetic consumer failure")
            }
            fail("consumer failure must propagate")
        } catch (_: IOException) { }
        assertTrue(input.closed)
        assertTrue(requireNotNull(imported).isClosed())
    }

    @Test fun cancellationDuringConsumerClosesImportedOwner() = runBlocking {
        val bytes = file()
        val entered = CompletableDeferred<Unit>()
        val imported = AtomicReference<SetupKeyFileV1.Opened>()
        val job = launch(Dispatchers.Default) {
            worker.useOpened(box, password, { ReadFile(bytes) }) {
                imported.set(it)
                entered.complete(Unit)
                awaitCancellation()
            }
        }
        withTimeout(15_000) { entered.await() }
        job.cancelAndJoin()
        assertTrue(imported.get().isClosed())
    }

    @Test fun oversizedDocumentStopsAtBoundAndClosesWithoutConsumer() = runBlocking {
        var reads = 0
        var closed = false
        var consumed = false
        val input = object : InputStream() {
            override fun read(): Int { reads++; return 65 }
            override fun close() { closed = true }
        }
        try {
            worker.useOpened(box, password, { input }) { consumed = true }
            fail("oversized file accepted")
        } catch (error: SetupKeyFileV1.Failure) {
            assertEquals(SetupKeyFileV1.Reason.FORMAT, error.reason)
        }
        assertEquals(SetupKeyFileV1.MAX_BYTES + 1, reads)
        assertTrue(closed)
        assertFalse(consumed)
    }

    // Actual JCA work returns late, independently of coroutine cancellation.
    // Counters observe a real imported key's destroy request; they do not claim
    // forensic deletion by SunEC or Android providers.
    class TrackedKey(private val actual: ECPrivateKey) : ECPrivateKey by actual {
        val closed = AtomicBoolean()
        override fun destroy() { closed.set(true); runCatching { actual.destroy() } }
        override fun isDestroyed(): Boolean = closed.get()
    }
    class TrackedEc : KeyFactorySpi() {
        private val actual = KeyFactory.getInstance("EC", "SunEC")
        override fun engineGeneratePublic(spec: KeySpec): PublicKey = actual.generatePublic(spec)
        override fun engineGeneratePrivate(spec: KeySpec): PrivateKey =
            TrackedKey(actual.generatePrivate(spec) as ECPrivateKey).also { keys.add(it) }
        override fun <T : KeySpec> engineGetKeySpec(key: Key, type: Class<T>): T = actual.getKeySpec(key, type)
        override fun engineTranslateKey(key: Key): Key = actual.translateKey(key)
    }
    class DelayedKdf : SecretKeyFactorySpi() {
        private val actual = SecretKeyFactory.getInstance("PBKDF2WithHmacSHA256", "SunJCE")
        override fun engineGenerateSecret(spec: KeySpec): SecretKey {
            val result = actual.generateSecret(spec)
            derived.countDown()
            check(release.await(15, TimeUnit.SECONDS))
            return result
        }
        override fun engineGetKeySpec(key: SecretKey, type: Class<*>): KeySpec = actual.getKeySpec(key, type)
        override fun engineTranslateKey(key: SecretKey): SecretKey = actual.translateKey(key)
    }
    class AuditProvider : Provider("SetupWorkerTestAudit", 1.0, "JVM cancellation audit only") {
        init {
            put("KeyFactory.EC", TrackedEc::class.java.name)
            put("SecretKeyFactory.PBKDF2WithHmacSHA256", DelayedKdf::class.java.name)
        }
    }
    companion object {
        private var derived = CountDownLatch(1)
        private var release = CountDownLatch(1)
        private val keys = ConcurrentLinkedQueue<TrackedKey>()
    }

    @Test fun cancelledLateKdfResultIsClosedAndNeverDelivered() = runBlocking {
        val bytes = file()
        derived = CountDownLatch(1)
        release = CountDownLatch(1)
        keys.clear()
        val audit = AuditProvider()
        Security.insertProviderAt(audit, 1)
        val delivered = AtomicBoolean()
        val input = ReadFile(bytes)
        val job = launch(Dispatchers.Default) {
            worker.useOpened(box, password, { input }) { delivered.set(true) }
        }
        try {
            assertTrue(derived.await(15, TimeUnit.SECONDS))
            job.cancel()
            release.countDown()
            withTimeout(15_000) { job.join() }
            assertTrue(job.isCancelled)
            assertTrue(input.closed)
            assertFalse(delivered.get())
            assertEquals(1, keys.size)
            assertTrue(keys.single().closed.get())
        } finally {
            release.countDown()
            job.cancelAndJoin()
            Security.removeProvider(audit.name)
            keys.clear()
        }
    }

    @Test fun outputWriteFlushAndCloseFailuresNeverReportSuccess() = runBlocking {
        for (failure in listOf("write", "flush", "close")) {
            var closed = false
            val output = object : OutputStream() {
                override fun write(byte: Int) { }
                override fun write(bytes: ByteArray, offset: Int, length: Int) {
                    if (failure == "write") throw IOException("synthetic write failure")
                }
                override fun flush() {
                    if (failure == "flush") throw IOException("synthetic flush failure")
                }
                override fun close() {
                    closed = true
                    if (failure == "close") throw IOException("synthetic close failure")
                }
            }
            try {
                worker.createAndWrite(box, password) { output }
                fail("failed output reported success")
            } catch (_: IOException) { }
            assertTrue(closed)
        }
    }

    @Test fun inputReadAndCloseFailuresNeverDeliverImportedOwner() = runBlocking {
        val bytes = file()
        for (failure in listOf("read", "close")) {
            var closed = false
            var delivered = false
            val input = object : ByteArrayInputStream(bytes) {
                override fun read(buffer: ByteArray, offset: Int, length: Int): Int {
                    if (failure == "read") throw IOException("synthetic read failure")
                    return super.read(buffer, offset, length)
                }
                override fun close() {
                    closed = true
                    if (failure == "close") throw IOException("synthetic close failure")
                    super.close()
                }
            }
            try {
                worker.useOpened(box, password, { input }) { delivered = true }
                fail("failed input reported success")
            } catch (_: IOException) { }
            assertTrue(closed)
            assertFalse(delivered)
        }
    }

    @Test fun cancellationDuringBlockingReadClosesWhenProviderReturns() = runBlocking {
        val bytes = file()
        val reading = CountDownLatch(1)
        val unblock = CountDownLatch(1)
        val closed = AtomicBoolean()
        val delivered = AtomicBoolean()
        val input = object : ByteArrayInputStream(bytes) {
            override fun read(buffer: ByteArray, offset: Int, length: Int): Int {
                reading.countDown()
                check(unblock.await(15, TimeUnit.SECONDS))
                return super.read(buffer, offset, length)
            }
            override fun close() { closed.set(true); super.close() }
        }
        val job = launch(Dispatchers.Default) {
            worker.useOpened(box, password, { input }) { delivered.set(true) }
        }
        try {
            assertTrue(reading.await(15, TimeUnit.SECONDS))
            job.cancel()
            unblock.countDown()
            withTimeout(15_000) { job.join() }
            assertTrue(job.isCancelled)
            assertTrue(closed.get())
            assertFalse(delivered.get())
        } finally {
            unblock.countDown()
            job.cancelAndJoin()
        }
    }

    @Test fun callerMutationDuringReadDoesNotChangeOwnedBoxOrPassword() = runBlocking {
        val bytes = file()
        val callerBox = box.copyOf()
        val callerPassword = password.copyOf()
        val reading = CountDownLatch(1)
        val unblock = CountDownLatch(1)
        val input = object : ByteArrayInputStream(bytes) {
            override fun read(buffer: ByteArray, offset: Int, length: Int): Int {
                reading.countDown()
                check(unblock.await(15, TimeUnit.SECONDS))
                return super.read(buffer, offset, length)
            }
        }
        val task = async(Dispatchers.Default) {
            worker.useOpened(callerBox, callerPassword, { input }) { it.metadata }
        }
        try {
            assertTrue(reading.await(15, TimeUnit.SECONDS))
            callerBox.fill(0)
            callerPassword.fill('x')
            unblock.countDown()
            val metadata = withTimeout(15_000) { task.await() }
            assertArrayEquals(box, metadata.boxId())
            assertTrue(callerBox.all { it == 0.toByte() })
            assertTrue(callerPassword.all { it == 'x' })
        } finally {
            unblock.countDown()
            task.cancelAndJoin()
        }
    }
}
