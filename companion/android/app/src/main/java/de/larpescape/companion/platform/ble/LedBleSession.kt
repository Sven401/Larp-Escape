package de.larpescape.companion.platform.ble

import android.annotation.SuppressLint
import android.bluetooth.*
import android.os.Handler
import android.os.SystemClock
import de.larpescape.companion.data.device.*
import de.larpescape.protocol.*
import java.security.SecureRandom
import java.util.ArrayDeque
import java.util.UUID

/** Owned by one foreground GATT generation, callbacks dispatched on the repository's main thread. */
@SuppressLint("MissingPermission")
internal class LedBleSession(
    private val connection: BluetoothGatt,
    private val handler: Handler,
    access: LedAccess,
    private val update: ((LedUiState) -> LedUiState) -> Unit,
    private val failed: (String) -> Unit,
) {
    private val protocol = LedProtocol(access, ByteArray(16).also { SecureRandom().nextBytes(it) })
    private val assembler = LedFrameAssembler()
    private val commands = LedCommandTracker()
    private val wake = WakeStateTracker(access.deviceId().joinToString("") { "%02x".format(it) })
    private var input: BluetoothGattCharacteristic? = null
    private var output: BluetoothGattCharacteristic? = null
    private var alive = true
    private var subscribed = false
    private var authenticated = false
    private var transfer = 0
    private var snapshot: LedStatus? = null
    private var receivedAt = 0L
    private var nfcRequest = 1L
    private var nfcPending: Long? = null
    private var nfcSequence = 0L
    private var nfcState = BoxNfcUiState()
    private var operation = 0
    private var lastMessage = "Box-Anmeldung wird geprüft …"
    private data class Transfer(val frames: List<ByteArray>, val complete: () -> Unit, var index: Int = 0)
    private val outgoing = ArrayDeque<Transfer>()
    private var writing = false
    private inline fun runSafely(block: () -> Unit) {
        if (!alive) return
        try { block() } catch (error: Exception) { failed(error.message ?: "Box-Verbindung fehlgeschlagen.") }
    }
    fun start() = runSafely {
        val service = connection.getService(SERVICE) ?: error("LED-Dienst fehlt.")
        input = service.getCharacteristic(COMMAND) ?: error("LED-Befehlseingang fehlt.")
        output = service.getCharacteristic(OUTPUT) ?: error("LED-Antwort fehlt.")
        check(input!!.properties and BluetoothGattCharacteristic.PROPERTY_WRITE != 0 &&
            output!!.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY != 0) { "Unpassender LED-Dienst." }
        val descriptor = output!!.getDescriptor(CCCD) ?: error("LED-Statusabonnement fehlt.")
        check(connection.setCharacteristicNotification(output, true)) { "LED-Status nicht abonnierbar." }
        update { it.copy(available = true, authenticated = false, current = false, message = lastMessage, wake = WakeUiState()) }
        check(connection.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) == BluetoothStatusCodes.SUCCESS)
        handler.postDelayed({ if (alive && !authenticated) failed("Box-Anmeldung nicht rechtzeitig bestätigt.") }, 10000)
    }
    fun descriptorWritten(descriptor: BluetoothGattDescriptor, status: Int) = runSafely {
        check(!subscribed && descriptor.uuid == CCCD && descriptor.characteristic.uuid == OUTPUT && status == BluetoothGatt.GATT_SUCCESS) { "LED-Statusabonnement fehlgeschlagen." }
        subscribed = true
        send(protocol.hello())
    }
    fun characteristicWritten(characteristic: BluetoothGattCharacteristic, status: Int) = runSafely {
        check(characteristic.uuid == COMMAND && writing && status == BluetoothGatt.GATT_SUCCESS) { "LED-Nachricht nicht vollständig übertragen." }
        writing = false
        val current = outgoing.first()
        current.index++
        if (current.index == current.frames.size) { outgoing.removeFirst(); current.complete() }
        writeNext()
        if (authenticated) publish()
    }
    fun changed(characteristic: BluetoothGattCharacteristic, value: ByteArray) = runSafely {
        check(subscribed && characteristic.uuid == OUTPUT)
        val message = assembler.accept(value, SystemClock.elapsedRealtime())
        handler.postDelayed({ if (alive && assembler.expired(SystemClock.elapsedRealtime())) failed("Boxnachricht unvollständig · Verbindung beendet.") }, 2000)
        if (message == null) return@runSafely
        check(message.size >= 2)
        when (message[1].toInt() and 255) {
            0x81 -> { check(!authenticated); send(protocol.proof(message)) }
            0x82 -> { check(!authenticated); accept(protocol.granted(message)); authenticated = true; lastMessage = "Box-Zugang bestätigt · LEDs bereit"; publish() }
            0x84 -> { check(authenticated); accept(protocol.status(message)); publish() }
            0x83 -> {
                check(authenticated)
                val result = protocol.result(message)
                check(commands.request == result.request) { "LED-Antwort gehört zu keinem offenen Auftrag." }
                accept(result.status)
                commands.received(result)?.let(::completed)
                publish()
            }
            0x85 -> {
                check(authenticated)
                val result = protocol.nfcResult(message)
                check(nfcPending == result.request && result.sequence > nfcSequence) { "NFC-Antwort gehört zu keinem offenen Auftrag." }
                check(result.nextRequest == result.request + 1) { "NFC-Auftrag wurde nicht abgeschlossen." }
                nfcPending = null; nfcRequest = result.nextRequest; nfcSequence = result.sequence
                nfcState = nfcState.copy(busy = false, result = result, message = result.message)
                publish()
            }
            else -> error("Unbekannte LED-Nachricht.")
        }
    }
    private fun accept(value: LedStatus) {
        if (snapshot != null && value.statusSequence <= snapshot!!.statusSequence) return
        val now = SystemClock.elapsedRealtime()
        val wakeChanged = wake.accept(value, now)
        snapshot = value; receivedAt = now
        if (value.wake != null) nfcState = nfcState.copy(supported = value.nfcSupported)
        // LED results can advance the shared sequence. The Wake expiry remains
        // scheduled independently, even when a result supersedes this status.
        if (wakeChanged) handler.postDelayed({ if (alive) publish() }, 1000)
        val sequence = value.statusSequence
        handler.postDelayed({ if (alive && snapshot?.statusSequence == sequence) publish() }, 1000)
    }
    private fun publish() {
        val value = snapshot ?: return
        val fresh = SystemClock.elapsedRealtime() - receivedAt in 0..999
        val notice = when {
            !fresh -> "Boxstatus überholt · Bedienung wartet auf neue Antwort"
            value.fault -> "LED-Ausgabe meldet einen Fehler"
            commands.request != null -> "Befehl gesendet · warte auf Boxbestätigung …"
            value.mode in 2..4 -> "LED-Start gesperrt · Spiel nicht vollständig gestoppt"
            value.completed && value.off -> "Box meldet: Effekt beendet, LEDs aus"
            else -> lastMessage
        }
        update { it.copy(available = true, authenticated = authenticated, current = fresh, ready = value.ready && !value.fault,
            gameStopped = if (value.mode == 0 || value.mode == 4) null else value.mode == 1,
            ledOnly = value.mode == 0, busy = nfcPending != null || commands.request != null || value.pending || outgoing.isNotEmpty(),
            active = LedEffect.entries.singleOrNull { effect -> effect.wire == value.effect }, message = notice,
            wake = wake.state(SystemClock.elapsedRealtime(), authenticated), nfc = nfcState) }
    }
    fun command(effect: Int) = runSafely {
        check(authenticated && nfcPending == null && commands.request == null && outgoing.isEmpty()) { "Ein LED-Auftrag läuft bereits." }
        val value = snapshot ?: error("Bestätigter Boxstatus fehlt.")
        check(SystemClock.elapsedRealtime() - receivedAt in 0..999) { "Boxstatus ist zu alt. Erneut verbinden." }
        val message = protocol.command(value, effect)
        commands.begin(value.nextRequest)
        val token = ++operation
        lastMessage = "Befehl gesendet · warte auf Boxbestätigung …"
        send(message) { commands.wrote()?.let(::completed); publish() }
        publish()
        handler.postDelayed({ if (alive && token == operation && commands.request != null) failed("LED-Ergebnis unbekannt · Verbindung beendet. Erneut verbinden und bewusst starten.") }, 5000)
    }
    fun nfcCommand(change: Boolean) = runSafely {
        check(authenticated && nfcState.supported && nfcPending == null && commands.request == null && outgoing.isEmpty()) { "Box ist noch beschäftigt oder unterstützt NFC nicht." }
        val value = snapshot ?: error("Boxstatus fehlt.")
        check(SystemClock.elapsedRealtime() - receivedAt in 0..999 && value.off && !value.pending) { "Zuerst LEDs stoppen und frischen Boxstatus abwarten." }
        val uid = if (change) {
            check(nfcState.canChange) { "Zuerst denselben Tag mit Flags 1 an der Box lesen." }
            nfcState.result!!.uid.copyOf()
        } else null
        val message = protocol.nfcCommand(nfcRequest, value.deviceMillis, uid)
        nfcPending = nfcRequest
        nfcState = nfcState.copy(busy = true, result = null, message = if (change) "Box schreibt einmalig · warte auf frisches Rücklesen …" else "Tag an die Box halten · Box liest …")
        val request = nfcRequest
        send(message); publish()
        handler.postDelayed({ if (alive && nfcPending == request) failed("NFC-Ausgang unklar · neu verbinden und zuerst nur lesen. Kein Auftrag wird automatisch wiederholt.") }, 45000)
    }
    private fun completed(result: LedResult) {
        lastMessage = when (result.outcome) {
            0 -> if (result.status.off) "Box bestätigt: LEDs aus" else "Box bestätigt: ${LedEffect.entries.single { it.wire == result.status.effect }.label} gestartet"
            1 -> "Box lehnt ab: Spiel ist nicht gestoppt"
            2 -> "Boxzustand hat sich geändert · erneut bewusst starten"
            4 -> "Box lehnt ab: LED-Ausgabe nicht bereit"
            8 -> "Box lehnt ab: Auftrag abgelaufen"
            9 -> "Box ist beschäftigt · erneut versuchen"
            10 -> "Box meldet einen Fehler der LED-Ausgabe"
            11 -> "Auftrag wurde durch eine andere Bedienung beendet"
            else -> "Box hat den LED-Auftrag abgelehnt (${result.outcome})"
        }
    }
    private fun send(message: ByteArray, complete: () -> Unit = {}) {
        check(transfer < 65535) { "Verbindung muss erneuert werden." }
        outgoing.addLast(Transfer(LedFrames.encode(message, ++transfer), complete))
        writeNext()
    }
    private fun writeNext() {
        if (writing || outgoing.isEmpty()) return
        val frame = outgoing.first().let { it.frames[it.index] }
        writing = true
        check(connection.writeCharacteristic(input!!, frame, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS) { "LED-Nachricht konnte nicht gesendet werden." }
    }
    fun close() { if (!alive) return; alive = false; protocol.close(); wake.clear(); outgoing.clear(); input = null; output = null }
    companion object {
        val SERVICE: UUID = UUID.fromString("6c790001-2f50-4d5a-a7f4-35a478c4b101")
        val COMMAND: UUID = UUID.fromString("6c790002-2f50-4d5a-a7f4-35a478c4b101")
        val OUTPUT: UUID = UUID.fromString("6c790003-2f50-4d5a-a7f4-35a478c4b101")
        val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }
}
