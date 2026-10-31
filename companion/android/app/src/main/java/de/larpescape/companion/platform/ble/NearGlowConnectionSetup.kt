package de.larpescape.companion.platform.ble

internal class NearGlowPriorityRequestRejected : Exception()

/** An accepted request is not proof of negotiated parameters; inspect the actual link. */
internal fun beginNearGlowConnection(requestHighPriority: () -> Boolean, discoverServices: () -> Boolean) {
    if (!requestHighPriority()) throw NearGlowPriorityRequestRejected()
    check(discoverServices())
}
