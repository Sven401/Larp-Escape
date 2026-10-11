package de.larpescape.protocol

import org.junit.Assert.*
import org.junit.Test

class DiagnosticGattLinkV1Test {
    private val route = DiagnosticGattFirmwareV1.route
    private val writes = setOf(route.controlRequest, route.stepRequest)
    private val replies = setOf(route.controlReply, route.stepReply)

    @Test fun fixedFirmwareRouteAndExactAttBoundary() {
        assertEquals("407e6a1d-1cc2-43fd-856b-4aff85a20010", route.service.toString())
        assertEquals("407e6a1d-1cc2-43fd-856b-4aff85a20011", route.controlRequest.toString())
        assertEquals("407e6a1d-1cc2-43fd-856b-4aff85a20012", route.controlReply.toString())
        assertEquals("407e6a1d-1cc2-43fd-856b-4aff85a20013", route.stepRequest.toString())
        assertEquals("407e6a1d-1cc2-43fd-856b-4aff85a20014", route.stepReply.toString())
        val tooSmall = DiagnosticGattLinkV1(1)
        assertTrue(tooSmall.discovered(route.service, writes, replies, replies))
        assertFalse(tooSmall.mtu(130, true))
        assertEquals(DiagnosticGattFailureV1.MTU_TOO_SMALL, tooSmall.failure)
        assertFalse(tooSmall.canReceive(1))
        val enough = DiagnosticGattLinkV1(2)
        assertTrue(enough.discovered(route.service, writes, replies, replies))
        assertTrue(enough.mtu(131, true))
        assertEquals(128, enough.effectiveAttValueBytes)
    }

    @Test fun descriptorAcksAreSequentialButNeverProveNotificationTransport() {
        val link = DiagnosticGattLinkV1(4)
        assertTrue(link.discovered(route.service, writes, replies, replies))
        assertFalse(link.descriptorWriteAcknowledged(route.controlReply, true))
        assertTrue(link.mtu(247, true))
        assertFalse(link.descriptorWriteAcknowledged(route.stepReply, true))
        assertTrue(link.descriptorWriteAcknowledged(route.controlReply, true))
        assertEquals(DiagnosticGattPhaseV1.STEP_CCCD_PENDING, link.phase)
        assertTrue(link.descriptorWriteAcknowledged(route.stepReply, true))
        assertEquals(DiagnosticGattPhaseV1.CCCD_WRITES_ACKNOWLEDGED, link.phase)
        assertFalse(link.preflight().isEmpty())
        assertFalse(link.canReceive(4))
        assertFalse(link.canReceive(3))
        link.failed(DiagnosticGattFailureV1.DISCONNECTED)
        assertFalse(link.canReceive(4))
        assertFalse(link.descriptorWriteAcknowledged(route.stepReply, true))
    }

    @Test fun missingGatesAndFailuresNeverBecomeReady() {
        val cases = listOf(
            Triple(null, writes, replies),
            Triple(route.service, writes - route.stepRequest, replies),
            Triple(route.service, writes, replies - route.stepReply),
        )
        cases.forEach { (service, w, n) ->
            val link = DiagnosticGattLinkV1(9)
            assertFalse(link.discovered(service, w, n, replies))
            assertEquals(DiagnosticGattPhaseV1.BLOCKED, link.phase)
            assertFalse(link.canReceive(9))
        }
        val noCccd = DiagnosticGattLinkV1(10)
        assertFalse(noCccd.discovered(route.service, writes, replies, replies - route.controlReply))
        assertEquals(DiagnosticGattFailureV1.MISSING_CCCD, noCccd.failure)
        val rejected = DiagnosticGattLinkV1(11)
        assertTrue(rejected.discovered(route.service, writes, replies, replies))
        assertFalse(rejected.mtu(131, false))
        assertEquals(DiagnosticGattFailureV1.MTU_CALLBACK_FAILED, rejected.failure)
        val cccdFailure = DiagnosticGattLinkV1(12)
        assertTrue(cccdFailure.discovered(route.service, writes, replies, replies))
        assertTrue(cccdFailure.mtu(131, true))
        assertFalse(cccdFailure.descriptorWriteAcknowledged(route.controlReply, false))
        assertEquals(DiagnosticGattFailureV1.CONTROL_CCCD_FAILED, cccdFailure.failure)
    }
}
