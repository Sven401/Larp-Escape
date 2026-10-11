package de.larpescape.companion.data.device

import org.junit.Assert.*
import org.junit.Test

class BleScanEvidenceV1Test {
    @Test fun noCallbackAndFilteredCallbacksAreDistinct() {
        assertEquals("reason=complete;result=no_callbacks;callbacks=0;filtered=0;productCandidates=0",
            BleScanEvidenceV1().summary("complete"))
        val filtered = BleScanEvidenceV1()
        repeat(3) { filtered.callback(); filtered.filtered() }
        assertEquals("reason=complete;result=callbacks_filtered;callbacks=3;filtered=3;productCandidates=0",
            filtered.summary("complete"))
    }

    @Test fun onlyCanonicalProductAddressesAreRetainedOnce() {
        val scan = BleScanEvidenceV1()
        scan.callback()
        assertFalse(scan.productCandidate("11:22:33:44:55:66", false))
        assertFalse(scan.productCandidate("foreign device", true))
        assertTrue(scan.productCandidate("A8:42:E3:CD:4B:18", true))
        assertFalse(scan.productCandidate("a8:42:e3:cd:4b:18", true))
        assertEquals("reason=stopped;result=product_advertisement_seen;callbacks=1;filtered=0;productCandidates=1",
            scan.summary("stopped"))
        assertFalse(scan.summary("stopped").contains("11:22:33"))
        assertFalse(scan.summary("stopped").contains("A8:42"))
    }

    @Test fun callbackWithoutCanonicalAdvertisementDoesNotClaimProduct() {
        val scan = BleScanEvidenceV1()
        scan.callback()
        assertEquals("reason=failed;result=no_product_advertisement;callbacks=1;filtered=0;productCandidates=0",
            scan.summary("failed"))
    }
}
