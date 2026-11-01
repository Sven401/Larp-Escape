package de.larpescape.protocol

import com.google.gson.JsonParser
import java.util.Base64
import java.security.Key
import java.security.KeyFactory
import java.security.KeyFactorySpi
import java.security.PrivateKey
import java.security.Provider
import java.security.PublicKey
import java.security.Security
import java.security.spec.KeySpec
import javax.crypto.SecretKey
import javax.crypto.SecretKeyFactory
import javax.crypto.SecretKeyFactorySpi
import org.junit.Assert.*
import org.junit.Test

/** JVM/provider tests of the unpublished candidate, no Android or box qualification. */
class SetupKeyFileV1Test {
    // Instrumentation only: production deliberately does not pin either provider.
    class CountedEc : KeyFactorySpi() {
        private val actual = KeyFactory.getInstance("EC", "SunEC")
        override fun engineGeneratePublic(spec: KeySpec): PublicKey = actual.generatePublic(spec)
        override fun engineGeneratePrivate(spec: KeySpec): PrivateKey {
            privateImports++
            return actual.generatePrivate(spec)
        }
        override fun <T : KeySpec> engineGetKeySpec(key: Key, type: Class<T>): T = actual.getKeySpec(key, type)
        override fun engineTranslateKey(key: Key): Key = actual.translateKey(key)
    }
    class CountedKdf : SecretKeyFactorySpi() {
        private val actual = SecretKeyFactory.getInstance("PBKDF2WithHmacSHA256", "SunJCE")
        override fun engineGenerateSecret(spec: KeySpec): SecretKey {
            kdfs++
            val result = actual.generateSecret(spec)
            afterKdf?.invoke()
            return result
        }
        override fun engineGetKeySpec(key: SecretKey, type: Class<*>): KeySpec = actual.getKeySpec(key, type)
        override fun engineTranslateKey(key: SecretKey): SecretKey = actual.translateKey(key)
    }
    class AuditProvider : Provider("SetupCandidateTestAudit", "1.0", "JVM test counters only") {
        init {
            put("KeyFactory.EC", CountedEc::class.java.name)
            put("SecretKeyFactory.PBKDF2WithHmacSHA256", CountedKdf::class.java.name)
        }
    }
    companion object {
        private var privateImports = 0
        private var kdfs = 0
        private var afterKdf: (() -> Unit)? = null
    }
    private val fixtures get() = requireNotNull(javaClass.getResourceAsStream("/setup-key-candidate/vectors.json"))
        .bufferedReader().use { JsonParser.parseReader(it).asJsonObject }
    private fun hex(text: String) = text.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
    private val box get() = hex(fixtures["box"].asString)
    private fun reject(reason: SetupKeyFileV1.Reason? = null, block: () -> Unit) {
        try { block(); fail("Expected closed rejection") }
        catch (error: SetupKeyFileV1.Failure) { if (reason != null) assertEquals(reason, error.reason) }
    }
    private fun original() = fixtures["cases"].asJsonArray[0].asJsonObject
    private fun editHeader(file: ByteArray, change: (String) -> String): ByteArray {
        val parts = file.toString(Charsets.US_ASCII).split('.').toMutableList()
        val text = Base64.getUrlDecoder().decode(parts[0]).toString(Charsets.UTF_8)
        parts[0] = Base64.getUrlEncoder().withoutPadding().encodeToString(change(text).toByteArray(Charsets.UTF_8))
        return parts.joinToString(".").toByteArray(Charsets.US_ASCII)
    }

    @Test fun independentPythonAesKwGcmUtf8AndExactDerFixtures() {
        for (item in fixtures["cases"].asJsonArray) {
            val row = item.asJsonObject
            val file = row["file"].asString.toByteArray(Charsets.US_ASCII)
            val password = row["password"].asString.toCharArray()
            if (row["accepted"].asBoolean) {
                SetupKeyFileV1.open(file, password, box).use {
                    assertFalse(it.isClosed())
                    assertArrayEquals(hex(fixtures["xy"].asString), it.metadata.publicKey())
                    assertArrayEquals(hex(fixtures["kid"].asString), it.metadata.keyId())
                }
            } else reject { SetupKeyFileV1.open(file, password, box).close() }
        }
    }

    @Test fun realGenerationCiphertextOnlyReimportAndOwnedPublicCopies() {
        val password = "public-generation-password-123".toCharArray()
        val savedPass = password.copyOf()
        val inputBox = box
        val prepared = SetupKeyFileV1.create(inputBox, password)
        assertArrayEquals(savedPass, password)
        val file = prepared.encodedFile()
        val savedFile = file.copyOf()
        inputBox.fill(0)
        prepared.encodedFile().fill(0)
        prepared.metadata.publicKey().fill(0)
        val opened = SetupKeyFileV1.open(file, password, box)
        assertArrayEquals(savedFile, file)
        assertArrayEquals(savedPass, password)
        assertArrayEquals(prepared.metadata.publicKey(), opened.metadata.publicKey())
        file.fill(0)
        password.fill('\u0000')
        assertFalse(opened.isClosed())
        opened.close()
        opened.close()
        assertTrue(opened.isClosed())
    }

    @Test fun boundsStrictJsonBase64AndContextStayClosed() {
        val row = original()
        val file = row["file"].asString.toByteArray(Charsets.US_ASCII)
        val pass = row["password"].asString.toCharArray()
        val changes: List<(String) -> String> = listOf(
            { it.replace("600000", "600001") }, { it.replace("600000", "600000.0") },
            { it.replace("600000", "6e5") }, { it.replace("600000", "\"600000\"") },
            { it.replace("\"alg\":", "\"unknown\":") }, { it + "{}" }, { "\ufeff" + it },
            { it.dropLast(1) + ",\"alg\":\"PBES2-HS256+A128KW\"}" },
            { it.replace("\"kty\":\"EC\"", "\"kty\":\"EC\",\"\\u006bty\":\"EC\"") },
            { it.replace("\"crv\":\"P-256\"", "\"crv\":\"P-384\"") },
            { it.replace("PBES2-HS256+A128KW", "PBES2-HS512+A256KW") }
        )
        for (change in changes) reject(SetupKeyFileV1.Reason.FORMAT) {
            SetupKeyFileV1.open(editHeader(file, change), pass, box).close()
        }
        for (bad in listOf(byteArrayOf(), ByteArray(4097), file + 10.toByte(),
            (" " + file.toString(Charsets.US_ASCII)).toByteArray(), file + '.'.code.toByte()))
            reject(SetupKeyFileV1.Reason.FORMAT) { SetupKeyFileV1.open(bad, pass, box).close() }
        val parts = file.toString(Charsets.US_ASCII).split('.')
        for (i in parts.indices) {
            val padded = parts.toMutableList().also { it[i] += "=" }.joinToString(".").toByteArray()
            reject(SetupKeyFileV1.Reason.FORMAT) { SetupKeyFileV1.open(padded, pass, box).close() }
        }
        reject(SetupKeyFileV1.Reason.BOX) { SetupKeyFileV1.open(file, pass, ByteArray(16) { 2 }).close() }
        reject { SetupKeyFileV1.open(file, "wrong-password-123456".toCharArray(), box).close() }
        // Syntactically valid original header mutation must fail GCM AAD authentication.
        reject(SetupKeyFileV1.Reason.FILE_OR_PASSWORD) {
            SetupKeyFileV1.open(editHeader(file) { it.replace(",", ", ") }, pass, box).close()
        }
    }

    @Test fun passwordAndPreexistingCancellationNeverProduceAnOwner() {
        for (pass in listOf("short".toCharArray(), CharArray(129) { 'a' },
            ("123456789012345" + '\ud800').toCharArray(), CharArray(65) { 'ü' }))
            reject(SetupKeyFileV1.Reason.FILE_OR_PASSWORD) { SetupKeyFileV1.create(box, pass) }
        try {
            Thread.currentThread().interrupt()
            reject(SetupKeyFileV1.Reason.CANCELLED) {
                SetupKeyFileV1.create(box, "public-test-password-123".toCharArray())
            }
        } finally { Thread.interrupted() }
        assertFalse(Thread.currentThread().isInterrupted)
    }

    @Test fun allShorterPrefixesRejectBeforeKdfAndBadTagBeforePrivateImport() {
        val row = original()
        val file = row["file"].asString.toByteArray(Charsets.US_ASCII)
        val pass = row["password"].asString.toCharArray()
        val provider = AuditProvider()
        assertEquals(1, Security.insertProviderAt(provider, 1))
        try {
            privateImports = 0
            kdfs = 0
            for (length in 0 until file.size) reject(SetupKeyFileV1.Reason.FORMAT) {
                SetupKeyFileV1.open(file.copyOf(length), pass, box).close()
            }
            val hostileHeaders = listOf(
                editHeader(file) { it.replace("PBES2-HS256+A128KW", "PBES2-HS256+\nA128KW") },
                editHeader(file) { it.replace("\"alg\"", "\"al\\ud800g\"") },
                editHeader(file) { it.replace("\"enc\":\"A256GCM\"", "\"enc\":null") },
                editHeader(file) { it.replace("600000", "0600000") },
                editHeader(file) { it.replace("600000", "true") }
            )
            for (bad in hostileHeaders) reject(SetupKeyFileV1.Reason.FORMAT) {
                SetupKeyFileV1.open(bad, pass, box).close()
            }
            val invalidUtf8 = file.toString(Charsets.US_ASCII).split('.').toMutableList()
            invalidUtf8[0] = Base64.getUrlEncoder().withoutPadding().encodeToString(byteArrayOf(0xc0.toByte(), 0xaf.toByte()))
            reject(SetupKeyFileV1.Reason.FORMAT) {
                SetupKeyFileV1.open(invalidUtf8.joinToString(".").toByteArray(), pass, box).close()
            }
            assertEquals(0, kdfs)
            assertEquals(0, privateImports)
            val parts = file.toString(Charsets.US_ASCII).split('.')
            for (index in 1..4) {
                val changed = parts.toMutableList()
                val bytes = Base64.getUrlDecoder().decode(parts[index])
                bytes[0] = (bytes[0].toInt() xor 1).toByte()
                changed[index] = Base64.getUrlEncoder().withoutPadding().encodeToString(bytes)
                reject { SetupKeyFileV1.open(changed.joinToString(".").toByteArray(), pass, box).close() }
            }
            assertEquals(4, kdfs)
            assertEquals(0, privateImports)
            SetupKeyFileV1.open(file, pass, box).close()
            assertEquals(5, kdfs)
            assertEquals(1, privateImports)
            afterKdf = { Thread.currentThread().interrupt() }
            try {
                reject(SetupKeyFileV1.Reason.CANCELLED) {
                    SetupKeyFileV1.open(file, pass, box).close()
                }
                assertEquals(1, privateImports)
            } finally { Thread.interrupted(); afterKdf = null }
            val borrowedFile = file.copyOf()
            val borrowedPass = pass.copyOf()
            val borrowedBox = box
            afterKdf = { borrowedFile.fill(0); borrowedPass.fill('\u0000'); borrowedBox.fill(0) }
            SetupKeyFileV1.open(borrowedFile, borrowedPass, borrowedBox).use {
                assertArrayEquals(box, it.metadata.boxId())
                assertArrayEquals(hex(fixtures["xy"].asString), it.metadata.publicKey())
            }
            assertEquals(2, privateImports)
        } finally {
            afterKdf = null
            Thread.interrupted()
            Security.removeProvider(provider.name)
        }
        assertNull(Security.getProvider(provider.name))
    }
}
