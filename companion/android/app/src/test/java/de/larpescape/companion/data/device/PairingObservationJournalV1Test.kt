package de.larpescape.companion.data.device

import org.junit.Assert.*
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class PairingObservationJournalV1Test {
    @get:Rule val folder = TemporaryFolder()

    private fun paired() = PairingObservationReportV1.start("2026-10-11T10:00:00Z", "report-1")
        .observe("t1", "ANDROID_APP_USER_ACTION", 4, "AA", "connection_attempt", "connectGatt_requested")
        .observe("t2", "ANDROID_BLUETOOTH_GATT_CALLBACK", 4, "AA", "connection_state", "status=0;state=2")
        .observe("t3", "ANDROID_BLUETOOTH_GATT_SERVICE_TABLE", 4, "AA", "product_service_present", "true")
        .observe("t4", "ANDROID_BOND_STATE_BROADCAST", 4, "AA", "bond_state", "OS_BONDED")

    @Test fun relaunchKeepsEvidenceButClosesLiveAssessment() {
        val dir = folder.newFolder("journal")
        PairingObservationJournalV1(dir).save(paired())
        val recovered = PairingObservationJournalV1(dir).loadRecovered("t5")!!
        assertEquals("report-1", recovered.reportId)
        assertEquals("OS_BONDED", recovered.events[3].value)
        assertEquals("ANDROID_APP_RECOVERY", recovered.events.last().source)
        assertEquals("NOT_CONNECTED", recovered.androidAssessment().code)
        val again = PairingObservationJournalV1(dir).loadRecovered("t6")!!
        assertEquals("NOT_CONNECTED", again.androidAssessment().code)
        assertEquals("OPEN", again.androidAssessment().firmwareSecurityEvidence)
    }

    @Test fun pendingExportSurvivesNewInstanceAndUsesOriginalSnapshot() {
        val dir = folder.newFolder("export")
        val first = PairingObservationJournalV1(dir)
        first.save(paired())
        val snapshot = first.prepareExport(paired(), "t5")
        val second = PairingObservationJournalV1(dir)
        assertArrayEquals(snapshot, second.pendingExport())
        val recovered = second.loadRecovered("t6")!!
        assertEquals("NOT_CONNECTED", recovered.androidAssessment().code)
        assertArrayEquals(snapshot, second.pendingExport())
        second.clearPendingExport()
        assertNull(second.pendingExport())
    }

    @Test fun unknownOrOversizedJournalNeverReopensAConnection() {
        val dir = folder.newFolder("invalid")
        val file = File(dir, "ble-pairing-observation-v1.json")
        file.writeText("{\"storageSchema\":\"future\",\"report\":{}}")
        assertThrows(IllegalArgumentException::class.java) { PairingObservationJournalV1(dir).loadRecovered("t5") }
        file.writeBytes(ByteArray(PairingObservationJournalV1.MAX_BYTES + 1))
        assertThrows(IllegalArgumentException::class.java) { PairingObservationJournalV1(dir).loadRecovered("t5") }
    }

    @Test fun invalidPendingSnapshotIsNotReturnedForExport() {
        val dir = folder.newFolder("bad-export")
        File(dir, "ble-pairing-export-pending-v1.json").writeText("{\"schema\":\"unknown\"}")
        assertThrows(IllegalArgumentException::class.java) { PairingObservationJournalV1(dir).pendingExport() }
    }
}
