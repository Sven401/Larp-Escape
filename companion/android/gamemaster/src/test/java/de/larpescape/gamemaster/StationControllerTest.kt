package de.larpescape.gamemaster

import de.larpescape.gamemaster.station.*
import de.larpescape.protocol.LedAccess
import de.larpescape.protocol.LedStatus
import org.junit.Assert.*
import org.junit.Test

class StationControllerTest {
    private val one = Station("one","Veranstaltung","Tor","AA:BB:CC:DD:EE:01","11".repeat(16))
    private val two = Station("two","Veranstaltung","Turm","AA:BB:CC:DD:EE:02","22".repeat(16))
    private fun access() = LedAccess.parse(("{\"schemaVersion\":1,\"deviceId\":\"" + "11".repeat(16) + "\",\"secret\":\"" + "22".repeat(32) + "\"}").toByteArray())
    private fun status(seq: Long = 1) = LedStatus(0,1,50,seq,0,5,0)
    private class Clock : StationScheduler {
        var now = 0L
        data class Task(val time: Long, val action: () -> Unit, var cancelled: Boolean = false)
        val tasks = mutableListOf<Task>()
        override fun after(milliseconds: Long,action: () -> Unit): Cancellation {
            val task = Task(now + milliseconds,action); tasks.add(task)
            return Cancellation { task.cancelled = true }
        }
        fun advance(ms: Long) {
            val end = now + ms
            while (true) {
                val task = tasks.filter { !it.cancelled && it.time <= end }.minByOrNull { it.time } ?: break
                tasks.remove(task); now = task.time; task.action()
            }
            now = end
        }
    }
    private class Link : StationLink {
        val visits = mutableListOf<Station>()
        val callbacks = mutableListOf<StationLink.Listener>()
        override fun connect(station: Station,access: LedAccess,listener: StationLink.Listener) { visits.add(station); callbacks.add(listener) }
        override fun close() {}
    }
    private val clock = Clock()
    private val link = Link()
    private val model = StationController(link,{ access() },{ clock.now },clock)
    private fun open() { model.environment(true,true,true); model.foreground(true); model.select(one) }
    @Test fun oneSelectionConnectsOnlyItsPreparedStationAndWaitsForProof() {
        open(); assertEquals(listOf(one),link.visits)
        assertEquals(StationPhase.CONNECTING,model.state.value.phase); assertNull(model.state.value.status)
        link.callbacks.last().confirmed(status()); assertEquals(status(),model.state.value.status)
    }
    @Test fun statusExpiresAtOneSecondAndNeverClaimsOffAfterExpiry() {
        open(); link.callbacks.last().confirmed(status()); clock.advance(999)
        assertEquals(StationPhase.CONFIRMED,model.state.value.phase)
        clock.advance(1); assertEquals(StationPhase.STALE,model.state.value.phase); assertNull(model.state.value.status)
        assertTrue(model.state.value.recovering)
    }
    @Test fun duplicateStatusDoesNotRenewFreshness() {
        open(); link.callbacks.last().confirmed(status()); clock.advance(900)
        link.callbacks.last().confirmed(status()); clock.advance(100)
        assertEquals(StationPhase.STALE,model.state.value.phase)
    }
    @Test fun newerStatusRenewsFreshness() {
        open(); link.callbacks.last().confirmed(status()); clock.advance(900)
        link.callbacks.last().confirmed(status(2)); clock.advance(101)
        assertEquals(StationPhase.CONFIRMED,model.state.value.phase)
        clock.advance(899); assertEquals(StationPhase.STALE,model.state.value.phase)
    }
    @Test fun oldStationCallbackCannotPopulateNewStation() {
        open(); val old = link.callbacks.last(); model.select(two); old.confirmed(status(9))
        assertEquals(two,model.state.value.station); assertNull(model.state.value.status)
        old.failed(LinkFailure.ACCESS); assertEquals(StationPhase.CONNECTING,model.state.value.phase)
    }
    @Test fun cancellationSuppressesLateCallbacksAndForegroundReconnect() {
        open(); val old = link.callbacks.last(); model.pause(); old.confirmed(status())
        model.foreground(false); model.foreground(true); clock.advance(30000)
        assertEquals(1,link.visits.size); assertEquals(StationPhase.PAUSED,model.state.value.phase)
    }
    @Test fun returningToListCancelsPendingRecovery() {
        open(); link.callbacks.last().confirmed(status()); link.callbacks.last().failed(LinkFailure.INTERRUPTED)
        model.stations(); clock.advance(30000)
        assertEquals(1,link.visits.size); assertNull(model.state.value.station)
    }
    @Test fun backgroundInvalidatesStatusAndReturnStartsNewGeneration() {
        open(); val old = link.callbacks.last(); old.confirmed(status()); model.foreground(false)
        assertNull(model.state.value.status); model.foreground(true); old.confirmed(status(99))
        assertNull(model.state.value.status); assertEquals(2,link.visits.size)
        link.callbacks.last().confirmed(status()); assertEquals(StationPhase.CONFIRMED,model.state.value.phase)
    }
    @Test fun failedInitialDiscoveryDoesNotLoop() {
        open(); link.callbacks.last().failed(LinkFailure.UNREACHABLE); clock.advance(60000)
        assertEquals(1,link.visits.size); assertEquals(StationPhase.UNREACHABLE,model.state.value.phase)
    }
    @Test fun exactlyOneAutomaticRecoveryAndNoInfiniteReconnectAfterBriefSuccess() {
        open(); link.callbacks.last().confirmed(status()); link.callbacks.last().failed(LinkFailure.INTERRUPTED)
        clock.advance(700); assertEquals(2,link.visits.size)
        link.callbacks.last().confirmed(status()); link.callbacks.last().failed(LinkFailure.INTERRUPTED)
        clock.advance(60000); assertEquals(2,link.visits.size); assertFalse(model.state.value.recovering)
        model.retry(); assertEquals(3,link.visits.size)
    }
    @Test fun permissionFixConnectsAutomaticallyButNotAfterCancellation() {
        model.foreground(true); model.select(one); assertEquals(StationPhase.PERMISSION,model.state.value.phase)
        model.environment(true,true,true); assertEquals(1,link.visits.size)
        link.callbacks.last().confirmed(status()); model.environment(false,true,true)
        assertNull(model.state.value.status); model.pause(); model.environment(true,true,true)
        assertEquals(1,link.visits.size)
    }
    @Test fun bluetoothFixContinuesSelectedStationWithoutManualScan() {
        model.environment(true,true,false); model.foreground(true); model.select(one)
        assertEquals(StationPhase.BLUETOOTH_OFF,model.state.value.phase)
        model.environment(true,true,true); assertEquals(listOf(one),link.visits)
    }
    @Test fun credentialFailureIsNotBypassedByRetry() {
        val missing = StationController(link,{ error("missing") },{ clock.now },clock)
        missing.environment(true,true,true); missing.foreground(true); missing.select(one); missing.retry()
        assertEquals(StationPhase.ACCESS,missing.state.value.phase); assertTrue(link.visits.isEmpty())
    }
    @Test fun assignmentAndAuthenticationFailureRemainBlocked() {
        for (failure in listOf(LinkFailure.ASSIGNMENT,LinkFailure.ACCESS)) {
            open(); link.callbacks.last().failed(failure); val count = link.visits.size; clock.advance(10000)
            assertEquals(count,link.visits.size); assertNull(model.state.value.status)
        }
    }
    @Test fun duplicatePreparedBoxesAndStationNamesAreRejected() {
        for (bad in listOf(two.copy(address = one.address),two.copy(deviceId = one.deviceId),two.copy(name = one.name))) {
            try { Station.validateCatalog(listOf(one,bad)); fail("duplicate accepted") } catch (_: IllegalArgumentException) {}
        }
    }
    @Test fun addressAndNamesAreValidated() {
        assertEquals("AA:BB:CC:DD:EE:01",Station.normalizeAddress(" aa:bb:cc:dd:ee:01 "))
        for (address in listOf("not a box","00:00:00:00:00:00","FF:FF:FF:FF:FF:FF")) {
            try { Station.normalizeAddress(address); fail("bad locator accepted") } catch (_: IllegalArgumentException) {}
        }
        try { one.copy(name = "\n"); fail("control character accepted") } catch (_: IllegalArgumentException) {}
    }
}
