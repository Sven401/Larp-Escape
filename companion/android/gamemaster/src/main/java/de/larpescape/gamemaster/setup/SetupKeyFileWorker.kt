package de.larpescape.gamemaster.setup

import de.larpescape.protocol.SetupKeyFileV1
import java.io.InputStream
import java.io.OutputStream
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.withContext

/** Owns file work and imported keys, never box/admin/GM authority.
 *  Stream factories must open the document selected for this invocation afresh.
 *  This class does not select documents, cache keys, or confirm enrollment.
 *  A blocking provider may finish after cancellation; its result is then closed.
 *  No claim is made that arbitrary provider IO/KDF can be interrupted promptly.
 */
internal class SetupKeyFileWorker(private val dispatcher: CoroutineDispatcher = Dispatchers.IO) {
    suspend fun createAndWrite(
        box: ByteArray,
        password: CharArray,
        openOutput: () -> OutputStream,
    ): SetupKeyFileV1.Metadata = withOwnedInputs(box, password) { ownedBox, ownedPassword ->
        withContext(dispatcher) {
            currentCoroutineContext().ensureActive()
            val prepared = SetupKeyFileV1.create(ownedBox, ownedPassword)
            val bytes = prepared.encodedFile()
            try {
                currentCoroutineContext().ensureActive()
                openOutput().use { output ->
                    currentCoroutineContext().ensureActive()
                    output.write(bytes)
                    output.flush()
                }
                currentCoroutineContext().ensureActive()
                // Only ciphertext/public data survived generation. This is not
                // proof that a user can reopen the saved document or a box.
                prepared.metadata
            } finally {
                bytes.fill(0)
            }
        }
    }

    /** The imported private-key owner stays inside this scope. Any escaped
     *  Opened reference is closed before return, exception, or cancellation.
     *  A future box-challenge consumer needs its separately qualified protocol.
     */
    suspend fun <T> useOpened(
        box: ByteArray,
        password: CharArray,
        openInput: () -> InputStream,
        consume: suspend (SetupKeyFileV1.Opened) -> T,
    ): T = withOwnedInputs(box, password) { ownedBox, ownedPassword ->
        withContext(dispatcher) {
            currentCoroutineContext().ensureActive()
            // Close the actual document stream BEFORE importing a key.
            val bytes = openInput().use { readBounded(it) }
            var opened: SetupKeyFileV1.Opened? = null
            try {
                currentCoroutineContext().ensureActive()
                opened = SetupKeyFileV1.open(bytes, ownedPassword, ownedBox)
                // Coroutine cancellation is independent of the JCA thread's
                // interrupt flag. Never deliver a late key to the consumer.
                currentCoroutineContext().ensureActive()
                consume(opened)
            } finally {
                opened?.close()
                bytes.fill(0)
            }
        }
    }

    private suspend fun readBounded(input: InputStream): ByteArray {
        val buffer = ByteArray(SetupKeyFileV1.MAX_BYTES + 1)
        var size = 0
        try {
            while (size < buffer.size) {
                currentCoroutineContext().ensureActive()
                val count = input.read(buffer, size, buffer.size - size)
                if (count < 0) break
                if (count == 0) {
                    val byte = input.read()
                    if (byte < 0) break
                    buffer[size++] = byte.toByte()
                } else {
                    size += count
                }
            }
            if (size == 0 || size > SetupKeyFileV1.MAX_BYTES)
                throw SetupKeyFileV1.Failure(SetupKeyFileV1.Reason.FORMAT)
            return buffer.copyOf(size)
        } finally {
            buffer.fill(0)
        }
    }

    private suspend fun <T> withOwnedInputs(
        box: ByteArray,
        password: CharArray,
        action: suspend (ByteArray, CharArray) -> T,
    ): T {
        if (box.size != 16) throw SetupKeyFileV1.Failure(SetupKeyFileV1.Reason.BOX)
        if (password.size > 256)
            throw SetupKeyFileV1.Failure(SetupKeyFileV1.Reason.FILE_OR_PASSWORD)
        // Copy before the first suspension; the caller keeps ownership.
        val ownedBox = box.copyOf()
        val ownedPassword = password.copyOf()
        try {
            return action(ownedBox, ownedPassword)
        } finally {
            ownedPassword.fill('\u0000')
            ownedBox.fill(0)
        }
    }
}
