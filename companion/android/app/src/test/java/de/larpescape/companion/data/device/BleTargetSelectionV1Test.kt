package de.larpescape.companion.data.device

import org.junit.Assert.*
import org.junit.Test

class BleTargetSelectionV1Test {
    @Test fun productScanKeepsFilterButDiagnosticScanMarksForeignDeviceUnverified() {
        assertNull(classifyBleTargetV1(BleScanModeV1.PRODUCT, "[TV] UE55", false, false, true))
        assertEquals(DeviceTargetKindV1.UNVERIFIED,
            classifyBleTargetV1(BleScanModeV1.UNVERIFIED_DIAGNOSTIC, "[TV] UE55", false, false, true))
        assertEquals(DeviceTargetKindV1.PRODUCT_ADVERTISED,
            classifyBleTargetV1(BleScanModeV1.UNVERIFIED_DIAGNOSTIC, "Unbenannt", true, false, true))
        assertEquals(DeviceTargetKindV1.LAB_ADVERTISED,
            classifyBleTargetV1(BleScanModeV1.PRODUCT, "Unbenannt", false, true, true))
        assertNull(classifyBleTargetV1(BleScanModeV1.PRODUCT, "ESP", false, true, false))
    }

    @Test fun unverifiedAndAdvertisedCandidatesNeedCanonicalGattService() {
        for (kind in listOf(DeviceTargetKindV1.UNVERIFIED, DeviceTargetKindV1.PRODUCT_ADVERTISED)) {
            assertFalse(acceptsGattServiceV1(kind, false, false))
            assertFalse(acceptsGattServiceV1(kind, false, true))
            assertTrue(acceptsGattServiceV1(kind, true, false))
        }
        assertTrue(acceptsGattServiceV1(DeviceTargetKindV1.LAB_ADVERTISED, false, true))
        assertFalse(acceptsGattServiceV1(DeviceTargetKindV1.LAB_ADVERTISED, false, false))
    }
}
