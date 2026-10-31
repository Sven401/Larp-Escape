package de.larpescape.companion.platform.access

import android.content.Context

/** Independent encrypted slots; importing B never replaces A or the existing LED access. */
class NearGlowAccessStore(context: Context) {
    private val stores = (0..1).map { slot ->
        LedAccessStore(context, "nearglow-access-v1-$slot.bin", "larpescape-nearglow-access-v1-$slot")
    }
    fun read(slot: Int): ByteArray? { require(slot in 0..1); return stores[slot].read() }
    fun save(slot: Int, bytes: ByteArray) { require(slot in 0..1); stores[slot].save(bytes) }
}
