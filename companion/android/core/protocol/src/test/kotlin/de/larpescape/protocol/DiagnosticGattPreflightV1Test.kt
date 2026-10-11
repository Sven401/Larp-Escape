package de.larpescape.protocol

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.UUID

class DiagnosticGattPreflightV1Test {
    private fun id(last: Int): UUID = UUID.fromString("00000000-0000-4000-8000-${last.toString().padStart(12, '0')}")
    private val route = DiagnosticGattRouteV1(id(1), id(2), id(3), id(4), id(5))
    private val observed = DiagnosticGattObservationV1(id(1), setOf(id(2), id(4)), setOf(id(3), id(5)), 128)

    @Test fun noReviewedFirmwareRouteCannotOpenTransport() {
        assertEquals(setOf(DiagnosticGattBlockV1.MISSING_REVIEWED_ROUTE), DiagnosticGattPreflightV1.blocks(null, observed))
        assertEquals(setOf(DiagnosticGattBlockV1.MISSING_CONNECTION_OBSERVATION), DiagnosticGattPreflightV1.blocks(route, null))
    }

    @Test fun everyRequiredWriteAndReplyMustBePresentOnOriginalConnection() {
        assertTrue(DiagnosticGattPreflightV1.blocks(route, observed).isEmpty())
        assertEquals(setOf(DiagnosticGattBlockV1.WRONG_SERVICE), DiagnosticGattPreflightV1.blocks(route, observed.copy(service = id(6))))
        for (write in observed.writableWithResponse) {
            assertEquals(setOf(DiagnosticGattBlockV1.MISSING_WRITE),
                DiagnosticGattPreflightV1.blocks(route, observed.copy(writableWithResponse = observed.writableWithResponse - write)))
        }
        for (reply in observed.subscribedNotifications) {
            assertEquals(setOf(DiagnosticGattBlockV1.MISSING_REPLY_SUBSCRIPTION),
                DiagnosticGattPreflightV1.blocks(route, observed.copy(subscribedNotifications = observed.subscribedNotifications - reply)))
        }
    }

    @Test fun attBudgetMustCarryLargestSingleV1Frame() {
        assertEquals(128, DiagnosticGattPreflightV1.MIN_ATT_VALUE_BYTES)
        assertTrue(DiagnosticControlV1.REQUEST_BYTES <= 128)
        assertTrue(DiagnosticControlV1.REPLY_BYTES <= 128)
        assertTrue(DiagnosticStepV1.REQUEST_BYTES <= 128)
        assertEquals(setOf(DiagnosticGattBlockV1.INSUFFICIENT_ATT_VALUE),
            DiagnosticGattPreflightV1.blocks(route, observed.copy(effectiveAttValueBytes = 127)))
        assertTrue(DiagnosticGattPreflightV1.blocks(route, observed.copy(effectiveAttValueBytes = 128)).isEmpty())
    }
}
