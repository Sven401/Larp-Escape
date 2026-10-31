package de.larpescape.companion.platform.ble

/** The production callback boundary. Predicates are evaluated on the serialized queue. */
internal class NearGlowCallbackDispatcher(
    private val post: (() -> Unit) -> Unit,
    private val isCurrent: () -> Boolean,
    private val hasPermission: () -> Boolean,
    private val failed: () -> Unit,
) {
    fun dispatch(action: () -> Unit) {
        post queued@{
            try {
                if (!isCurrent()) return@queued
                check(hasPermission())
                action()
            } catch (_: Exception) {
                failed()
            }
        }
    }
}
