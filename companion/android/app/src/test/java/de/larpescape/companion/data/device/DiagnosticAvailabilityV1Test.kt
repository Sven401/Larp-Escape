package de.larpescape.companion.data.device

import de.larpescape.protocol.DiagnosticGattFailureV1
import de.larpescape.protocol.DiagnosticGattPhaseV1
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test

class DiagnosticAvailabilityV1Test {
    @Test fun observedFailuresHaveDistinctUserVisibleReasons() {
        val expected = mapOf(
            DiagnosticGattFailureV1.WRONG_SERVICE to DiagnosticAvailabilityV1.SERVICE_MISSING,
            DiagnosticGattFailureV1.MISSING_CHARACTERISTIC to DiagnosticAvailabilityV1.CHARACTERISTIC_MISSING,
            DiagnosticGattFailureV1.AMBIGUOUS_CHARACTERISTIC to DiagnosticAvailabilityV1.CHARACTERISTIC_AMBIGUOUS,
            DiagnosticGattFailureV1.WRONG_PROPERTIES to DiagnosticAvailabilityV1.PROPERTIES_WRONG,
            DiagnosticGattFailureV1.MISSING_CCCD to DiagnosticAvailabilityV1.CCCD_MISSING,
            DiagnosticGattFailureV1.MTU_TOO_SMALL to DiagnosticAvailabilityV1.MTU_TOO_SMALL,
            DiagnosticGattFailureV1.SETUP_TIMEOUT to DiagnosticAvailabilityV1.SETUP_TIMEOUT,
        )
        expected.forEach { (failure, availability) ->
            assertEquals(availability, diagnosticAvailability(DiagnosticGattPhaseV1.BLOCKED, failure, "LINK_BLOCKED_$failure"))
            assertFalse(availability.detail.isBlank())
        }
    }

    @Test fun actualTransportSetupNeverBecomesDevicePermission() {
        assertEquals(DiagnosticAvailabilityV1.MTU_PENDING,
            diagnosticAvailability(DiagnosticGattPhaseV1.MTU_PENDING, DiagnosticGattFailureV1.NONE, "LINK_MTU_PENDING_NONE"))
        assertEquals(DiagnosticAvailabilityV1.CONTROL_SUBSCRIPTION_PENDING,
            diagnosticAvailability(DiagnosticGattPhaseV1.CONTROL_CCCD_PENDING, DiagnosticGattFailureV1.NONE, "LINK_CONTROL_CCCD_PENDING_NONE"))
        assertEquals(DiagnosticAvailabilityV1.SUBSCRIPTION_UNPROVEN,
            diagnosticAvailability(DiagnosticGattPhaseV1.CCCD_WRITES_ACKNOWLEDGED, DiagnosticGattFailureV1.NONE, "SUBSCRIPTION_UNPROVEN"))
        assertEquals(DiagnosticAvailabilityV1.ROLE_MISSING,
            diagnosticAvailability(DiagnosticGattPhaseV1.TRANSPORT_READY, DiagnosticGattFailureV1.NONE, "ROLE_ISSUER_MISSING"))
        assertEquals(DiagnosticAvailabilityV1.ROLE_CHANGED,
            diagnosticAvailability(DiagnosticGattPhaseV1.TRANSPORT_READY, DiagnosticGattFailureV1.NONE, "ROLE_GENERATION_MISMATCH"))
        assertEquals(DiagnosticAvailabilityV1.TRANSPORT_ONLY,
            diagnosticAvailability(DiagnosticGattPhaseV1.TRANSPORT_READY, DiagnosticGattFailureV1.NONE, "NONE_TRANSPORT_ONLY"))
        assertEquals(DiagnosticAvailabilityV1.DISCONNECTED, DeviceUiState().diagnosticAvailability)
    }
}
