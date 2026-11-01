package de.larpescape.protocol

import com.google.gson.stream.JsonReader
import com.google.gson.stream.JsonToken
import java.io.StringReader
import java.math.BigInteger
import java.nio.ByteBuffer
import java.nio.CharBuffer
import java.nio.charset.CodingErrorAction
import java.security.AlgorithmParameters
import java.security.GeneralSecurityException
import java.security.KeyFactory
import java.security.KeyPairGenerator
import java.security.MessageDigest
import java.security.PrivateKey
import java.security.SecureRandom
import java.security.Signature
import java.security.interfaces.ECPrivateKey
import java.security.interfaces.ECPublicKey
import java.security.spec.ECFieldFp
import java.security.spec.ECGenParameterSpec
import java.security.spec.ECParameterSpec
import java.security.spec.ECPoint
import java.security.spec.ECPublicKeySpec
import java.security.spec.PKCS8EncodedKeySpec
import java.util.Base64
import javax.crypto.AEADBadTagException
import javax.crypto.Cipher
import javax.crypto.SecretKey
import javax.crypto.SecretKeyFactory
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.PBEKeySpec
import javax.crypto.spec.SecretKeySpec
import javax.security.auth.Destroyable

/** User-approved file profile v1; no UI/box/enrollment path. Blocking: use an owned worker.
 *  An opened file is neither a live administrator nor a GM or D0 origin credential.
 *  The isolated API34 probe is not full product/device qualification. UI cancellation
 *  and actual box-challenge qualification remain open. Close discarded late results.
 */
object SetupKeyFileV1 {
    const val MAX_BYTES = 4096
    const val ITERATIONS = 600_000
    private const val ALGORITHM = "PBES2-HS256+A128KW"
    private const val ENCRYPTION = "A256GCM"
    private const val TYPE = "larp.setup-key.v1"
    private val decoder = Base64.getUrlDecoder()
    private val encoder = Base64.getUrlEncoder().withoutPadding()
    private val order = BigInteger("FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551", 16)
    // Exact PrivateKeyInfo v0 / id-ecPublicKey / secp256r1 / ECPrivateKey v1 / scalar32.
    private val derPrefix = hex("3041020100301306072a8648ce3d020106082a8648ce3d030107042730250201010420")
    private val curve: ECParameterSpec by lazy {
        AlgorithmParameters.getInstance("EC").apply { init(ECGenParameterSpec("secp256r1")) }
            .getParameterSpec(ECParameterSpec::class.java)
    }

    enum class Reason { FORMAT, FILE_OR_PASSWORD, BOX, UNAVAILABLE, CANCELLED }
    class Failure(val reason: Reason) : Exception(reason.name)

    class Metadata internal constructor(box: ByteArray, xy: ByteArray, kid: ByteArray) {
        private val boundBox = box.copyOf()
        private val publicXy = xy.copyOf()
        private val fingerprint = kid.copyOf()
        fun boxId() = boundBox.copyOf()
        fun publicKey() = publicXy.copyOf()
        fun keyId() = fingerprint.copyOf()
    }

    class Prepared internal constructor(bytes: ByteArray, val metadata: Metadata) {
        private val ciphertext = bytes.copyOf()
        fun encodedFile() = ciphertext.copyOf()
        // Deliberately NO generation-time private key or signer in this value.
    }

    class Opened internal constructor(private var key: ECPrivateKey?, val metadata: Metadata) : AutoCloseable {
        @Synchronized fun isClosed() = key == null
        @Synchronized override fun close() {
            val old = key
            key = null
            dispose(old)
        }
        // Deliberately no administrative, peer-auth or D0 signing API. Actual
        // fresh box-challenge and setup command formats are a separate boundary.
    }

    private data class Header(val metadata: Metadata, val salt: ByteArray, val publicKey: ECPublicKey)

    /** Creates ciphertext + public metadata only. Never retains the generator key. */
    fun create(boxId: ByteArray, password: CharArray): Prepared {
        expect(boxId.size == 16, Reason.BOX)
        expect(password.size <= 256, Reason.FILE_OR_PASSWORD)
        val box = boxId.copyOf()
        val pass = password.copyOf()
        var generated: PrivateKey? = null
        var der: ByteArray? = null
        var cek: ByteArray? = null
        var kek: SecretKey? = null
        try {
            validateBox(box)
            validatePassword(pass)
            checkpoint()
            val random = SecureRandom()
            val keyPair = KeyPairGenerator.getInstance("EC").apply {
                initialize(ECGenParameterSpec("secp256r1"), random)
            }.generateKeyPair()
            generated = keyPair.private
            val privateKey = keyPair.private as? ECPrivateKey ?: fail(Reason.UNAVAILABLE)
            val publicKey = keyPair.public as? ECPublicKey ?: fail(Reason.UNAVAILABLE)
            parameters(privateKey.params ?: fail(Reason.UNAVAILABLE))
            parameters(publicKey.params ?: fail(Reason.UNAVAILABLE))
            val xy = xy(publicKey.w)
            point(xy)
            val metadata = Metadata(box, xy, kid(xy))
            pair(privateKey, publicKey, metadata)
            der = normalized(privateKey)
            val salt = ByteArray(16).also(random::nextBytes)
            cek = ByteArray(32).also(random::nextBytes)
            val iv = ByteArray(12).also(random::nextBytes)
            val text = header(metadata, salt)
            val protected = b64(text.toByteArray(Charsets.UTF_8))
            checkpoint()
            kek = derive(pass, salt)
            checkpoint()
            val wrapping = Cipher.getInstance("AESWrap").apply { init(Cipher.WRAP_MODE, kek) }
            val wrapped = wrapping.wrap(SecretKeySpec(cek, "AES"))
            expect(wrapped.size == 40, Reason.UNAVAILABLE)
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.ENCRYPT_MODE, SecretKeySpec(cek, "AES"), GCMParameterSpec(128, iv))
            cipher.updateAAD(protected.toByteArray(Charsets.US_ASCII))
            val encrypted = cipher.doFinal(der)
            expect(encrypted.size == 83, Reason.UNAVAILABLE)
            val file = listOf(protected, b64(wrapped), b64(iv),
                b64(encrypted.copyOfRange(0, 67)), b64(encrypted.copyOfRange(67, 83)))
                .joinToString(".").toByteArray(Charsets.US_ASCII)
            checkpoint()
            expect(file.size <= MAX_BYTES, Reason.UNAVAILABLE)
            return Prepared(file, metadata)
        } catch (error: Failure) {
            throw error
        } catch (_: GeneralSecurityException) {
            fail(Reason.UNAVAILABLE)
        } catch (_: java.security.ProviderException) {
            fail(Reason.UNAVAILABLE)
        } finally {
            pass.fill('\u0000')
            der?.fill(0)
            cek?.fill(0)
            dispose(kek)
            dispose(generated)
        }
    }

    /** A real reimport, no cached generator key. This STILL grants no box rights. */
    fun open(file: ByteArray, password: CharArray, expectedBox: ByteArray): Opened {
        expect(file.size in 1..MAX_BYTES, Reason.FORMAT)
        expect(expectedBox.size == 16, Reason.BOX)
        expect(password.size <= 256, Reason.FILE_OR_PASSWORD)
        val input = file.copyOf()
        val pass = password.copyOf()
        val box = expectedBox.copyOf()
        var privateKey: ECPrivateKey? = null
        var plain: ByteArray? = null
        var cek: ByteArray? = null
        var kek: SecretKey? = null
        var unwrapped: SecretKey? = null
        try {
            validateBox(box)
            validatePassword(pass)
            checkpoint()
            expect(input.all { it.toInt() in 33..126 }, Reason.FORMAT)
            val parts = input.toString(Charsets.US_ASCII).split('.')
            expect(parts.size == 5 && parts.all(String::isNotEmpty), Reason.FORMAT)
            val headerBytes = un64(parts[0], 1, 1024)
            val parsed = parse(headerBytes)
            val wrapped = un64(parts[1], 40, 40)
            val iv = un64(parts[2], 12, 12)
            val ciphertext = un64(parts[3], 67, 67)
            val tag = un64(parts[4], 16, 16)
            expect(parsed.metadata.boxId().contentEquals(box), Reason.BOX)
            // All structural and bounded-public-key checks precede expensive KDF.
            checkpoint()
            kek = derive(pass, parsed.salt)
            checkpoint()
            try {
                val wrapping = Cipher.getInstance("AESWrap").apply { init(Cipher.UNWRAP_MODE, kek) }
                unwrapped = wrapping.unwrap(wrapped, "AES", Cipher.SECRET_KEY) as? SecretKey
                    ?: fail(Reason.FILE_OR_PASSWORD)
                cek = unwrapped!!.encoded ?: fail(Reason.FILE_OR_PASSWORD)
                expect(cek!!.size == 32, Reason.FILE_OR_PASSWORD)
                val cipher = Cipher.getInstance("AES/GCM/NoPadding")
                cipher.init(Cipher.DECRYPT_MODE, SecretKeySpec(cek, "AES"), GCMParameterSpec(128, iv))
                cipher.updateAAD(parts[0].toByteArray(Charsets.US_ASCII))
                // doFinal authenticates the ENTIRE message. No streaming plaintext
                // reaches PKCS8 parsing, provider import or any key owner before tag.
                plain = cipher.doFinal(ciphertext + tag)
            } catch (_: AEADBadTagException) {
                fail(Reason.FILE_OR_PASSWORD)
            } catch (_: java.security.InvalidKeyException) {
                fail(Reason.FILE_OR_PASSWORD)
            }
            checkpoint()
            val bytes = plain!!
            expect(bytes.size == 67 && bytes.copyOfRange(0, 35).contentEquals(derPrefix), Reason.FILE_OR_PASSWORD)
            val scalarBytes = bytes.copyOfRange(35,67)
            val scalar = try { BigInteger(1,scalarBytes) } finally { scalarBytes.fill(0) }
            expect(scalar.signum() > 0 && scalar < order, Reason.FILE_OR_PASSWORD)
            privateKey = KeyFactory.getInstance("EC").generatePrivate(PKCS8EncodedKeySpec(bytes)) as? ECPrivateKey
                ?: fail(Reason.FILE_OR_PASSWORD)
            parameters(privateKey!!.params ?: fail(Reason.FILE_OR_PASSWORD))
            expect(privateKey!!.s == scalar, Reason.FILE_OR_PASSWORD)
            pair(privateKey!!, parsed.publicKey, parsed.metadata)
            checkpoint()
            val owned = Opened(privateKey, parsed.metadata)
            privateKey = null
            return owned
        } catch (error: Failure) {
            throw error
        } catch (_: GeneralSecurityException) {
            fail(Reason.UNAVAILABLE)
        } catch (_: java.security.ProviderException) {
            fail(Reason.UNAVAILABLE)
        } finally {
            pass.fill('\u0000')
            plain?.fill(0)
            cek?.fill(0)
            dispose(kek)
            dispose(unwrapped)
            dispose(privateKey)
        }
    }

    private fun header(metadata: Metadata, salt: ByteArray): String {
        val public = metadata.publicKey()
        return "{\"alg\":\"$ALGORITHM\",\"enc\":\"$ENCRYPTION\",\"p2s\":\"${b64(salt)}\",\"p2c\":$ITERATIONS," +
            "\"typ\":\"$TYPE\",\"cty\":\"application/pkcs8\",\"box\":\"${b64(metadata.boxId())}\",\"kid\":\"${b64(metadata.keyId())}\"," +
            "\"jwk\":{\"kty\":\"EC\",\"crv\":\"P-256\",\"x\":\"${b64(public.copyOfRange(0,32))}\",\"y\":\"${b64(public.copyOfRange(32,64))}\"}}"
    }

    private fun parse(bytes: ByteArray): Header {
        val text = try {
            Charsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes)).toString()
        } catch (_: java.nio.charset.CharacterCodingException) { fail(Reason.FORMAT) }
        expect(!text.startsWith('\ufeff'), Reason.FORMAT)
        val fields = mutableSetOf<String>()
        var box: ByteArray? = null
        var fingerprint: ByteArray? = null
        var xy: ByteArray? = null
        var salt: ByteArray? = null
        try {
            JsonReader(StringReader(text)).use { reader ->
                reader.isLenient = false
                reader.beginObject()
                while (reader.hasNext()) {
                    val name = reader.nextName()
                    expect(fields.add(name), Reason.FORMAT)
                    when (name) {
                        "alg" -> expect(string(reader) == ALGORITHM, Reason.FORMAT)
                        "enc" -> expect(string(reader) == ENCRYPTION, Reason.FORMAT)
                        "typ" -> expect(string(reader) == TYPE, Reason.FORMAT)
                        "cty" -> expect(string(reader) == "application/pkcs8", Reason.FORMAT)
                        "p2s" -> salt = un64(string(reader), 16, 16)
                        "box" -> box = un64(string(reader), 16, 16).also(::validateBox)
                        "kid" -> fingerprint = un64(string(reader), 32, 32)
                        "jwk" -> xy = jwk(reader)
                        "p2c" -> {
                            expect(reader.peek() == JsonToken.NUMBER, Reason.FORMAT)
                            expect(reader.nextString() == ITERATIONS.toString(), Reason.FORMAT)
                        }
                        else -> fail(Reason.FORMAT)
                    }
                }
                reader.endObject()
                expect(reader.peek() == JsonToken.END_DOCUMENT, Reason.FORMAT)
            }
        } catch (error: Failure) { throw error }
        catch (_: Exception) { fail(Reason.FORMAT) }
        expect(fields == setOf("alg", "enc", "typ", "cty", "p2s", "box", "kid", "jwk", "p2c"), Reason.FORMAT)
        expect(kid(xy!!).contentEquals(fingerprint!!), Reason.FORMAT)
        return Header(Metadata(box!!, xy!!, fingerprint!!), salt!!, point(xy!!))
    }

    private fun jwk(reader: JsonReader): ByteArray {
        val names = mutableSetOf<String>()
        var x: ByteArray? = null
        var y: ByteArray? = null
        reader.beginObject()
        while (reader.hasNext()) {
            val name = reader.nextName()
            expect(names.add(name), Reason.FORMAT)
            when (name) {
                "kty" -> expect(string(reader) == "EC", Reason.FORMAT)
                "crv" -> expect(string(reader) == "P-256", Reason.FORMAT)
                "x" -> x = un64(string(reader), 32, 32)
                "y" -> y = un64(string(reader), 32, 32)
                else -> fail(Reason.FORMAT)
            }
        }
        reader.endObject()
        expect(names == setOf("kty", "crv", "x", "y"), Reason.FORMAT)
        return x!! + y!!
    }

    private fun string(reader: JsonReader): String {
        expect(reader.peek() == JsonToken.STRING, Reason.FORMAT)
        return reader.nextString()
    }
    private fun un64(text: String, minimum: Int, maximum: Int): ByteArray {
        expect(text.length <= (maximum * 4 + 2) / 3 && text.length % 4 != 1 &&
            text.all { it in 'A'..'Z' || it in 'a'..'z' || it in '0'..'9' || it == '-' || it == '_' }, Reason.FORMAT)
        val value = try { decoder.decode(text) } catch (_: IllegalArgumentException) { fail(Reason.FORMAT) }
        expect(value.size in minimum..maximum && b64(value) == text, Reason.FORMAT)
        return value
    }
    private fun b64(bytes: ByteArray) = encoder.encodeToString(bytes)
    private fun derive(password: CharArray, salt: ByteArray): SecretKey {
        val actualSalt = ALGORITHM.toByteArray(Charsets.US_ASCII) + byteArrayOf(0) + salt
        val spec = PBEKeySpec(password, actualSalt, ITERATIONS, 128)
        var material: ByteArray? = null
        var derived: SecretKey? = null
        try {
            derived = SecretKeyFactory.getInstance("PBKDF2WithHmacSHA256").generateSecret(spec)
            material = derived.encoded ?: fail(Reason.UNAVAILABLE)
            expect(material!!.size == 16, Reason.UNAVAILABLE)
            return SecretKeySpec(material, "AES")
        } finally {
            spec.clearPassword()
            material?.fill(0)
            dispose(derived)
        }
    }
    private fun validatePassword(chars: CharArray) {
        var codepoints = 0
        var at = 0
        while (at < chars.size) {
            val c = chars[at++]
            if (Character.isHighSurrogate(c)) {
                expect(at < chars.size && Character.isLowSurrogate(chars[at]), Reason.FILE_OR_PASSWORD)
                at++
            } else expect(!Character.isLowSurrogate(c), Reason.FILE_OR_PASSWORD)
            codepoints++
            expect(codepoints <= 128, Reason.FILE_OR_PASSWORD)
        }
        expect(codepoints >= 16, Reason.FILE_OR_PASSWORD)
        val utf8 = Charsets.UTF_8.newEncoder().onMalformedInput(CodingErrorAction.REPORT)
            .onUnmappableCharacter(CodingErrorAction.REPORT).encode(CharBuffer.wrap(chars))
        try { expect(utf8.remaining() <= 128, Reason.FILE_OR_PASSWORD) }
        finally { if (utf8.hasArray()) utf8.array().fill(0) }
    }
    private fun validateBox(value: ByteArray) = expect(value.size == 16 && value.any { it != 0.toByte() }, Reason.BOX)
    private fun normalized(key: ECPrivateKey): ByteArray {
        expect(key.s.signum() > 0 && key.s < order, Reason.FILE_OR_PASSWORD)
        val scalar = width(key.s)
        try { return derPrefix + scalar }
        finally { scalar.fill(0) }
    }
    private fun width(value: BigInteger): ByteArray {
        val bytes = value.toByteArray()
        try {
            val content = if (bytes.size == 33 && bytes[0] == 0.toByte()) bytes.copyOfRange(1,33) else bytes.copyOf()
            try { expect(content.size <= 32 && value.signum() >= 0, Reason.FILE_OR_PASSWORD)
                return ByteArray(32-content.size) + content }
            finally { content.fill(0) }
        } finally { bytes.fill(0) }
    }
    private fun xy(point: ECPoint) = width(point.affineX) + width(point.affineY)
    private fun point(xy: ByteArray): ECPublicKey {
        expect(xy.size == 64, Reason.FORMAT)
        val x = BigInteger(1, xy.copyOfRange(0,32))
        val y = BigInteger(1, xy.copyOfRange(32,64))
        val prime = (curve.curve.field as ECFieldFp).p
        expect(x < prime && y < prime && y.modPow(BigInteger.valueOf(2),prime) ==
            x.modPow(BigInteger.valueOf(3),prime).add(curve.curve.a.multiply(x)).add(curve.curve.b).mod(prime), Reason.FORMAT)
        return KeyFactory.getInstance("EC").generatePublic(ECPublicKeySpec(ECPoint(x,y),curve)) as? ECPublicKey
            ?: fail(Reason.UNAVAILABLE)
    }
    private fun parameters(value: ECParameterSpec) {
        expect(value.curve.field is ECFieldFp && (value.curve.field as ECFieldFp).p == (curve.curve.field as ECFieldFp).p &&
            value.curve.a == curve.curve.a && value.curve.b == curve.curve.b &&
            value.generator == curve.generator && value.order == order && value.cofactor == 1, Reason.FILE_OR_PASSWORD)
    }
    private fun kid(xy: ByteArray): ByteArray = MessageDigest.getInstance("SHA-256").digest(
        hex("a5010203262001215820") + xy.copyOfRange(0,32) + hex("225820") + xy.copyOfRange(32,64))
    private fun pair(privateKey: PrivateKey, publicKey: ECPublicKey, metadata: Metadata) {
        val nonce = ByteArray(32).also(SecureRandom()::nextBytes)
        val message = "larp.setup.file.pair.v1\u0000".toByteArray(Charsets.US_ASCII) + metadata.boxId() + metadata.keyId() + nonce
        val signature = Signature.getInstance("SHA256withECDSA")
        signature.initSign(privateKey)
        signature.update(message)
        val proof = signature.sign()
        signature.initVerify(publicKey)
        signature.update(message)
        expect(signature.verify(proof), Reason.FILE_OR_PASSWORD)
    }
    private fun checkpoint() { if (Thread.currentThread().isInterrupted) fail(Reason.CANCELLED) }
    private fun dispose(value: Any?) { try { (value as? Destroyable)?.destroy() } catch (_: Exception) { } }
    private fun expect(ok: Boolean, reason: Reason) { if (!ok) fail(reason) }
    private fun fail(reason: Reason): Nothing = throw Failure(reason)
    private fun hex(value: String) = value.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
}
