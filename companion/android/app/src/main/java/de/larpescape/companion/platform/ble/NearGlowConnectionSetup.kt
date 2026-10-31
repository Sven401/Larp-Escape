package de.larpescape.companion.platform.ble

internal class NearGlowPriorityRequestRejected : Exception()
internal class NearGlowMtuRequestRejected : Exception()
internal class NearGlowMtuInsufficient : Exception()

/** Serialize HIGH -> MTU request -> negotiated MTU -> discovery. Never assume acceptance is negotiation. */
internal class NearGlowConnectionSetup {
    private enum class Phase { IDLE, MTU_PENDING, DISCOVERING }
    private var phase = Phase.IDLE
    var negotiatedMtu: Int? = null
        private set
    var maxFrameBytes = 20
        private set

    fun begin(requestHighPriority: () -> Boolean, requestMtu: () -> Boolean) {
        check(phase == Phase.IDLE)
        if (!requestHighPriority()) throw NearGlowPriorityRequestRejected()
        phase = Phase.MTU_PENDING
        if (!requestMtu()) throw NearGlowMtuRequestRejected()
    }

    /** False means an identical late notification; it must not restart discovery or assembly. */
    fun mtuChanged(sdkSuccess: Boolean, mtu: Int, discoverServices: () -> Boolean): Boolean {
        check(sdkSuccess)
        if (phase == Phase.DISCOVERING) {
            check(mtu == negotiatedMtu)
            return false
        }
        check(phase == Phase.MTU_PENDING)
        if (mtu < 156) throw NearGlowMtuInsufficient()
        require(mtu <= 517)
        negotiatedMtu = mtu
        maxFrameBytes = minOf(mtu - 3, 244)
        phase = Phase.DISCOVERING
        check(discoverServices())
        return true
    }
}
