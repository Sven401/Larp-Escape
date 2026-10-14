package de.larpescape.companion.ui.device

import de.larpescape.companion.data.device.*
import de.larpescape.protocol.StatusFreshness
import kotlinx.coroutines.flow.MutableStateFlow
import org.junit.Assert.*
import org.junit.Test

class DeviceViewModelTest {
    private class FakeRepository : DeviceRepository {
        override val state = MutableStateFlow(DeviceUiState())
        var scans = 0
        var connections = 0
        var closes = 0
        var grants = 0
        var starts = 0
        var stops = 0
        override fun selectLedEffect(effect: LedEffect) { state.value = state.value.copy(leds = state.value.leds.copy(selected = effect)) }
        override fun startLedEffect() { starts++ }
        override fun stopLeds() { stops++ }
        override fun importLedAccess(bytes: ByteArray) { }
        override fun scan() { scans++; state.value = state.value.copy(scanning = true) }
        override fun connect(id: String) {
            connections++
            state.value = state.value.copy(connected = true, status = "sample", freshness = StatusFreshness.CURRENT)
        }
        override fun disconnect() {
            state.value = state.value.copy(connected = false, status = null, freshness = StatusFreshness.DISCONNECTED, leds = state.value.leds.disconnected())
        }
        override fun close() { closes++; disconnect(); state.value = state.value.copy(scanning = false, targets = emptyList()) }
        override fun permissionsChanged(granted: Boolean, denied: Boolean) {
            if (granted) grants++
            if (!granted) close()
            state.value = state.value.copy(permissionMissing = !granted,
                message = if (denied) "denied" else if (granted) "ready" else "required")
        }
    }
    @Test fun denialIsVisibleAndDoesNotStartBle() {
        val repo = FakeRepository(); val model = DeviceViewModel(repo)
        model.foregroundStarted(false); model.permissionRequestStarted(); model.permissionResult(false)
        model.scan(); model.connect("device")
        assertEquals("denied", model.state.value.message)
        assertTrue(model.state.value.permissionMissing)
        assertEquals(0, repo.scans); assertEquals(0, repo.connections)
    }
    @Test fun grantStartsExactlyOneRequestedScanAndRechecksDoNotRestartIt() {
        val repo = FakeRepository(); val model = DeviceViewModel(repo)
        model.foregroundStarted(false); model.permissionRequestStarted(); model.permissionResult(true)
        model.permissionsChanged(true); model.foregroundStarted(true)
        assertEquals(1, repo.scans)
        assertFalse(model.state.value.permissionMissing)
    }
    @Test fun unsolicitedGrantDoesNotStartScan() {
        val repo = FakeRepository(); val model = DeviceViewModel(repo)
        model.foregroundStarted(false); model.permissionResult(true)
        assertEquals(0, repo.scans)
    }
    @Test fun backgroundCancelsPendingPermissionActionEvenIfLateGrantArrives() {
        val repo = FakeRepository(); val model = DeviceViewModel(repo)
        model.foregroundStarted(false); model.permissionRequestStarted(); model.foregroundEnded()
        model.permissionResult(true); model.scan(); model.connect("device")
        assertEquals(0, repo.grants)
        model.foregroundStarted(true)
        assertEquals(0, repo.scans); assertEquals(0, repo.connections)
    }
    @Test fun backgroundClearsStatusAndReturnDoesNotAutoReconnect() {
        val repo = FakeRepository(); val model = DeviceViewModel(repo)
        model.foregroundStarted(true); model.connect("device")
        model.foregroundEnded()
        assertNull(model.state.value.status)
        assertFalse(model.state.value.connected)
        assertEquals(StatusFreshness.DISCONNECTED, model.state.value.freshness)
        model.foregroundStarted(true)
        assertEquals(1, repo.connections); assertEquals(0, repo.scans)
        model.scan(); assertEquals(1, repo.scans)
    }
    @Test fun permissionRevocationClearsDataAndPreventsFurtherActions() {
        val repo = FakeRepository(); val model = DeviceViewModel(repo)
        model.foregroundStarted(true); model.connect("device")
        model.permissionsChanged(false); model.scan(); model.connect("device")
        assertNull(model.state.value.status)
        assertFalse(model.state.value.connected)
        assertTrue(model.state.value.permissionMissing)
        assertEquals(1, repo.connections); assertEquals(0, repo.scans)
    }
    @Test fun recreationKeepsSingleSessionWhileNewModelDoesNotRestoreOldStatus() {
        val repo = FakeRepository(); val model = DeviceViewModel(repo)
        model.foregroundStarted(true); model.connect("device")
        // Configuration recreation retains the ViewModel and skips foregroundEnded.
        model.foregroundStarted(true); model.permissionsChanged(true)
        assertEquals(1, repo.connections); assertEquals(0, repo.closes)
        assertEquals("sample", model.state.value.status)
        assertNull(DeviceViewModel(FakeRepository()).state.value.status)
    }
    @Test fun effectSelectionDoesNotSendAndStartRemainsDeliberate() {
        val repo = FakeRepository(); val model = DeviceViewModel(repo)
        model.foregroundStarted(true)
        repo.state.value = repo.state.value.copy(leds = LedUiState(available = true, authenticated = true,
            current = true, ready = true, ledOnly = true, active = LedEffect.TWINKLE))
        model.selectLedEffect(LedEffect.RIPPLE)
        assertEquals(LedEffect.RIPPLE, model.state.value.leds.selected)
        assertEquals(LedEffect.TWINKLE, model.state.value.leds.active)
        assertEquals(0, repo.starts); assertEquals(0, repo.stops)
        model.startLedEffect(); model.stopLeds(); model.startLedEffect()
        assertEquals(2, repo.starts); assertEquals(1, repo.stops)
    }
    @Test fun unknownOrActiveGameCannotStartButLedOnlyCan() {
        val repo = FakeRepository(); val model = DeviceViewModel(repo)
        model.foregroundStarted(true)
        val base = LedUiState(available = true, authenticated = true, current = true, ready = true)
        listOf(base, base.copy(gameStopped = false), base.copy(gameStopped = true, current = false),
            base.copy(gameStopped = true, authenticated = false), base.copy(ledOnly = true, busy = true),
            base.copy(ledOnly = true, ready = false)).forEach {
            repo.state.value = repo.state.value.copy(leds = it); model.startLedEffect()
        }
        assertEquals(0, repo.starts)
        repo.state.value = repo.state.value.copy(leds = base.copy(gameStopped = true)); model.startLedEffect()
        repo.state.value = repo.state.value.copy(leds = base.copy(ledOnly = true)); model.startLedEffect()
        assertEquals(2, repo.starts)
    }
    @Test fun closingConnectionInvalidatesLedsWithoutRestartOnReturn() {
        val repo = FakeRepository(); val model = DeviceViewModel(repo)
        model.foregroundStarted(true)
        repo.state.value = repo.state.value.copy(leds = LedUiState(available = true, authenticated = true,
            current = true, ready = true, ledOnly = true, active = LedEffect.TWINKLE))
        model.foregroundEnded(); model.foregroundStarted(true); model.startLedEffect(); model.stopLeds()
        assertNull(model.state.value.leds.active)
        assertFalse(model.state.value.leds.authenticated)
        assertFalse(model.state.value.leds.current)
        assertEquals(0, repo.starts); assertEquals(0, repo.stops)
    }
}
