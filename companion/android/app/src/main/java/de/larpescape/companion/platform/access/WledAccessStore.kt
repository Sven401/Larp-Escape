package de.larpescape.companion.platform.access

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.AtomicFile
import java.io.File
import java.io.FileNotFoundException
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/** Only ciphertext reaches storage; Android Keystore keeps the wrapping key non-exportable. */
class WledAccessStore(context: Context) {
    private val file = AtomicFile(File(context.noBackupFilesDir, "wled-access-v1.bin"))
    private val alias = "larpescape-wled-access-v1"
    private fun key(create: Boolean): SecretKey {
        val store = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        (store.getKey(alias, null) as? SecretKey)?.let { return it }
        check(create) { "Einrichtungsschlüssel fehlt. Box-Zugang erneut importieren." }
        return KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore").run {
            init(KeyGenParameterSpec.Builder(alias, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setKeySize(256).build())
            generateKey()
        }
    }
    fun read(): ByteArray? {
        // openRead also restores AtomicFile's previous committed copy after an interrupted write.
        val bytes = try { file.openRead().use { it.readNBytes(4097) } }
            catch (_: FileNotFoundException) { return null }
        check(bytes.size in 30..4096 && bytes[0] == 1.toByte()) { "Box-Zugang beschädigt. Erneut importieren." }
        return Cipher.getInstance("AES/GCM/NoPadding").run {
            init(Cipher.DECRYPT_MODE, key(false), GCMParameterSpec(128, bytes.copyOfRange(1, 13)))
            doFinal(bytes.copyOfRange(13, bytes.size))
        }
    }
    fun save(bytes: ByteArray) {
        require(bytes.size in 1..2048)
        val cipher = Cipher.getInstance("AES/GCM/NoPadding").apply { init(Cipher.ENCRYPT_MODE, key(true)) }
        val encrypted = byteArrayOf(1) + cipher.iv + cipher.doFinal(bytes)
        val stream = file.startWrite()
        try { stream.write(encrypted); file.finishWrite(stream) }
        catch (error: Exception) { file.failWrite(stream); throw error }
    }
}
