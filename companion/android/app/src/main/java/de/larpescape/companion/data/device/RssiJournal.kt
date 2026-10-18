package de.larpescape.companion.data.device

import de.larpescape.protocol.EspNowResult
import de.larpescape.protocol.LedResult
import java.io.File
import java.io.FileOutputStream
import java.util.UUID

/** One foreground session owner; no keys or raw authenticated messages are stored. */
class RssiJournal(private val directory: File, private val wall: () -> Long, private val elapsed: () -> Long) {
    var latest: File? = directory.listFiles()?.filter { it.name.endsWith(".csv") }?.maxByOrNull { it.name }
        private set
    var samples: Long = latest?.useLines { rows -> rows.count { it.startsWith("sample,") }.toLong() } ?: 0L
        private set
    private var current: File? = null
    private var peer = ""
    private var twinkle = false
    fun begin(address: ByteArray, control: Boolean) {
        event("superseded")
        check(directory.isDirectory || directory.mkdirs()) { "Messprotokoll-Ordner nicht verfügbar" }
        peer = address.joinToString(":") { "%02X".format(it.toInt() and 255) }; twinkle = control
        val file = File(directory, "rssi-${wall()}-${UUID.randomUUID()}.csv")
        check(file.createNewFile())
        current = file; latest = file; samples = 0
        append("event,wall_ms,elapsed_ms,request,device_ms,sequence,outcome,accepted,link,received,rssi_dbm,filtered_dbm,target_percent,receiver_count,receiver_boot,led_effect,led_revision,peer,twinkle")
        event("start")
    }
    fun event(name: String, request: Long? = null) {
        if (current == null) return
        require(name.matches(Regex("[a-z_]+")))
        row(name, request)
    }
    fun sample(result: EspNowResult) {
        if (current == null) return
        row("sample", result.led.request, result)
        samples++ // Only a durably written row counts as saved.
    }
    fun close() { try { event("disconnect") } finally { current = null } }
    fun led(result: LedResult) { row(if (result.status.off) "led_off_result" else "led_result", result.request, null, result) }
    private fun row(event: String, request: Long?, r: EspNowResult? = null, led: LedResult? = r?.led) {
        val s = led?.status
        append(listOf(event, wall(), elapsed(), request, s?.deviceMillis, s?.statusSequence, led?.outcome,
            r?.accepted, r?.link, r?.received, r?.rssi, r?.filteredRssi, r?.intensity,
            r?.receiverCount, r?.receiverBoot, s?.effect, s?.revision, peer, twinkle)
            .joinToString(",") { it?.toString() ?: "" })
    }
    private fun append(line: String) {
        val file = current ?: return
        check(file.length() < 32L * 1024 * 1024) { "Messprotokoll voll · Messung wird beendet" }
        try {
            FileOutputStream(file, true).use { stream ->
                stream.write((line + "\n").toByteArray(Charsets.UTF_8)); stream.fd.sync()
            }
        } catch (error: Exception) {
            throw IllegalStateException("Messprotokoll konnte nicht gespeichert werden · Messung beendet", error)
        }
    }
}
