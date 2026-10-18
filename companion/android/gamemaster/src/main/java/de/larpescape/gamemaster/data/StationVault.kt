package de.larpescape.gamemaster.data

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.AtomicFile
import de.larpescape.gamemaster.station.Station
import de.larpescape.protocol.LedAccess
import java.io.*
import java.security.KeyStore
import java.util.UUID
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/** GM owns this encrypted catalog and Keystore alias. No access to the Dev app's directory. */
class StationVault(context: Context) {
    private val file = AtomicFile(File(context.noBackupFilesDir, "stations-v1.bin"))
    private val alias = "larpescape-gamemaster-stations-v1"
    private fun key(create: Boolean): SecretKey {
        val store = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        (store.getKey(alias, null) as? SecretKey)?.let { return it }
        check(create) { "Stationszugang kann nicht geöffnet werden. Einrichtung prüfen lassen." }
        return KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore").run {
            init(KeyGenParameterSpec.Builder(alias, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE).setKeySize(256).build())
            generateKey()
        }
    }
    private fun read(): List<StationEntry> {
        val encrypted = try { file.openRead().use { it.readNBytes(131073) } } catch (_: FileNotFoundException) { return emptyList() }
        require(encrypted.size in 30..131072 && encrypted[0] == 1.toByte())
        val plain = Cipher.getInstance("AES/GCM/NoPadding").run {
            init(Cipher.DECRYPT_MODE, key(false), GCMParameterSpec(128, encrypted.copyOfRange(1,13)))
            doFinal(encrypted.copyOfRange(13,encrypted.size))
        }
        return try { StationCatalogCodec.decode(plain) } finally { plain.fill(0) }
    }
    @Synchronized fun stations(): List<Station> = read().let { entries -> try { entries.map { it.station } } finally { entries.forEach { it.bytes.fill(0) } } }
    @Synchronized fun access(station: Station): LedAccess = read().let { entries ->
        try { LedAccess.parse(entries.single { it.station == station }.bytes) }
        finally { entries.forEach { it.bytes.fill(0) } }
    }
    @Synchronized fun save(id: String?, event: String, name: String, address: String, credential: ByteArray?): List<Station> {
        val entries = read().toMutableList()
        try {
            val existing = id?.let { target -> entries.single { it.station.id == target } }
            val bytes = credential?.copyOf() ?: existing?.bytes?.copyOf() ?: error("Box-Zugangsdatei auswählen.")
            try {
                val access = LedAccess.parse(bytes)
                val station = Station(id ?: UUID.randomUUID().toString(),event.trim(),name.trim(),Station.normalizeAddress(address),access.deviceId().joinToString("") { "%02x".format(it) })
                val next = entries.filter { it.station.id != station.id } + StationEntry(station,bytes)
                Station.validateCatalog(next.map { it.station })
                val plain = StationCatalogCodec.encode(next)
                val cipher = Cipher.getInstance("AES/GCM/NoPadding").apply { init(Cipher.ENCRYPT_MODE,key(true)) }
                val encrypted = try { byteArrayOf(1) + cipher.iv + cipher.doFinal(plain) } finally { plain.fill(0) }
                val stream = file.startWrite()
                try { stream.write(encrypted); file.finishWrite(stream) } catch (error: Exception) { file.failWrite(stream); throw error }
                return next.map { it.station }
            } finally { bytes.fill(0) }
        } finally { entries.forEach { it.bytes.fill(0) } }
    }
}
