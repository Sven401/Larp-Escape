package de.larpescape.protocol

import java.math.BigInteger
import java.security.KeyPair
import java.security.KeyPairGenerator
import java.security.PrivateKey
import java.security.PublicKey
import java.security.Signature
import java.security.spec.ECGenParameterSpec
import javax.crypto.Cipher
import javax.crypto.KeyAgreement
import javax.crypto.Mac
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

/**
 * Isolated JCA capability candidate for APP_MUTUAL_V1 review. This is not a
 * selected wire suite, enrollment implementation, session, or authority proof.
 * Keys generated here are ephemeral software keys; persistent identity keys
 * require a separate Android Keystore and device capability review.
 */
internal object AppMutualCandidateCrypto {
    private val p256Order = BigInteger(
        "ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551", 16
    )

    fun ephemeralP256(): KeyPair = KeyPairGenerator.getInstance("EC").apply {
        initialize(ECGenParameterSpec("secp256r1"))
    }.generateKeyPair()

    fun sign(privateKey: PrivateKey, transcript: ByteArray): ByteArray =
        Signature.getInstance("SHA256withECDSA").run {
            initSign(privateKey)
            update(transcript)
            sign()
        }

    fun verify(publicKey: PublicKey, transcript: ByteArray, derSignature: ByteArray): Boolean =
        Signature.getInstance("SHA256withECDSA").run {
            initVerify(publicKey)
            update(transcript)
            verify(derSignature)
        }

    fun ecdh(privateKey: PrivateKey, publicKey: PublicKey): ByteArray =
        KeyAgreement.getInstance("ECDH").run {
            init(privateKey)
            doPhase(publicKey, true)
            generateSecret()
        }

    fun hmacSha256(key: ByteArray, data: ByteArray): ByteArray = Mac.getInstance("HmacSHA256").run {
        init(SecretKeySpec(key, "HmacSHA256"))
        doFinal(data)
    }

    fun aesGcmEncrypt(key: ByteArray, nonce: ByteArray, aad: ByteArray, plaintext: ByteArray): ByteArray =
        aesGcm(Cipher.ENCRYPT_MODE, key, nonce, aad, plaintext)

    fun aesGcmDecrypt(key: ByteArray, nonce: ByteArray, aad: ByteArray, ciphertext: ByteArray): ByteArray =
        aesGcm(Cipher.DECRYPT_MODE, key, nonce, aad, ciphertext)

    private fun aesGcm(mode: Int, key: ByteArray, nonce: ByteArray, aad: ByteArray, data: ByteArray): ByteArray {
        require(key.size == 32 && nonce.size == 12)
        return Cipher.getInstance("AES/GCM/NoPadding").run {
            init(mode, SecretKeySpec(key, "AES"), GCMParameterSpec(128, nonce))
            updateAAD(aad)
            doFinal(data)
        }
    }

    /** Strict P-256 ECDSA ASN.1 DER signature to fixed 64-byte r||s. */
    fun derToRaw(der: ByteArray): ByteArray {
        require(der.size in 8..72 && der[0] == 0x30.toByte() && (der[1].toInt() and 0xff) == der.size - 2)
        var pos = 2
        val r = readInteger(der, pos)
        pos = r.second
        val s = readInteger(der, pos)
        require(s.second == der.size)
        return fixedScalar(r.first) + fixedScalar(s.first)
    }

    /** Fixed 64-byte r||s to canonical ASN.1 DER signature. */
    fun rawToDer(raw: ByteArray): ByteArray {
        require(raw.size == 64)
        val r = encodedInteger(raw.copyOfRange(0, 32))
        val s = encodedInteger(raw.copyOfRange(32, 64))
        val content = byteArrayOf(0x02, r.size.toByte()) + r + byteArrayOf(0x02, s.size.toByte()) + s
        return byteArrayOf(0x30, content.size.toByte()) + content
    }

    private fun readInteger(der: ByteArray, offset: Int): Pair<BigInteger, Int> {
        require(offset + 2 <= der.size && der[offset] == 0x02.toByte())
        val size = der[offset + 1].toInt() and 0xff
        val end = offset + 2 + size
        require(size in 1..33 && end <= der.size)
        val first = der[offset + 2].toInt() and 0xff
        require(first and 0x80 == 0)
        require(size == 1 || first != 0 || (der[offset + 3].toInt() and 0x80) != 0)
        val value = BigInteger(1, der.copyOfRange(offset + 2, end))
        require(value > BigInteger.ZERO && value < p256Order)
        return value to end
    }

    private fun fixedScalar(value: BigInteger): ByteArray {
        require(value > BigInteger.ZERO && value < p256Order)
        val bytes = value.toByteArray().dropWhile { it == 0.toByte() }.toByteArray()
        require(bytes.size <= 32)
        return ByteArray(32 - bytes.size) + bytes
    }

    private fun encodedInteger(scalar: ByteArray): ByteArray {
        val value = BigInteger(1, scalar)
        require(value > BigInteger.ZERO && value < p256Order)
        return value.toByteArray()
    }
}
