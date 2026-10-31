package de.larpescape.companion.platform.ble

import org.junit.Assert.*
import org.junit.Test

class NearGlowConnectionSetupTest {
    @Test fun priorityIsRequestedBeforeDiscoveryAndRejectionDoesNotStartDiscovery() {
        val order = mutableListOf<String>()
        beginNearGlowConnection({ order.add("HIGH"); true }, { order.add("discover"); true })
        assertEquals(listOf("HIGH", "discover"), order)
        order.clear()
        assertThrows(NearGlowPriorityRequestRejected::class.java) {
            beginNearGlowConnection({ order.add("HIGH rejected"); false }, { order.add("discover"); true })
        }
        assertEquals(listOf("HIGH rejected"), order)
    }

    @Test fun prioritySdkExceptionFalseAndDiscoveryFailureUseTheProductionGuard() {
        for (failure in 0..2) {
            val queue = ArrayDeque<() -> Unit>()
            var caught: Exception? = null; var active = true; var completed = false
            val calls = mutableListOf<String>()
            val guard = NearGlowCallbackDispatcher({ queue.addLast(it) }, { active }, { true }, { _, error ->
                caught = error; active = false
            })
            guard.dispatch(NearGlowCallbackStage.CONNECTION) {
                beginNearGlowConnection({
                    calls.add("HIGH")
                    if (failure == 0) throw SecurityException()
                    failure != 1
                }, { calls.add("discover"); false })
                completed = true
            }
            while (queue.isNotEmpty()) queue.removeFirst().invoke()
            assertFalse(completed); assertFalse(active); assertNotNull(caught)
            if (failure == 0) assertTrue(caught is SecurityException)
            if (failure == 1) assertTrue(caught is NearGlowPriorityRequestRejected)
            assertEquals(if (failure == 2) listOf("HIGH", "discover") else listOf("HIGH"), calls)
        }
    }
}
