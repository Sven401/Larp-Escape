package de.larpescape.companion.platform.ble

import org.junit.Assert.*
import org.junit.Test

class NearGlowConnectionSetupTest {
    @Test fun acceptedMtuRequestDoesNotDiscoverUntilNegotiationAndDuplicateCannotRestart() {
        val order = mutableListOf<String>()
        val setup = NearGlowConnectionSetup()
        setup.begin({ order.add("HIGH"); true }, { order.add("MTU request"); true })
        assertEquals(listOf("HIGH", "MTU request"), order)
        assertNull(setup.negotiatedMtu); assertEquals(20, setup.maxFrameBytes)
        assertTrue(setup.mtuChanged(true, 256) { order.add("discover"); true })
        assertEquals(listOf("HIGH", "MTU request", "discover"), order)
        assertEquals(256, setup.negotiatedMtu); assertEquals(244, setup.maxFrameBytes)
        assertFalse(setup.mtuChanged(true, 256) { error("Duplicate restarted discovery") })
        assertThrows(IllegalStateException::class.java) { setup.mtuChanged(true, 247) { true } }
        assertThrows(IllegalStateException::class.java) { setup.begin({ true }, { true }) }
    }

    @Test fun minimumAndMaximumNegotiatedBudgetsAreEnforced() {
        for ((mtu, budget) in listOf(156 to 153, 247 to 244, 256 to 244, 517 to 244)) {
            val setup = NearGlowConnectionSetup(); setup.begin({ true }, { true })
            setup.mtuChanged(true, mtu) { true }
            assertEquals(budget, setup.maxFrameBytes)
        }
        assertThrows(IllegalStateException::class.java) { NearGlowConnectionSetup().mtuChanged(true, 256) { true } }
        val oversized = NearGlowConnectionSetup(); oversized.begin({ true }, { true })
        assertThrows(IllegalArgumentException::class.java) { oversized.mtuChanged(true, 518) { true } }
    }

    @Test fun rejectedRequestsSdkExceptionsAndInvalidMtuCallbacksStayInsideProductionGuard() {
        for (failure in 0..6) {
            val queue = ArrayDeque<() -> Unit>()
            var caught: Exception? = null; var active = true; var completed = false
            val calls = mutableListOf<String>()
            val setup = NearGlowConnectionSetup()
            val guard = NearGlowCallbackDispatcher({ queue.addLast(it) }, { active }, { true }, { _, error ->
                caught = error; active = false
            })
            guard.dispatch(NearGlowCallbackStage.CONNECTION) {
                setup.begin({
                    calls.add("HIGH")
                    if (failure == 1) throw SecurityException()
                    failure != 0
                }, {
                    calls.add("MTU request")
                    if (failure == 3) throw SecurityException()
                    failure != 2
                })
                setup.mtuChanged(failure != 4, if (failure == 5) 155 else 256) {
                    calls.add("discover"); failure != 6
                }
                completed = true
            }
            while (queue.isNotEmpty()) queue.removeFirst().invoke()
            assertFalse(completed); assertFalse(active); assertNotNull(caught)
            if (failure == 0) assertTrue(caught is NearGlowPriorityRequestRejected)
            if (failure == 1 || failure == 3) assertTrue(caught is SecurityException)
            if (failure == 2) assertTrue(caught is NearGlowMtuRequestRejected)
            if (failure == 5) assertTrue(caught is NearGlowMtuInsufficient)
            assertEquals(when (failure) {
                0, 1 -> listOf("HIGH")
                6 -> listOf("HIGH", "MTU request", "discover")
                else -> listOf("HIGH", "MTU request")
            }, calls)
        }
    }

    @Test fun oldQueuedMtuCallbackCannotDiscoverOrCloseNewSession() {
        val queue = ArrayDeque<() -> Unit>()
        val old = Any(); val next = Any(); var current: Any? = old
        val setup = NearGlowConnectionSetup(); setup.begin({ true }, { true })
        var discovered = 0; var failures = 0
        val guard = NearGlowCallbackDispatcher({ queue.addLast(it) }, { current === old }, { true }, { _, _ -> failures++; current = null })
        guard.dispatch(NearGlowCallbackStage.MTU) { setup.mtuChanged(true, 256) { discovered++; true } }
        current = next
        while (queue.isNotEmpty()) queue.removeFirst().invoke()
        assertSame(next, current); assertEquals(0, discovered); assertEquals(0, failures)
        assertNull(setup.negotiatedMtu)
    }
}
