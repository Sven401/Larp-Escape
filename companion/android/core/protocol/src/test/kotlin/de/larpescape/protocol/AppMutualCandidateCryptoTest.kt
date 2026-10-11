package de.larpescape.protocol

import java.security.SecureRandom
import javax.crypto.AEADBadTagException
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test

class AppMutualCandidateCryptoTest {
    private fun hex(value: String): ByteArray = value.chunked(2).map { it.toInt(16).toByte() }.toByteArray()

    @Test fun p256SignaturesAndDerRawInterop() {
        val identity = AppMutualCandidateCrypto.ephemeralP256()
        val other = AppMutualCandidateCrypto.ephemeralP256()
        val transcript = "candidate transcript only".toByteArray()
        val der = AppMutualCandidateCrypto.sign(identity.private, transcript)
        val raw = AppMutualCandidateCrypto.derToRaw(der)

        assertTrue(AppMutualCandidateCrypto.verify(identity.public, transcript, der))
        assertFalse(AppMutualCandidateCrypto.verify(other.public, transcript, der))
        assertFalse(AppMutualCandidateCrypto.verify(identity.public, "altered".toByteArray(), der))
        assertTrue(AppMutualCandidateCrypto.verify(identity.public, transcript, AppMutualCandidateCrypto.rawToDer(raw)))
        assertArrayEquals(der, AppMutualCandidateCrypto.rawToDer(raw))
        assertTrue(raw.size == 64)
    }

    @Test fun derRawParsingRejectsMalformedAndNoncanonicalScalars() {
        val fixedDer = hex("3006020101020102")
        val fixedRaw = ByteArray(64).also { it[31] = 1; it[63] = 2 }
        assertArrayEquals(fixedRaw, AppMutualCandidateCrypto.derToRaw(fixedDer))
        assertArrayEquals(fixedDer, AppMutualCandidateCrypto.rawToDer(fixedRaw))
        val der = AppMutualCandidateCrypto.sign(AppMutualCandidateCrypto.ephemeralP256().private, byteArrayOf(1))
        assertThrows(IllegalArgumentException::class.java) {
            AppMutualCandidateCrypto.derToRaw(der + 0)
        }
        assertThrows(IllegalArgumentException::class.java) {
            AppMutualCandidateCrypto.derToRaw(byteArrayOf(0x30, 0x06, 0x02, 0x01, 0x00, 0x02, 0x01, 0x01))
        }
        assertThrows(IllegalArgumentException::class.java) {
            AppMutualCandidateCrypto.derToRaw(hex("300702020001020101"))
        }
        assertThrows(IllegalArgumentException::class.java) {
            AppMutualCandidateCrypto.rawToDer(ByteArray(64))
        }
        assertThrows(IllegalArgumentException::class.java) {
            AppMutualCandidateCrypto.rawToDer(ByteArray(63))
        }
    }

    @Test fun ephemeralEcdhAgreesAndHmacSha256IsDeterministic() {
        val app = AppMutualCandidateCrypto.ephemeralP256()
        val box = AppMutualCandidateCrypto.ephemeralP256()
        val appSecret = AppMutualCandidateCrypto.ecdh(app.private, box.public)
        val boxSecret = AppMutualCandidateCrypto.ecdh(box.private, app.public)
        assertArrayEquals(appSecret, boxSecret)
        assertTrue(appSecret.size == 32)
        val data = "reviewed HKDF inputs still needed".toByteArray()
        assertArrayEquals(
            AppMutualCandidateCrypto.hmacSha256(appSecret, data),
            AppMutualCandidateCrypto.hmacSha256(boxSecret, data)
        )
        assertFalse(
            AppMutualCandidateCrypto.hmacSha256(appSecret, data).contentEquals(
                AppMutualCandidateCrypto.hmacSha256(appSecret, data + 1)
            )
        )
        // RFC 4231 test case 1, independent of the generated ECDH secret.
        assertArrayEquals(
            hex("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"),
            AppMutualCandidateCrypto.hmacSha256(ByteArray(20) { 0x0b }, "Hi There".toByteArray())
        )
    }

    @Test fun aesGcmRejectsWrongKeyNonceAndAlteredAadCiphertext() {
        val random = SecureRandom()
        val key = ByteArray(32).also(random::nextBytes)
        val nonce = ByteArray(12).also(random::nextBytes)
        val aad = "birth and direction candidate".toByteArray()
        val plaintext = "closed capability test".toByteArray()
        val ciphertext = AppMutualCandidateCrypto.aesGcmEncrypt(key, nonce, aad, plaintext)
        assertArrayEquals(plaintext, AppMutualCandidateCrypto.aesGcmDecrypt(key, nonce, aad, ciphertext))
        assertThrows(AEADBadTagException::class.java) {
            AppMutualCandidateCrypto.aesGcmDecrypt(key, nonce, aad + 1, ciphertext)
        }
        val changed = ciphertext.clone().also { it[0] = (it[0].toInt() xor 1).toByte() }
        assertThrows(AEADBadTagException::class.java) {
            AppMutualCandidateCrypto.aesGcmDecrypt(key, nonce, aad, changed)
        }
        assertThrows(IllegalArgumentException::class.java) {
            AppMutualCandidateCrypto.aesGcmEncrypt(ByteArray(16), nonce, aad, plaintext)
        }
        assertThrows(IllegalArgumentException::class.java) {
            AppMutualCandidateCrypto.aesGcmEncrypt(key, ByteArray(8), aad, plaintext)
        }
    }
}
