package de.larpescape.gamemaster.station

import de.larpescape.protocol.LedAccess
import de.larpescape.protocol.LedStatus
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

data class Station(val id: String, val event: String, val name: String, val address: String, val deviceId: String) {
    init {
        require(id.matches(Regex("[A-Za-z0-9_-]{1,64}")))
        require(event.length in 1..80 && name.length in 1..64 && (event + name).none { it.isISOControl() }) { "Veranstaltung und Station benötigen einen gültigen Namen." }
        require(address == normalizeAddress(address))
        require(deviceId.matches(Regex("[0-9a-f]{32}")) && deviceId.any { it != '0' })
    }
    companion object {
        fun normalizeAddress(value: String): String = value.trim().uppercase(java.util.Locale.ROOT).also {
            require(it.matches(Regex("[0-9A-F]{2}(:[0-9A-F]{2}){5}")) && it != "00:00:00:00:00:00" && it != "FF:FF:FF:FF:FF:FF") { "Gültige Bluetoothadresse der zugeordneten Box eingeben." }
        }
        fun validateCatalog(stations: List<Station>) {
            require(stations.size <= 32) { "Auf diesem Handy sind höchstens 32 Stationen vorbereitet." }
            require(stations.map { it.id }.distinct().size == stations.size)
            require(stations.map { it.address }.distinct().size == stations.size && stations.map { it.deviceId }.distinct().size == stations.size) { "Diese Box ist bereits einer Station zugeordnet." }
            require(stations.map { it.event.lowercase(java.util.Locale.ROOT) to it.name.lowercase(java.util.Locale.ROOT) }.distinct().size == stations.size) { "Diese Station ist bereits vorhanden." }
        }
    }
}

enum class StationPhase { LIST, PAUSED, BACKGROUND, PERMISSION, BLUETOOTH_OFF, UNSUPPORTED, CONNECTING, CONFIRMED, INTERRUPTED, STALE, UNREACHABLE, ACCESS, ASSIGNMENT }
data class StationState(val station: Station? = null, val phase: StationPhase = StationPhase.LIST, val status: LedStatus? = null, val recovering: Boolean = false)
enum class LinkFailure { UNREACHABLE, ACCESS, ASSIGNMENT, INTERRUPTED, STALE, PERMISSION, BLUETOOTH_OFF }
fun interface Cancellation { fun cancel() }
fun interface StationScheduler { fun after(milliseconds: Long, action: () -> Unit): Cancellation }
interface StationLink {
    interface Listener { fun confirmed(status: LedStatus); fun failed(reason: LinkFailure) }
    fun connect(station: Station, access: LedAccess, listener: Listener)
    fun close()
}

/** One selected station, one foreground connection, one bounded recovery. Never sends an action. */
class StationController(
    private val link: StationLink,
    private val credentials: (Station) -> LedAccess,
    private val clock: () -> Long,
    private val scheduler: StationScheduler,
) {
    private val mutable = MutableStateFlow(StationState())
    val state = mutable.asStateFlow()
    private var foreground = false
    private var wanted = false
    private var permission = false
    private var bluetooth = false
    private var supported = true
    private var generation = 0L
    private var automaticRecovery = 1
    private var sequence = 0L
    private var expiry: Cancellation? = null
    private var recovery: Cancellation? = null

    fun environment(permission: Boolean, supported: Boolean, bluetooth: Boolean) {
        val changed = this.permission != permission || this.supported != supported || this.bluetooth != bluetooth
        this.permission = permission; this.supported = supported; this.bluetooth = bluetooth
        if (!foreground || !wanted || state.value.station == null || !changed) return
        if (!permission || !supported || !bluetooth || state.value.phase in setOf(StationPhase.PERMISSION, StationPhase.BLUETOOTH_OFF, StationPhase.UNSUPPORTED)) begin()
    }
    fun foreground(value: Boolean) {
        if (foreground == value) return
        foreground = value
        if (!wanted || state.value.station == null) return
        if (value) { automaticRecovery = 1; begin() }
        else halt(StationPhase.BACKGROUND)
    }
    fun select(station: Station) {
        stop(); mutable.value = StationState(station, StationPhase.PAUSED)
        wanted = true; automaticRecovery = 1
        if (foreground) begin()
    }
    fun retry() {
        if (state.value.station == null) return
        wanted = true; automaticRecovery = 1
        if (foreground) begin()
    }
    fun pause() { wanted = false; halt(StationPhase.PAUSED) }
    fun stations() { wanted = false; stop(); mutable.value = StationState() }
    fun close() { wanted = false; stop() }
    private fun stop() {
        generation++; expiry?.cancel(); expiry = null; recovery?.cancel(); recovery = null
        link.close(); sequence = 0
    }
    private fun halt(phase: StationPhase) {
        stop(); mutable.value = state.value.copy(phase = phase, status = null, recovering = false)
    }
    private fun begin() {
        val station = state.value.station ?: return
        if (!foreground || !wanted) return
        val blocked = when { !supported -> StationPhase.UNSUPPORTED; !permission -> StationPhase.PERMISSION; !bluetooth -> StationPhase.BLUETOOTH_OFF; else -> null }
        if (blocked != null) { halt(blocked); return }
        halt(StationPhase.CONNECTING)
        val access = try { credentials(station) } catch (_: Exception) { halt(StationPhase.ACCESS); return }
        val token = generation
        try {
            link.connect(station, access, object : StationLink.Listener {
                override fun confirmed(status: LedStatus) {
                    if (generation != token || !wanted || !foreground || status.statusSequence <= sequence) return
                    sequence = status.statusSequence
                    mutable.value = StationState(station, StationPhase.CONFIRMED, status)
                    expiry?.cancel()
                    val received = clock()
                    expiry = scheduler.after(1000) {
                        if (generation == token && clock() - received >= 1000) failed(LinkFailure.STALE)
                    }
                }
                override fun failed(reason: LinkFailure) {
                    if (generation != token || !wanted || !foreground) return
                    val wasConfirmed = state.value.phase == StationPhase.CONFIRMED
                    halt(when (reason) {
                        LinkFailure.PERMISSION -> StationPhase.PERMISSION
                        LinkFailure.BLUETOOTH_OFF -> StationPhase.BLUETOOTH_OFF
                        LinkFailure.ACCESS -> StationPhase.ACCESS
                        LinkFailure.ASSIGNMENT -> StationPhase.ASSIGNMENT
                        LinkFailure.UNREACHABLE -> StationPhase.UNREACHABLE
                        LinkFailure.INTERRUPTED -> StationPhase.INTERRUPTED
                        LinkFailure.STALE -> StationPhase.STALE
                    })
                    if (wasConfirmed && reason in setOf(LinkFailure.INTERRUPTED, LinkFailure.STALE, LinkFailure.UNREACHABLE) && automaticRecovery > 0) {
                        automaticRecovery--
                        mutable.value = state.value.copy(recovering = true)
                        val retryToken = generation
                        recovery = scheduler.after(700) { if (retryToken == generation && wanted && foreground) begin() }
                    }
                }
            })
        } catch (_: SecurityException) { permission = false; halt(StationPhase.PERMISSION) }
        catch (_: Exception) { halt(StationPhase.UNREACHABLE) }
    }
}
