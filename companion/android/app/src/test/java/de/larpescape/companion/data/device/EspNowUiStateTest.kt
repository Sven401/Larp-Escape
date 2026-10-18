package de.larpescape.companion.data.device

import org.junit.Assert.*
import org.junit.Test

class EspNowUiStateTest {
    private val ready = EspNowUiState(peerText="02:03:04:05:06:07",supported=true,current=true,ready=true)
    @Test fun onlyFreshSupportedReadyIdleTargetCanSend() {
        assertTrue(ready.canSend)
        for (state in listOf(ready.copy(supported=false), ready.copy(current=false),ready.copy(ready=false),
            ready.copy(busy=true),ready.copy(measuring=true),ready.copy(peerText=""),ready.copy(peerText="FF:FF:FF:FF:FF:FF"))) assertFalse(state.canSend)
        assertNull(ready.result) // Editing target alone never creates a result.
    }
    @Test fun disconnectRevokesReadinessAndPreservesOnlyTarget() {
        val state=ready.copy(waiting=true,busy=true).disconnected()
        assertFalse(state.canSend); assertFalse(state.waiting); assertNull(state.result)
        assertEquals(ready.peerText,state.peerText); assertTrue(state.message.contains("unbekannt"))
        val measuring=ready.copy(measuring=true,rssiSupported=true,rssiCurrent=true).disconnected()
        assertFalse(measuring.measuring); assertFalse(measuring.rssiSupported); assertFalse(measuring.rssiCurrent)
    }
}
